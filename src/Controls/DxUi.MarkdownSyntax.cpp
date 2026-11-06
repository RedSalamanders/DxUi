#include "DxUi.MarkdownBlocks.h"
#include "DxUi.MarkdownCharacterReference.h"
#include "DxUi.MarkdownEntityData.h"
#include "DxUi.MarkdownInlines.h"
#include "DxUi.MarkdownMetadata.h"
#include "DxUi.MarkdownSourceCorrelation.h"
#include "DxUi.TextElements.h"
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

} // namespace

bool Detail::DecodeMarkdownCharacterReference(std::wstring_view input, std::size_t first, MarkdownLexeme& token) noexcept
{
    return CharacterReference(input, first, token);
}

MarkdownSyntax::MarkdownSyntax(
    std::wstring_view source, std::uint64_t revision, MarkdownProfile profile, MarkdownExtensions extensions, std::optional<MarkdownSourceRegion> sourceRegion)
    : source_(source),
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
    if (! sourceRegionRequest_)
        return std::nullopt;
    if (sourceRegionRequest_->kind == MarkdownSourceRegionKind::Inline)
        return sourceRegionRequest_;
    if (sourceRegionNode_ == MarkdownNoNode)
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
MarkdownInlineStatistics MarkdownSyntax::InlineStatistics() const noexcept
{
    return ParsedData().inlineStatistics_;
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
    if (parsedDataOwner_)
        return parsedDataOwner_->TryLexemeValue(index, output);
    if (index >= lexemes_.size())
        return SourceResult::InvalidRange;
    const auto& token = lexemes_[index];
    if (token.kind == MarkdownLexemeKind::Text)
        return TrySlice(token.source, output);
    if (token.valueLength > token.value.size())
        return SourceResult::InvalidRange;
    output = std::wstring_view(token.value.data(), token.valueLength);
    return SourceResult::Success;
}
SourceResult MarkdownSyntax::TryLexemeSourceAnchor(std::size_t index, std::size_t valueOffset, bool trailing, std::size_t& output) const noexcept
{
    if (parsedDataOwner_)
        return parsedDataOwner_->TryLexemeSourceAnchor(index, valueOffset, trailing, output);
    std::wstring_view value;
    if (TryLexemeValue(index, value) != SourceResult::Success)
        return SourceResult::InvalidRange;
    const auto& token = lexemes_[index];
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
                         std::optional<MarkdownSourceRegion> sourceRegion)
        // The private constructor prevents exposing unparsed mutable shells.
        // shared_ptr owns immediately and deletes on control-block failure too.
        : syntax(std::shared_ptr<MarkdownSyntax>(new MarkdownSyntax(source, revision, profile, extensions, sourceRegion)))
    {
    }
    static std::optional<std::wstring> BlockSource(const MarkdownSyntax& syntax)
    {
        if (! syntax.sourceRegionRequest_ || syntax.sourceRegionRequest_->kind != MarkdownSourceRegionKind::Inline)
            return std::nullopt;
        std::wstring source(syntax.source_);
        std::fill(source.begin() + static_cast<std::ptrdiff_t>(syntax.sourceRegionRequest_->source.first),
                  source.begin() + static_cast<std::ptrdiff_t>(syntax.sourceRegionRequest_->source.last),
                  L'x');
        // A deleted object between delimiter runs must not turn its containing
        // paragraph into a thematic break before inline parsing.
        const auto range = syntax.sourceRegionRequest_->source;
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
    std::wstring_view blockSource                  = blockSourceStorage ? std::wstring_view(*blockSourceStorage) : std::wstring_view(syntax->source_);
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
        if (change && change->kind == SourceChangeKind::TrackedEdits && ! discardOldIdentities)
        {
            // Lexical constructs have a maximum 33-unit named reference (or a
            // two-unit escape/CRLF). Keep a safety margin on both edit edges.
            constexpr std::size_t margin = 34u;
            lexicalPrefixLimit           = commonFirst > margin ? commonFirst - margin : 0u;
            const auto oldSafe           = commonOldEnd + std::min(margin, a.size() - commonOldEnd);
            const auto& lexemes          = previous->Lexemes();
            auto suffix = std::lower_bound(lexemes.begin(), lexemes.end(), oldSafe, [](const auto& token, auto at) { return token.source.first < at; });
            while (suffix != lexemes.end() && suffix->kind == MarkdownLexemeKind::Text)
                ++suffix;
            if (suffix != lexemes.end())
            {
                lexicalSuffixIndex = static_cast<std::size_t>(suffix - lexemes.begin());
                lexicalSuffixStart = commonNewEnd + (suffix->source.first - commonOldEnd);
            }
        }
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
        else if (node.block.kind == MarkdownBlockKind::FrontMatter && node.frontMatterKind == MarkdownFrontMatterKind::Toml)
        {
            const auto issue =
                Detail::ValidateTomlMetadata(syntax->Source().substr(node.block.content.first, node.block.content.last - node.block.content.first));
            if (issue != Detail::MetadataIssue::None)
                syntax->diagnostics_.push_back({issue == Detail::MetadataIssue::Capacity ? MarkdownDiagnosticCode::TomlFrontMatterCapacity
                                                                                         : MarkdownDiagnosticCode::InvalidTomlFrontMatter,
                                                node.block.content,
                                                nodeIndex});
        }
        else if (node.block.kind == MarkdownBlockKind::FrontMatter && node.frontMatterKind == MarkdownFrontMatterKind::Yaml)
        {
            const auto issue =
                Detail::ValidateYamlMetadata(syntax->Source().substr(node.block.content.first, node.block.content.last - node.block.content.first));
            if (issue != Detail::MetadataIssue::None)
            {
                const auto code = issue == Detail::MetadataIssue::Capacity          ? MarkdownDiagnosticCode::YamlFrontMatterCapacity
                                  : issue == Detail::MetadataIssue::DuplicateKey    ? MarkdownDiagnosticCode::DuplicateYamlKey
                                  : issue == Detail::MetadataIssue::UnresolvedAlias ? MarkdownDiagnosticCode::UnresolvedYamlAlias
                                                                                    : MarkdownDiagnosticCode::InvalidYamlFrontMatter;
                syntax->diagnostics_.push_back({code, node.block.content, nodeIndex});
            }
        }
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
    Detail::MarkdownBlocks blocks{
        blockSource,
        syntax->profile_,
        syntax->nodes_,
        syntax->emptyLines_,
        syntax->extensions_,
        syntax->sourceRegionRequest_ && syntax->sourceRegionRequest_->kind == MarkdownSourceRegionKind::Block ? syntax->sourceRegionRequest_ : std::nullopt,
        syntax->sourceRegionRequest_ && syntax->sourceRegionRequest_->kind == MarkdownSourceRegionKind::Inline && syntax->source_.empty()};
    Detail::MarkdownInlines inlines{syntax->sourceRegionRequest_ && syntax->sourceRegionRequest_->kind == MarkdownSourceRegionKind::Inline &&
                                            syntax->sourceRegionRequest_->source.first == syntax->sourceRegionRequest_->source.last
                                        ? std::wstring_view(syntax->source_)
                                        : blockSource,
                                    syntax->source_,
                                    syntax->sourceRegionRequest_,
                                    syntax->profile_,
                                    syntax->extensions_,
                                    syntax->nodes_,
                                    syntax->inlines_,
                                    syntax->footnotes_,
                                    syntax->footnoteReferences_,
                                    syntax->diagnostics_,
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
                    ! validRange(node.block.info) || ! validRange(node.label) || ! validRange(node.destination) || ! validRange(node.title))
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
        closedRoot.reset();
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
        const auto& source = blockSource;
        if (previous && change && change->kind == SourceChangeKind::TrackedEdits && previous->Extensions() == syntax->Extensions() &&
            ! previous->sourceRegionRequest_ && ! syntax->sourceRegionRequest_ && cursor == lineFirst && blocks.CanImportClosedRoot())
        {
            const bool prefix = cursor < commonFirst;
            const bool suffix = cursor >= commonNewEnd;
            if (prefix || suffix)
            {
                const auto oldPosition    = prefix ? cursor : commonOldEnd + (cursor - commonNewEnd);
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
                                                    blank->source.last > blank->source.first && (! prefix || blank->source.last <= commonFirst);
                        // Front matter depends on absolute document start. A
                        // suffix crossing that boundary must be recognized again
                        // in either direction, even when its bytes are unchanged.
                        const bool sameStartContext = (cursor == 0u) == (oldPosition == 0u);
                        // Other suffix closure remains valid through EOF; prefix
                        // roots require an unchanged document-owned separator.
                        if (sameStartContext && (followingBlank || suffix) && old.block.closed && old.block.source.last > old.block.source.first)
                        {
                            closedRoot.emplace();
                            closedRoot->root = closedRoot->scan = reusableRootIndex - 1u;
                            closedRoot->newFirst                = cursor;
                            return;
                        }
                    }
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
        if (previous && lexicalPrefixIndex < previous->Lexemes().size())
        {
            const auto& token = previous->Lexemes()[lexicalPrefixIndex];
            if (token.source.first == lexicalCursor && token.source.last <= lexicalPrefixLimit)
            {
                syntax->lexemes_.push_back(token);
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
                syntax->lexemes_.push_back(token);
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
    Failure::Scope failureScope;
    MarkdownExtensions resolved{};
    if (! syntax || ! TryResolveExtensions(profile, extensions, resolved) || syntax->Profile() != profile || syntax->Extensions() != resolved ||
        syntax->sourceRegionRequest_)
        return false;

    const auto& canonical = syntax->parsedDataOwner_ ? syntax->parsedDataOwner_ : syntax;
    if (canonical->parsedDataOwner_ || canonical->sourceRegionRequest_ || canonical->Profile() != profile || canonical->Extensions() != resolved)
        return false;

    if (published_ && published_->parsedDataOwner_ && published_->Revision() == revision && published_->Profile() == profile &&
        published_->Extensions() == resolved && publishedSource_ && publishedSource_->IsSameRevision(source) && published_->parsedDataOwner_ == canonical)
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
    const auto change = publishedSource_ ? std::optional(source.ChangedRangeFrom(*publishedSource_)) : std::nullopt;
    if (change && change->kind == SourceChangeKind::Unchanged && published_ && published_->Revision() == revision && published_->Profile() == profile &&
        published_->Extensions() == enabled && ! published_->sourceRegionRequest_)
    {
        Cancel();
        publishedSource_ = source;
        return MarkdownResult::Success;
    }
    const auto flat = source.Text();
    BeginSource(flat, revision, profile, enabled, change);
    if (preparation_)
        preparation_->snapshot = source;
    else
        publishedSource_ = source;
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
    const auto change = publishedSource_ ? std::optional(source.ChangedRangeFrom(*publishedSource_)) : std::nullopt;
    const auto flat   = source.Text();
    BeginSource(flat, revision, profile, enabled, change, region);
    if (preparation_)
        preparation_->snapshot = source;
    else
        publishedSource_ = source;
    return MarkdownResult::Success;
}
void MarkdownParser::BeginSource(std::wstring_view source,
                                 std::uint64_t revision,
                                 MarkdownProfile profile,
                                 MarkdownExtensions extensions,
                                 std::optional<SourceChangeRange> change,
                                 std::optional<MarkdownSourceRegion> sourceRegion)
{
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
    Failure::Scope failureScope;
    if (! preparation_)
        return {published_ ? MarkdownPreparationStatus::Complete : MarkdownPreparationStatus::Idle};
    MarkdownAdvance result{MarkdownPreparationStatus::Pending};
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
        {
            pending.inlinesComplete = pending.inlines.Step();
            if (pending.inlinesComplete)
                pending.syntax->inlineStatistics_ = {
                    pending.inlines.AutolinkInspectionUnits(), pending.inlines.AutolinkDecodedUnits(), pending.inlines.WikiInspectionUnits()};
        }
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
        if (pending.blocks.CapacityExceeded() || pending.inlines.CapacityExceeded())
        {
            preparation_.reset();
            result.status = MarkdownPreparationStatus::Failed;
            result.result = MarkdownResult::CapacityExceeded;
            break;
        }
    }
    return result;
}
} // namespace DxUi::RichText
