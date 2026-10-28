#include "DxUi.MarkdownBlocks.h"
#include "DxUi.MarkdownCharacterReference.h"
#include "DxUi.MarkdownEntityData.h"
#include "DxUi.MarkdownInlines.h"
#include "DxUi.MarkdownSourceCorrelation.h"
#include "DxUi.TextElements.h"
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
void ValidateProfile(MarkdownProfile profile)
{
    if (profile != MarkdownProfile::CommonMark0312 && profile != MarkdownProfile::Gfm029 && profile != MarkdownProfile::RichMarkdown)
        throw std::invalid_argument("Unknown Markdown profile");
}
bool TryResolveExtensions(MarkdownProfile profile, std::optional<MarkdownExtensions> extensions, MarkdownExtensions& result) noexcept
{
    if (profile != MarkdownProfile::CommonMark0312 && profile != MarkdownProfile::Gfm029 && profile != MarkdownProfile::RichMarkdown)
        return false;
    result = extensions.value_or(profile == MarkdownProfile::CommonMark0312 ? MarkdownExtensions::None : MarkdownExtensions::All);
    return ! (static_cast<unsigned>(result) & ~static_cast<unsigned>(MarkdownExtensions::All)) &&
           (profile != MarkdownProfile::CommonMark0312 || result == MarkdownExtensions::None);
}
MarkdownExtensions ValidateExtensions(MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    ValidateProfile(profile);
    MarkdownExtensions result{};
    if (! TryResolveExtensions(profile, extensions, result))
        throw std::invalid_argument("Invalid Markdown extensions for this profile");
    return result;
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

} // namespace

bool Detail::DecodeMarkdownCharacterReference(std::wstring_view input, std::size_t first, MarkdownLexeme& token) noexcept
{
    return CharacterReference(input, first, token);
}

