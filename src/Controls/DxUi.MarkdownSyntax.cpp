#include "DxUi.MarkdownEntityData.h"
#include "DxUi.TextElements.h"
#include <DxUi/MarkdownSyntax.h>
#include <iterator>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
bool Space(wchar_t value) noexcept
{
    return value == L' ' || value == L'\t';
}
bool Punctuation(wchar_t value) noexcept
{
    return (value >= L'!' && value <= L'/') || (value >= L':' && value <= L'@') || (value >= L'[' && value <= L'`') || (value >= L'{' && value <= L'~');
}
void ValidateProfile(MarkdownProfile profile)
{
    if (profile != MarkdownProfile::CommonMark0312 && profile != MarkdownProfile::Gfm029 && profile != MarkdownProfile::RichMarkdown)
        throw std::invalid_argument("Unknown Markdown profile");
}
std::wstring_view ValidateSource(std::wstring_view text, MarkdownProfile profile)
{
    ValidateProfile(profile);
    if (text.size() > Document::MaximumUnits)
        throw std::length_error("Markdown source exceeds bounded preparation capacity");
    for (std::size_t offset = 0; offset < text.size(); ++offset)
    {
        if (IsUtf16LeadSurrogate(text[offset]))
        {
            if (++offset == text.size() || ! IsUtf16TrailSurrogate(text[offset]))
                throw std::invalid_argument("Unpaired Markdown source surrogate");
        }
        else if (IsUtf16TrailSurrogate(text[offset]))
            throw std::invalid_argument("Unpaired Markdown source surrogate");
    }
    // NUL is a valid source scalar here. Its lexical replacement does not edit
    // the original. The existing editor Document admission remains separate.
    return text;
}
void AppendScalar(MarkdownLexeme& token, std::uint32_t scalar) noexcept
{
    if (! scalar || scalar > 0x10ffffu || (scalar >= 0xd800u && scalar <= 0xdfffu))
        scalar = 0xfffdu;
    if (scalar < 0x10000u)
        token.value[token.valueLength++] = static_cast<wchar_t>(scalar);
    else
    {
        scalar -= 0x10000u;
        token.value[token.valueLength++] = static_cast<wchar_t>(0xd800u + (scalar >> 10u));
        token.value[token.valueLength++] = static_cast<wchar_t>(0xdc00u + (scalar & 1023u));
    }
}
bool CharacterReference(std::wstring_view source, std::size_t first, MarkdownLexeme& token) noexcept
{
    auto cursor = first + 1u;
    if (cursor == source.size())
        return false;
    if (source[cursor] == L'#')
    {
        ++cursor;
        unsigned base = 10u, maximumDigits = 7u;
        if (cursor < source.size() && (source[cursor] == L'x' || source[cursor] == L'X'))
        {
            ++cursor;
            base          = 16u;
            maximumDigits = 6u;
        }
        std::uint32_t scalar = 0u;
        unsigned digits      = 0u;
        while (cursor < source.size() && digits < maximumDigits)
        {
            const auto character = source[cursor];
            unsigned digit       = 16u;
            if (character >= L'0' && character <= L'9')
                digit = static_cast<unsigned>(character - L'0');
            else if (character >= L'a' && character <= L'f')
                digit = 10u + static_cast<unsigned>(character - L'a');
            else if (character >= L'A' && character <= L'F')
                digit = 10u + static_cast<unsigned>(character - L'A');
            if (digit >= base)
                break;
            scalar = scalar * base + digit;
            ++cursor;
            ++digits;
        }
        if (! digits || cursor == source.size() || source[cursor] != L';')
            return false;
        token = {MarkdownLexemeKind::CharacterReference, {first, cursor + 1u}};
        AppendScalar(token, scalar);
        return true;
    }
    // Longest authoritative spelling is 33 UTF-16/ASCII units including & and ;.
    // No suffix rescans of arbitrary document length, even for hostile '&aaaa'.
    while (cursor < source.size() && cursor - first < 33u)
    {
        const auto character = source[cursor++];
        if (character == L';')
        {
            const auto name   = source.substr(first, cursor - first);
            const auto* entry = std::lower_bound(std::begin(MarkdownEntityData::kEntities),
                                                 std::end(MarkdownEntityData::kEntities),
                                                 name,
                                                 [](const auto& candidate, std::wstring_view wanted) { return candidate.name < wanted; });
            if (entry == std::end(MarkdownEntityData::kEntities) || entry->name != name)
                return false;
            token = {MarkdownLexemeKind::CharacterReference, {first, cursor}};
            AppendScalar(token, entry->first);
            if (entry->second)
                AppendScalar(token, entry->second);
            return true;
        }
        if (! ((character >= L'a' && character <= L'z') || (character >= L'A' && character <= L'Z') || (character >= L'0' && character <= L'9')))
            break;
    }
    return false;
}

