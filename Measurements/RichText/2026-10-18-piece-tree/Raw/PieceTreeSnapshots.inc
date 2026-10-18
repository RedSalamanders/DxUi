// Inactive snapshot-method draft; see PieceTree.Design.md.
SourceSnapshot::SourceSnapshot() = default;
std::size_t SourceSnapshot::Length() const noexcept
{
    return state_ ? state_->length : 0u;
}
std::wstring SourceSnapshot::Text() const
{
    return Slice(0u, Length());
}
wchar_t SourceSnapshot::CharacterAt(std::size_t offset) const noexcept
{
    auto* node = state_.get();
    if (!node || offset >= node->length)
        return L'\0';
    while (node->height)
    {
        const auto index = node->Find(offset);
        offset -= node->Start(index);
        node = std::get<State::Branches>(node->entries)[index].node.get();
    }
    const auto index = node->Find(offset);
    const auto& piece = std::get<State::Leaves>(node->entries)[index];
    return (*piece.buffer)[piece.bufferStart + offset - node->Start(index)];
}
bool SourceSnapshot::IsBoundary(std::size_t offset) const noexcept
{
    if (offset > Length())
        return false;
    if (!offset || offset == Length())
        return true;
    return IsEditJoinAllowed(CharacterAt(offset - 1u), CharacterAt(offset));
}
std::wstring SourceSnapshot::Slice(std::size_t first, std::size_t last) const
{
    if (first > last || !IsBoundary(first) || !IsBoundary(last))
        throw std::out_of_range("Invalid snapshot slice");
    std::wstring result;
    result.reserve(last - first);
    State::Visit(state_.get(), first, last, [&](std::wstring_view fragment)
    {
        result.append(fragment);
        return true;
    });
    return result;
}
void Document::Reset(std::wstring text)
{
    Validate(text);
    SourceSnapshot next;
    if (!text.empty())
    {
        auto buffer = std::make_shared<const std::wstring>(std::move(text));
        const SourceSnapshot::State::Fragment fragment{buffer, 0u, buffer->size()};
        next.state_ = SourceSnapshot::State::MakeLeaves(std::span<const SourceSnapshot::State::Fragment>(&fragment, 1u)).first;
    }
    source_ = std::move(next);
    selection_ = {};
    undo_.clear();
    redo_.clear();
    ++revision_;
}
bool Document::ReplaceImpl(std::size_t begin, std::size_t end, std::wstring text, Selection after, bool recordHistory)
{
    Validate(text);
    const auto length = source_.Length();
    if (begin > end || !source_.IsBoundary(begin) || !source_.IsBoundary(end))
        throw std::out_of_range("Invalid replacement range");
    if (text.size() > MaximumUnits - (length - (end - begin)))
        throw std::length_error("Bounded rich-text source limit");
    const auto resultingLength = length - (end - begin) + text.size();
    const auto insertedEnd = begin + text.size();
    const auto futureCharacter = [&](size_t offset) noexcept
    {
        if (offset < begin)
            return source_.CharacterAt(offset);
        if (offset < insertedEnd)
            return text[offset - begin];
        return source_.CharacterAt(end + offset - insertedEnd);
    };
    const auto validFutureBoundary = [&](size_t offset) noexcept
    {
        if (offset > resultingLength)
            return false;
        return !offset || offset == resultingLength || IsEditJoinAllowed(futureCharacter(offset - 1u), futureCharacter(offset));
    };
    // Inspect the virtual prefix/insertion/suffix join before any storage or
    // history allocation. These checks include newly formed cross-piece CRLF.
    if (!validFutureBoundary(after.anchor) || !validFutureBoundary(after.caret))
        throw std::out_of_range("Invalid resulting selection");
    bool equal = end - begin == text.size();
    size_t compared = 0u;
    if (equal)
        SourceSnapshot::State::Visit(source_.state_.get(), begin, end, [&](std::wstring_view fragment)
        {
            equal = fragment == std::wstring_view(text).substr(compared, fragment.size());
            compared += fragment.size();
            return equal;
        });
    if (equal)
    {
        selection_ = after;
        return false;
    }
    SourceSnapshot::State::Fragment insertion;
    if (!text.empty())
    {
        insertion.buffer = std::make_shared<const std::wstring>(std::move(text));
        insertion.length = insertion.buffer->size();
    }
    SourceSnapshot candidate(SourceSnapshot::State::Replace(source_.state_, begin, end, insertion));
    std::vector<History> nextUndo;
    if (recordHistory)
    {
        nextUndo = undo_;
        Push(nextUndo, {source_, selection_});
    }
    source_ = std::move(candidate);
    if (recordHistory)
    {
        undo_.swap(nextUndo);
        redo_.clear();
    }
    selection_ = after;
    ++revision_;
    return true;
}