MarkdownSyntax::MarkdownSyntax(
    std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, MarkdownExtensions extensions, std::optional<MarkdownSourceRegion> sourceRegion)
    : source_(ValidateSource(source, profile)),
      revision_(revision),
      profile_(profile),
      extensions_(extensions),
      blocks_(0u),
      lexemes_(0u),
      nodes_(0u),
      emptyLines_(0u),
      inlines_(0u),
      diagnostics_(0u),
      footnotes_(0u),
      footnoteReferences_(0u),
      sourceRegionRequest_(sourceRegion)
{
}
MarkdownSyntax::MarkdownSyntax(std::shared_ptr<const MarkdownSyntax> parsedDataOwner,
                               std::uint64_t revision,
                               MarkdownProfile profile,
                               MarkdownExtensions extensions,
                               std::uint64_t documentIdentity)
    : source_(0u, L'\0'),
      revision_(revision),
      profile_(profile),
      extensions_(extensions),
      blocks_(0u),
      lexemes_(0u),
      nodes_(0u),
      emptyLines_(0u),
      inlines_(0u),
      diagnostics_(0u),
      footnotes_(0u),
      footnoteReferences_(0u),
      documentIdentity_(documentIdentity),
      sourceRegionRequest_(std::nullopt),
      parsedDataOwner_(std::move(parsedDataOwner))
{
}
const MarkdownSyntax& MarkdownSyntax::ParsedData() const noexcept
{
    return parsedDataOwner_ ? *parsedDataOwner_ : *this;
}
std::wstring_view MarkdownSyntax::Source() const noexcept
{
    return ParsedData().source_;
}
std::optional<MarkdownSourceRegion> MarkdownSyntax::SourceRegion() const noexcept
{
    if (! sourceRegionRequest_ || sourceRegionNode_ == MarkdownNoNode)
        return std::nullopt;
    return MarkdownSourceRegion{nodes_[sourceRegionNode_].block.source, sourceRegionRequest_->session};
}
std::size_t MarkdownSyntax::SourceRegionNode() const noexcept
{
    return sourceRegionNode_;
}
std::uint64_t MarkdownSyntax::Revision() const noexcept
{
    return revision_;
}
MarkdownProfile MarkdownSyntax::Profile() const noexcept
{
    return profile_;
}
MarkdownExtensions MarkdownSyntax::Extensions() const noexcept
{
    return extensions_;
}
const std::vector<MarkdownBlock>& MarkdownSyntax::Blocks() const noexcept
{
    return ParsedData().blocks_;
}
const std::vector<MarkdownNode>& MarkdownSyntax::Nodes() const noexcept
{
    return ParsedData().nodes_;
}
std::span<const MarkdownEmptyLine> MarkdownSyntax::EmptyLines() const noexcept
{
    return ParsedData().emptyLines_;
}
std::uint64_t MarkdownSyntax::DocumentIdentity() const noexcept
{
    return documentIdentity_;
}
MarkdownBlockStatistics MarkdownSyntax::BlockStatistics() const noexcept
{
    return ParsedData().blockStatistics_;
}
const std::vector<MarkdownLexeme>& MarkdownSyntax::Lexemes() const noexcept
{
    return ParsedData().lexemes_;
}
const std::vector<MarkdownInline>& MarkdownSyntax::Inlines() const noexcept
{
    return ParsedData().inlines_;
}
std::span<const MarkdownDiagnostic> MarkdownSyntax::Diagnostics() const noexcept
{
    return ParsedData().diagnostics_;
}
std::span<const MarkdownFootnote> MarkdownSyntax::Footnotes() const noexcept
{
    return ParsedData().footnotes_;
}
std::span<const MarkdownFootnoteReference> MarkdownSyntax::FootnoteReferences() const noexcept
{
    return ParsedData().footnoteReferences_;
}
const MarkdownFootnoteReference* MarkdownSyntax::FootnoteReferenceFor(std::size_t inlineNode) const noexcept
{
    const auto& references = ParsedData().footnoteReferences_;
    const auto found =
        std::lower_bound(references.begin(), references.end(), inlineNode, [](const auto& reference, auto wanted) { return reference.inlineNode < wanted; });
    return found != references.end() && found->inlineNode == inlineNode ? &*found : nullptr;
}
std::wstring_view MarkdownSyntax::Slice(MarkdownRange range) const
{
    const auto source = Source();
    if (range.first > range.last || range.last > source.size())
        throw std::out_of_range("Markdown source range");
    return source.substr(range.first, range.last - range.first);
}
std::wstring_view MarkdownSyntax::LexemeValue(std::size_t index) const
{
    if (parsedDataOwner_)
        return parsedDataOwner_->LexemeValue(index);
    const auto& token = lexemes_.at(index);
    return token.kind == MarkdownLexemeKind::Text ? Slice(token.source) : std::wstring_view(token.value.data(), token.valueLength);
}
std::size_t MarkdownSyntax::LexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing) const
{
    if (parsedDataOwner_)
        return parsedDataOwner_->LexemeSourceAnchor(index, valueOffset, trailing);
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
                         std::optional<MarkdownSourceRegion> sourceRegion)
        // The private constructor prevents exposing unparsed mutable shells.
        // shared_ptr owns immediately and deletes on control-block failure too.
        : syntax(std::shared_ptr<MarkdownSyntax>(new MarkdownSyntax(source, revision, profile, extensions, sourceRegion)))
    {
    }
    std::shared_ptr<MarkdownSyntax> syntax;
    std::shared_ptr<const MarkdownSyntax> previous;
    std::optional<SourceSnapshot> snapshot;
    std::optional<SourceChangeRange> change;
    using NodeKey = std::tuple<std::uint64_t, std::size_t, std::size_t, MarkdownBlockKind>;
    // Generated missing table cells can have identical empty source ranges.
    // Preserve their insertion order and consume each old identity only once.
    std::multimap<NodeKey, std::size_t> oldNodes;
    std::size_t commonFirst{}, commonOldEnd{}, commonNewEnd{}, identityCursor{}, oldIndexCursor{};
    std::uint64_t nextIdentity = 1u;
    bool identitiesComplete{};
    bool discardOldIdentities{};
    void SetPrevious(std::shared_ptr<const MarkdownSyntax> old, std::uint64_t document, std::uint64_t next)
    {
        syntax->documentIdentity_ = document;
        previous                  = std::move(old);
        nextIdentity              = next;
        if (! previous || previous->Profile() != syntax->Profile())
        {
            previous.reset();
            return;
        }
        const auto a = previous->Source(), b = syntax->Source();
        const auto correlation = Detail::CorrelateMarkdownSource(
            a.size(), b.size(), change, [&](std::size_t at) noexcept { return a[at]; }, [&](std::size_t at) noexcept { return b[at]; });
        commonFirst          = correlation.first;
        commonOldEnd         = correlation.previousEnd;
        commonNewEnd         = correlation.currentEnd;
        discardOldIdentities = correlation.discardPrevious;
    }
    void IdentityUnit()
    {
        if (previous && ! discardOldIdentities && oldIndexCursor < previous->Nodes().size())
        {
            const auto& oldNode = previous->Nodes()[oldIndexCursor];
            const auto& block   = oldNode.block;
            const auto parent   = oldNode.parent == MarkdownNoNode ? 0u : previous->Nodes()[oldNode.parent].identity;
            oldNodes.emplace(NodeKey{parent, block.source.first, block.source.last, block.kind}, oldIndexCursor++);
            return;
        }
        if (identityCursor == syntax->nodes_.size())
        {
            std::sort(syntax->diagnostics_.begin(), syntax->diagnostics_.end(), [](const auto& a, const auto& b) {
                return std::tie(a.source.first, a.source.last, a.code) < std::tie(b.source.first, b.source.last, b.code);
            });
            identitiesComplete = true;
            return;
        }
        const auto nodeIndex = identityCursor++;
        auto& node           = syntax->nodes_[nodeIndex];
        if (node.block.kind == MarkdownBlockKind::SourceEditor)
            syntax->sourceRegionNode_ = nodeIndex;
        // Reuse the existing resumable final-node pass; never rescan all nodes
        // on a host input/read path just to discover incomplete envelopes.
        if (node.block.kind == MarkdownBlockKind::FrontMatter && ! node.block.closed)
            syntax->diagnostics_.push_back({MarkdownDiagnosticCode::UnclosedFrontMatter, node.block.info, nodeIndex});
        if (identityCursor == 1u && previous)
        {
            node.identity = previous->Nodes()[0].identity;
            return;
        }
        auto first = node.block.source.first, last = node.block.source.last;
        bool mapped = previous && ! discardOldIdentities;
        if (node.block.kind == MarkdownBlockKind::SourceEditor && (! previous || ! previous->sourceRegionRequest_ || ! syntax->sourceRegionRequest_ ||
                                                                   previous->sourceRegionRequest_->session != syntax->sourceRegionRequest_->session))
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
            const auto parentIdentity = node.parent == MarkdownNoNode ? 0u : syntax->nodes_[node.parent].identity;
            const auto reuseIdentity  = [&](std::size_t candidateFirst, std::size_t candidateLast)
            {
                const auto found = oldNodes.find(NodeKey{parentIdentity, candidateFirst, candidateLast, node.block.kind});
                if (found == oldNodes.end())
                    return std::uint64_t{};
                const auto& old   = previous->Nodes()[found->second];
                const bool parent = node.parent == MarkdownNoNode
                                        ? old.parent == MarkdownNoNode
                                        : old.parent != MarkdownNoNode && syntax->nodes_[node.parent].identity == previous->Nodes()[old.parent].identity;
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
    Detail::MarkdownBlocks blocks{syntax->source_, syntax->profile_, syntax->nodes_, syntax->emptyLines_, syntax->extensions_, syntax->sourceRegionRequest_};
    Detail::MarkdownInlines inlines{syntax->source_,
                                    syntax->profile_,
                                    syntax->extensions_,
                                    syntax->nodes_,
                                    syntax->inlines_,
                                    syntax->footnotes_,
                                    syntax->footnoteReferences_,
                                    syntax->diagnostics_,
                                    CharacterReference};
    std::size_t cursor{}, lineFirst{}, lexicalCursor{}, literalFirst{}, rootCursor{}, rootNode = MarkdownNoNode;
    bool blocksComplete{}, lexemesComplete{}, rootsComplete{}, inlinesComplete{};
    void ScanBlockUnit()
    {
        if (blocks.AdvanceTableCell())
            return;
        const auto& source = syntax->source_;
        if (cursor == source.size())
        {
            if (lineFirst < cursor)
            {
                blocks.Line(lineFirst, cursor, cursor);
                lineFirst = cursor;
                return; // A final row's generated cells must finish before compaction.
            }
            blocks.Finish();
            syntax->blockStatistics_ = blocks.Statistics();
            rootNode                 = syntax->nodes_[0].firstChild;
            blocksComplete           = true;
            return;
        }
        const auto character = source[cursor++];
        if (character == L'\r' || character == L'\n')
        {
            const auto bodyEnd = cursor - 1u;
            if (character == L'\r' && cursor < source.size() && source[cursor] == L'\n')
                ++cursor;
            blocks.Line(lineFirst, bodyEnd, cursor);
            lineFirst = cursor;
        }
    }
    void RootUnit()
    {
        auto& roots = syntax->blocks_;
        if (rootNode != MarkdownNoNode && rootCursor == syntax->nodes_[rootNode].block.source.first)
        {
            roots.push_back(syntax->nodes_[rootNode].block);
            rootCursor = roots.back().source.last;
            rootNode   = syntax->nodes_[rootNode].nextSibling;
        }
        else if (rootCursor < syntax->source_.size())
        {
            const auto first = rootCursor;
            do
            {
                ++rootCursor;
            } while (rootCursor < syntax->source_.size() && syntax->source_[rootCursor - 1u] != L'\r' && syntax->source_[rootCursor - 1u] != L'\n');
            if (syntax->source_[rootCursor - 1u] == L'\r' && rootCursor < syntax->source_.size() && syntax->source_[rootCursor] == L'\n')
                ++rootCursor;
            auto bodyEnd = rootCursor;
            while (bodyEnd > first && (syntax->source_[bodyEnd - 1u] == L'\r' || syntax->source_[bodyEnd - 1u] == L'\n'))
                --bodyEnd;
            roots.push_back({MarkdownBlockKind::BlankLine, {first, rootCursor}, {first, bodyEnd}});
        }
        else
            rootsComplete = true;
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
#pragma warning(pop)

MarkdownParser::MarkdownParser() noexcept  = default;
MarkdownParser::~MarkdownParser() noexcept = default;
MarkdownParser::MarkdownParser(MarkdownParser&& other) noexcept
    : preparation_(std::move(other.preparation_)),
      published_(std::move(other.published_)),
      documentIdentity_(std::exchange(other.documentIdentity_, 0u)),
      nextNodeIdentity_(std::exchange(other.nextNodeIdentity_, 1u)),
      publishedSource_(std::move(other.publishedSource_))
{
    other.publishedSource_.reset();
}
MarkdownParser& MarkdownParser::operator=(MarkdownParser&& other) noexcept
{
    if (this != &other)
    {
        preparation_      = std::move(other.preparation_);
        published_        = std::move(other.published_);
        documentIdentity_ = std::exchange(other.documentIdentity_, 0u);
        nextNodeIdentity_ = std::exchange(other.nextNodeIdentity_, 1u);
        publishedSource_  = std::move(other.publishedSource_);
        other.publishedSource_.reset();
    }
    return *this;
}
bool MarkdownParser::HasPendingPreparation() const noexcept
{
    return static_cast<bool>(preparation_);
}
std::shared_ptr<const MarkdownSyntax> MarkdownParser::Published() const noexcept
{
    return published_;
}
bool MarkdownParser::TryAdoptParsedSnapshot(const SourceSnapshot& source,
                                            std::uint64_t revision,
                                            const std::shared_ptr<const MarkdownSyntax>& syntax,
                                            MarkdownProfile profile,
                                            std::optional<MarkdownExtensions> extensions)
{
    MarkdownExtensions resolved{};
    if (! syntax || ! TryResolveExtensions(profile, extensions, resolved) || syntax->Profile() != profile || syntax->Extensions() != resolved ||
        syntax->sourceRegionRequest_)
        return false;

    const auto& canonical = syntax->parsedDataOwner_ ? syntax->parsedDataOwner_ : syntax;
    if (canonical->parsedDataOwner_ || canonical->sourceRegionRequest_ || canonical->Profile() != profile || canonical->Extensions() != resolved ||
        ! source.MatchesText(canonical->Source()))
        return false;

    if (published_ && published_->parsedDataOwner_ && published_->Revision() == revision && published_->Profile() == profile &&
        published_->Extensions() == resolved && publishedSource_ && publishedSource_->IsSameRevision(source) && published_->parsedDataOwner_ == canonical)
    {
        // Reasserting the current publication supersedes a later staged
        // request, just like Begin of an already published exact snapshot.
        Cancel();
        return true;
    }

    const auto identity = nextDocumentIdentity.fetch_add(1u, std::memory_order_relaxed);
    auto staged         = std::shared_ptr<MarkdownSyntax>(new MarkdownSyntax(canonical, revision, profile, resolved, identity));

    // All potentially throwing validation/allocation completed. Commit only
    // no-throw ownership and scalar updates so a refused/failed adoption keeps
    // the old staged and published state intact.
    preparation_.reset();
    published_        = std::move(staged);
    documentIdentity_ = identity;
    nextNodeIdentity_ = 1u;
    publishedSource_  = source;
    return true;
}
void MarkdownParser::Cancel() noexcept
{
    preparation_.reset();
}
void MarkdownParser::Clear() noexcept
{
    Cancel();
    published_.reset();
    documentIdentity_ = 0u;
    nextNodeIdentity_ = 1u;
    publishedSource_.reset();
}
void MarkdownParser::Begin(std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    BeginSource(source, revision, profile, ValidateExtensions(profile, extensions), std::nullopt);
}
void MarkdownParser::Begin(const SourceSnapshot& source, std::uint64_t revision, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    const auto enabled = ValidateExtensions(profile, extensions);
    const auto change  = publishedSource_ ? std::optional(source.ChangedRangeFrom(*publishedSource_)) : std::nullopt;
    if (change && change->kind == SourceChangeKind::Unchanged && published_ && published_->Revision() == revision && published_->Profile() == profile &&
        published_->Extensions() == enabled && ! published_->sourceRegionRequest_)
    {
        Cancel();
        publishedSource_ = source;
        return;
    }
    const auto flat = source.Text();
    BeginSource(flat, revision, profile, enabled, change);
    if (preparation_)
        preparation_->snapshot = source;
    else
        publishedSource_ = source;
}
void MarkdownParser::BeginSourceRegion(
    const SourceSnapshot& source, std::uint64_t revision, MarkdownSourceRegion region, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions)
{
    if (! region.session)
        throw std::invalid_argument("Source editing requires a nonzero session");
    if (region.source.first > region.source.last || ! source.IsBoundary(region.source.first) || ! source.IsBoundary(region.source.last))
        throw std::out_of_range("Invalid source editing envelope");
    const auto enabled = ValidateExtensions(profile, extensions);
    const auto change  = publishedSource_ ? std::optional(source.ChangedRangeFrom(*publishedSource_)) : std::nullopt;
    const auto flat    = source.Text();
    BeginSource(flat, revision, profile, enabled, change, region);
    if (preparation_)
        preparation_->snapshot = source;
    else
        publishedSource_ = source;
}
void MarkdownParser::BeginSource(std::wstring_view source,
                                 std::uint64_t revision,
                                 MarkdownProfile profile,
                                 MarkdownExtensions extensions,
                                 std::optional<SourceChangeRange> change,
                                 std::optional<MarkdownSourceRegion> sourceRegion)
{
    ValidateProfile(profile);
    const auto same = [&](const auto& syntax)
    {
        return syntax && syntax->Revision() == revision && syntax->Profile() == profile && syntax->Extensions() == extensions && syntax->Source() == source &&
               syntax->sourceRegionRequest_ == sourceRegion;
    };
    const auto sameChange = [](const auto& a, const auto& b)
    { return (! a && ! b) || (a && b && a->kind == b->kind && a->begin == b->begin && a->previousEnd == b->previousEnd && a->currentEnd == b->currentEnd); };
    if (preparation_ && same(preparation_->syntax) && sameChange(preparation_->change, change))
        return;
    if (same(published_) && (! change || change->kind == SourceChangeKind::Unchanged))
    {
        Cancel();
        return;
    }
    const auto borrowed     = published_ && published_->parsedDataOwner_;
    const auto document     = borrowed            ? nextDocumentIdentity.fetch_add(1u, std::memory_order_relaxed)
                              : documentIdentity_ ? documentIdentity_
                                                  : nextDocumentIdentity.fetch_add(1u, std::memory_order_relaxed);
    const auto nextIdentity = borrowed ? 1u : nextNodeIdentity_;
    auto staged             = std::make_unique<Preparation>(source, revision, profile, extensions, sourceRegion);
    staged->change          = change;
    staged->SetPrevious(borrowed ? std::shared_ptr<const MarkdownSyntax>{} : published_, document, nextIdentity);
    preparation_      = std::move(staged);
    documentIdentity_ = document;
    nextNodeIdentity_ = nextIdentity;
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
            else if (! pending.rootsComplete)
                pending.RootUnit();
            else if (! pending.lexemesComplete)
                pending.ScanLexicalUnit();
            else if (! pending.inlinesComplete)
                pending.inlinesComplete = pending.inlines.Step();
            else if (! pending.identitiesComplete)
                pending.IdentityUnit();
            else
            {
                nextNodeIdentity_ = pending.nextIdentity;
                publishedSource_  = std::move(pending.snapshot);
                published_        = std::move(pending.syntax);
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
