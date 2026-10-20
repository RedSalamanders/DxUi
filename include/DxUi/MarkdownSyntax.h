#pragma once
#include <DxUi/RichDocument.h>
#include <array>
#include <optional>
#include <span>

namespace DxUi::RichText
{
// Requested dialect, carried in every result. Parser conformance and native
// semantic rendering/editing have separate qualification gates; see rich-text docs.
enum class MarkdownProfile
{
    CommonMark0312,
    Gfm029,
    RichMarkdown
};
enum class MarkdownExtensions : unsigned
{
    None          = 0u,
    Tables        = 1u,
    TaskLists     = 2u,
    Strikethrough = 4u,
    Autolinks     = 8u,
    TagFilter     = 16u,
    All           = 31u
};
constexpr MarkdownExtensions operator|(MarkdownExtensions a, MarkdownExtensions b) noexcept
{
    return static_cast<MarkdownExtensions>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr bool HasMarkdownExtension(MarkdownExtensions enabled, MarkdownExtensions feature) noexcept
{
    return (static_cast<unsigned>(enabled) & static_cast<unsigned>(feature)) != 0u;
}
struct MarkdownRange
{
    std::size_t first{}, last{};
    bool operator==(const MarkdownRange&) const = default;
};
enum class MarkdownBlockKind
{
    Paragraph,
    BlankLine,
    AtxHeading,
    SetextHeading,
    ThematicBreak,
    FencedCode,
    Document,
    BlockQuote,
    List,
    ListItem,
    IndentedCode,
    HtmlBlock,
    ReferenceDefinition,
    Table,
    TableRow,
    TableCell,
    FrontMatter,
    FootnoteDefinition
};
struct MarkdownBlock
{
    MarkdownBlockKind kind{};
    MarkdownRange source, content, info;
    unsigned heading{}, fenceIndent{};
    wchar_t fenceMarker{};
    std::size_t fenceLength{};
    bool closed = true;
};
inline constexpr std::size_t MarkdownNoNode = static_cast<std::size_t>(-1);
enum class MarkdownAlignment
{
    None,
    Left,
    Center,
    Right
};
enum class MarkdownTask
{
    None,
    Unchecked,
    Checked
};
// RichMarkdown alone recognizes a leading metadata envelope. This identifies
// its language, not a successful metadata validation or a reserialization API.
enum class MarkdownFrontMatterKind
{
    None,
    Yaml,
    Toml
};
enum class MarkdownDiagnosticCode
{
    UnclosedFrontMatter,
    DuplicateFootnoteDefinition,
    UndefinedFootnote
};
// Diagnostics name exact raw source ranges in this immutable publication.
// They do not change source, imply metadata validity, or carry SDK/UI objects.
struct MarkdownDiagnostic
{
    MarkdownDiagnosticCode code{};
    MarkdownRange source;
    std::size_t node = MarkdownNoNode;
};
enum class MarkdownInlineKind
{
    Text,
    SoftBreak,
    HardBreak,
    Code,
    Emphasis,
    Strong,
    Strikethrough,
    Link,
    Image,
    Html,
    FootnoteReference
};
// Resolved inline values, with original spelling retained in Source(). Indices
// are revision-local. source encloses delimiters and any stripped line prefixes;
// value/destination/title are semantic values, not an editing coordinate map.
struct MarkdownInline
{
    MarkdownInline() : value(0u, L' '), destination(0u, L' '), title(0u, L' '), filteredHtmlOpeners(0u)
    {
    }
    MarkdownInline(const MarkdownInline&)            = default;
    MarkdownInline& operator=(const MarkdownInline&) = default;
    MarkdownInlineKind kind{};
    MarkdownRange source;
    std::size_t firstChild = MarkdownNoNode, nextSibling = MarkdownNoNode;
    std::wstring value, destination, title;
    // UTF-16 offsets in an opaque Html value whose '<' is filtered by GFM.
    // This is the tagfilter extension, not an HTML sanitizer or execution API.
    std::vector<std::size_t> filteredHtmlOpeners;
};
// Optional extension metadata does not enlarge every ordinary inline node.
struct MarkdownFootnoteReference
{
    std::size_t inlineNode{}, footnote = MarkdownNoNode, occurrence{};
};
struct MarkdownFootnote
{
    MarkdownFootnote() : label(0u, L' '), references(0u)
    {
    }
    MarkdownFootnote(const MarkdownFootnote&)            = default;
    MarkdownFootnote& operator=(const MarkdownFootnote&) = default;
    std::wstring label; // Normalized identity; exact spelling stays in Source().
    std::size_t definition = MarkdownNoNode, ordinal{};
    std::vector<std::size_t> references; // Final dense inline indexes, reachable reading order.
};
// Tree indices are revision-local; identities belong to DocumentIdentity().
// Container source ranges include their prefixes and overlap their descendants.
// Blank trivia stays in Blocks()/Source(), rather than becoming semantic nodes.
struct MarkdownContentLine
{
    MarkdownRange source;
    unsigned leadingSpaces{}; // Unconsumed columns of a partially consumed tab.
};
// A physical line accepted as empty after the block recognizer consumed its
// container prefixes. This is source trivia, not a semantic text node. Offsets
// include the original EOL; insertion is before it. Owner is revision-local.
struct MarkdownEmptyLine
{
    MarkdownRange source;
    std::size_t insertion{}, owner{};
    unsigned indent{};
};
struct MarkdownNode
{
    // MSVC Debug's default/move vector constructors are noexcept but allocate
    // iterator proxies. Empty-size/copy construction is fallible, so allocation
    // injection can unwind staging rather than terminating the process.
    MarkdownNode() : lines(0u)
    {
    }
    MarkdownNode(const MarkdownNode&)            = default;
    MarkdownNode& operator=(const MarkdownNode&) = default;
    MarkdownBlock block;
    std::uint64_t identity{};
    std::size_t parent = MarkdownNoNode, firstChild = MarkdownNoNode, nextSibling = MarkdownNoNode;
    std::vector<MarkdownContentLine> lines;
    MarkdownRange label, destination, title;
    unsigned listStart{}, contentIndent{}, htmlType{};
    wchar_t listMarker{};
    bool tight              = true;
    std::size_t firstInline = MarkdownNoNode;
    MarkdownAlignment alignment{};
    MarkdownTask task{};
    bool tableHeader{};
    MarkdownFrontMatterKind frontMatterKind{};
};
struct MarkdownBlockStatistics
{
    // Prefix metadata/cursor visits and container closure visits. These are
    // deterministic diagnostic counters, not elapsed-time or total CPU bounds.
    std::size_t prefixInspectionUnits{}, containerFinalizations{};
};
// Context-free lexical candidates. Inline precedence (code, HTML, links,
// delimiters) must resolve them before using their values for rich rendering.
// In particular, an entity-looking candidate inside code is still literal code.
enum class MarkdownLexemeKind
{
    Text,
    EscapedPunctuation,
    CharacterReference,
    LineEnding,
    NulReplacement
};
struct MarkdownLexeme
{
    MarkdownLexemeKind kind{};
    MarkdownRange source;
    std::array<wchar_t, 4> value{};
    std::size_t valueLength{};
};

// An owned exact source plus lossless partitions. Sibling block ranges and,
// separately, lexeme ranges cover every original UTF-16 unit exactly once.
// Delimiters, unknown syntax, CR/LF/CRLF and NUL are never reconstructed from
// decoded output. No public mutator; readers can retain a published revision.
class MarkdownSyntax
{
public:
    // Combined block/inline staging budget, independent of source length.
    // Temporary shells count until their compacting pass finishes. Capacity
    // failure throws length_error without publishing a partial syntax tree.
    static constexpr std::size_t MaximumNodeRecords  = 32768u;
    MarkdownSyntax(const MarkdownSyntax&)            = delete;
    MarkdownSyntax& operator=(const MarkdownSyntax&) = delete;
    std::wstring_view Source() const noexcept;
    std::uint64_t Revision() const noexcept;
    MarkdownProfile Profile() const noexcept;
    MarkdownExtensions Extensions() const noexcept;
    const std::vector<MarkdownBlock>& Blocks() const noexcept;
    // Node zero is the document. Follow firstChild/nextSibling for source order;
    // finalizing reference definitions can append nodes after their siblings.
    const std::vector<MarkdownNode>& Nodes() const noexcept;
    std::span<const MarkdownEmptyLine> EmptyLines() const noexcept;
    std::uint64_t DocumentIdentity() const noexcept;
    MarkdownBlockStatistics BlockStatistics() const noexcept;
    const std::vector<MarkdownLexeme>& Lexemes() const noexcept;
    // Dense semantic forest rooted by each leaf's firstInline. Opaque HTML
    // blocks also have an Html value. Retired delimiter/bracket shells are gone.
    const std::vector<MarkdownInline>& Inlines() const noexcept;
    std::span<const MarkdownDiagnostic> Diagnostics() const noexcept;
    std::span<const MarkdownFootnote> Footnotes() const noexcept;
    std::span<const MarkdownFootnoteReference> FootnoteReferences() const noexcept;
    const MarkdownFootnoteReference* FootnoteReferenceFor(std::size_t inlineNode) const noexcept;
    std::wstring_view Slice(MarkdownRange range) const;
    std::wstring_view LexemeValue(std::size_t index) const;
    // Text has source-backed interior ACPs; transformed values have explicit
    // leading/trailing anchors, including multi-scalar named entity expansion.
    // This is lexical affinity, not yet a formatted-document editing command.
    std::size_t LexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing = false) const;

private:
    explicit MarkdownSyntax(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, MarkdownExtensions extensions);
    std::wstring source_;
    std::uint64_t revision_{};
    MarkdownProfile profile_{};
    MarkdownExtensions extensions_{};
    std::vector<MarkdownBlock> blocks_;
    std::vector<MarkdownLexeme> lexemes_;
    std::vector<MarkdownNode> nodes_;
    std::vector<MarkdownEmptyLine> emptyLines_;
    std::vector<MarkdownInline> inlines_;
    std::vector<MarkdownDiagnostic> diagnostics_;
    std::vector<MarkdownFootnote> footnotes_;
    std::vector<MarkdownFootnoteReference> footnoteReferences_;
    std::uint64_t documentIdentity_{};
    MarkdownBlockStatistics blockStatistics_;
    friend class MarkdownParser;
};

enum class MarkdownPreparationStatus
{
    Idle,
    Pending,
    Complete
};
struct MarkdownAdvance
{
    MarkdownPreparationStatus status{};
    std::size_t workUnits{};
};
// Owned parser, no foreign implementation/dependency, worker, host or graphics.
// Begin copies/validates bounded source. Advance resumes source/lexical cursors,
// generated table cells, root partitions, reference/inline/autolink preparation, dense forest indexes
// and block identity reconciliation. A physical-line transition, reference
// finalization, construct lookahead and emphasis resolution are bounded
// indivisible operations; they can inspect more than one source unit. Budgets
// count steps, not character visits, allocator time or wall-clock deadlines.
// Each revision reparses blocks/inlines, so container/fence/reference dependencies cannot
// inherit the older v0 projection's line-local reuse assumptions.
class MarkdownParser
{
public:
    MarkdownParser() noexcept;
    ~MarkdownParser() noexcept;
    MarkdownParser(MarkdownParser&&) noexcept;
    MarkdownParser& operator=(MarkdownParser&&) noexcept;
    MarkdownParser(const MarkdownParser&)            = delete;
    MarkdownParser& operator=(const MarkdownParser&) = delete;
    // An identical pending/completed request coalesces. Equal revision numbers
    // with different source or profile are distinct requests. Failed Begin leaves
    // the previous request intact; failed Advance discards only staged work.
    // Defaults: no extensions for CommonMark; all specified GFM extensions for
    // Gfm029/RichMarkdown. Explicit options also reproduce original spec vectors
    // whose extension lists are per-example. CommonMark rejects GFM options.
    void Begin(std::wstring_view source,
               std::uint64_t revision,
               MarkdownProfile profile                      = MarkdownProfile::CommonMark0312,
               std::optional<MarkdownExtensions> extensions = std::nullopt);
    // Prefer snapshots for edits: tracked source ancestry distinguishes edits
    // among identical paragraphs. String callers use deterministic prefix/suffix
    // mapping; that fallback cannot recover which duplicate the caller deleted.
    void Begin(const SourceSnapshot& source,
               std::uint64_t revision,
               MarkdownProfile profile                      = MarkdownProfile::CommonMark0312,
               std::optional<MarkdownExtensions> extensions = std::nullopt);
    MarkdownAdvance Advance(std::size_t maximumWorkUnits = 4096);
    bool HasPendingPreparation() const noexcept;
    std::shared_ptr<const MarkdownSyntax> Published() const noexcept;
    void Cancel() noexcept;
    void Clear() noexcept;

private:
    struct Preparation;
    std::unique_ptr<Preparation> preparation_;
    std::shared_ptr<const MarkdownSyntax> published_;
    std::uint64_t documentIdentity_{}, nextNodeIdentity_ = 1u;
    std::optional<SourceSnapshot> publishedSource_;
    void BeginSource(
        std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, MarkdownExtensions extensions, std::optional<SourceChangeRange> change);
};
} // namespace DxUi::RichText
