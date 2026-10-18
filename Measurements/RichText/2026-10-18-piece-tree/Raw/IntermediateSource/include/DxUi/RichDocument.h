#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace DxUi::RichText
{
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

bool IsValidUtf16(std::wstring_view text) noexcept;
bool IsEditBoundary(std::wstring_view text, std::size_t offset) noexcept;
std::wstring DecodeUtf8(std::string_view bytes);
std::string EncodeUtf8(std::wstring_view text);

// A revision shares immutable buffers and balanced piece-tree paths with background
// jobs. Editing never changes an earlier snapshot. No HWND, graphics or worker
// ownership.
class SourceSnapshot
{
public:
    SourceSnapshot();
    std::wstring Text() const;
    std::size_t Length() const noexcept;
    // Inspect UTF-16 offsets without materializing a whole revision. Tree lookup
    // is logarithmic; Slice visits only intersecting pieces after that lookup.
    // Scalar/CRLF boundaries are preserved; immutable snapshots support worker reads.
    bool IsBoundary(std::size_t offset) const noexcept;
    std::wstring Slice(std::size_t first, std::size_t last) const;

private:
    struct PieceTreeNode;
    std::shared_ptr<const PieceTreeNode> state_;
    explicit SourceSnapshot(std::shared_ptr<const PieceTreeNode> state) : state_(std::move(state))
    {
    }
    wchar_t CharacterAt(std::size_t offset) const noexcept;
    friend class Document;
};

class Document
{
public:
    // Bounded v0 core. Raising capacity requires separate incremental-layout and
    // resource qualification; this migration does not claim large-file support.
    static constexpr std::size_t MaximumUnits   = 64 * 1024;
    static constexpr std::size_t MaximumHistory = 128;
    explicit Document(std::wstring text = {});
    SourceSnapshot Snapshot() const noexcept
    {
        return source_;
    }
    std::wstring Text() const
    {
        return source_.Text();
    }
    std::size_t Length() const noexcept
    {
        return source_.Length();
    }
    std::uint64_t Revision() const noexcept
    {
        return revision_;
    }
    Selection GetSelection() const noexcept
    {
        return selection_;
    }
    void SetSelection(Selection selection);
    void Reset(std::wstring text);
    bool Replace(std::size_t begin, std::size_t end, std::wstring text, Selection after);
    // OS composition previews and their rollback change source/selection with a
    // fresh revision, preserving both histories. Restore the entry text before
    // recording the final committed replacement as one ordinary transaction.
    bool ReplaceTransient(std::wstring text, Selection selection);
    bool ReplaceSelection(std::wstring text);
    bool Undo();
    bool Redo();
    bool CanUndo() const noexcept
    {
        return ! undo_.empty();
    }
    bool CanRedo() const noexcept
    {
        return ! redo_.empty();
    }

private:
    struct History
    {
        SourceSnapshot source;
        Selection selection;
    };
    SourceSnapshot source_;
    Selection selection_{};
    std::uint64_t revision_{};
    std::vector<History> undo_, redo_;
    static void Push(std::vector<History>& history, History entry);
    bool ReplaceImpl(std::size_t begin, std::size_t end, std::wstring text, Selection after, bool recordHistory);
};
} // namespace DxUi::RichText
