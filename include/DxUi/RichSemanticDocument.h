#pragma once
#include <DxUi/RichMarkdown.h>
#include <DxUi/RichSemantic.h>
#include <optional>

namespace DxUi::RichText
{
namespace Detail
{
struct SemanticExtensions;
struct SemanticExtensionAccess;
} // namespace Detail
enum class SemanticRole
{
    Document,
    Paragraph,
    Heading,
    Quote,
    List,
    ListItem,
    Code,
    Html,
    Table,
    Row,
    Cell,
    Link,
    Image,
    Task,
    Rule,
    Metadata,
    Footnote,
    FootnoteReference,
    FootnoteBacklink,
    Diagram,
    Math
};
struct SemanticElement
{
    SemanticElement() : name(0u, L' '), destination(0u, L' '), title(0u, L' ')
    {
    }
    SemanticElement(const SemanticElement&)            = default;
    SemanticElement& operator=(const SemanticElement&) = default;
    SemanticRole role{};
    MarkdownRange source, display, taskMarker;
    std::size_t parent = MarkdownNoNode, firstChild = MarkdownNoNode, nextSibling = MarkdownNoNode;
    std::uint64_t blockIdentity{};
    std::size_t syntaxNode = MarkdownNoNode;
    unsigned heading{};
    MarkdownTask task{};
    MarkdownAlignment alignment{};
    bool tableHeader{}, nativeHtml{};
    std::size_t rows{}, columns{};
    std::wstring name, destination, title;
};
struct SemanticParagraph
{
    MarkdownRange display;
    std::size_t element{}, syntaxNode{}, table = MarkdownNoNode, row{}, column{}, columns{};
    unsigned indent{}, flags{}, heading{};
    MarkdownAlignment alignment{};
    // Native preparation measures a shared gutter per List. The generated
    // prefix retains its original display ownership and syntax ListItem owner.
    std::size_t markerEnd{}, markerOwner = MarkdownNoNode;
    std::size_t emptyInsertion = MarkdownNoNode;
};
// Internal document navigation has no URL semantics and never changes source.
// Generated backlinks own an empty source range inside their definition; their
// separate target points to the complete original reference marker.
struct SemanticNavigation
{
    std::size_t element{};
    MarkdownRange target;
};
enum class SemanticDiagnosticKind
{
    UnsupportedDiagram,
    MalformedDiagram,
    DiagramCapacity,
    UnclosedDiagram,
    EmptyDiagram,
    UnsupportedMath
};
struct SemanticDiagnostic
{
    SemanticDiagnosticKind kind{};
    MarkdownRange source;
    std::size_t element{};
};
// One immutable semantic publication. Raw source remains in Syntax(); rendered
// values never replace source ACPs. Only the documented balanced native HTML
// subset is projected; other HTML stays literal. There is no DOM, script,
// remote fetching or implicit navigation. Images are atomic alternatives.
class SemanticDocument
{
public:
    static constexpr std::size_t MaximumElements         = 32768u;
    static constexpr std::size_t MaximumParagraphs       = 4096u;
    SemanticDocument(const SemanticDocument&)            = delete;
    SemanticDocument& operator=(const SemanticDocument&) = delete;
    const std::shared_ptr<const MarkdownSyntax>& Syntax() const noexcept
    {
        return syntax_;
    }
    // Process-local publication identity, distinct even when the same syntax
    // is rendered again after disabling/re-enabling semantic presentation.
    std::uint64_t Identity() const noexcept
    {
        return identity_;
    }
    const SourceDisplayMap& Map() const noexcept
    {
        return *map_;
    }
    std::span<const SemanticElement> Elements() const noexcept
    {
        return elements_;
    }
    std::span<const SemanticParagraph> Paragraphs() const noexcept
    {
        return paragraphs_;
    }
    std::span<const SemanticNavigation> Navigation() const noexcept
    {
        return navigation_;
    }
    std::optional<MarkdownRange> NavigationTarget(std::size_t element) const noexcept;
    // Visible source-bearing code/HTML/metadata leaf, or the active opaque
    // editor. Revision-local eligibility only; the retained owner guards actions.
    std::optional<std::size_t> SourceEditingNode(std::size_t element) const noexcept;
    // Native extension diagnostics own exact raw source ranges. Human-readable
    // explanations live in the named element's title; Syntax() remains immutable.
    std::span<const SemanticDiagnostic> Diagnostics() const noexcept;
    // Revision-local syntax index of the code/HTML/metadata leaf presented as
    // exact source, including fences/prefixes/EOLs. Other leaves keep semantics.
    // MarkdownNoNode denotes the ordinary semantic presentation.
    std::size_t SourceBlock() const noexcept
    {
        return sourceBlock_;
    }
    std::span<const StyleRun> Runs() const noexcept
    {
        return runs_;
    }
    // Complete source partition for accessibility Format units. Hidden syntax
    // has Regular style; transformed values retain their complete raw owner.
    // Generated/repeated display text never invents another source interval.
    std::span<const StyleRun> SourceRuns() const noexcept
    {
        return sourceRuns_;
    }

private:
    explicit SemanticDocument(std::shared_ptr<const MarkdownSyntax> syntax);
    std::shared_ptr<const MarkdownSyntax> syntax_;
    std::uint64_t identity_{};
    std::shared_ptr<const SourceDisplayMap> map_;
    std::vector<SemanticElement> elements_;
    std::vector<SemanticParagraph> paragraphs_;
    std::vector<SemanticNavigation> navigation_;
    std::vector<StyleRun> runs_, sourceRuns_;
    // Optional document-level state keeps native graphs out of ordinary inline
    // values and unrelated controls. No SDK/host resource belongs to semantics.
    std::shared_ptr<Detail::SemanticExtensions> extensions_;
    std::size_t sourceBlock_ = MarkdownNoNode;
    friend class SemanticBuilder;
    friend struct Detail::SemanticExtensionAccess;
};
// Owner-serialized pure preparation. Each advance resumes block/inline events;
// one bounded leaf normalization/value tokenization, image alternative or final
// map/index build is indivisible. Budgets count events, not wall-clock deadlines.
// Begin/Advance failures retain the previous publication; explicit retry is legal.
class SemanticBuilder
{
public:
    SemanticBuilder() noexcept;
    ~SemanticBuilder() noexcept;
    SemanticBuilder(const SemanticBuilder&)            = delete;
    SemanticBuilder& operator=(const SemanticBuilder&) = delete;
    void Begin(std::shared_ptr<const MarkdownSyntax> syntax);
    // One current source-bearing block; this does not alter syntax or source.
    // Invalid/non-leaf and hidden literal-declaration descendants are rejected
    // before replacing pending work.
    void Begin(std::shared_ptr<const MarkdownSyntax> syntax, std::size_t sourceBlock);
    MarkdownAdvance Advance(std::size_t maximumEvents = 4096u);
    void Cancel() noexcept;
    void Clear() noexcept;
    bool HasPendingPreparation() const noexcept;
    std::shared_ptr<const SemanticDocument> Published() const noexcept;

private:
    // Host-owned geometry rejection is presentation state for one layout key.
    // It never edits syntax and is not a consumer-authored source mask.
    void BeginWithNativeFallbacks(std::shared_ptr<const MarkdownSyntax> syntax, std::size_t sourceBlock, std::span<const MarkdownRange> sourceObjects);
    struct Preparation;
    std::unique_ptr<Preparation> next_;
    std::shared_ptr<const SemanticDocument> published_;
    friend struct Detail::SemanticExtensionAccess;
};
} // namespace DxUi::RichText
