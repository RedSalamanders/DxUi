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
    PreviousTableCell,
    SetQuote,
    SetCodeBlock,
    SetCodeLanguage,
    InsertHardBreak,
    InsertMath,
    InsertThematicBreak,
    InsertImage,
    ExitCodeBlock,
    ClearFormatting,
    InsertEscapedText,
    SetTableAlignment,
    MoveTableRowUp,
    MoveTableRowDown,
    MoveTableColumnLeft,
    MoveTableColumnRight,
    InsertFootnote,
    MoveLineUp,
    MoveLineDown,
    RenumberList
};
enum class MarkdownQuoteAction
{
    Toggle,
    Indent,
    Outdent
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
enum class MarkdownEnterPolicy
{
    RawNewline,
    NewParagraph
};
enum class MarkdownTableInputFormat
{
    Empty,
    Tsv,
    Csv
};
// Shared extension admission only; source and payload checks remain separate.
bool IsCommandFeatureEnabled(const MarkdownSyntax& syntax, MarkdownCommand command, MarkdownTextStyle style, MarkdownListKind listKind) noexcept;
// Resolved footnotes permit edits only inside their owned content.
std::optional<std::size_t> FindCommandFootnoteOwner(const MarkdownSyntax& syntax, Selection selection) noexcept;
// A selection ending on an EOL still belongs to the preceding physical line.
bool IsSingleLineCommandSelection(const MarkdownSyntax& syntax, Selection selection) noexcept;
// Admission for heading conversion, without constructing source edits.
bool CanSetHeading(const MarkdownSyntax& syntax, Selection selection, unsigned level) noexcept;
enum class MarkdownCommandStatus
{
    Unavailable,
    Unchanged,
    Changed
};
struct MarkdownCommandState
{
    // Unknown means that determining applicability requires the full planner.
    std::optional<bool> available;
    bool active = false, mixed = false;
    unsigned headingLevel = 0u;
    std::optional<MarkdownListKind> listKind;
};
// Shared nonallocating admission predicate for the source-exact code exit.
std::optional<std::size_t> FindExitCodeBlock(const MarkdownSyntax& syntax, Selection selection) noexcept;
std::optional<std::size_t> FindCodeLanguageTarget(const MarkdownSyntax& syntax, Selection selection, std::wstring_view language) noexcept;
bool CanInsertMath(const MarkdownSyntax& syntax, Selection selection, std::wstring_view text, bool display) noexcept;
bool CanInsertEscapedText(const MarkdownSyntax& syntax, Selection selection, std::wstring_view text) noexcept;
bool CanInsertThematicBreak(const MarkdownSyntax& syntax, Selection selection) noexcept;
// Joining is defined only at a single caret; richer site admission is separate.
constexpr bool CanJoinSelection(Selection selection) noexcept
{
    return selection.anchor == selection.caret;
}
// Finds an eligible leaf and visible edge; sibling/container checks still apply.
std::optional<std::size_t> FindJoinLeaf(const MarkdownSyntax& syntax, Selection selection, bool backwards, bool* simpleApplicable = nullptr) noexcept;
struct MarkdownCommandRequest
{
    MarkdownCommand command{};
    MarkdownListKind listKind       = MarkdownListKind::Unordered;
    MarkdownTextStyle style         = MarkdownTextStyle::Strong;
    MarkdownQuoteAction quoteAction = MarkdownQuoteAction::Toggle;
    bool mathDisplay                = false;
    unsigned headingLevel           = 1u;
    std::optional<unsigned> listStart;
    // Revision-local TableCell identity, needed when padded cells share an ACP.
    // Other commands infer their target from the exact source selection.
    std::size_t targetCell   = MarkdownNoNode;
    std::size_t tableColumns = 2u, tableBodyRows = 1u;
    // Optional UTF-16 caret offset in SetTableCell's replacement text.
    std::optional<std::size_t> tableCellCaretOffset;
    // Borrowed only while building. Cell text is raw single-line Markdown;
    // InsertFootnote uses text as the exact raw footnote label.
    // destination/title are semantic Unicode values serialized by the planner.
    std::wstring_view text, destination, title;
    MarkdownAlignment tableAlignment = MarkdownAlignment::None;
    MarkdownEnterPolicy enterPolicy  = MarkdownEnterPolicy::RawNewline;
    // InsertTable may consume a quoted delimited Unicode payload in text.
    MarkdownTableInputFormat tableInputFormat = MarkdownTableInputFormat::Empty;
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
