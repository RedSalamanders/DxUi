#include "DxUi.MarkdownBlocks.h"
#include "DxUi.MarkdownCharacterReference.h"
#include "DxUi.MarkdownEntityData.h"
#include "DxUi.MarkdownInlines.h"
#include "DxUi.MarkdownMetadata.h"
#include "DxUi.MarkdownSourceCorrelation.h"
#include "DxUi.TextElements.h"
#include "Support/WorkCounters.h"
#include <DxUi/Failure.h>
#include <DxUi/MarkdownSyntax.h>
#include <atomic>
#include <iterator>
#include <map>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
std::atomic<std::uint64_t> nextDocumentIdentity{1u};
bool Punctuation(wchar_t value) noexcept
{
    return (value >= L'!' && value <= L'/') || (value >= L':' && value <= L'@') || (value >= L'[' && value <= L'`') || (value >= L'{' && value <= L'~');
}
bool TryResolveExtensions(MarkdownProfile profile, std::optional<MarkdownExtensions> extensions, MarkdownExtensions& result) noexcept
{
    if (profile != MarkdownProfile::CommonMark0312 && profile != MarkdownProfile::Gfm029 && profile != MarkdownProfile::RichMarkdown)
        return false;
    result = extensions.value_or(profile == MarkdownProfile::CommonMark0312 ? MarkdownExtensions::None
                                 : profile == MarkdownProfile::RichMarkdown ? MarkdownExtensions::RichAll
                                                                            : MarkdownExtensions::All);
    return ! (static_cast<unsigned>(result) &
              ~static_cast<unsigned>(profile == MarkdownProfile::RichMarkdown ? MarkdownExtensions::RichOptionalMask : MarkdownExtensions::All)) &&
           (profile != MarkdownProfile::CommonMark0312 || result == MarkdownExtensions::None);
}
void RequireSuccess(MarkdownResult result)
{
    if (result != MarkdownResult::Success)
        Failure::FailContract(E_INVALIDARG);
}
MarkdownResult ValidateSource(std::wstring_view text) noexcept
{
    if (text.size() > Document::MaximumUnits)
        return MarkdownResult::CapacityExceeded;
    // NUL is a valid source scalar here. Its lexical replacement does not edit
    // the original. The existing editor Document admission remains separate.
    return IsValidSourceUtf16(text) ? MarkdownResult::Success : MarkdownResult::InvalidSource;
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

// Incremental parse reuse (plan item W7.2). What a publication carries for its parser's next parse travels in the
// deleter of the publication's own shared_ptr control block, so the public type keeps its layout: the effective link
// reference definitions, the inline staging of each leaf, the physical line count and the fresh top-level starts. A
// publication without it (an adopted parse) is never a reuse source.
struct ReuseState
{
    ReuseState() : references(0u), leafStaging(0u), freshRoots(0u)
    {
    }
    std::vector<Detail::MarkdownReferenceValue> references;
    std::vector<std::uint32_t> leafStaging;
    // Sorted source starts of the top-level blocks whose first line the block recognizer began with no document-level
    // leaf or table row open (containers that line closed do not matter). Only these can be imported: a block born from a paragraph's tail (a table header, the
    // leftover of reference-definition extraction) or from a container's leftover depended on what was open before it.
    std::vector<std::size_t> freshRoots;
    std::size_t physicalLines{};
};
struct SyntaxDeleter
{
    std::shared_ptr<ReuseState> state;
    void operator()(MarkdownSyntax* syntax) const noexcept
    {
        delete syntax;
    }
};
const ReuseState* ReuseOf(const std::shared_ptr<const MarkdownSyntax>& syntax) noexcept
{
    const auto* deleter = syntax ? std::get_deleter<SyntaxDeleter>(syntax) : nullptr;
    return deleter ? deleter->state.get() : nullptr;
}
// Line breaks counted at their last unit (LF, or CR not followed by LF) in [first, last): with the unit before a window
// included, the counts of an unchanged prefix and suffix are equal in both revisions.
std::size_t LineBreaks(std::wstring_view text, std::size_t first, std::size_t last) noexcept
{
    std::size_t count{};
    for (auto at = first; at < last; ++at)
        if (text[at] == L'\n' || (text[at] == L'\r' && (at + 1u == text.size() || text[at + 1u] != L'\n')))
            ++count;
    return count;
}
// The new revision's flat source from the previous publication's source and the tracked edit window: only the window
// is read out of the piece tree. False when the metadata does not fit the previous source.
bool ComposeSource(const SourceSnapshot& source,
                   const std::shared_ptr<const MarkdownSyntax>& published,
                   const std::optional<SourceChangeRange>& change,
                   std::wstring& result)
{
    if (! published || ! change || change->kind != SourceChangeKind::TrackedEdits)
        return false;
    const auto previous = published->Source();
    const auto length   = source.Length();
    if (change->begin > change->previousEnd || change->previousEnd > previous.size() || change->begin > change->currentEnd || change->currentEnd > length ||
        previous.size() - change->previousEnd != length - change->currentEnd)
        return false;
    std::wstring window(0u, L' ');
    if (source.TrySlice(change->begin, change->currentEnd, window) != SourceResult::Success)
        return false;
    result.reserve(length);
    result.append(previous.substr(0u, change->begin));
    result.append(window);
    result.append(previous.substr(change->previousEnd));
    ::DxUi::Detail::Work().rich.markdownSourceUnitsMaterialized += window.size();
    return true;
}

} // namespace

bool Detail::DecodeMarkdownCharacterReference(std::wstring_view input, std::size_t first, MarkdownLexeme& token) noexcept
{
    return CharacterReference(input, first, token);
}

