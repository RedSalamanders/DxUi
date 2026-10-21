#pragma once
#include <DxUi/RichSemantic.h>

namespace DxUi::RichText
{
enum class MarkdownCommand
{
    InsertParagraph,
    SetList,
    IndentList,
    OutdentList,
    JoinBackward,
    JoinForward,
    SetHeading,
    ToggleStyle,
    SetLink,
    RemoveLink,
    ToggleTask,
    InsertTable,
    InsertTableRow,
    DeleteTableRow,
    InsertTableColumn,
    DeleteTableColumn,
    SetTableCell,
    NextTableCell,
    PreviousTableCell
};
enum class MarkdownListKind
{
    None,
    Unordered,
    Ordered,
    Task
};
enum class MarkdownTextStyle
{
    Strong,
    Emphasis,
    Strikethrough,
    Code
};
enum class MarkdownCommandStatus
{
    Unavailable,
    Unchanged,
    Changed
};
struct MarkdownCommandRequest
{
    MarkdownCommand command{};
    MarkdownListKind listKind = MarkdownListKind::Unordered;
    MarkdownTextStyle style   = MarkdownTextStyle::Strong;
    unsigned headingLevel     = 1u;
    std::optional<unsigned> listStart;
    // Revision-local TableCell identity, needed when padded cells share an ACP.
    // Other commands infer their target from the exact source selection.
    std::size_t targetCell   = MarkdownNoNode;
    std::size_t tableColumns = 2u, tableBodyRows = 1u;
    // Borrowed only while building. Cell text is raw single-line Markdown;
    // destination/title are semantic Unicode values serialized by the planner.
    std::wstring_view text, destination, title;
};
struct MarkdownCommandEdit
{
    MarkdownCommandEdit() : replacement(0u, L' ')
    {
    }
    MarkdownCommandEdit(MarkdownRange range, std::wstring_view value) : source(range), replacement(value)
    {
    }
    MarkdownCommandEdit(const MarkdownCommandEdit&)            = default;
    MarkdownCommandEdit& operator=(const MarkdownCommandEdit&) = default;
    MarkdownRange source{};
    std::wstring replacement;
};
namespace Detail
{
struct MarkdownCommandPlanner;
}
// An immutable, owned source plan. It does not parse, publish, pump a host or
// retain borrowed request strings. Rejected plans leave Document completely intact.
class MarkdownCommandPlan
{
public:
    MarkdownCommandPlan(const MarkdownCommandPlan&)            = default;
    MarkdownCommandPlan& operator=(const MarkdownCommandPlan&) = default;
    bool IsApplicable() const noexcept
    {
        return applicable_;
    }
    const SourceSnapshot& ExpectedSource() const noexcept
    {
        return source_;
    }
    std::uint64_t ExpectedRevision() const noexcept
    {
        return revision_;
    }
    const std::shared_ptr<const MarkdownSyntax>& Syntax() const noexcept
    {
        return syntax_;
    }
    Selection Before() const noexcept
    {
        return before_;
    }
    Selection After() const noexcept
    {
        return after_;
    }
    std::size_t AfterCell() const noexcept
    {
        return afterCell_;
    }
    std::span<const MarkdownCommandEdit> Edits() const noexcept
    {
        return edits_;
    }

private:
    MarkdownCommandPlan(const SourceSnapshot& source, std::uint64_t revision, std::shared_ptr<const MarkdownSyntax> syntax, Selection before);
    SourceSnapshot source_;
    std::uint64_t revision_{};
    std::shared_ptr<const MarkdownSyntax> syntax_;
    Selection before_, after_;
    std::vector<MarkdownCommandEdit> edits_;
    std::size_t afterCell_ = MarkdownNoNode;
    bool applicable_{};
    friend struct Detail::MarkdownCommandPlanner;
};
MarkdownCommandPlan BuildMarkdownCommand(const SourceSnapshot& source,
                                         std::uint64_t revision,
                                         std::shared_ptr<const MarkdownSyntax> syntax,
                                         Selection selection,
                                         const MarkdownCommandRequest& request);
// Visual deletion already resolved through primary source ownership. Empty
// inline wrappers are removed only when all their owned body is being deleted.
// Raw TSF/programmatic replacement does not use this formatting cleanup policy.
MarkdownCommandPlan BuildMarkdownDeletion(const SourceSnapshot& source,
                                          std::uint64_t revision,
                                          std::shared_ptr<const MarkdownSyntax> syntax,
                                          Selection selection,
                                          std::span<const MarkdownRange> ranges);
bool ApplyMarkdownCommand(Document& document, const MarkdownCommandPlan& plan);
} // namespace DxUi::RichText