// Derived in one resumable pass. Classification never scans the physical line
// again, including a maximum-capacity line of fence/heading punctuation.
struct PhysicalLine
{
    std::size_t first{}, contentFirst{}, runEnd{}, trimEnd{}, tailFirst{}, hashFirst{}, beforeHashes{};
    std::size_t nonSpace{};
    unsigned indent{};
    wchar_t marker{};
    bool hasContent{}, inRun = true, tailHasContent{}, tailBacktick{}, onlyMarker = true, trailingHashes{};
    void Visit(wchar_t character, std::size_t offset) noexcept
    {
        if (! hasContent)
        {
            if (Space(character))
            {
                indent = std::min(4u, character == L'\t' ? (indent + 4u) & ~3u : indent + 1u);
                return;
            }
            hasContent   = true;
            contentFirst = offset;
            marker       = character;
        }
        if (inRun && character == marker)
            runEnd = offset + 1u;
        else
        {
            inRun = false;
            if (character == L'`')
                tailBacktick = true;
            if (! Space(character) && ! tailHasContent)
            {
                tailHasContent = true;
                tailFirst      = offset;
            }
        }
        if (! Space(character))
        {
            trailingHashes = character == L'#';
            onlyMarker     = onlyMarker && character == marker;
            ++nonSpace;
            trimEnd = offset + 1u;
        }
    }
};
} // namespace

MarkdownSyntax::MarkdownSyntax(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile)
    : source_(ValidateSource(source, profile)),
      revision_(revision),
      profile_(profile),
      blocks_(0u),
      lexemes_(0u)
{
}
std::wstring_view MarkdownSyntax::Source() const noexcept
{
    return source_;
}
std::uint64_t MarkdownSyntax::Revision() const noexcept
{
    return revision_;
}
MarkdownProfile MarkdownSyntax::Profile() const noexcept
{
    return profile_;
}
const std::vector<MarkdownBlock>& MarkdownSyntax::Blocks() const noexcept
{
    return blocks_;
}
const std::vector<MarkdownLexeme>& MarkdownSyntax::Lexemes() const noexcept
{
    return lexemes_;
}
std::wstring_view MarkdownSyntax::Slice(MarkdownRange range) const
{
    if (range.first > range.last || range.last > source_.size())
        throw std::out_of_range("Markdown source range");
    return std::wstring_view(source_).substr(range.first, range.last - range.first);
}
std::wstring_view MarkdownSyntax::LexemeValue(std::size_t index) const
{
    const auto& token = lexemes_.at(index);
    return token.kind == MarkdownLexemeKind::Text ? Slice(token.source) : std::wstring_view(token.value.data(), token.valueLength);
}
std::size_t MarkdownSyntax::LexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing) const
{
    const auto& token = lexemes_.at(index);
    const auto length = LexemeValue(index).size();
    if (valueOffset > length)
        throw std::out_of_range("Markdown lexical value offset");
    if (token.kind == MarkdownLexemeKind::Text)
        return token.source.first + valueOffset;
    if (! valueOffset)
        return token.source.first;
    return valueOffset == length || trailing ? token.source.last : token.source.first;
}

struct MarkdownParser::Preparation
{
    explicit Preparation(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile)
        // The private constructor prevents exposing unparsed mutable shells.
        // shared_ptr owns immediately and deletes on control-block failure too.
        : syntax(std::shared_ptr<MarkdownSyntax>(new MarkdownSyntax(source, revision, profile)))
    {
    }
    std::shared_ptr<MarkdownSyntax> syntax;
    PhysicalLine line;
    std::size_t cursor{}, lexicalCursor{}, literalFirst{};
    bool blocksComplete{}, lexemesComplete{}, hashRun{};

