#pragma once
#include <DxUi/RichMarkdown.h>
#include <DxUi/RichSemantic.h>

namespace DxUi::RichText
{
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
    Metadata
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
    std::vector<StyleRun> runs_, sourceRuns_;
    friend class SemanticBuilder;
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
    MarkdownAdvance Advance(std::size_t maximumEvents = 4096u);
    void Cancel() noexcept;
    void Clear() noexcept;
    bool HasPendingPreparation() const noexcept;
    std::shared_ptr<const SemanticDocument> Published() const noexcept;

private:
    struct Preparation;
    std::unique_ptr<Preparation> next_;
    std::shared_ptr<const SemanticDocument> published_;
};
} // namespace DxUi::RichText
