#pragma once
#include <DxUi/RichDocument.h>
#include <array>

namespace DxUi::RichText
{
// Requested dialect, carried in every result. These names are grammar targets,
// not a claim that the current block/lexical preparation passes either corpus.
enum class MarkdownProfile
{
    CommonMark0312,
    Gfm029,
    RichMarkdown
};
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
    FencedCode
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
    MarkdownSyntax(const MarkdownSyntax&)            = delete;
    MarkdownSyntax& operator=(const MarkdownSyntax&) = delete;
    std::wstring_view Source() const noexcept;
    std::uint64_t Revision() const noexcept;
    MarkdownProfile Profile() const noexcept;
    const std::vector<MarkdownBlock>& Blocks() const noexcept;
    const std::vector<MarkdownLexeme>& Lexemes() const noexcept;
    std::wstring_view Slice(MarkdownRange range) const;
    std::wstring_view LexemeValue(std::size_t index) const;
    // Text has source-backed interior ACPs; transformed values have explicit
    // leading/trailing anchors, including multi-scalar named entity expansion.
    // This is lexical affinity, not yet a formatted-document editing command.
    std::size_t LexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing = false) const;

private:
    explicit MarkdownSyntax(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile);
    std::wstring source_;
    std::uint64_t revision_{};
    MarkdownProfile profile_{};
    std::vector<MarkdownBlock> blocks_;
    std::vector<MarkdownLexeme> lexemes_;
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
// Begin copies/validates the bounded source once. Advance resumes line scanning,
// root leaf-block recognition and lexical partitioning; it never restarts a scan
// after yielding. One work unit visits one source unit or scalar/CRLF, finalizes one
// physical line, or recognizes one bounded (<=33-unit) lexical candidate.
// Budgets bound operations, not allocator time or a hard wall-clock deadline.
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
    void Begin(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile = MarkdownProfile::CommonMark0312);
    MarkdownAdvance Advance(std::size_t maximumWorkUnits = 4096);
    bool HasPendingPreparation() const noexcept;
    std::shared_ptr<const MarkdownSyntax> Published() const noexcept;
    void Cancel() noexcept;
    void Clear() noexcept;

private:
    struct Preparation;
    std::unique_ptr<Preparation> preparation_;
    std::shared_ptr<const MarkdownSyntax> published_;
};
} // namespace DxUi::RichText