    void FinishLine(std::size_t bodyEnd, std::size_t end)
    {
        auto& blocks         = syntax->blocks_;
        const auto runLength = line.hasContent ? line.runEnd - line.contentFirst : 0u;
        const bool fence     = line.hasContent && line.indent <= 3u && runLength >= 3u && (line.marker == L'`' || line.marker == L'~');
        if (! blocks.empty() && blocks.back().kind == MarkdownBlockKind::FencedCode && ! blocks.back().closed)
        {
            auto& block = blocks.back();
            if (fence && line.marker == block.fenceMarker && runLength >= block.fenceLength && ! line.tailHasContent)
            {
                block.source.last  = end;
                block.content.last = line.first;
                block.closed       = true;
            }
            else
            {
                block.source.last  = end;
                block.content.last = end;
            }
        }
        else if (! line.hasContent)
            blocks.push_back({MarkdownBlockKind::BlankLine, {line.first, end}, {line.first, bodyEnd}});
        else if (line.indent <= 3u && (line.marker == L'=' || line.marker == L'-') && ! line.tailHasContent && ! blocks.empty() &&
                 blocks.back().kind == MarkdownBlockKind::Paragraph)
        {
            auto& block       = blocks.back();
            block.kind        = MarkdownBlockKind::SetextHeading;
            block.heading     = line.marker == L'=' ? 1u : 2u;
            block.source.last = end;
        }
        else if (fence && (line.marker == L'~' || ! line.tailBacktick))
        {
            const auto infoFirst = line.tailHasContent ? line.tailFirst : bodyEnd;
            blocks.push_back({MarkdownBlockKind::FencedCode,
                              {line.first, end},
                              {end, end},
                              {infoFirst, std::max(infoFirst, line.trimEnd)},
                              0u,
                              line.indent,
                              line.marker,
                              runLength,
                              false});
        }
        else if (line.indent <= 3u && line.marker == L'#' && runLength <= 6u && (line.runEnd == bodyEnd || Space(syntax->source_[line.runEnd])))
        {
            const auto first = line.tailHasContent ? line.tailFirst : bodyEnd;
            auto last        = std::max(first, line.trimEnd);
            if (line.trailingHashes && line.hashFirst >= first && line.hashFirst < last &&
                (line.hashFirst == line.runEnd || (line.hashFirst > line.runEnd && Space(syntax->source_[line.hashFirst - 1u]))))
                last = std::max(first, line.beforeHashes);
            blocks.push_back({MarkdownBlockKind::AtxHeading, {line.first, end}, {first, last}, {}, static_cast<unsigned>(runLength)});
        }
        else if (line.indent <= 3u && line.onlyMarker && line.nonSpace >= 3u && (line.marker == L'*' || line.marker == L'_' || line.marker == L'-'))
            blocks.push_back({MarkdownBlockKind::ThematicBreak, {line.first, end}, {line.contentFirst, line.trimEnd}});
        else if (! blocks.empty() && blocks.back().kind == MarkdownBlockKind::Paragraph)
        {
            blocks.back().source.last  = end;
            blocks.back().content.last = line.trimEnd;
        }
        else
            blocks.push_back({MarkdownBlockKind::Paragraph, {line.first, end}, {line.contentFirst, line.trimEnd}});
        line       = {};
        line.first = end;
        hashRun    = false;
    }
    void ScanBlockUnit()
    {
        const auto& source = syntax->source_;
        if (cursor == source.size())
        {
            if (line.first < cursor)
                FinishLine(cursor, cursor);
            blocksComplete = true;
            return;
        }
        const auto character = source[cursor];
        if (character == L'\r' || character == L'\n')
        {
            const auto bodyEnd = cursor++;
            if (character == L'\r' && cursor < source.size() && source[cursor] == L'\n')
                ++cursor;
            FinishLine(bodyEnd, cursor);
            return;
        }
        if (character == L'#')
        {
            if (! hashRun)
            {
                line.hashFirst    = cursor;
                line.beforeHashes = line.trimEnd;
            }
            hashRun = true;
        }
        else
            hashRun = false;
        line.Visit(character, cursor);
        ++cursor;
        // Scanning can observe an interior surrogate ACP; publication ranges
        // are formed only at line boundaries. It never truncates the source.
    }
    void FlushLiteral(std::size_t end)
    {
        if (literalFirst < end)
            syntax->lexemes_.push_back({MarkdownLexemeKind::Text, {literalFirst, end}});
    }
    void ScanLexicalUnit()
    {
        const auto& source = syntax->source_;
        if (lexicalCursor == source.size())
        {
            FlushLiteral(lexicalCursor);
            lexemesComplete = true;
            return;
        }
        const auto first     = lexicalCursor;
        const auto character = source[first];
        MarkdownLexeme token;
        if (character == L'&' && CharacterReference(source, first, token))
            lexicalCursor = token.source.last;
        else if (character == L'\\' && first + 1u < source.size() && Punctuation(source[first + 1u]))
        {
            lexicalCursor += 2u;
            token = {MarkdownLexemeKind::EscapedPunctuation, {first, lexicalCursor}};
            AppendScalar(token, source[first + 1u]);
        }
        else if (character == L'\r' || character == L'\n' || character == L'\0')
        {
            ++lexicalCursor;
            if (character == L'\r' && lexicalCursor < source.size() && source[lexicalCursor] == L'\n')
                ++lexicalCursor;
            token = {character == L'\0' ? MarkdownLexemeKind::NulReplacement : MarkdownLexemeKind::LineEnding, {first, lexicalCursor}};
            AppendScalar(token, character == L'\0' ? 0xfffdu : static_cast<std::uint32_t>(L'\n'));
        }
        else
        {
            lexicalCursor = StepToNextCodePoint(source, first);
            return;
        }
        FlushLiteral(first);
        syntax->lexemes_.push_back(token);
        literalFirst = lexicalCursor;
    }
};

