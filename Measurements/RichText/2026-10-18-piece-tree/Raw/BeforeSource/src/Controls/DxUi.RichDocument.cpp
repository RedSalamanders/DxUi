#include <DxUi/RichDocument.h>
#include <limits>
#include <stdexcept>
#include <windows.h>

namespace DxUi::RichText
{
namespace
{
bool High(wchar_t c) noexcept
{
    return c >= 0xd800 && c <= 0xdbff;
}
bool Low(wchar_t c) noexcept
{
    return c >= 0xdc00 && c <= 0xdfff;
}
void Validate(std::wstring_view text)
{
    if (text.size() > Document::MaximumUnits)
        throw std::length_error("Rich-text v0 source exceeds 64 Ki UTF-16 units");
    if (! IsValidUtf16(text))
        throw std::invalid_argument("Invalid UTF-16 source");
}
} // namespace
bool IsValidUtf16(std::wstring_view text) noexcept
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (High(text[i]))
        {
            if (++i == text.size() || ! Low(text[i]))
                return false;
        }
        else if (Low(text[i]) || text[i] == L'\0')
            return false;
    }
    return true;
}
bool IsEditBoundary(std::wstring_view text, std::size_t offset) noexcept
{
    if (offset > text.size())
        return false;
    if (offset == 0 || offset == text.size())
        return true;
    return ! (High(text[offset - 1]) && Low(text[offset])) && ! (text[offset - 1] == L'\r' && text[offset] == L'\n');
}
std::wstring DecodeUtf8(std::string_view bytes)
{
    if (bytes.empty())
        return {};
    if (bytes.size() > Document::MaximumUnits * 3u || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("UTF-8 input too large");
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (! length)
        throw std::invalid_argument("Invalid UTF-8 input");
    // A UTF-16 unit needs at most three UTF-8 bytes; paired supplementary
    // scalars need four bytes for two units. Reject both bounds before allocation.
    if (static_cast<std::size_t>(length) > Document::MaximumUnits)
        throw std::length_error("Decoded UTF-8 source exceeds capacity");
    std::wstring text(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), text.data(), length) != length)
        throw std::runtime_error("UTF-8 conversion failed");
    Validate(text);
    return text;
}
std::string EncodeUtf8(std::wstring_view text)
{
    Validate(text);
    if (text.empty())
        return {};
    const auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (! length)
        throw std::invalid_argument("Invalid UTF-16 input");
    std::string bytes(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), bytes.data(), length, nullptr, nullptr) != length)
        throw std::runtime_error("UTF-16 conversion failed");
    return bytes;
}
SourceSnapshot::SourceSnapshot() : state_(std::make_shared<State>())
{
}
std::size_t SourceSnapshot::Length() const noexcept
{
    return state_->length;
}
std::wstring SourceSnapshot::Text() const
{
    std::wstring text;
    text.reserve(state_->length);
    std::size_t sourceStart = 0;
    for (const auto& piece : state_->pieces)
    {
        text.append(*piece.buffer, piece.bufferStart, piece.sourceEnd - sourceStart);
        sourceStart = piece.sourceEnd;
    }
    return text;
}
std::size_t SourceSnapshot::FindPieceIndex(std::size_t offset) const noexcept
{
    // Ends are strictly increasing, including after splitting/coalescing. An
    // offset equal to a piece's exclusive end belongs to the following piece.
    const auto found = std::upper_bound(
        state_->pieces.begin(), state_->pieces.end(), offset, [](std::size_t position, const Piece& piece) noexcept { return position < piece.sourceEnd; });
    return static_cast<std::size_t>(found - state_->pieces.begin());
}
wchar_t SourceSnapshot::CharacterAt(std::size_t offset) const noexcept
{
    const auto index = FindPieceIndex(offset);
    if (index == state_->pieces.size())
        return L'\0'; // Only valid offsets are queried by IsBoundary.
    const auto& piece      = state_->pieces[index];
    const auto sourceStart = index ? state_->pieces[index - 1].sourceEnd : 0;
    return (*piece.buffer)[piece.bufferStart + offset - sourceStart];
}
bool SourceSnapshot::IsBoundary(std::size_t offset) const noexcept
{
    if (offset > Length())
        return false;
    if (! offset || offset == Length())
        return true;
    const auto before = CharacterAt(offset - 1), after = CharacterAt(offset);
    return ! (High(before) && Low(after)) && ! (before == L'\r' && after == L'\n');
}
std::wstring SourceSnapshot::Slice(std::size_t first, std::size_t last) const
{
    if (first > last || ! IsBoundary(first) || ! IsBoundary(last))
        throw std::out_of_range("Invalid snapshot slice");
    std::wstring result;
    result.reserve(last - first);
    if (first == last)
        return result;
    for (auto index = FindPieceIndex(first); index < state_->pieces.size(); ++index)
    {
        const auto& piece      = state_->pieces[index];
        const auto sourceStart = index ? state_->pieces[index - 1].sourceEnd : 0;
        const auto left = std::max(first, sourceStart), right = std::min(last, piece.sourceEnd);
        if (left < right)
            result.append(*piece.buffer, piece.bufferStart + left - sourceStart, right - left);
        if (piece.sourceEnd >= last)
            break;
    }
    return result;
}
Document::Document(std::wstring text)
{
    Reset(std::move(text));
}
void Document::Reset(std::wstring text)
{
    Validate(text);
    auto next    = std::make_shared<SourceSnapshot::State>();
    next->length = text.size();
    if (! text.empty())
        next->pieces.push_back({std::make_shared<const std::wstring>(std::move(text)), 0, next->length});
    source_    = SourceSnapshot(std::move(next));
    selection_ = {};
    undo_.clear();
    redo_.clear();
    ++revision_;
}
void Document::SetSelection(Selection selection)
{
    if (! source_.IsBoundary(selection.anchor) || ! source_.IsBoundary(selection.caret))
        throw std::out_of_range("Selection splits a Unicode scalar or CRLF");
    selection_ = selection;
}
void Document::Push(std::vector<History>& history, History entry)
{
    // Office CUndoStack::PushAntiEvent (RVA 0x166610) uses bounded history and
    // grouped inverse events. This owned core uses immutable snapshots instead;
    // one public edit is one transaction, with explicit bounded retention.
    if (history.size() == MaximumHistory)
        history.erase(history.begin());
    history.push_back(std::move(entry));
}
bool Document::Replace(std::size_t begin, std::size_t end, std::wstring text, Selection after)
{
    return ReplaceImpl(begin, end, std::move(text), after, true);
}
bool Document::ReplaceTransient(std::wstring text, Selection selection)
{
    return ReplaceImpl(0, Length(), std::move(text), selection, false);
}
bool Document::ReplaceImpl(std::size_t begin, std::size_t end, std::wstring text, Selection after, bool recordHistory)
{
    Validate(text);
    const auto length = source_.Length();
    if (begin > end || ! source_.IsBoundary(begin) || ! source_.IsBoundary(end))
        throw std::out_of_range("Invalid replacement range");
    if (text.size() > MaximumUnits - (length - (end - begin)))
        throw std::length_error("Bounded rich-text source limit");

    // Share immutable buffers; only descriptors and the inserted buffer are new.
    // Boundary validation reads adjacent pieces instead of flattening old/new
    // source strings on every keystroke. Cumulative ends index reads without an
    // extra prefix array. Edits still copy descriptors in this bounded vector,
    // not the future large-document tree or incremental layout implementation.
    auto next        = std::make_shared<SourceSnapshot::State>();
    next->length     = length - (end - begin) + text.size();
    auto appendPiece = [&](const std::shared_ptr<const std::wstring>& buffer, std::size_t bufferStart, std::size_t count)
    {
        const auto sourceStart = next->pieces.empty() ? 0 : next->pieces.back().sourceEnd;
        if (! next->pieces.empty())
        {
            auto& previous           = next->pieces.back();
            const auto previousStart = next->pieces.size() > 1 ? next->pieces[next->pieces.size() - 2].sourceEnd : 0;
            if (previous.buffer == buffer && previous.bufferStart + (sourceStart - previousStart) == bufferStart)
            {
                previous.sourceEnd += count;
                return;
            }
        }
        next->pieces.push_back({buffer, bufferStart, sourceStart + count});
    };
    auto appendSlice = [&](std::size_t first, std::size_t last)
    {
        if (first == last)
            return;
        for (auto index = source_.FindPieceIndex(first); index < source_.state_->pieces.size(); ++index)
        {
            const auto& piece      = source_.state_->pieces[index];
            const auto sourceStart = index ? source_.state_->pieces[index - 1].sourceEnd : 0;
            const auto left = std::max(first, sourceStart), right = std::min(last, piece.sourceEnd);
            if (left < right)
                appendPiece(piece.buffer, piece.bufferStart + left - sourceStart, right - left);
            if (piece.sourceEnd >= last)
                break;
        }
    };

    // A same-content edit updates selection but creates no history/revision.
    bool equal           = end - begin == text.size();
    std::size_t compared = 0;
    for (auto index = source_.FindPieceIndex(begin); equal && compared < text.size() && index < source_.state_->pieces.size(); ++index)
    {
        const auto& piece      = source_.state_->pieces[index];
        const auto sourceStart = index ? source_.state_->pieces[index - 1].sourceEnd : 0;
        if (sourceStart >= end)
            break;
        const auto left = std::max(begin, sourceStart), right = std::min(end, piece.sourceEnd);
        if (left < right)
        {
            const auto count = right - left;
            equal = std::wstring_view(*piece.buffer).substr(piece.bufferStart + left - sourceStart, count) == std::wstring_view(text).substr(compared, count);
            compared += count;
        }
    }
    appendSlice(0, begin);
    if (! text.empty())
    {
        auto buffer = std::make_shared<const std::wstring>(std::move(text));
        appendPiece(buffer, 0, buffer->size());
    }
    appendSlice(end, length);
    SourceSnapshot candidate(std::move(next));
    // Future boundaries can cross piece joins, e.g. an inserted LF after CR.
    // Validate before publishing source, selection, revision or either history.
    if (! candidate.IsBoundary(after.anchor) || ! candidate.IsBoundary(after.caret))
        throw std::out_of_range("Invalid resulting selection");
    if (equal)
    {
        selection_ = after;
        return false;
    }
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
bool Document::ReplaceSelection(std::wstring text)
{
    const auto end = selection_.Start() + text.size();
    return Replace(selection_.Start(), selection_.End(), std::move(text), {end, end});
}
bool Document::Undo()
{
    if (undo_.empty())
        return false;
    auto nextRedo = redo_;
    Push(nextRedo, {source_, selection_});
    auto old   = undo_.back();
    source_    = old.source;
    selection_ = old.selection;
    undo_.pop_back();
    redo_.swap(nextRedo);
    ++revision_;
    return true;
}
bool Document::Redo()
{
    if (redo_.empty())
        return false;
    auto nextUndo = undo_;
    Push(nextUndo, {source_, selection_});
    auto next  = redo_.back();
    source_    = next.source;
    selection_ = next.selection;
    redo_.pop_back();
    undo_.swap(nextUndo);
    ++revision_;
    return true;
}
} // namespace DxUi::RichText