MarkdownSyntax::MarkdownSyntax(
    std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, MarkdownExtensions extensions, std::optional<MarkdownSourceRegion> sourceRegion)
    : _source(source),
      _revision(revision),
      _profile(profile),
      _extensions(extensions),
      _blocks(0u),
      _lexemes(0u),
      _nodes(0u),
      _emptyLines(0u),
      _inlines(0u),
      _diagnostics(0u),
      _footnotes(0u),
      _footnoteReferences(0u),
      _sourceRegionRequest(sourceRegion)
{
}
MarkdownSyntax::MarkdownSyntax(std::shared_ptr<const MarkdownSyntax> parsedDataOwner,
                               std::uint64_t revision,
                               MarkdownProfile profile,
                               MarkdownExtensions extensions,
                               std::uint64_t documentIdentity)
    : _source(0u, L'\0'),
      _revision(revision),
      _profile(profile),
      _extensions(extensions),
      _blocks(0u),
      _lexemes(0u),
      _nodes(0u),
      _emptyLines(0u),
      _inlines(0u),
      _diagnostics(0u),
      _footnotes(0u),
      _footnoteReferences(0u),
      _documentIdentity(documentIdentity),
      _sourceRegionRequest(std::nullopt),
      _parsedDataOwner(std::move(parsedDataOwner))
{
}
const MarkdownSyntax& MarkdownSyntax::ParsedData() const noexcept
{
    return _parsedDataOwner ? *_parsedDataOwner : *this;
}
std::wstring_view MarkdownSyntax::Source() const noexcept
{
    return ParsedData()._source;
}
std::optional<MarkdownSourceRegion> MarkdownSyntax::SourceRegion() const noexcept
{
    if (! _sourceRegionRequest)
        return std::nullopt;
    if (_sourceRegionRequest->kind == MarkdownSourceRegionKind::Inline)
        return _sourceRegionRequest;
    if (_sourceRegionNode == MarkdownNoNode)
        return std::nullopt;
    return MarkdownSourceRegion{_nodes[_sourceRegionNode].block.source, _sourceRegionRequest->session};
}
std::size_t MarkdownSyntax::SourceRegionNode() const noexcept
{
    return _sourceRegionNode;
}
std::uint64_t MarkdownSyntax::Revision() const noexcept
{
    return _revision;
}
MarkdownProfile MarkdownSyntax::Profile() const noexcept
{
    return _profile;
}
MarkdownExtensions MarkdownSyntax::Extensions() const noexcept
{
    return _extensions;
}
const std::vector<MarkdownBlock>& MarkdownSyntax::Blocks() const noexcept
{
    return ParsedData()._blocks;
}
const std::vector<MarkdownNode>& MarkdownSyntax::Nodes() const noexcept
{
    return ParsedData()._nodes;
}
std::span<const MarkdownEmptyLine> MarkdownSyntax::EmptyLines() const noexcept
{
    return ParsedData()._emptyLines;
}
std::uint64_t MarkdownSyntax::DocumentIdentity() const noexcept
{
    return _documentIdentity;
}
MarkdownBlockStatistics MarkdownSyntax::BlockStatistics() const noexcept
{
    return ParsedData()._blockStatistics;
}
MarkdownInlineStatistics MarkdownSyntax::InlineStatistics() const noexcept
{
    return ParsedData()._inlineStatistics;
}
const std::vector<MarkdownLexeme>& MarkdownSyntax::Lexemes() const noexcept
{
    return ParsedData()._lexemes;
}
const std::vector<MarkdownInline>& MarkdownSyntax::Inlines() const noexcept
{
    return ParsedData()._inlines;
}
std::span<const MarkdownDiagnostic> MarkdownSyntax::Diagnostics() const noexcept
{
    return ParsedData()._diagnostics;
}
std::span<const MarkdownFootnote> MarkdownSyntax::Footnotes() const noexcept
{
    return ParsedData()._footnotes;
}
std::span<const MarkdownFootnoteReference> MarkdownSyntax::FootnoteReferences() const noexcept
{
    return ParsedData()._footnoteReferences;
}
const MarkdownFootnoteReference* MarkdownSyntax::FootnoteReferenceFor(std::size_t inlineNode) const noexcept
{
    const auto& references = ParsedData()._footnoteReferences;
    const auto found =
        std::lower_bound(references.begin(), references.end(), inlineNode, [](const auto& reference, auto wanted) { return reference.inlineNode < wanted; });
    return found != references.end() && found->inlineNode == inlineNode ? &*found : nullptr;
}
SourceResult MarkdownSyntax::TrySlice(MarkdownRange range, std::wstring_view& output) const noexcept
{
    const auto source = Source();
    if (range.first > range.last || range.last > source.size())
        return SourceResult::InvalidRange;
    output = source.substr(range.first, range.last - range.first);
    return SourceResult::Success;
}
SourceResult MarkdownSyntax::TryLexemeValue(std::size_t index, std::wstring_view& output) const noexcept
{
    if (_parsedDataOwner)
        return _parsedDataOwner->TryLexemeValue(index, output);
    if (index >= _lexemes.size())
        return SourceResult::InvalidRange;
    const auto& token = _lexemes[index];
    if (token.kind == MarkdownLexemeKind::Text)
        return TrySlice(token.source, output);
    if (token.valueLength > token.value.size())
        return SourceResult::InvalidRange;
    output = std::wstring_view(token.value.data(), token.valueLength);
    return SourceResult::Success;
}
SourceResult MarkdownSyntax::TryLexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing, std::size_t& output) const noexcept
{
    if (_parsedDataOwner)
        return _parsedDataOwner->TryLexemeSourceAnchor(index, valueOffset, trailing, output);
    std::wstring_view value;
    if (TryLexemeValue(index, value) != SourceResult::Success)
        return SourceResult::InvalidRange;
    const auto& token = _lexemes[index];
    const auto length = value.size();
    if (valueOffset > length)
        return SourceResult::InvalidRange;
    if (token.kind == MarkdownLexemeKind::Text)
        output = token.source.first + valueOffset;
    else if (! valueOffset)
        output = token.source.first;
    else
        output = valueOffset == length || trailing ? token.source.last : token.source.first;
    return SourceResult::Success;
}
std::wstring_view MarkdownSyntax::Slice(MarkdownRange range) const
{
    std::wstring_view output;
    if (TrySlice(range, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return output;
}
std::wstring_view MarkdownSyntax::LexemeValue(std::size_t index) const
{
    std::wstring_view output;
    if (TryLexemeValue(index, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return output;
}
std::size_t MarkdownSyntax::LexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing) const
{
    std::size_t output{};
    if (TryLexemeSourceAnchor(index, valueOffset, trailing, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return output;
}

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4626 5027)
struct MarkdownParser::Preparation
{
    explicit Preparation(std::wstring_view source,
                         std::uint64_t revision,
                         MarkdownProfile profile,
                         MarkdownExtensions extensions,
                         std::optional<MarkdownSourceRegion> sourceRegion,
                         std::wstring* owned)
        : syntax(Create(source, revision, profile, extensions, sourceRegion, owned))
    {
    }
    // The private constructor prevents exposing unparsed mutable shells. shared_ptr owns immediately and deletes on
    // control-block failure too; its deleter carries the reuse state (W7.2). An owned (composed) source is moved in
    // before any later member views it, so the source is copied once.
    static std::shared_ptr<MarkdownSyntax> Create(std::wstring_view source,
                                                  std::uint64_t revision,
                                                  MarkdownProfile profile,
                                                  MarkdownExtensions extensions,
                                                  std::optional<MarkdownSourceRegion> sourceRegion,
                                                  std::wstring* owned)
    {
        auto state = std::make_shared<ReuseState>();
        std::shared_ptr<MarkdownSyntax> created(new MarkdownSyntax(owned ? std::wstring_view{} : source, revision, profile, extensions, sourceRegion),
                                                SyntaxDeleter{std::move(state)});
        if (owned)
            created->_source.swap(*owned);
        return created;
    }
    // MarkdownParser::BeginSource, with an optional owned copy of source to adopt instead of copying it.
    static void Begin(MarkdownParser& parser,
                      std::wstring_view source,
                      std::uint64_t revision,
                      MarkdownProfile profile,
                      MarkdownExtensions extensions,
                      std::optional<SourceChangeRange> change,
                      std::optional<MarkdownSourceRegion> sourceRegion,
                      std::wstring* owned)
    {
        const auto same = [&](const auto& syntax)
        {
            return syntax && syntax->Revision() == revision && syntax->Profile() == profile && syntax->Extensions() == extensions &&
                   syntax->Source() == source && syntax->_sourceRegionRequest == sourceRegion;
        };
        const auto sameChange = [](const auto& a, const auto& b)
        {
            return (! a && ! b) || (a && b && a->kind == b->kind && a->begin == b->begin && a->previousEnd == b->previousEnd && a->currentEnd == b->currentEnd);
        };
        if (parser._preparation && same(parser._preparation->syntax) && sameChange(parser._preparation->change, change))
            return;
        if (same(parser._published) && (! change || change->kind == SourceChangeKind::Unchanged))
        {
            parser.Cancel();
            return;
        }
        const auto borrowed     = parser._published && parser._published->_parsedDataOwner;
        const auto document     = borrowed                   ? nextDocumentIdentity.fetch_add(1u, std::memory_order_relaxed)
                                  : parser._documentIdentity ? parser._documentIdentity
                                                             : nextDocumentIdentity.fetch_add(1u, std::memory_order_relaxed);
        const auto nextIdentity = borrowed ? 1u : parser._nextNodeIdentity;
        const auto units        = source.size();
        auto staged             = std::make_unique<Preparation>(source, revision, profile, extensions, sourceRegion, owned);
        ++::DxUi::Detail::Work().rich.markdownParses;
        ::DxUi::Detail::Work().rich.markdownSourceUnitsCopied += units;
        staged->change = change;
        staged->SetPrevious(borrowed ? std::shared_ptr<const MarkdownSyntax>{} : parser._published, document, nextIdentity);
        parser._preparation      = std::move(staged);
        parser._documentIdentity = document;
        parser._nextNodeIdentity = nextIdentity;
    }
    static std::optional<std::wstring> BlockSource(const MarkdownSyntax& syntax)
    {
        if (! syntax._sourceRegionRequest || syntax._sourceRegionRequest->kind != MarkdownSourceRegionKind::Inline)
            return std::nullopt;
        std::wstring source(syntax._source);
        std::fill(source.begin() + static_cast<std::ptrdiff_t>(syntax._sourceRegionRequest->source.first),
                  source.begin() + static_cast<std::ptrdiff_t>(syntax._sourceRegionRequest->source.last),
                  L'x');
        // A deleted object between delimiter runs must not turn its containing
        // paragraph into a thematic break before inline parsing.
        const auto range = syntax._sourceRegionRequest->source;
        if (range.first == range.last && ! source.empty())
        {
            const auto before = range.first ? source.find_last_of(L"\r\n", range.first - 1u) : std::wstring::npos;
            const auto first  = before == std::wstring::npos ? 0u : before + 1u;
            const auto after  = source.find_first_of(L"\r\n", range.first);
            const auto last   = after == std::wstring::npos ? source.size() : after;
            const auto line   = std::wstring_view(source).substr(first, last - first);
            if (line.size() >= 3u && line.find_first_not_of(L"*_ -\t") == std::wstring_view::npos && range.first < last)
                source[range.first] = L'x';
        }
        return source;
    }
    std::shared_ptr<MarkdownSyntax> syntax;
    std::optional<std::wstring> blockSourceStorage = BlockSource(*syntax);
    std::wstring_view blockSource                  = blockSourceStorage ? std::wstring_view(*blockSourceStorage) : std::wstring_view(syntax->_source);
    std::shared_ptr<const MarkdownSyntax> previous;
    std::optional<SourceSnapshot> snapshot;
    std::optional<SourceChangeRange> change;
    using NodeKey = std::tuple<std::uint64_t, std::size_t, std::size_t, MarkdownBlockKind>;
    // Generated missing table cells can have identical empty source ranges.
    // Preserve their insertion order and consume each old identity only once.
    std::multimap<NodeKey, std::size_t> oldNodes;
    std::size_t commonFirst{}, commonOldEnd{}, commonNewEnd{}, identityCursor{}, oldIndexCursor{};
    std::size_t lexicalPrefixLimit{}, lexicalPrefixIndex{}, lexicalSuffixIndex = MarkdownNoNode, lexicalSuffixStart{};
    bool copyingLexicalSuffix{};
    std::uint64_t nextIdentity = 1u;
    bool identitiesComplete{};
    bool discardOldIdentities{};
    // Incremental reuse (plan item W7.2): admitted for a compatible previous publication that carries reuse state. The
    // reuse window is the correlation envelope with its unchanged edge units trimmed, so a conservative tracked range
    // does not widen the reparse; identity reconciliation keeps the trusted envelope.
    bool reuse{};
    const ReuseState* previousState{};
    std::size_t reuseFirst{}, reuseOldEnd{}, reuseNewEnd{}, sourceLines{};
    std::vector<Detail::MarkdownImportedSubtree> importedSubtrees = std::vector<Detail::MarkdownImportedSubtree>(0u);
    std::size_t importedRootCursor{}, identitySubtree{}, oldSkip{};
    // Line starts this parse reached with no document-level leaf or table row open, then the top-level starts among them (W7.2).
    std::vector<std::size_t> freshLines = std::vector<std::size_t>(0u);
    std::vector<std::size_t> freshRoots = std::vector<std::size_t>(0u);
    std::size_t freshCursor{};
    bool FreshInPrevious(std::size_t start) const noexcept
    {
        return std::binary_search(previousState->freshRoots.begin(), previousState->freshRoots.end(), start);
    }
    // After an imported block the new parse continues with nothing open. That matches the previous parse only if it too
    // had nothing open at the next top-level block: separated by document-level blank lines alone, the next block began
    // fresh, or the block reached the end. A paragraph whose last line became a table header, for example, was still
    // open at the next line.
    bool EndsAtFreshStart(std::size_t start) const noexcept
    {
        const auto& roots = previous->Blocks();
        const auto blanks = previous->EmptyLines();
        auto at           = std::lower_bound(roots.begin(), roots.end(), start, [](const auto& block, auto position) { return block.source.first < position; });
        if (at == roots.end() || at->source.first != start)
            return false;
        for (++at; at != roots.end() && at->kind == MarkdownBlockKind::BlankLine; ++at)
        {
            const auto blank =
                std::lower_bound(blanks.begin(), blanks.end(), at->source.first, [](const auto& line, auto position) { return line.source.first < position; });
            if (blank == blanks.end() || blank->source.first != at->source.first || blank->owner != 0u)
                return false;
        }
        return at == roots.end() || FreshInPrevious(at->source.first);
    }
    void SetPrevious(std::shared_ptr<const MarkdownSyntax> old, std::uint64_t document, std::uint64_t next)
    {
        syntax->_documentIdentity = document;
        previous                  = std::move(old);
        nextIdentity              = next;
        if (previous && previous->Profile() != syntax->Profile())
            previous.reset();
        const auto b = syntax->Source();
        if (! previous)
        {
            ReserveLines(LineBreaks(b, 0u, b.size()) + 1u);
            return;
        }
        const auto a           = previous->Source();
        const auto correlation = Detail::CorrelateMarkdownSource(
            a.size(), b.size(), change, [&](std::size_t at) noexcept { return a[at]; }, [&](std::size_t at) noexcept { return b[at]; });
        commonFirst          = correlation.first;
        commonOldEnd         = correlation.previousEnd;
        commonNewEnd         = correlation.currentEnd;
        discardOldIdentities = correlation.discardPrevious;
        reuseFirst           = commonFirst;
        reuseOldEnd          = commonOldEnd;
        reuseNewEnd          = commonNewEnd;
        const bool envelope  = ! discardOldIdentities && (! change || change->kind == SourceChangeKind::TrackedEdits);
        if (envelope)
        {
            // A tracked range may include unchanged units; trimming reads only the window.
            while (reuseFirst < reuseOldEnd && reuseFirst < reuseNewEnd && a[reuseFirst] == b[reuseFirst])
                ++reuseFirst;
            while (reuseOldEnd > reuseFirst && reuseNewEnd > reuseFirst && a[reuseOldEnd - 1u] == b[reuseNewEnd - 1u])
            {
                --reuseOldEnd;
                --reuseNewEnd;
            }
        }
        previousState = ReuseOf(previous);
        reuse =
            envelope && previousState && previous->Extensions() == syntax->Extensions() && ! previous->_sourceRegionRequest && ! syntax->_sourceRegionRequest;
        if (reuse)
        {
            // Physical lines of the unchanged prefix and suffix are the previous publication's; count the window only.
            const auto from = reuseFirst ? reuseFirst - 1u : 0u;
            ReserveLines(previousState->physicalLines - LineBreaks(a, from, reuseOldEnd) + LineBreaks(b, from, reuseNewEnd));
        }
        else
            ReserveLines(LineBreaks(b, 0u, b.size()) + 1u);
        if (envelope)
        {
            // Lexical constructs have a maximum 33-unit named reference (or a
            // two-unit escape/CRLF). Keep a safety margin on both edit edges.
            // No lexeme crosses a line break (a CR can still join a following
            // LF), so the margins also stop at the edit's own line (W7.2).
            constexpr std::size_t margin = 34u;
            lexicalPrefixLimit           = reuseFirst > margin ? reuseFirst - margin : 0u;
            for (auto at = reuseFirst; at > lexicalPrefixLimit; --at)
                if (a[at - 1u] == L'\n' || a[at - 1u] == L'\r')
                {
                    lexicalPrefixLimit = a[at - 1u] == L'\n' ? at : at - 1u;
                    break;
                }
            auto oldSafe = reuseOldEnd + std::min(margin, a.size() - reuseOldEnd);
            for (auto at = reuseOldEnd; at < oldSafe; ++at)
                if (a[at] == L'\n' || a[at] == L'\r')
                {
                    oldSafe = at;
                    break;
                }
            const auto& lexemes = previous->Lexemes();
            auto suffix         = std::lower_bound(lexemes.begin(), lexemes.end(), oldSafe, [](const auto& token, auto at) { return token.source.first < at; });
            while (suffix != lexemes.end() && suffix->kind == MarkdownLexemeKind::Text)
                ++suffix;
            if (suffix != lexemes.end())
            {
                lexicalSuffixIndex = static_cast<std::size_t>(suffix - lexemes.begin());
                lexicalSuffixStart = reuseNewEnd + (suffix->source.first - reuseOldEnd);
            }
        }
    }
    // The exact physical line count of this source, carried to the next parse; the block recognizer reserves its
    // empty-line vector for the lines of its (possibly region-masked) block source, which has the same line breaks
    // except under a source-region request.
    void ReserveLines(std::size_t lines)
    {
        sourceLines = lines;
        blocks.ReserveEmptyLines(blockSourceStorage ? LineBreaks(blockSource, 0u, blockSource.size()) + 1u : lines);
    }
    // An unchanged closed envelope imported from the previous parse: its validation result is the previous one (W7.2).
    void CarryFrontMatterResult(const MarkdownNode& node, std::size_t nodeIndex, std::size_t oldIndex)
    {
        const auto content     = previous->Nodes()[oldIndex].block.content;
        const auto diagnostics = previous->Diagnostics();
        auto found = std::lower_bound(diagnostics.begin(), diagnostics.end(), content.first, [](const auto& item, auto at) { return item.source.first < at; });
        for (; found != diagnostics.end() && found->source.first == content.first; ++found)
            if (found->node == oldIndex && found->code != MarkdownDiagnosticCode::UnclosedFrontMatter &&
                found->code != MarkdownDiagnosticCode::DuplicateFootnoteDefinition && found->code != MarkdownDiagnosticCode::UndefinedFootnote)
                syntax->_diagnostics.push_back({found->code, node.block.content, nodeIndex});
        ++::DxUi::Detail::Work().rich.markdownFrontMatterValidationsReused;
    }
    void IdentityUnit()
    {
        if (previous && ! discardOldIdentities && oldIndexCursor < previous->Nodes().size())
        {
            // Imported subtrees keep their identities directly (W7.2); only the other old nodes are indexed.
            while (oldSkip < importedSubtrees.size() && importedSubtrees[oldSkip].oldFirst + importedSubtrees[oldSkip].count <= oldIndexCursor)
                ++oldSkip;
            if (oldSkip < importedSubtrees.size() && importedSubtrees[oldSkip].oldFirst <= oldIndexCursor)
            {
                oldIndexCursor = importedSubtrees[oldSkip].oldFirst + importedSubtrees[oldSkip].count;
                return;
            }
            const auto& oldNode = previous->Nodes()[oldIndexCursor];
            const auto& block   = oldNode.block;
            const auto parent   = oldNode.parent == MarkdownNoNode ? 0u : previous->Nodes()[oldNode.parent].identity;
            oldNodes.emplace(NodeKey{parent, block.source.first, block.source.last, block.kind}, oldIndexCursor++);
            ++::DxUi::Detail::Work().rich.identityMapNodes;
            return;
        }
        if (identityCursor == syntax->_nodes.size())
        {
            std::sort(syntax->_diagnostics.begin(), syntax->_diagnostics.end(), [](const auto& a, const auto& b) {
                return std::tie(a.source.first, a.source.last, a.code) < std::tie(b.source.first, b.source.last, b.code);
            });
            identitiesComplete = true;
            return;
        }
        const auto nodeIndex = identityCursor++;
        auto& node           = syntax->_nodes[nodeIndex];
        // The old node an imported subtree copied this node from (W7.2).
        auto carried = MarkdownNoNode;
        while (identitySubtree < importedSubtrees.size() && importedSubtrees[identitySubtree].first + importedSubtrees[identitySubtree].count <= nodeIndex)
            ++identitySubtree;
        if (identitySubtree < importedSubtrees.size() && importedSubtrees[identitySubtree].first <= nodeIndex)
            carried = importedSubtrees[identitySubtree].oldFirst + (nodeIndex - importedSubtrees[identitySubtree].first);
        if (node.block.kind == MarkdownBlockKind::SourceEditor)
            syntax->_sourceRegionNode = nodeIndex;
        // Reuse the existing resumable final-node pass; never rescan all nodes
        // on a host input/read path just to discover incomplete envelopes.
        if (node.block.kind == MarkdownBlockKind::FrontMatter && ! node.block.closed)
            syntax->_diagnostics.push_back({MarkdownDiagnosticCode::UnclosedFrontMatter, node.block.info, nodeIndex});
        else if (node.block.kind == MarkdownBlockKind::FrontMatter && carried != MarkdownNoNode)
            CarryFrontMatterResult(node, nodeIndex, carried);
        else if (node.block.kind == MarkdownBlockKind::FrontMatter && node.frontMatterKind == MarkdownFrontMatterKind::Toml)
        {
            ++::DxUi::Detail::Work().rich.markdownFrontMatterValidations;
            const auto issue =
                Detail::ValidateTomlMetadata(syntax->Source().substr(node.block.content.first, node.block.content.last - node.block.content.first));
            if (issue != Detail::MetadataIssue::None)
                syntax->_diagnostics.push_back({issue == Detail::MetadataIssue::Capacity ? MarkdownDiagnosticCode::TomlFrontMatterCapacity
                                                                                         : MarkdownDiagnosticCode::InvalidTomlFrontMatter,
                                                node.block.content,
                                                nodeIndex});
        }
        else if (node.block.kind == MarkdownBlockKind::FrontMatter && node.frontMatterKind == MarkdownFrontMatterKind::Yaml)
        {
            ++::DxUi::Detail::Work().rich.markdownFrontMatterValidations;
            const auto issue =
                Detail::ValidateYamlMetadata(syntax->Source().substr(node.block.content.first, node.block.content.last - node.block.content.first));
            if (issue != Detail::MetadataIssue::None)
            {
                const auto code = issue == Detail::MetadataIssue::Capacity          ? MarkdownDiagnosticCode::YamlFrontMatterCapacity
                                  : issue == Detail::MetadataIssue::DuplicateKey    ? MarkdownDiagnosticCode::DuplicateYamlKey
                                  : issue == Detail::MetadataIssue::UnresolvedAlias ? MarkdownDiagnosticCode::UnresolvedYamlAlias
                                                                                    : MarkdownDiagnosticCode::InvalidYamlFrontMatter;
                syntax->_diagnostics.push_back({code, node.block.content, nodeIndex});
            }
        }
        if (identityCursor == 1u && previous)
        {
            node.identity = previous->Nodes()[0].identity;
            return;
        }
        if (carried != MarkdownNoNode)
        {
            node.identity = previous->Nodes()[carried].identity;
            return;
        }
        auto first = node.block.source.first, last = node.block.source.last;
        bool mapped = previous && ! discardOldIdentities;
        if (node.block.kind == MarkdownBlockKind::SourceEditor && (! previous || ! previous->_sourceRegionRequest || ! syntax->_sourceRegionRequest ||
                                                                   previous->_sourceRegionRequest->session != syntax->_sourceRegionRequest->session))
            mapped = false;
        // Only unchanged prefix/suffix and a single enclosing edited node map.
        // The ancestry check prevents identities migrating between containers.
        if (first >= commonNewEnd && previous)
        {
            first = commonOldEnd + (first - commonNewEnd);
            last  = commonOldEnd + (last - commonNewEnd);
        }
        else if (last <= commonFirst)
        {
        }
        else if (first <= commonFirst && last >= commonNewEnd && previous)
            last = commonOldEnd + (last - commonNewEnd);
        else
            mapped = false;
        if (mapped)
        {
            const auto parentIdentity = node.parent == MarkdownNoNode ? 0u : syntax->_nodes[node.parent].identity;
            const auto reuseIdentity  = [&](std::size_t candidateFirst, std::size_t candidateLast)
            {
                const auto found = oldNodes.find(NodeKey{parentIdentity, candidateFirst, candidateLast, node.block.kind});
                if (found == oldNodes.end())
                    return std::uint64_t{};
                const auto& old   = previous->Nodes()[found->second];
                const bool parent = node.parent == MarkdownNoNode
                                        ? old.parent == MarkdownNoNode
                                        : old.parent != MarkdownNoNode && syntax->_nodes[node.parent].identity == previous->Nodes()[old.parent].identity;
                if (! parent)
                    return std::uint64_t{};
                const auto identity = old.identity;
                oldNodes.erase(found);
                return identity;
            };
            node.identity = reuseIdentity(first, last);
            // At a deletion boundary a surviving suffix/prefix and an edited
            // enclosing node are both possible. Prefer an exact surviving node;
            // otherwise retain the compatible enclosure (e.g. EOF Backspace).
            const auto original = node.block.source;
            if (! node.identity && original.first <= commonFirst && original.last >= commonNewEnd)
                node.identity = reuseIdentity(original.first, commonOldEnd + (original.last - commonNewEnd));
        }
        if (! node.identity)
            node.identity = nextIdentity++;
    }
    Detail::MarkdownBlocks blocks{
        blockSource,
        syntax->_profile,
        syntax->_nodes,
        syntax->_emptyLines,
        syntax->_extensions,
        syntax->_sourceRegionRequest && syntax->_sourceRegionRequest->kind == MarkdownSourceRegionKind::Block ? syntax->_sourceRegionRequest : std::nullopt,
        syntax->_sourceRegionRequest && syntax->_sourceRegionRequest->kind == MarkdownSourceRegionKind::Inline && syntax->_source.empty(),
        false};
    Detail::MarkdownInlines inlines{syntax->_sourceRegionRequest && syntax->_sourceRegionRequest->kind == MarkdownSourceRegionKind::Inline &&
                                            syntax->_sourceRegionRequest->source.first == syntax->_sourceRegionRequest->source.last
                                        ? std::wstring_view(syntax->_source)
                                        : blockSource,
                                    syntax->_source,
                                    syntax->_sourceRegionRequest,
                                    syntax->_profile,
                                    syntax->_extensions,
                                    syntax->_nodes,
                                    syntax->_inlines,
                                    syntax->_footnotes,
                                    syntax->_footnoteReferences,
                                    syntax->_diagnostics,
                                    CharacterReference};
    std::size_t cursor{}, lineFirst{}, lexicalCursor{}, literalFirst{}, rootCursor{}, rootNode = MarkdownNoNode;
    std::size_t reusableRootIndex = 1u;
    struct ClosedRootReuse
    {
        ClosedRootReuse() : remap(0u)
        {
        }
        enum class Phase
        {
            ValidateNodes,
            ValidateLinks,
            ValidateEmpty,
            CopyNodes,
            CopyLines,
            CopyEmpty
        };
        Phase phase = Phase::ValidateNodes;
        std::size_t root{}, scan{}, end{}, line = MarkdownNoNode, emptyBegin{}, emptyCursor{}, newFirst{};
        std::vector<std::size_t> remap;
    };
    std::optional<ClosedRootReuse> closedRoot;
    bool blocksComplete{}, lexemesComplete{}, rootsComplete{}, inlinesComplete{};
    void ClosedRootUnit()
    {
        auto& work          = *closedRoot;
        const auto& nodes   = previous->Nodes();
        const auto blanks   = previous->EmptyLines();
        const auto& root    = nodes[work.root];
        const auto oldFirst = root.block.source.first, oldLast = root.block.source.last;
        const auto validRange = [&](MarkdownRange range) noexcept
        { return (range.first == 0u && range.last == 0u) || (range.first >= oldFirst && range.first <= range.last && range.last <= oldLast); };
        const auto fail = [&]() noexcept { closedRoot.reset(); };
        if (work.phase == ClosedRootReuse::Phase::ValidateNodes)
        {
            if (work.scan == nodes.size() || (work.scan > work.root && nodes[work.scan].parent == 0u))
            {
                work.end = work.scan;
                if ((root.nextSibling == MarkdownNoNode ? work.end != nodes.size() : root.nextSibling != work.end) ||
                    ! blocks.CanImportNodeCount(work.end - work.root))
                {
                    fail();
                    return;
                }
                work.scan  = work.root;
                work.phase = ClosedRootReuse::Phase::ValidateLinks;
                return;
            }
            const auto index = work.scan;
            const auto& node = nodes[index];
            if (work.line == MarkdownNoNode)
            {
                const bool parentValid = index == work.root ? node.parent == 0u : node.parent >= work.root && node.parent < index;
                const bool childValid =
                    node.firstChild == MarkdownNoNode || (node.firstChild > index && node.firstChild < nodes.size() && nodes[node.firstChild].parent == index);
                const bool siblingValid = index == work.root || node.nextSibling == MarkdownNoNode ||
                                          (node.nextSibling > index && node.nextSibling < nodes.size() && nodes[node.nextSibling].parent == node.parent);
                if (! parentValid || ! childValid || ! siblingValid ||
                    (node.block.kind == MarkdownBlockKind::SourceEditor || node.block.kind == MarkdownBlockKind::ReferenceDefinition) ||
                    (node.block.source.first < oldFirst || node.block.source.first > node.block.source.last || node.block.source.last > oldLast) ||
                    (node.block.content.first < oldFirst || node.block.content.first > node.block.content.last || node.block.content.last > oldLast) ||
                    ! validRange(node.block.info) || ! validRange(node.label) || ! validRange(node.destination) || ! validRange(node.title) ||
                    // A code, HTML or envelope leaf whose content reaches the end of the source can still be the open
                    // leaf when a full parse finishes, which then records no final insertion line; the previous parse
                    // shows that by a source ending in a line break without that line. Parse it again instead.
                    ((node.block.kind == MarkdownBlockKind::FencedCode || node.block.kind == MarkdownBlockKind::IndentedCode ||
                      node.block.kind == MarkdownBlockKind::HtmlBlock || node.block.kind == MarkdownBlockKind::FrontMatter) &&
                     node.block.content.last == previous->Source().size() && PreviousEndedInLeaf()))
                {
                    fail();
                    return;
                }
                work.line = 0u;
                return;
            }
            if (work.line < node.lines.size())
            {
                const auto line = node.lines[work.line++].source;
                if (line.first < oldFirst || line.first > line.last || line.last > oldLast)
                    fail();
                return;
            }
            ++work.scan;
            work.line = MarkdownNoNode;
            return;
        }
        if (work.phase == ClosedRootReuse::Phase::ValidateLinks)
        {
            if (work.scan < work.end)
            {
                const auto index = work.scan++;
                const auto& node = nodes[index];
                if ((node.firstChild != MarkdownNoNode && node.firstChild >= work.end) ||
                    (index != work.root && node.nextSibling != MarkdownNoNode && node.nextSibling >= work.end))
                    fail();
                return;
            }
            const auto blank =
                std::lower_bound(blanks.begin(), blanks.end(), oldFirst, [](const auto& value, auto position) { return value.source.first < position; });
            work.emptyBegin = work.emptyCursor = static_cast<std::size_t>(blank - blanks.begin());
            work.phase                         = ClosedRootReuse::Phase::ValidateEmpty;
            return;
        }
        if (work.phase == ClosedRootReuse::Phase::ValidateEmpty)
        {
            if (work.emptyCursor < blanks.size() && blanks[work.emptyCursor].source.first < oldLast)
            {
                const auto& blank            = blanks[work.emptyCursor++];
                const auto retainedEnd       = root.lines.empty() ? oldFirst : root.lines.back().source.last;
                const bool trailingCodeBlank = root.block.kind == MarkdownBlockKind::IndentedCode && blank.owner == 0u && blank.source.first >= retainedEnd &&
                                               blank.source.last <= oldLast;
                if ((! trailingCodeBlank && (blank.owner < work.root || blank.owner >= work.end)) || ! validRange(blank.source) ||
                    blank.insertion < blank.source.first || blank.insertion > blank.source.last)
                    fail();
                return;
            }
            work.remap.reserve(work.end - work.root);
            work.scan  = work.root;
            work.line  = 0u;
            work.phase = ClosedRootReuse::Phase::CopyNodes;
            return;
        }
        if (work.phase == ClosedRootReuse::Phase::CopyNodes)
        {
            if (work.scan == work.end)
            {
                work.emptyCursor = work.emptyBegin;
                work.phase       = ClosedRootReuse::Phase::CopyEmpty;
                return;
            }
            const auto& node    = nodes[work.scan];
            const auto parent   = work.scan == work.root ? 0u : work.remap[node.parent - work.root];
            const auto imported = blocks.ImportClosedNode(node, parent, oldFirst, work.newFirst);
            if (imported == MarkdownNoNode)
                return;
            ++::DxUi::Detail::Work().rich.markdownNodesCopied;
            work.remap.push_back(imported);
            work.line  = 0u;
            work.phase = ClosedRootReuse::Phase::CopyLines;
            return;
        }
        if (work.phase == ClosedRootReuse::Phase::CopyLines)
        {
            const auto& node = nodes[work.scan];
            if (work.line < node.lines.size())
            {
                blocks.ImportClosedLine(work.remap.back(), node.lines[work.line++], oldFirst, work.newFirst);
                return;
            }
            ++work.scan;
            work.phase = ClosedRootReuse::Phase::CopyNodes;
            return;
        }
        if (work.emptyCursor < blanks.size() && blanks[work.emptyCursor].source.first < oldLast)
        {
            const auto& blank = blanks[work.emptyCursor++];
            blocks.ImportClosedEmptyLine(blank, blank.owner == 0u ? 0u : work.remap[blank.owner - work.root], oldFirst, work.newFirst);
            return;
        }
        cursor    = work.newFirst + (oldLast - oldFirst);
        lineFirst = cursor;
        blocks.FinishClosedRoot(cursor);
        reusableRootIndex = work.end;
        importedSubtrees.push_back({MarkdownNoNode, work.end - work.root, work.root, oldFirst, work.newFirst});
        closedRoot.reset();
    }
    // The unchanged prefix line that ends at `last` is terminated there, before the edit: text typed at the edit cannot
    // continue it (an EOF line without a line ending would continue; an LF typed after a CR would join its CRLF).
    bool PreviousEndedInLeaf() const noexcept
    {
        const auto source = previous->Source();
        const auto blanks = previous->EmptyLines();
        return ! source.empty() && (source.back() == L'\n' || source.back() == L'\r') && (blanks.empty() || blanks.back().source.first != source.size());
    }
    bool LineEndsBefore(std::size_t last) const noexcept
    {
        const auto source = previous->Source();
        return last > 0u && last <= reuseFirst && (source[last - 1u] == L'\n' || (source[last - 1u] == L'\r' && last < reuseFirst));
    }
    void ScanBlockUnit()
    {
        if (closedRoot)
        {
            ClosedRootUnit();
            return;
        }
        if (blocks.AdvanceTableCell())
            return;
        if (cursor == lineFirst && blocks.StartsFresh() && (freshLines.empty() || freshLines.back() != cursor))
            freshLines.push_back(cursor);
        const auto& source = blockSource;
        if (reuse && cursor == lineFirst && blocks.CanImportClosedRoot())
        {
            const bool prefix = cursor < reuseFirst;
            const bool suffix = cursor >= reuseNewEnd;
            if (prefix || suffix)
            {
                const auto oldPosition    = prefix ? cursor : reuseOldEnd + (cursor - reuseNewEnd);
                const auto& previousNodes = previous->Nodes();
                if (reusableRootIndex < previousNodes.size())
                {
                    const auto& old = previousNodes[reusableRootIndex];
                    if (old.parent != 0u || old.block.source.first < oldPosition)
                    {
                        ++reusableRootIndex;
                        return;
                    }
                    if (old.block.source.first == oldPosition)
                    {
                        ++reusableRootIndex;
                        const auto& blanks = previous->EmptyLines();
                        const auto blank   = std::lower_bound(
                            blanks.begin(), blanks.end(), old.block.source.last, [](const auto& line, auto position) { return line.source.first < position; });
                        const bool followingBlank = blank != blanks.end() && blank->owner == 0u && blank->source.first == old.block.source.last &&
                                                    blank->source.last > blank->source.first &&
                                                    (! prefix || (blank->source.last <= reuseFirst && LineEndsBefore(blank->source.last)));
                        // A closed front-matter envelope ends at its closing line: nothing after it can extend it. Its
                        // line ending must precede the edit, or an LF typed after a CR would join that line.
                        const auto envelopeLast = old.block.source.last;
                        const bool closedEnvelope =
                            prefix && old.block.kind == MarkdownBlockKind::FrontMatter && envelopeLast <= reuseFirst && LineEndsBefore(envelopeLast);
                        // Front matter depends on absolute document start. A
                        // suffix crossing that boundary must be recognized again
                        // in either direction, even when its bytes are unchanged.
                        const bool sameStartContext = (cursor == 0u) == (oldPosition == 0u);
                        // Other suffix closure remains valid through EOF; prefix
                        // roots require an unchanged document-owned separator.
                        // The old block must also have begun with nothing open: then it parses the same from this
                        // state, which is the state the new parse has here (nothing open but the document).
                        if (sameStartContext && (followingBlank || suffix || closedEnvelope) && old.block.closed &&
                            old.block.source.last > old.block.source.first && FreshInPrevious(old.block.source.first) &&
                            (! suffix || followingBlank || EndsAtFreshStart(old.block.source.first)))
                        {
                            closedRoot.emplace();
                            closedRoot->root = closedRoot->scan = reusableRootIndex - 1u;
                            closedRoot->newFirst                = cursor;
                            return;
                        }
                    }
                }
                // A document-level blank line between unchanged blocks is copied too (W7.2): with nothing open, Line
                // records any whitespace-only line the same way, so no unit of it needs scanning.
                const auto& blanks = previous->EmptyLines();
                const auto blank =
                    std::lower_bound(blanks.begin(), blanks.end(), oldPosition, [](const auto& line, auto position) { return line.source.first < position; });
                if (blank != blanks.end() && blank->source.first == oldPosition && blank->owner == 0u && blank->source.last > blank->source.first &&
                    (suffix || (blank->source.last <= reuseFirst && LineEndsBefore(blank->source.last))))
                {
                    blocks.ImportDocumentBlankLine(*blank, oldPosition, cursor);
                    cursor    = cursor + (blank->source.last - blank->source.first);
                    lineFirst = cursor;
                    return;
                }
            }
        }
        if (cursor == source.size())
        {
            if (lineFirst < cursor)
            {
                blocks.Line(lineFirst, cursor, cursor);
                lineFirst = cursor;
                return; // A final row's generated cells must finish before compaction.
            }
            blocks.Finish();
            syntax->_blockStatistics = blocks.Statistics();
            rootNode                 = syntax->_nodes[0].firstChild;
            blocksComplete           = true;
            return;
        }
        const auto character = source[cursor++];
        ++::DxUi::Detail::Work().rich.markdownBlockUnitsScanned;
        if (character == L'\r' || character == L'\n')
        {
            const auto bodyEnd = cursor - 1u;
            if (character == L'\r' && cursor < source.size() && source[cursor] == L'\n')
            {
                ++cursor;
                ++::DxUi::Detail::Work().rich.markdownBlockUnitsScanned;
            }
            blocks.Line(lineFirst, bodyEnd, cursor);
            lineFirst = cursor;
        }
    }
    void RootUnit()
    {
        auto& roots = syntax->_blocks;
        if (rootNode != MarkdownNoNode && rootCursor == syntax->_nodes[rootNode].block.source.first)
        {
            // Locate each imported subtree in the compacted tree (W7.2): imports and roots are both in source order.
            const auto& root = syntax->_nodes[rootNode];
            while (importedRootCursor < importedSubtrees.size() && importedSubtrees[importedRootCursor].newSource < root.block.source.first)
                ++importedRootCursor;
            if (importedRootCursor < importedSubtrees.size() && importedSubtrees[importedRootCursor].newSource == root.block.source.first &&
                previous->Nodes()[importedSubtrees[importedRootCursor].oldFirst].block.kind == root.block.kind &&
                rootNode + importedSubtrees[importedRootCursor].count <= syntax->_nodes.size())
                importedSubtrees[importedRootCursor++].first = rootNode;
            while (freshCursor < freshLines.size() && freshLines[freshCursor] < root.block.source.first)
                ++freshCursor;
            if (freshCursor < freshLines.size() && freshLines[freshCursor] == root.block.source.first)
                freshRoots.push_back(root.block.source.first);
            roots.push_back(syntax->_nodes[rootNode].block);
            rootCursor = roots.back().source.last;
            rootNode   = syntax->_nodes[rootNode].nextSibling;
        }
        else if (rootCursor < syntax->_source.size())
        {
            const auto first = rootCursor;
            // An unchanged trivia line keeps its previous partition entry, shifted (W7.2): no unit of it is read.
            if (reuse && (first < reuseFirst || first >= reuseNewEnd))
            {
                const auto oldFirst  = first < reuseFirst ? first : reuseOldEnd + (first - reuseNewEnd);
                const auto& oldRoots = previous->Blocks();
                const auto old       = std::lower_bound(
                    oldRoots.begin(), oldRoots.end(), oldFirst, [](const auto& block, auto position) { return block.source.first < position; });
                if (old != oldRoots.end() && old->kind == MarkdownBlockKind::BlankLine && old->source.first == oldFirst && old->source.last > oldFirst &&
                    (first >= reuseNewEnd || LineEndsBefore(old->source.last)) &&
                    (rootNode == MarkdownNoNode || first + (old->source.last - oldFirst) <= syntax->_nodes[rootNode].block.source.first))
                {
                    rootCursor = first + (old->source.last - oldFirst);
                    roots.push_back({MarkdownBlockKind::BlankLine, {first, rootCursor}, {first, first + (old->content.last - oldFirst)}});
                    return;
                }
            }
            do
            {
                ++rootCursor;
            } while (rootCursor < syntax->_source.size() && syntax->_source[rootCursor - 1u] != L'\r' && syntax->_source[rootCursor - 1u] != L'\n');
            if (syntax->_source[rootCursor - 1u] == L'\r' && rootCursor < syntax->_source.size() && syntax->_source[rootCursor] == L'\n')
                ++rootCursor;
            auto bodyEnd = rootCursor;
            while (bodyEnd > first && (syntax->_source[bodyEnd - 1u] == L'\r' || syntax->_source[bodyEnd - 1u] == L'\n'))
                --bodyEnd;
            ::DxUi::Detail::Work().rich.markdownRootTriviaUnitsScanned += rootCursor - first;
            roots.push_back({MarkdownBlockKind::BlankLine, {first, rootCursor}, {first, bodyEnd}});
        }
        else
        {
            std::erase_if(importedSubtrees, [](const auto& subtree) { return subtree.first == MarkdownNoNode; });
            if (reuse && ! importedSubtrees.empty())
                inlines.EnableReuse({&previous->Nodes(), &previous->Inlines(), &previousState->references, &previousState->leafStaging, &importedSubtrees});
            rootsComplete = true;
        }
    }
    void FlushLiteral(std::size_t end)
    {
        if (literalFirst < end)
            syntax->_lexemes.push_back({MarkdownLexemeKind::Text, {literalFirst, end}});
    }
    void ScanLexicalUnit()
    {
        const auto& source = syntax->_source;
        if (previous && lexicalPrefixIndex < previous->Lexemes().size())
        {
            const auto& token = previous->Lexemes()[lexicalPrefixIndex];
            if (token.source.first == lexicalCursor && token.source.last <= lexicalPrefixLimit)
            {
                syntax->_lexemes.push_back(token);
                lexicalCursor = literalFirst = token.source.last;
                ++lexicalPrefixIndex;
                return;
            }
        }
        if (lexicalSuffixIndex != MarkdownNoNode && (copyingLexicalSuffix || lexicalCursor == lexicalSuffixStart))
        {
            if (! copyingLexicalSuffix)
            {
                FlushLiteral(lexicalCursor);
                copyingLexicalSuffix = true;
            }
            const auto& old = previous->Lexemes();
            if (lexicalSuffixIndex < old.size())
            {
                auto token         = old[lexicalSuffixIndex++];
                token.source.first = commonNewEnd + (token.source.first - commonOldEnd);
                token.source.last  = commonNewEnd + (token.source.last - commonOldEnd);
                syntax->_lexemes.push_back(token);
                lexicalCursor = literalFirst = token.source.last;
                return;
            }
            lexemesComplete = true;
            return;
        }
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
            ::DxUi::Detail::Work().rich.markdownLexicalUnitsScanned += lexicalCursor - first;
            return;
        }
        ::DxUi::Detail::Work().rich.markdownLexicalUnitsScanned += lexicalCursor - first;
        FlushLiteral(first);
        syntax->_lexemes.push_back(token);
        literalFirst = lexicalCursor;
    }
};
#pragma warning(pop)

MarkdownParser::MarkdownParser() noexcept  = default;
MarkdownParser::~MarkdownParser() noexcept = default;
MarkdownParser::MarkdownParser(MarkdownParser&& other) noexcept
    : _preparation(std::move(other._preparation)),
      _published(std::move(other._published)),
      _documentIdentity(std::exchange(other._documentIdentity, 0u)),
      _nextNodeIdentity(std::exchange(other._nextNodeIdentity, 1u)),
      _publishedSource(std::move(other._publishedSource))
{
    other._publishedSource.reset();
}
MarkdownParser& MarkdownParser::operator=(MarkdownParser&& other) noexcept
{
    if (this != &other)
    {
        _preparation      = std::move(other._preparation);
        _published        = std::move(other._published);
        _documentIdentity = std::exchange(other._documentIdentity, 0u);
        _nextNodeIdentity = std::exchange(other._nextNodeIdentity, 1u);
        _publishedSource  = std::move(other._publishedSource);
        other._publishedSource.reset();
    }
    return *this;
}
bool MarkdownParser::HasPendingPreparation() const noexcept
{
    return static_cast<bool>(_preparation);
}
std::shared_ptr<const MarkdownSyntax> MarkdownParser::Published() const noexcept
{
    return _published;
}
bool MarkdownParser::TryAdoptParsedSnapshot(const SourceSnapshot& source,
                                            std::uint64_t revision,
                                            const std::shared_ptr<const MarkdownSyntax>& syntax,
                                            MarkdownProfile profile,
                                            std::optional<MarkdownExtensions> extensions)
{
    Failure::Scope failureScope;
    MarkdownExtensions resolved{};
    if (! syntax || ! TryResolveExtensions(profile, extensions, resolved) || syntax->Profile() != profile || syntax->Extensions() != resolved ||
        syntax->_sourceRegionRequest)
        return false;

    const auto& canonical = syntax->_parsedDataOwner ? syntax->_parsedDataOwner : syntax;
    if (canonical->_parsedDataOwner || canonical->_sourceRegionRequest || canonical->Profile() != profile || canonical->Extensions() != resolved)
        return false;

    if (_published && _published->_parsedDataOwner && _published->Revision() == revision && _published->Profile() == profile &&
        _published->Extensions() == resolved && _publishedSource && _publishedSource->IsSameRevision(source) && _published->_parsedDataOwner == canonical)
    {
        // The exact immutable source revision was already validated against this
        // canonical parse. Retire later staged work without walking its pieces.
        Cancel();
        return true;
    }
    if (! source.MatchesText(canonical->Source()))
        return false;

    const auto identity = nextDocumentIdentity.fetch_add(1u, std::memory_order_relaxed);
    auto staged         = std::shared_ptr<MarkdownSyntax>(new MarkdownSyntax(canonical, revision, profile, resolved, identity));

    // All potentially throwing validation/allocation completed. Commit only
    // no-throw ownership and scalar updates so a refused/failed adoption keeps
    // the old staged and published state intact.
    _preparation.reset();
    _published        = std::move(staged);
    _documentIdentity = identity;
    _nextNodeIdentity = 1u;
    _publishedSource  = source;
    return true;
}
void MarkdownParser::Cancel() noexcept
{
    _preparation.reset();
}
void MarkdownParser::Clear() noexcept
{
    Cancel();
    _published.reset();
    _documentIdentity = 0u;
    _nextNodeIdentity = 1u;
    _publishedSource.reset();
}
void MarkdownParser::Begin(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    RequireSuccess(TryBegin(source, revision, profile, extensions));
}
MarkdownResult MarkdownParser::TryBegin(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    Failure::Scope failureScope;
    MarkdownExtensions enabled{};
    if (! TryResolveExtensions(profile, extensions, enabled))
        return MarkdownResult::InvalidProfile;
    const auto result = ValidateSource(source);
    if (result != MarkdownResult::Success)
        return result;
    BeginSource(source, revision, profile, enabled, std::nullopt);
    return MarkdownResult::Success;
}
void MarkdownParser::Begin(const SourceSnapshot& source, std::uint64_t revision, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    RequireSuccess(TryBegin(source, revision, profile, extensions));
}
MarkdownResult MarkdownParser::TryBegin(const SourceSnapshot& source,
                                        std::uint64_t revision,
                                        MarkdownProfile profile,
                                        std::optional<MarkdownExtensions> extensions)
{
    Failure::Scope failureScope;
    MarkdownExtensions enabled{};
    if (! TryResolveExtensions(profile, extensions, enabled))
        return MarkdownResult::InvalidProfile;
    const auto change = _publishedSource ? std::optional(source.ChangedRangeFrom(*_publishedSource)) : std::nullopt;
    if (change && change->kind == SourceChangeKind::Unchanged && _published && _published->Revision() == revision && _published->Profile() == profile &&
        _published->Extensions() == enabled && ! _published->_sourceRegionRequest)
    {
        Cancel();
        _publishedSource = source;
        return MarkdownResult::Success;
    }
    std::wstring flat(0u, L' ');
    if (! ComposeSource(source, _published, change, flat))
    {
        flat = source.Text();
        ::DxUi::Detail::Work().rich.markdownSourceUnitsMaterialized += flat.size();
    }
    Preparation::Begin(*this, flat, revision, profile, enabled, change, std::nullopt, &flat);
    if (_preparation)
        _preparation->snapshot = source;
    else
        _publishedSource = source;
    return MarkdownResult::Success;
}
void MarkdownParser::BeginSourceRegion(
    const SourceSnapshot& source, std::uint64_t revision, MarkdownSourceRegion region, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    RequireSuccess(TryBeginSourceRegion(source, revision, region, profile, extensions));
}
MarkdownResult MarkdownParser::TryBeginSourceRegion(
    const SourceSnapshot& source, std::uint64_t revision, MarkdownSourceRegion region, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    Failure::Scope failureScope;
    if (! region.session)
        return MarkdownResult::InvalidRange;
    if ((region.kind != MarkdownSourceRegionKind::Block && region.kind != MarkdownSourceRegionKind::Inline) ||
        (region.inlineKind != MarkdownInlineSourceKind::Math && region.inlineKind != MarkdownInlineSourceKind::Link &&
         region.inlineKind != MarkdownInlineSourceKind::Image) ||
        (region.kind == MarkdownSourceRegionKind::Block && region.inlineKind != MarkdownInlineSourceKind::Math))
        return MarkdownResult::InvalidRange;
    if (region.source.first > region.source.last || ! source.IsBoundary(region.source.first) || ! source.IsBoundary(region.source.last))
        return MarkdownResult::InvalidRange;
    MarkdownExtensions enabled{};
    if (! TryResolveExtensions(profile, extensions, enabled))
        return MarkdownResult::InvalidProfile;
    const auto change = _publishedSource ? std::optional(source.ChangedRangeFrom(*_publishedSource)) : std::nullopt;
    std::wstring flat(0u, L' ');
    if (! ComposeSource(source, _published, change, flat))
    {
        flat = source.Text();
        ::DxUi::Detail::Work().rich.markdownSourceUnitsMaterialized += flat.size();
    }
    Preparation::Begin(*this, flat, revision, profile, enabled, change, region, &flat);
    if (_preparation)
        _preparation->snapshot = source;
    else
        _publishedSource = source;
    return MarkdownResult::Success;
}
void MarkdownParser::BeginSource(std::wstring_view source,
                                 std::uint64_t revision,
                                 MarkdownProfile profile,
                                 MarkdownExtensions extensions,
                                 std::optional<SourceChangeRange> change,
                                 std::optional<MarkdownSourceRegion> sourceRegion)
{
    Preparation::Begin(*this, source, revision, profile, extensions, change, sourceRegion, nullptr);
}
MarkdownAdvance MarkdownParser::Advance(std::size_t maximumWorkUnits)
{
    Failure::Scope failureScope;
    if (! _preparation)
        return {_published ? MarkdownPreparationStatus::Complete : MarkdownPreparationStatus::Idle};
    MarkdownAdvance result{MarkdownPreparationStatus::Pending};
    auto& pending = *_preparation;
    while (result.workUnits < maximumWorkUnits)
    {
        ++result.workUnits;
        if (! pending.blocksComplete)
            pending.ScanBlockUnit();
        else if (! pending.rootsComplete)
            pending.RootUnit();
        else if (! pending.lexemesComplete)
            pending.ScanLexicalUnit();
        else if (! pending.inlinesComplete)
        {
            pending.inlinesComplete = pending.inlines.Step();
            if (pending.inlinesComplete)
                pending.syntax->_inlineStatistics = {
                    pending.inlines.AutolinkInspectionUnits(), pending.inlines.AutolinkDecodedUnits(), pending.inlines.WikiInspectionUnits()};
        }
        else if (! pending.identitiesComplete)
            pending.IdentityUnit();
        else
        {
            // What the next parse may reuse travels with this publication (W7.2).
            if (const auto* deleter = std::get_deleter<SyntaxDeleter>(pending.syntax); deleter && deleter->state)
            {
                pending.inlines.TakeReuseState(deleter->state->references, deleter->state->leafStaging);
                deleter->state->physicalLines = pending.sourceLines;
                deleter->state->freshRoots.swap(pending.freshRoots);
            }
            _nextNodeIdentity = pending.nextIdentity;
            _publishedSource  = std::move(pending.snapshot);
            _published        = std::move(pending.syntax);
            _preparation.reset();
            result.status = MarkdownPreparationStatus::Complete;
            break;
        }
        if (pending.blocks.CapacityExceeded() || pending.inlines.CapacityExceeded())
        {
            _preparation.reset();
            result.status = MarkdownPreparationStatus::Failed;
            result.result = MarkdownResult::CapacityExceeded;
            break;
        }
    }
    return result;
}
} // namespace DxUi::RichText