MarkdownParser::MarkdownParser() noexcept                            = default;
MarkdownParser::~MarkdownParser() noexcept                           = default;
MarkdownParser::MarkdownParser(MarkdownParser&&) noexcept            = default;
MarkdownParser& MarkdownParser::operator=(MarkdownParser&&) noexcept = default;
bool MarkdownParser::HasPendingPreparation() const noexcept
{
    return static_cast<bool>(preparation_);
}
std::shared_ptr<const MarkdownSyntax> MarkdownParser::Published() const noexcept
{
    return published_;
}
void MarkdownParser::Cancel() noexcept
{
    preparation_.reset();
}
void MarkdownParser::Clear() noexcept
{
    Cancel();
    published_.reset();
}
void MarkdownParser::Begin(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile)
{
    ValidateProfile(profile);
    const auto same = [&](const auto& syntax)
    { return syntax && syntax->Revision() == revision && syntax->Profile() == profile && syntax->Source() == source; };
    if (preparation_ && same(preparation_->syntax))
        return;
    if (same(published_))
    {
        Cancel();
        return;
    }
    auto staged  = std::make_unique<Preparation>(source, revision, profile);
    preparation_ = std::move(staged);
}
MarkdownAdvance MarkdownParser::Advance(std::size_t maximumWorkUnits)
{
    if (! preparation_)
        return {published_ ? MarkdownPreparationStatus::Complete : MarkdownPreparationStatus::Idle};
    MarkdownAdvance result{MarkdownPreparationStatus::Pending};
    try
    {
        auto& pending = *preparation_;
        while (result.workUnits < maximumWorkUnits)
        {
            ++result.workUnits;
            if (! pending.blocksComplete)
                pending.ScanBlockUnit();
            else if (! pending.lexemesComplete)
                pending.ScanLexicalUnit();
            else
            {
                published_ = std::move(pending.syntax);
                preparation_.reset();
                result.status = MarkdownPreparationStatus::Complete;
                break;
            }
        }
    }
    catch (const std::exception&)
    {
        Cancel();
        throw;
    }
    return result;
}
} // namespace DxUi::RichText
