#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace DxUi
{
class RichTextEditor;
}

namespace DxUi::RichText
{
// Expected source admission refusals. Unexpected allocation failures are not
// represented by this result.
enum class SourceResult
{
    Success,
    InvalidSource,
    CapacityExceeded,
    InvalidRange,
    Stale,
    InvalidArgument
};

struct Selection
{
    std::size_t anchor{}, caret{};
    std::size_t Start() const noexcept
    {
        return std::min(anchor, caret);
    }
    std::size_t End() const noexcept
    {
        return std::max(anchor, caret);
    }
    bool operator==(const Selection&) const = default;
};

// Zero-based logical source coordinates; column counts UTF-16 code units.
struct SourceLocation
{
    std::size_t line{}, column{};
    bool operator==(const SourceLocation&) const = default;
};

bool IsValidUtf16(std::wstring_view text) noexcept;
// Counted RichText source admits U+0000; metadata and other existing callers remain strict.
bool IsValidSourceUtf16(std::wstring_view text) noexcept;
bool IsEditBoundary(std::wstring_view text, std::size_t offset) noexcept;
std::wstring DecodeUtf8(std::string_view bytes);
std::string EncodeUtf8(std::wstring_view text);
[[nodiscard]] SourceResult TryDecodeUtf8(std::string_view bytes, std::wstring& text);
[[nodiscard]] SourceResult TryEncodeUtf8(std::wstring_view text, std::string& bytes);

enum class SourceChangeKind
{
    Unchanged,
    TrackedEdits,
    FullReplacement
};
struct SourceChangeRange
{
    SourceChangeKind kind = SourceChangeKind::FullReplacement;
    std::size_t begin{}, previousEnd{}, currentEnd{};
};

class Document;

// A revision shares immutable buffers and balanced piece-tree paths with background
// jobs. Editing never changes an earlier snapshot. No HWND, graphics or worker
// ownership.
class SourceSnapshot
{
public:
    SourceSnapshot() noexcept;
    std::wstring Text() const;
    std::size_t Length() const noexcept;
    // Compare against caller-owned text without flattening this immutable tree.
    // Used for exact admission of an already parsed result; does not allocate.
    bool MatchesText(std::wstring_view text) const noexcept;
    // Exact immutable revision identity, stronger than equal spelling/revision
    // counters from different documents. Does not walk or materialize source.
    bool IsSameRevision(const SourceSnapshot& other) const noexcept
    {
        return _state == other._state;
    }
    // Inspect UTF-16 offsets without materializing a whole revision. Tree lookup
    // is logarithmic; Slice visits only intersecting pieces after that lookup.
    // Scalar/CRLF boundaries are preserved; immutable snapshots support worker reads.
    bool IsBoundary(std::size_t offset) const noexcept;
    // CRLF is one line break. Invalid revisions and non-edit boundaries return no location.
    // Uses immutable line summaries and buffer indexes without allocating,
    // flattening, or scanning all preceding source units.
    std::optional<SourceLocation> LocationAt(std::size_t offset) const noexcept;
    // One UTF-16 unit by tree lookup (logarithmic), U+0000 past the end. Reads
    // nothing else and allocates nothing.
    wchar_t UnitAt(std::size_t offset) const noexcept;
    // The offset where zero-based logical line `line` starts (CRLF is one
    // break), from the immutable line summaries without scanning or copying
    // source; none past the last line or for an invalid revision.
    std::optional<std::size_t> LineStart(std::size_t line) const noexcept;
    std::wstring Slice(std::size_t first, std::size_t last) const;
    [[nodiscard]] SourceResult TrySlice(std::size_t first, std::size_t last, std::wstring& result) const;
    // A conservative replacement interval, safe for Slice in both snapshots.
    // Tracked ranges may include unchanged text; missing bounded ancestry falls
    // back to the whole source. Queries allocate nothing and mutate no cache.
    SourceChangeRange ChangedRangeFrom(const SourceSnapshot& previous) const noexcept;

private:
    struct PieceTreeNode;
    struct Revision;
    std::shared_ptr<const Revision> _state;
    explicit SourceSnapshot(std::shared_ptr<const Revision> state) : _state(std::move(state))
    {
    }
    wchar_t CharacterAt(std::size_t offset) const noexcept;
    const PieceTreeNode* RootNode() const noexcept;
    // Work counters: attributes a copy to the editor's own document or to any other Document.
    void NoteSliceCopy(std::size_t units, bool whole) const noexcept;
    friend class Document;
    // Marker admission compares existing delimiters without allocating.
    friend SourceResult TryToggleMarker(Document&, std::wstring_view, bool&);
};

class Document
{
public:
    // Bounded v0 core. Raising capacity requires separate incremental-layout and
    // resource qualification; this migration does not claim large-file support.
    static constexpr std::size_t MaximumUnits   = 64 * 1024;
    static constexpr std::size_t MaximumHistory = 128;
    // Borrow source only for this call; changed transactions stage an owned copy.
    // Views let valid same-content edits return before any string/container allocation.
    explicit Document(std::wstring_view text = {});
    SourceSnapshot Snapshot() const noexcept
    {
        return _source;
    }
    std::wstring Text() const
    {
        return _source.Text();
    }
    std::size_t Length() const noexcept
    {
        return _source.Length();
    }
    std::uint64_t Revision() const noexcept
    {
        return _revision;
    }
    Selection GetSelection() const noexcept
    {
        return _selection;
    }
    // A refusal leaves the document, selection and history unchanged.
    [[nodiscard]] SourceResult TryReset(std::wstring_view text);
    [[nodiscard]] SourceResult TrySetSelection(Selection selection) noexcept;
    // On refusal, changed is false and no document state is published.
    [[nodiscard]] SourceResult TryReplace(std::size_t begin, std::size_t end, std::wstring_view text, Selection after, bool& changed);
    [[nodiscard]] SourceResult TryReplaceWholeText(std::wstring_view text, Selection after, bool& changed);
    [[nodiscard]] SourceResult TryReplaceTransient(std::wstring_view text, Selection selection, bool& changed);
    [[nodiscard]] SourceResult TryReplaceSelection(std::wstring_view text, bool& changed);
    void SetSelection(Selection selection);
    void Reset(std::wstring_view text);
    bool Replace(std::size_t begin, std::size_t end, std::wstring_view text, Selection after);
    // A service supplies complete text, but one recorded transaction retains
    // only its scalar/CRLF-safe changed interval and shares untouched pieces.
    bool ReplaceWholeText(std::wstring_view text, Selection after);
    // OS composition previews and their rollback change source/selection with a
    // fresh revision, preserving both histories. Restore the entry text before
    // recording the final committed replacement as one ordinary transaction.
    // Undo/redo decline while a changed transient preview is active. An explicit
    // recorded edit supersedes it and uses the entry source for undo.
    bool ReplaceTransient(std::wstring_view text, Selection selection);
    bool ReplaceSelection(std::wstring_view text);
    bool Undo();
    bool Redo();
    bool CanUndo() const noexcept
    {
        return ! _transientEntry && ! _undo.Empty();
    }
    bool CanRedo() const noexcept
    {
        return ! _transientEntry && ! _redo.Empty();
    }

private:
    struct History
    {
        SourceSnapshot source;
        Selection selection;
    };
    std::optional<History> _transientEntry;
    SourceSnapshot _source;
    Selection _selection{};
    std::uint64_t _revision{};
    std::uint64_t _selectionEpoch{}; // Detect selection moves even when a later move returns to the same ACP.
    // Work counters only: a RichTextEditor marks its own document so copies of it count apart from other Documents'.
    bool _editorSource{};
    void CountAsEditorSource() noexcept;
    friend class ::DxUi::RichTextEditor;
    [[nodiscard]] bool AdoptReadOnlySourceSnapshot(const SourceSnapshot& source, Selection selection) noexcept;
    struct HistoryRing
    {
        HistoryRing() : entries(0u)
        {
        }
        HistoryRing(const HistoryRing&)            = default;
        HistoryRing& operator=(const HistoryRing&) = default;
        HistoryRing(HistoryRing&& other);
        HistoryRing& operator=(HistoryRing&& other) noexcept;
        [[nodiscard]] bool Empty() const noexcept
        {
            return count == 0u;
        }
        void PreparePush();
        void Push(History entry) noexcept;
        History TakeBack() noexcept;
        void Clear() noexcept;
        std::vector<History> entries;
        std::size_t first{}, count{};
    };
    HistoryRing _undo, _redo;
    bool ReplaceImpl(std::size_t begin, std::size_t end, std::wstring_view text, Selection after, bool recordHistory, bool coalesceHistory = false);
    bool ReplaceWholeTextImpl(std::wstring_view text, Selection after, bool recordHistory);
    [[nodiscard]] SourceResult TryReplaceImpl(
        std::size_t begin, std::size_t end, std::wstring_view text, Selection after, bool recordHistory, bool coalesceHistory, bool& changed);
    [[nodiscard]] SourceResult TryReplaceWholeTextImpl(std::wstring_view text, Selection after, bool recordHistory, bool& changed);
};
} // namespace DxUi::RichText
