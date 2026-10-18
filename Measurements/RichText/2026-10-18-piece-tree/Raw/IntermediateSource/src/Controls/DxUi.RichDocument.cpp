#include "DxUi.TextElements.h"
#include <DxUi/RichDocument.h>
#include <array>
#include <limits>
#include <span>
#include <stdexcept>
#include <variant>
#include <windows.h>

namespace DxUi::RichText
{
namespace
{
bool IsEditJoinAllowed(wchar_t before, wchar_t after) noexcept
{
    return ! (IsUtf16LeadSurrogate(before) && IsUtf16TrailSurrogate(after)) && ! (before == L'\r' && after == L'\n');
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
        if (IsUtf16LeadSurrogate(text[i]))
        {
            if (++i == text.size() || ! IsUtf16TrailSurrogate(text[i]))
                return false;
        }
        else if (IsUtf16TrailSurrogate(text[i]) || text[i] == L'\0')
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
    return IsEditJoinAllowed(text[offset - 1], text[offset]);
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
struct SourceSnapshot::PieceTreeNode
{
    // Immutable B+ tree. Every non-root node has 16..32 entries; only the root
    // may be smaller. Exclusive cumulative ends index either pieces or children.
    // A splice copies the two boundary paths and reuses unaffected subtrees.
    using Pointer                          = std::shared_ptr<const PieceTreeNode>;
    static constexpr size_t MaximumEntries = 32u, MinimumEntries = MaximumEntries / 2u;
    struct Fragment
    {
        std::shared_ptr<const std::wstring> buffer;
        size_t bufferStart{}, length{};
    };
    struct Piece
    {
        std::shared_ptr<const std::wstring> buffer;
        size_t bufferStart{}, sourceEnd{};
    };
    struct Child
    {
        Pointer node;
        size_t sourceEnd{};
    };
    using Leaves   = std::array<Piece, MaximumEntries>;
    using Branches = std::array<Child, MaximumEntries>;
    struct SpliceResult
    {
        Pointer first, second;
    };
    std::variant<Leaves, Branches> entries;
    size_t length{}, count{}, height{};

    explicit PieceTreeNode(std::span<const Fragment> fragments) : entries(Leaves{}), count(fragments.size())
    {
        auto& pieces = std::get<Leaves>(entries);
        for (size_t index = 0u; index < count; ++index)
        {
            length += fragments[index].length;
            pieces[index] = {fragments[index].buffer, fragments[index].bufferStart, length};
        }
    }
    explicit PieceTreeNode(std::span<const Pointer> children) : entries(Branches{}), count(children.size()), height(children.front()->height + 1u)
    {
        auto& branches = std::get<Branches>(entries);
        for (size_t index = 0u; index < count; ++index)
        {
            length += children[index]->length;
            branches[index] = {children[index], length};
        }
    }
    size_t EntryEnd(size_t index) const noexcept
    {
        return height ? std::get<Branches>(entries)[index].sourceEnd : std::get<Leaves>(entries)[index].sourceEnd;
    }
    size_t EntryStart(size_t index) const noexcept
    {
        return index ? EntryEnd(index - 1u) : 0u;
    }
    size_t FindEntry(size_t offset) const noexcept
    {
        size_t first = 0u, last = count;
        while (first < last)
        {
            const auto middle = first + (last - first) / 2u;
            if (EntryEnd(middle) <= offset)
                first = middle + 1u;
            else
                last = middle;
        }
        return first;
    }
    Fragment GetFragment(size_t index) const noexcept
    {
        const auto& piece = std::get<Leaves>(entries)[index];
        return {piece.buffer, piece.bufferStart, piece.sourceEnd - EntryStart(index)};
    }
    static void AppendCoalesced(std::array<Fragment, MaximumEntries * 2u + 2u>& fragments, size_t& used, Fragment fragment)
    {
        if (! fragment.length)
            return;
        if (used && fragments[used - 1u].buffer == fragment.buffer && fragments[used - 1u].bufferStart + fragments[used - 1u].length == fragment.bufferStart)
            fragments[used - 1u].length += fragment.length;
        else
            fragments[used++] = std::move(fragment);
    }
    static SpliceResult BuildLeaves(std::span<const Fragment> fragments)
    {
        if (fragments.empty())
            return {};
        if (fragments.size() <= MaximumEntries)
            return {std::make_shared<const PieceTreeNode>(fragments), {}};
        const auto split = fragments.size() / 2u;
        return {std::make_shared<const PieceTreeNode>(fragments.first(split)), std::make_shared<const PieceTreeNode>(fragments.subspan(split))};
    }
    static SpliceResult RebalanceSiblings(const Pointer& left, const Pointer& right)
    {
        if (! left->height)
        {
            std::array<Fragment, MaximumEntries * 2u + 2u> fragments;
            size_t used = 0u;
            for (const auto* node : {left.get(), right.get()})
                for (size_t index = 0u; index < node->count; ++index)
                    AppendCoalesced(fragments, used, node->GetFragment(index));
            return BuildLeaves({fragments.data(), used});
        }
        std::array<Pointer, MaximumEntries * 2u + 2u> children;
        size_t used = 0u;
        for (const auto* node : {left.get(), right.get()})
            for (size_t index = 0u; index < node->count; ++index)
                children[used++] = std::get<Branches>(node->entries)[index].node;
        // A boundary split can leave a one-child chain. Normalize the lower
        // level while merging its parents, rather than retaining an underfull
        // descendant inside an otherwise full new branch.
        return BuildBranches(children, used);
    }
    static SpliceResult BuildBranches(std::array<Pointer, MaximumEntries * 2u + 2u>& children, size_t used)
    {
        // Deletion may thin either boundary path. Merge/redistribute adjacent
        // siblings before publishing their parent, including lower-level chains.
        for (size_t index = 0u; used > 1u && index < used;)
        {
            if (children[index]->count >= MinimumEntries)
            {
                ++index;
                continue;
            }
            const auto left   = index ? index - 1u : 0u;
            const auto merged = RebalanceSiblings(children[left], children[left + 1u]);
            children[left]    = merged.first;
            if (merged.second)
                children[left + 1u] = merged.second;
            else
            {
                for (size_t move = left + 1u; move + 1u < used; ++move)
                    children[move] = std::move(children[move + 1u]);
                children[--used].reset();
            }
            index = left;
        }
        if (! used)
            return {};
        if (used <= MaximumEntries)
            return {std::make_shared<const PieceTreeNode>(std::span<const Pointer>(children.data(), used)), {}};
        const auto split = used / 2u;
        return {std::make_shared<const PieceTreeNode>(std::span<const Pointer>(children.data(), split)),
                std::make_shared<const PieceTreeNode>(std::span<const Pointer>(children.data() + split, used - split))};
    }
    static SpliceResult Splice(const Pointer& node, size_t first, size_t last, const Fragment& insertion)
    {
        if (! node)
            return insertion.length ? BuildLeaves(std::span<const Fragment>(&insertion, 1u)) : SpliceResult{};
        if (! node->height)
        {
            // One split piece plus an insertion needs at most 34 entries.
            // Sibling joins can stage 64; fixed arrays cover both without a heap.
            std::array<Fragment, MaximumEntries * 2u + 2u> fragments;
            size_t used = 0u;
            for (size_t index = 0u; index < node->count; ++index)
            {
                const auto begin = node->EntryStart(index), end = node->EntryEnd(index);
                if (begin >= first)
                    break;
                auto piece   = node->GetFragment(index);
                piece.length = std::min(first, end) - begin;
                AppendCoalesced(fragments, used, std::move(piece));
            }
            AppendCoalesced(fragments, used, insertion);
            for (auto index = node->FindEntry(last); index < node->count; ++index)
            {
                const auto begin = node->EntryStart(index), end = node->EntryEnd(index);
                const auto offset = std::max(last, begin) - begin;
                auto piece        = node->GetFragment(index);
                piece.bufferStart += offset;
                piece.length = end - begin - offset;
                AppendCoalesced(fragments, used, std::move(piece));
            }
            return BuildLeaves({fragments.data(), used});
        }
        std::array<Pointer, MaximumEntries * 2u + 2u> children;
        size_t used          = 0u;
        const auto anchor    = first == node->length ? node->count - 1u : node->FindEntry(first);
        const auto& branches = std::get<Branches>(node->entries);
        for (size_t index = 0u; index < node->count; ++index)
        {
            const auto begin = node->EntryStart(index), end = node->EntryEnd(index);
            if (index != anchor && (end <= first || begin >= last))
                children[used++] = branches[index].node;
            else if (index == anchor || first > begin || last < end)
            {
                const auto relativeFirst = first > begin ? std::min(first - begin, end - begin) : 0u;
                const auto relativeLast  = last > begin ? std::min(last - begin, end - begin) : 0u;
                const auto replacement   = Splice(branches[index].node, relativeFirst, relativeLast, index == anchor ? insertion : Fragment{});
                if (replacement.first)
                    children[used++] = replacement.first;
                if (replacement.second)
                    children[used++] = replacement.second;
            }
            // Fully removed middle subtrees need no traversal or new nodes.
        }
        return BuildBranches(children, used);
    }
    static Pointer ReplaceRange(const Pointer& root, size_t first, size_t last, const Fragment& insertion)
    {
        auto result  = Splice(root, first, last, insertion);
        Pointer next = result.first;
        if (result.second)
        {
            const std::array<Pointer, 2u> children{result.first, result.second};
            next = std::make_shared<const PieceTreeNode>(std::span<const Pointer>(children));
        }
        // Preserve equal heights during recursion; collapse root-only chains
        // after the complete splice so repeated cuts cannot grow tree height.
        while (next && next->height && next->count == 1u)
            next = std::get<Branches>(next->entries)[0].node;
        return next;
    }
    template <typename Visitor> static bool VisitRange(const PieceTreeNode* node, size_t first, size_t last, Visitor&& visitor)
    {
        if (first == last)
            return true;
        for (auto index = node->FindEntry(first); index < node->count; ++index)
        {
            const auto begin = node->EntryStart(index), end = node->EntryEnd(index);
            const auto left = std::max(first, begin), right = std::min(last, end);
            if (node->height)
            {
                if (! VisitRange(std::get<Branches>(node->entries)[index].node.get(), left - begin, right - begin, visitor))
                    return false;
            }
            else
            {
                const auto& piece = std::get<Leaves>(node->entries)[index];
                if (! visitor(std::wstring_view(*piece.buffer).substr(piece.bufferStart + left - begin, right - left)))
                    return false;
            }
            if (end >= last)
                break;
        }
        return true;
    }
};

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
    if (! node || offset >= node->length)
        return L'\0';
    while (node->height)
    {
        const auto index = node->FindEntry(offset);
        offset -= node->EntryStart(index);
        node = std::get<PieceTreeNode::Branches>(node->entries)[index].node.get();
    }
    const auto index  = node->FindEntry(offset);
    const auto& piece = std::get<PieceTreeNode::Leaves>(node->entries)[index];
    return (*piece.buffer)[piece.bufferStart + offset - node->EntryStart(index)];
}
bool SourceSnapshot::IsBoundary(std::size_t offset) const noexcept
{
    if (offset > Length())
        return false;
    if (! offset || offset == Length())
        return true;
    return IsEditJoinAllowed(CharacterAt(offset - 1u), CharacterAt(offset));
}
std::wstring SourceSnapshot::Slice(std::size_t first, std::size_t last) const
{
    if (first > last || ! IsBoundary(first) || ! IsBoundary(last))
        throw std::out_of_range("Invalid snapshot slice");
    std::wstring result;
    result.reserve(last - first);
    PieceTreeNode::VisitRange(state_.get(),
                              first,
                              last,
                              [&](std::wstring_view fragment)
    {
        result.append(fragment);
        return true;
    });
    return result;
}
Document::Document(std::wstring text)
{
    Reset(std::move(text));
}
void Document::Reset(std::wstring text)
{
    Validate(text);
    SourceSnapshot next;
    if (! text.empty())
    {
        auto buffer = std::make_shared<const std::wstring>(std::move(text));
        const SourceSnapshot::PieceTreeNode::Fragment fragment{buffer, 0u, buffer->size()};
        next.state_ = SourceSnapshot::PieceTreeNode::BuildLeaves(std::span<const SourceSnapshot::PieceTreeNode::Fragment>(&fragment, 1u)).first;
    }
    source_    = std::move(next);
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
    const auto resultingLength = length - (end - begin) + text.size();
    const auto insertedEnd     = begin + text.size();
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
        return ! offset || offset == resultingLength || IsEditJoinAllowed(futureCharacter(offset - 1u), futureCharacter(offset));
    };
    // Inspect the virtual prefix/insertion/suffix join before any storage or
    // history allocation. These checks include newly formed cross-piece CRLF.
    if (! validFutureBoundary(after.anchor) || ! validFutureBoundary(after.caret))
        throw std::out_of_range("Invalid resulting selection");
    bool equal      = end - begin == text.size();
    size_t compared = 0u;
    if (equal)
        SourceSnapshot::PieceTreeNode::VisitRange(source_.state_.get(),
                                                  begin,
                                                  end,
                                                  [&](std::wstring_view fragment)
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
    SourceSnapshot::PieceTreeNode::Fragment insertion;
    if (! text.empty())
    {
        insertion.buffer = std::make_shared<const std::wstring>(std::move(text));
        insertion.length = insertion.buffer->size();
    }
    SourceSnapshot candidate(SourceSnapshot::PieceTreeNode::ReplaceRange(source_.state_, begin, end, insertion));
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
