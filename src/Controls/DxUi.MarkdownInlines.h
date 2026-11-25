#pragma once
#include "DxUi.MarkdownLabels.h"
#include "DxUi.MarkdownUnicodeData.h"
#include "DxUi.MarkdownWork.h"
#include "DxUi.TextElements.h"
#include "Support/WorkCounters.h"
#include <DxUi/Configuration.h>
#include <DxUi/Failure.h>
#include <DxUi/MarkdownSyntax.h>
#include <DxUi/RichMath.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>

namespace DxUi::RichText::Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
inline thread_local std::size_t testMathOwnershipProbeWork{};
#endif
// Incremental parse reuse (plan item W7.2). A link reference definition as resolution sees it: the first definition of
// each normalized label, in label order. Two parses resolve every link alike exactly when these lists are equal.
struct MarkdownReferenceValue
{
    MarkdownReferenceValue() : label(0u, L' '), destination(0u, L' '), title(0u, L' ')
    {
    }
    std::wstring label, destination, title;
    bool operator==(const MarkdownReferenceValue&) const = default;
};
// A closed top-level subtree the block pass imported from the previous parse: new node indices [first, first + count)
// are old indices [oldFirst, oldFirst + count), and its source moved from oldSource to newSource.
struct MarkdownImportedSubtree
{
    std::size_t first = MarkdownNoNode, count{}, oldFirst{}, oldSource{}, newSource{};
};
// What an incremental inline pass may copy from: the previous parse's nodes and inlines, its effective definitions and
// the inline staging of each of its leaves, and the imported subtrees in node order.
struct MarkdownInlineReuse
{
    const std::vector<MarkdownNode>* oldBlocks{};
    const std::vector<MarkdownInline>* oldInlines{};
    const std::vector<MarkdownReferenceValue>* oldReferences{};
    const std::vector<std::uint32_t>* oldLeafStaging{};
    const std::vector<MarkdownImportedSubtree>* subtrees{};
};
// Owned Unicode inline recognizer. The block pass must finish before global
// references are collected. No decoding is applied inside code or raw HTML.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4626 5027)
class MarkdownInlines
{
    using Kind             = MarkdownInlineKind;
    using ReferenceDecoder = bool (*)(std::wstring_view, std::size_t, MarkdownLexeme&) noexcept;
    std::wstring_view _source;
    std::wstring_view _rawSource;
    std::optional<MarkdownSourceRegion> _sourceRegion;
    bool _sourceRegionEmitted{};
    std::size_t _wikiClose   = MarkdownNoNode;
    std::size_t _wikiScanned = 0u;
    MarkdownProfile _profile;
    MarkdownExtensions _extensions;
    std::vector<MarkdownNode>& _blocks;
    std::vector<MarkdownInline>& _nodes;
    std::vector<MarkdownFootnote>& _footnotes;
    std::vector<MarkdownFootnoteReference>& _footnoteReferences;
    std::vector<MarkdownDiagnostic>& _diagnostics;
    ReferenceDecoder _referenceDecoder;
    struct Reference
    {
        Reference() : label(0u, L' '), destination(0u, L' '), title(0u, L' ')
        {
        }
        Reference(const Reference&)            = default;
        Reference& operator=(const Reference&) = default;
        std::wstring label, destination, title;
        std::size_t source{};
    };
    struct Delimiter
    {
        std::size_t node{}, length{}, previous = MarkdownNoNode, next = MarkdownNoNode;
        wchar_t marker{};
        bool open{}, close{};
        std::size_t originalLength{};
    };
    struct Bracket
    {
        std::size_t node{}, offset{}, delimiter = MarkdownNoNode;
        bool image{}, active = true, nested{};
    };
    struct AutolinkVisit
    {
        std::size_t node, parent;
    };
    struct AutolinkFacts
    {
        std::size_t hostEnd{}, emailEnd{}, nextAt = MarkdownNoNode, nextDoubleDot = MarkdownNoNode;
        std::size_t previousDot = MarkdownNoNode, nonDotEnd{}, nonUnderscoreEnd{}, underscores{};
    };
    struct AutolinkRegion
    {
        AutolinkVisit visit;
        std::size_t end{}, next = MarkdownNoNode, current{}, deferred = MarkdownNoNode;
        bool scanning{};
    };
    struct FootnoteVisit
    {
        std::size_t node{};
        bool inlineNode{}, definitionRoot{}, followSibling = true;
    };
    std::vector<Reference> _references;
    std::vector<Delimiter> _delimiters;
    std::vector<Bracket> _brackets;
    std::vector<AutolinkVisit> _autolinkWalk;
    std::vector<AutolinkFacts> _autolinkFacts;
    std::optional<AutolinkRegion> _autolinkRegion;
    std::size_t _autolinkFactsCursor{};
    unsigned _autolinkFactsPhase{};
    std::size_t _autolinkInspectionUnits{}, _autolinkDecodedUnits{}, _wikiInspectionUnits{};
    std::vector<FootnoteVisit> _footnoteWalk;
    std::vector<std::size_t> _reachableDefinitions;
    std::vector<std::size_t> _previous, _offsets, _backtickNext;
    std::vector<std::array<std::uint32_t, 9u>> _htmlNext;
    std::array<std::size_t, 4u> _htmlLast{};
    std::wstring _text;
    // Incremental reuse (W7.2): this parse's effective definitions and per-leaf staging (both carried to the next
    // parse), the subtree walk of a copied leaf, and the staging copied leaves stand for beyond the nodes they copy.
    std::vector<MarkdownReferenceValue> _effectiveReferences;
    std::vector<std::uint32_t> _leafStaging;
    std::vector<std::size_t> _copyStack;
    std::optional<MarkdownInlineReuse> _reuse;
    std::size_t _reuseCursor{}, _virtualStaging{}, _leafStart{};
    bool _reuseActive{};
    std::size_t _blockCursor{}, _referenceCursor{}, _cursor{}, _root = MarkdownNoNode, _tail = MarkdownNoNode;
    std::size_t _delimiterTail = MarkdownNoNode, _delimiterHead = MarkdownNoNode;
    std::size_t _lastLiteral = MarkdownNoNode, _compactCursor{}, _compactCount{};
    unsigned _compactPhase{};
    std::size_t _inlineFirst{}, _inlineLimit{};
    std::size_t _autolinkDelimiter = MarkdownNoNode, _underscoreOpen{};
    bool _autolinkBoundary{}, _emailBoundary{};
    bool _referencesComplete{}, _leafActive{}, _autolinksActive{};
    bool _referencesSorted{}, _compactComplete{}, _footnoteWalkStarted{};
    bool _footnoteReferencesFinal{};
    bool _capacityExceeded{};
    std::size_t _footnoteReferenceCursor{}, _footnoteReferenceCount{};
    std::size_t _footnoteUniqueCursor{}, _footnoteUniqueCount{}, _footnoteOrdinal{}, _footnoteQueueHead{};

    static bool AsciiLetter(wchar_t c) noexcept
    {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
    }
    static bool Digit(wchar_t c) noexcept
    {
        return c >= L'0' && c <= L'9';
    }
    static bool AsciiPunctuation(wchar_t c) noexcept
    {
        return (c >= L'!' && c <= L'/') || (c >= L':' && c <= L'@') || (c >= L'[' && c <= L'`') || (c >= L'{' && c <= L'~');
    }
    static bool SourceSpace(wchar_t c) noexcept
    {
        return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
    }
    static bool GfmSpace(wchar_t c) noexcept
    {
        return SourceSpace(c) || c == L'\v' || c == L'\f';
    }
    bool GrammarSpace(wchar_t c) const noexcept
    {
        return SourceSpace(c) || (_profile == MarkdownProfile::Gfm029 && (c == L'\v' || c == L'\f'));
    }
    template <std::size_t N> static bool Contains(const MarkdownUnicodeData::Range (&ranges)[N], std::uint32_t scalar) noexcept
    {
        const auto* entry = std::lower_bound(std::begin(ranges), std::end(ranges), scalar, [](const auto& range, auto value) { return range.last < value; });
        return entry != std::end(ranges) && entry->first <= scalar;
    }
    static std::uint32_t Scalar(std::wstring_view text, std::size_t offset) noexcept
    {
        const auto c = static_cast<std::uint32_t>(text[offset]);
        return IsUtf16LeadSurrogate(text[offset]) && offset + 1u < text.size()
                   ? 0x10000u + ((c - 0xd800u) << 10u) + static_cast<std::uint32_t>(text[offset + 1u]) - 0xdc00u
                   : c;
    }
    static bool White(std::uint32_t c) noexcept
    {
        return c == 0u || c == 9u || c == 10u || c == 12u || c == 13u || Contains(MarkdownUnicodeData::kSpace, c);
    }
    bool Punctuation(std::uint32_t c) const noexcept
    {
        // CommonMark 0.31.2 added Unicode symbols; strict GFM retains its older
        // wording: ASCII punctuation plus Unicode general category P.
        return Contains(MarkdownUnicodeData::kPunctuation, c) &&
               (_profile != MarkdownProfile::Gfm029 || c < 128u || ! Contains(MarkdownUnicodeData::kSymbol, c));
    }
    static void AppendScalar(std::wstring& result, std::uint32_t scalar)
    {
        if (scalar < 0x10000u)
            result.push_back(static_cast<wchar_t>(scalar));
        else
        {
            scalar -= 0x10000u;
            result.push_back(static_cast<wchar_t>(0xd800u + (scalar >> 10u)));
            result.push_back(static_cast<wchar_t>(0xdc00u + (scalar & 1023u)));
        }
    }
    std::wstring FoldLabel(std::wstring_view text) const
    {
        return NormalizeMarkdownLabel(text, _profile);
    }
    std::wstring Decode(std::wstring_view input) const
    {
        std::wstring result(0u, L' ');
        for (std::size_t i = 0; i < input.size();)
        {
            MarkdownLexeme entity;
            if (input[i] == L'&' && _referenceDecoder(input, i, entity))
            {
                result.append(entity.value.data(), entity.valueLength);
                i = entity.source.last;
            }
            else if (input[i] == L'\\' && i + 1u < input.size() && AsciiPunctuation(input[i + 1u]))
            {
                result.push_back(input[i + 1u]);
                i += 2u;
            }
            else
            {
                result.push_back(input[i] ? input[i] : L'\ufffd');
                ++i;
            }
        }
        return result;
    }
    std::wstring Fragment(const MarkdownNode& block, MarkdownRange range) const
    {
        std::wstring result(0u, L' ');
        for (const auto& line : block.lines)
            for (auto i = std::max(line.source.first, range.first); i < std::min(line.source.last, range.last); ++i)
            {
                if (_source[i] == L'\r')
                {
                    result.push_back(L'\n');
                    if (i + 1u < line.source.last && _source[i + 1u] == L'\n')
                        ++i;
                }
                else
                    result.push_back(_source[i]);
            }
        return result;
    }
    void CollectReference()
    {
        const auto& block = _blocks[_referenceCursor++];
        if (block.block.kind == MarkdownBlockKind::ReferenceDefinition || block.block.kind == MarkdownBlockKind::FootnoteDefinition)
            ::DxUi::Detail::Work().rich.markdownReferenceUnitsRead +=
                (block.label.last - block.label.first) + (block.destination.last - block.destination.first) + (block.title.last - block.title.first);
        if (block.block.kind == MarkdownBlockKind::FootnoteDefinition)
        {
            _footnotes.emplace_back();
            auto& note      = _footnotes.back();
            note.label      = FoldLabel(_source.substr(block.label.first, block.label.last - block.label.first));
            note.definition = _referenceCursor - 1u;
            return;
        }
        if (block.block.kind != MarkdownBlockKind::ReferenceDefinition)
            return;
        _references.emplace_back();
        auto& reference       = _references.back();
        reference.label       = FoldLabel(Fragment(block, block.label));
        reference.destination = Decode(Fragment(block, block.destination));
        reference.title       = Decode(Fragment(block, block.title));
        reference.source      = block.block.source.first;
    }
    const Reference* Lookup(std::wstring_view label) const
    {
        if (label.empty())
            return nullptr;
        std::size_t characters = 0;
        for (std::size_t i = 0; i < label.size(); i = StepToNextCodePoint(label, i))
            if (++characters > 999u)
                return nullptr;
        const auto folded = FoldLabel(label);
        if (folded.empty())
            return nullptr;
        const auto entry = std::lower_bound(
            _references.begin(), _references.end(), folded, [](const auto& reference, const auto& wanted) { return reference.label < wanted; });
        return entry != _references.end() && entry->label == folded ? &*entry : nullptr;
    }
    MarkdownRange Range(std::size_t first, std::size_t last) const noexcept
    {
        return {_offsets[first],
                last > first ? _offsets[last - 1u] + (((_text[last - 1u] == L'\n' && _source[_offsets[last - 1u]] == L'\r') ||
                                                       (_text[last - 1u] == L'|' && _source[_offsets[last - 1u]] == L'\\')) &&
                                                              _offsets[last - 1u] + 1u < _source.size() && _source[_offsets[last - 1u] + 1u] == _text[last - 1u]
                                                          ? 2u
                                                          : 1u)
                             : _offsets[first]};
    }
    // Staging records for node-capacity admission: the nodes staged so far plus, for each copied leaf, the shells its
    // parse would have staged beyond the nodes it published, so a copy admits exactly what a full parse admits.
    std::size_t Staged() const noexcept
    {
        return _nodes.size() + _virtualStaging;
    }
    void FinishLeafStaging(std::size_t staging) noexcept
    {
        if (_blockCursor < _leafStaging.size())
            _leafStaging[_blockCursor] = static_cast<std::uint32_t>(staging);
    }
    // W7.2: once the definitions are known, copying is admitted when they resolve every label as the previous parse did.
    void DecideReuse()
    {
        _leafStaging.assign(_blocks.size(), 0u);
        for (const auto& reference : _references)
            if (_effectiveReferences.empty() || _effectiveReferences.back().label != reference.label)
            {
                _effectiveReferences.emplace_back();
                _effectiveReferences.back().label       = reference.label;
                _effectiveReferences.back().destination = reference.destination;
                _effectiveReferences.back().title       = reference.title;
            }
        if (! _reuse || _reuse->subtrees->empty())
            return;
        _reuseActive = *_reuse->oldReferences == _effectiveReferences;
        if (! _reuseActive)
            ++::DxUi::Detail::Work().rich.markdownInlineFullReparses;
    }
    // W7.2: the current block lies in an imported subtree whose old leaf can be copied: returns its old node index.
    std::size_t ImportedOldBlock() noexcept
    {
        if (! _reuseActive)
            return MarkdownNoNode;
        const auto& subtrees = *_reuse->subtrees;
        while (_reuseCursor < subtrees.size() && subtrees[_reuseCursor].first + subtrees[_reuseCursor].count <= _blockCursor)
            ++_reuseCursor;
        if (_reuseCursor == subtrees.size() || subtrees[_reuseCursor].first > _blockCursor)
            return MarkdownNoNode;
        return subtrees[_reuseCursor].oldFirst + (_blockCursor - subtrees[_reuseCursor].first);
    }
    // W7.2: copies the unchanged leaf's inline forest from the previous parse, shifted, instead of scanning its text.
    // Refuses (the leaf is parsed) for a forest with a footnote reference or source editor, or one that is not a single
    // contiguous index range of the old dense forest. Indivisible, proportional to the leaf's nodes.
    bool CopyLeaf(std::size_t oldIndex)
    {
        const auto& oldBlocks  = *_reuse->oldBlocks;
        const auto& oldInlines = *_reuse->oldInlines;
        if (oldIndex >= oldBlocks.size() || oldBlocks[oldIndex].block.kind != _blocks[_blockCursor].block.kind || oldIndex >= _reuse->oldLeafStaging->size())
            return false;
        const auto& subtree = (*_reuse->subtrees)[_reuseCursor];
        const auto root     = oldBlocks[oldIndex].firstInline;
        std::size_t low = root, high = root, count = 0u;
        _copyStack.clear();
        if (root != MarkdownNoNode)
            _copyStack.push_back(root);
        while (! _copyStack.empty())
        {
            const auto index = _copyStack.back();
            _copyStack.pop_back();
            if (index >= oldInlines.size() || count == oldInlines.size())
                return false;
            const auto& node = oldInlines[index];
            if (node.kind == Kind::FootnoteReference || node.kind == Kind::SourceEditor)
                return false;
            ++count;
            low  = (std::min)(low, index);
            high = (std::max)(high, index);
            if (node.firstChild != MarkdownNoNode)
                _copyStack.push_back(node.firstChild);
            if (node.nextSibling != MarkdownNoNode)
                _copyStack.push_back(node.nextSibling);
        }
        if (count && high - low + 1u != count)
            return false;
        const std::size_t staging = (std::max)(static_cast<std::size_t>((*_reuse->oldLeafStaging)[oldIndex]), count);
        // A full parse of this leaf stages `staging` records; it fails exactly when they do not all fit.
        if (_blocks.size() >= MarkdownSyntax::MaximumNodeRecords || Staged() + staging > MarkdownSyntax::MaximumNodeRecords - _blocks.size())
        {
            _capacityExceeded = true;
            return true;
        }
        const auto base  = _nodes.size();
        const auto shift = [&](std::size_t at) noexcept { return subtree.newSource + (at - subtree.oldSource); };
        for (std::size_t index = low; count && index <= high; ++index)
        {
            _nodes.push_back(oldInlines[index]);
            _previous.push_back(MarkdownNoNode);
            auto& node        = _nodes.back();
            node.source.first = shift(node.source.first);
            node.source.last  = shift(node.source.last);
            if (node.firstChild != MarkdownNoNode)
                node.firstChild = base + (node.firstChild - low);
            if (node.nextSibling != MarkdownNoNode)
                node.nextSibling = base + (node.nextSibling - low);
        }
        _virtualStaging += staging - count;
        FinishLeafStaging(staging);
        _blocks[_blockCursor].firstInline = count ? base + (root - low) : MarkdownNoNode;
        ++_blockCursor;
        auto& work = ::DxUi::Detail::Work().rich;
        ++work.markdownInlineLeavesCopied;
        work.markdownNodesCopied += count;
        return true;
    }
    std::size_t Append(Kind kind, std::size_t first, std::size_t last, std::wstring_view value = {}, bool mergeText = true)
    {
        const auto range = Range(first, last);
        if (kind == Kind::Text && mergeText && _tail != MarkdownNoNode && _tail == _lastLiteral && _nodes[_tail].source.last == range.first)
        {
            _nodes[_tail].value.append(value);
            _nodes[_tail].source.last = range.last;
            return _tail;
        }
        if (_blocks.size() >= MarkdownSyntax::MaximumNodeRecords || Staged() >= MarkdownSyntax::MaximumNodeRecords - _blocks.size())
        {
            _capacityExceeded = true;
            return MarkdownNoNode;
        }
        const auto index = _nodes.size();
        _nodes.emplace_back();
        _previous.push_back(_tail);
        auto& node  = _nodes.back();
        node.kind   = kind;
        node.source = range;
        node.value  = value;
        if (_tail == MarkdownNoNode)
            _root = index;
        else
            _nodes[_tail].nextSibling = index;
        _tail        = index;
        _lastLiteral = kind == Kind::Text && mergeText ? index : MarkdownNoNode;
        return index;
    }
    void FilterHtml(std::size_t index)
    {
        if (! HasMarkdownExtension(_extensions, MarkdownExtensions::TagFilter))
            return;
        auto& node                               = _nodes[index];
        const auto raw                           = std::wstring_view(node.value);
        constexpr std::wstring_view disallowed[] = {L"title", L"textarea", L"style", L"xmp", L"iframe", L"noembed", L"noframes", L"script", L"plaintext"};
        for (std::size_t i = 0u; i < raw.size(); ++i)
        {
            if (raw[i] != L'<')
                continue;
            auto first = i + 1u;
            if (first < raw.size() && raw[first] == L'/')
                ++first;
            for (const auto tag : disallowed)
            {
                if (first + tag.size() >= raw.size())
                    continue;
                bool equal = true;
                for (std::size_t j = 0; j < tag.size(); ++j)
                {
                    auto c = raw[first + j];
                    if (c >= L'A' && c <= L'Z')
                        c = static_cast<wchar_t>(c + 32);
                    if (c != tag[j])
                    {
                        equal = false;
                        break;
                    }
                }
                const auto boundary = first + tag.size();
                const bool end =
                    GfmSpace(raw[boundary]) || raw[boundary] == L'>' || (raw[boundary] == L'/' && boundary + 1u < raw.size() && raw[boundary + 1u] == L'>');
                if (equal && end)
                {
                    node.filteredHtmlOpeners.push_back(i);
                    break;
                }
            }
        }
    }
    void OpaqueHtmlBlock()
    {
        if (_blocks.size() >= MarkdownSyntax::MaximumNodeRecords || Staged() >= MarkdownSyntax::MaximumNodeRecords - _blocks.size())
        {
            _capacityExceeded = true;
            return;
        }
        const auto index = _nodes.size();
        _nodes.emplace_back();
        _previous.push_back(MarkdownNoNode);
        FinishLeafStaging(1u);
        auto& block       = _blocks[_blockCursor++];
        block.firstInline = index;
        auto& node        = _nodes[index];
        node.kind         = Kind::Html;
        node.source       = block.block.source;
        ++::DxUi::Detail::Work().rich.markdownInlineLeavesParsed;
        for (const auto& line : block.lines)
        {
            ::DxUi::Detail::Work().rich.markdownInlineUnitsScanned += line.source.last - line.source.first;
            node.value.append(line.leadingSpaces, L' ');
            for (auto i = line.source.first; i < line.source.last; ++i)
            {
                if (_source[i] == L'\r')
                {
                    node.value.push_back(L'\n');
                    if (i + 1u < line.source.last && _source[i + 1u] == L'\n')
                        ++i;
                }
                else
                    node.value.push_back(_source[i] ? _source[i] : L'\ufffd');
            }
        }
        if (! node.value.empty() && node.value.back() != L'\n')
            node.value.push_back(L'\n');
        FilterHtml(index);
    }
    bool Compact()
    {
        // Delimiter/bracket shells are staging only. Publish a dense semantic
        // forest; each marking, index and copy transition is separately budgeted.
        if (! _compactPhase)
        {
            _previous.assign(_nodes.size(), MarkdownNoNode);
            _offsets.clear();
            _compactCursor = 0u;
            _compactPhase  = 1u;
        }
        else if (_compactPhase == 1u)
        {
            if (_compactCursor < _blocks.size())
            {
                const auto root = _blocks[_compactCursor++].firstInline;
                if (root != MarkdownNoNode)
                    _offsets.push_back(root);
            }
            else
            {
                _compactPhase  = 2u;
                _compactCursor = 0u;
            }
        }
        else if (_compactPhase == 2u)
        {
            if (! _offsets.empty())
            {
                const auto index = _offsets.back();
                _offsets.pop_back();
                if (_previous[index] != MarkdownNoNode)
                    DxUi::Failure::FailContract(E_UNEXPECTED);
                _previous[index] = 0u;
                if (_nodes[index].firstChild != MarkdownNoNode)
                    _offsets.push_back(_nodes[index].firstChild);
                if (_nodes[index].nextSibling != MarkdownNoNode)
                    _offsets.push_back(_nodes[index].nextSibling);
            }
            else
                _compactPhase = 3u;
        }
        else if (_compactPhase == 3u)
        {
            if (_compactCursor < _nodes.size())
            {
                auto& entry = _previous[_compactCursor++];
                if (entry != MarkdownNoNode)
                    entry = _compactCount++;
            }
            else
            {
                _compactPhase  = 4u;
                _compactCursor = 0u;
            }
        }
        else if (_compactPhase == 4u)
        {
            if (_compactCursor < _blocks.size())
            {
                auto& root = _blocks[_compactCursor++].firstInline;
                if (root != MarkdownNoNode)
                    root = _previous[root];
            }
            else
            {
                _compactPhase  = 5u;
                _compactCursor = 0u;
            }
        }
        else if (_compactCursor < _nodes.size())
        {
            const auto index = _compactCursor++, target = _previous[index];
            if (target == MarkdownNoNode)
                return false;
            if (target != index)
                _nodes[target] = _nodes[index];
            auto& node = _nodes[target];
            if (node.firstChild != MarkdownNoNode)
                node.firstChild = _previous[node.firstChild];
            if (node.nextSibling != MarkdownNoNode)
                node.nextSibling = _previous[node.nextSibling];
        }
        else
        {
            _nodes.resize(_compactCount);
            return true;
        }
        return false;
    }
    void Remove(std::size_t index)
    {
        const auto next = _nodes[index].nextSibling, previous = _previous[index];
        if (previous == MarkdownNoNode)
            _root = next;
        else
            _nodes[previous].nextSibling = next;
        if (next == MarkdownNoNode)
            _tail = previous;
        else
            _previous[next] = previous;
        _nodes[index].nextSibling = MarkdownNoNode;
    }
    void RemoveDelimiter(std::size_t index) noexcept
    {
        auto& delimiter = _delimiters[index];
        if (delimiter.previous != MarkdownNoNode)
            _delimiters[delimiter.previous].next = delimiter.next;
        else
            _delimiterHead = delimiter.next;
        if (delimiter.next != MarkdownNoNode)
            _delimiters[delimiter.next].previous = delimiter.previous;
        else
            _delimiterTail = delimiter.previous;
    }
    void Emphasis(std::size_t bottom)
    {
        // Six failed-opener floors per marker distinguish whether the closer
        // can also open and its length modulo three. The CommonMark rule of
        // three rejects ambiguous matches; floors avoid rescanning failed runs.
        std::array<std::array<std::size_t, 6>, 6> lower;
        for (auto& marker : lower)
            marker.fill(bottom);
        auto closer = bottom == MarkdownNoNode ? _delimiterHead : _delimiters[bottom].next;
        while (closer != MarkdownNoNode)
        {
            const auto close = _delimiters[closer];
            if (! close.close)
            {
                closer = close.next;
                continue;
            }
            const auto marker   = close.marker == L'*'   ? 0u
                                  : close.marker == L'_' ? 1u
                                  : close.marker == L'~' ? 2u
                                  : close.marker == L'^' ? 3u
                                  : close.marker == L'=' ? 4u
                                                         : 5u;
            const auto category = (close.open ? 3u : 0u) + close.originalLength % 3u;
            auto opener         = close.previous;
            while (opener != bottom && opener != MarkdownNoNode)
            {
                if (lower[marker][category] != MarkdownNoNode && opener <= lower[marker][category])
                {
                    opener = bottom;
                    break;
                }
                const auto& open = _delimiters[opener];
                const bool odd = (close.marker == L'*' || close.marker == L'_') && (close.open || open.close) &&
                                 (close.originalLength + open.originalLength) % 3u == 0u && (close.originalLength % 3u != 0u || open.originalLength % 3u != 0u);
                if (open.marker == close.marker && open.open && ! odd && ((close.marker == L'*' || close.marker == L'_') || open.length == close.length))
                    break;
                opener = open.previous;
            }
            if (opener == bottom || opener == MarkdownNoNode)
            {
                lower[marker][category] = close.previous;
                const auto index        = closer;
                closer                  = close.next;
                if (! close.open)
                    RemoveDelimiter(index);
                continue;
            }
            auto& open          = _delimiters[opener];
            const auto use      = close.marker != L'*' && close.marker != L'_' ? close.length : open.length >= 2u && close.length >= 2u ? 2u : 1u;
            const auto openNode = open.node, closeNode = close.node;
            const auto first = _nodes[openNode].nextSibling, last = _previous[closeNode];
            const auto wrapper = _nodes.size();
            if (_blocks.size() >= MarkdownSyntax::MaximumNodeRecords || Staged() >= MarkdownSyntax::MaximumNodeRecords - _blocks.size())
            {
                _capacityExceeded = true;
                return;
            }
            _nodes.emplace_back();
            _previous.push_back(openNode);
            _nodes[wrapper].kind         = close.marker == L'~'   ? (use == 2u ? Kind::Strikethrough : Kind::Subscript)
                                           : close.marker == L'^' ? Kind::Superscript
                                           : close.marker == L'=' ? Kind::Highlight
                                           : close.marker == L'+' ? Kind::Inserted
                                           : use == 2u            ? Kind::Strong
                                                                  : Kind::Emphasis;
            _nodes[wrapper].source       = {_nodes[openNode].source.last - use, _nodes[closeNode].source.first + use};
            _nodes[wrapper].firstChild   = first;
            _nodes[wrapper].nextSibling  = closeNode;
            _nodes[openNode].nextSibling = wrapper;
            _previous[closeNode]         = wrapper;
            _previous[first]             = MarkdownNoNode;
            _nodes[last].nextSibling     = MarkdownNoNode;
            auto between                 = open.next;
            while (between != closer)
            {
                const auto next = _delimiters[between].next;
                RemoveDelimiter(between);
                between = next;
            }
            open.length -= use;
            _delimiters[closer].length -= use;
            _nodes[openNode].value.resize(open.length);
            _nodes[openNode].source.last -= use;
            _nodes[closeNode].value.erase(0u, use);
            _nodes[closeNode].source.first += use;
            if (! open.length)
            {
                Remove(openNode);
                RemoveDelimiter(opener);
            }
            if (! _delimiters[closer].length)
            {
                const auto next = _delimiters[closer].next;
                Remove(closeNode);
                RemoveDelimiter(closer);
                closer = next;
            }
        }
        auto remove = bottom == MarkdownNoNode ? _delimiterTail : _delimiters[bottom].next;
        if (bottom == MarkdownNoNode)
            while (remove != MarkdownNoNode)
            {
                const auto previous = _delimiters[remove].previous;
                RemoveDelimiter(remove);
                remove = previous;
            }
        else
            while (remove != MarkdownNoNode)
            {
                const auto next = _delimiters[remove].next;
                RemoveDelimiter(remove);
                remove = next;
            }
    }
    void StartLeaf()
    {
        const auto& block = _blocks[_blockCursor];
        _text.clear();
        _wikiClose   = MarkdownNoNode;
        _wikiScanned = 0u;
        _offsets.clear();
        _delimiters.clear();
        _brackets.clear();
        _cursor = 0u;
        _root = _tail = _delimiterTail = _delimiterHead = _lastLiteral = MarkdownNoNode;
        _leafStart                                                     = Staged();
        ++::DxUi::Detail::Work().rich.markdownInlineLeavesParsed;
        for (const auto& line : block.lines)
        {
            ::DxUi::Detail::Work().rich.markdownInlineUnitsScanned += line.source.last - line.source.first;
            for (unsigned space = 0; space < line.leadingSpaces; ++space)
            {
                _text.push_back(L' ');
                _offsets.push_back(line.source.first);
            }
            for (auto i = line.source.first; i < line.source.last; ++i)
            {
                // Table row splitting precedes inline parsing: an escaped pipe
                // loses its backslash even inside a code span.
                _offsets.push_back(i);
                if (block.block.kind == MarkdownBlockKind::TableCell && _source[i] == L'\\' && i + 1u < line.source.last && _source[i + 1u] == L'|')
                    ++i;
                if (_source[i] == L'\r')
                {
                    _text.push_back(L'\n');
                    if (i + 1u < line.source.last && _source[i + 1u] == L'\n')
                        ++i;
                }
                else
                    _text.push_back(_source[i] ? _source[i] : L'\ufffd');
            }
        }
        const bool taskWhitespace =
            _text.size() > 3u && GfmSpace(_text[3]) && (_text[3] != L'\n' || _text.find_first_not_of(L" \t\r\n", 4u) != std::wstring::npos);
        while (! _text.empty() && SourceSpace(_text.back()))
        {
            _text.pop_back();
            _offsets.pop_back();
        }
        // A zero-length active interval still needs a token boundary: without
        // one, a delimiter run on either side could consume the insertion site.
        if (! _sourceRegionEmitted && _sourceRegion && _sourceRegion->kind == MarkdownSourceRegionKind::Inline &&
            _sourceRegion->source.first == _sourceRegion->source.last && _sourceRegion->source.first >= block.block.content.first &&
            _sourceRegion->source.first <= block.block.content.last)
        {
            const auto found = std::lower_bound(_offsets.begin(), _offsets.end(), _sourceRegion->source.first);
            const auto index = static_cast<std::size_t>(found - _offsets.begin());
            _offsets.insert(found, _sourceRegion->source.first);
            _text.insert(_text.begin() + static_cast<std::ptrdiff_t>(index), L'\ufffc');
        }
        _offsets.push_back(block.block.content.last);
        // The math ownership probes share one leaf-local index. Multiple
        // unmatched openers must never each search the remaining leaf anew.
        _htmlNext.clear();
        _htmlLast.fill(std::wstring::npos);
        const bool htmlTerminators = _text.find(L'<') != std::wstring::npos;
        AccountMarkdownWork(&MarkdownWorkCounts::htmlIndexUnits, _text.size());
        constexpr auto none = std::numeric_limits<std::uint32_t>::max();
        std::array<std::uint32_t, 9u> next{{none, none, none, none, none, none, none, none, none}};
        if (_profile == MarkdownProfile::RichMarkdown && _text.find_first_of(L"$\\") != std::wstring::npos)
        {
            if (_text.size() >= none)
            {
                _capacityExceeded = true;
                return;
            }
            _htmlNext.resize(_text.size() + 1u);
            _htmlNext.back() = next;
            std::size_t slashes{};
            for (std::size_t i = 0u; i < _text.size(); ++i)
            {
                std::uint32_t flags{};
                const bool escaped = slashes % 2u != 0u;
                if (! escaped && _text[i] == L'\\' && i + 1u < _text.size())
                {
                    if (_text[i + 1u] == L')')
                        flags |= 1u;
                    if (_text[i + 1u] == L']')
                        flags |= 2u;
                }
                if (! escaped && _text[i] == L'$')
                {
                    if (i && ! SourceSpace(_text[i - 1u]) && (i + 1u == _text.size() || ! Digit(_text[i + 1u])))
                        flags |= 4u;
                    if (i + 1u < _text.size() && _text[i + 1u] == L'$')
                        flags |= 8u;
                }
                _htmlNext[i][4] = flags;
                slashes         = _text[i] == L'\\' ? slashes + 1u : 0u;
#if DXUI_ENABLE_DIAGNOSTICS
                ++testMathOwnershipProbeWork;
#endif
            }
        }
        const auto indexTerminator = [&](std::size_t i)
        {
            if (! htmlTerminators && _htmlNext.empty())
                return;
            AccountMarkdownWork(&MarkdownWorkCounts::htmlIndexUnits);
            const auto remember = [&](std::size_t slot)
            {
                if (_htmlLast[slot] == std::wstring::npos)
                    _htmlLast[slot] = i;
                next[slot] = static_cast<std::uint32_t>(i);
            };
            if (_text[i] == L'>')
                remember(0u);
            if (i + 2u < _text.size() && _text[i] == L'-' && _text[i + 1u] == L'-' && _text[i + 2u] == L'>')
                remember(1u);
            if (i + 1u < _text.size() && _text[i] == L'?' && _text[i + 1u] == L'>')
                remember(2u);
            if (i + 2u < _text.size() && _text[i] == L']' && _text[i + 1u] == L']' && _text[i + 2u] == L'>')
                remember(3u);
            if (_htmlNext.empty())
                return;
#if DXUI_ENABLE_DIAGNOSTICS
            ++testMathOwnershipProbeWork;
#endif
            const auto flags = _htmlNext[i][4];
            for (std::size_t bit = 0u; bit < 4u; ++bit)
                if ((flags & (1u << bit)) != 0u)
                    next[4u + bit] = static_cast<std::uint32_t>(i);
            if (_text[i] == L'\n')
                next[8] = static_cast<std::uint32_t>(i);
            _htmlNext[i] = next;
        };
        // Index equal-length backtick runs once, so unmatched runs cannot cause
        // repeated scans over an entire paragraph.
        _backtickNext.assign(_text.size(), MarkdownNoNode);
        std::vector<std::size_t> last(_text.size() + 1u, MarkdownNoNode);
        for (std::size_t end = _text.size(); end > 0u;)
        {
            if (_text[end - 1u] != L'`')
            {
                indexTerminator(end - 1u);
                --end;
                continue;
            }
            auto first = end - 1u;
            while (first > 0u && _text[first - 1u] == L'`')
                --first;
            for (auto i = end; i > first; --i)
                indexTerminator(i - 1u);
            const auto length    = end - first;
            _backtickNext[first] = last[length];
            last[length]         = first;
            end                  = first;
        }
        // GFM task markers apply only to the first paragraph of a list item.
        if (HasMarkdownExtension(_extensions, MarkdownExtensions::TaskLists) && block.block.kind == MarkdownBlockKind::Paragraph &&
            block.parent != MarkdownNoNode && _blocks[block.parent].block.kind == MarkdownBlockKind::ListItem &&
            _blocks[block.parent].firstChild == _blockCursor && _text.size() >= 3u && _text[0] == L'[' && _text[2] == L']' &&
            (GfmSpace(_text[1]) || _text[1] == L'x' || _text[1] == L'X') && taskWhitespace)
        {
            _blocks[block.parent].task = GfmSpace(_text[1]) ? MarkdownTask::Unchecked : MarkdownTask::Checked;
            _cursor                    = 3u;
            while (_cursor < _text.size() && GfmSpace(_text[_cursor]))
                ++_cursor;
        }
        _leafActive = true;
    }
    void SkipSpace(std::wstring_view text, std::size_t& i) const noexcept
    {
        while (i < text.size() && GrammarSpace(text[i]))
            ++i;
    }
    bool InlineLink(std::size_t& end, std::wstring& destination, std::wstring& title) const
    {
        auto i = end;
        if (i == _text.size() || _text[i++] != L'(')
            return false;
        SkipSpace(_text, i);
        auto first = i;
        if (i < _text.size() && _text[i] == L'<')
        {
            first = ++i;
            while (i < _text.size() && _text[i] != L'>')
            {
                if (_text[i] == L'\n' || _text[i] == L'<')
                    return false;
                if (_text[i] == L'\\' && i + 1u < _text.size() && AsciiPunctuation(_text[i + 1u]))
                    ++i;
                ++i;
            }
            if (i == _text.size())
                return false;
            destination = Decode(std::wstring_view(_text).substr(first, i - first));
            ++i;
        }
        else
        {
            unsigned depth = 0;
            while (i < _text.size())
            {
                AccountMarkdownWork(&MarkdownWorkCounts::linkDestinationUnits);
                const auto c = _text[i];
                if (c <= L' ' || c == 127)
                    break;
                if (c == L'\\' && i + 1u < _text.size() && AsciiPunctuation(_text[i + 1u]))
                {
                    i += 2u;
                    continue;
                }
                if (c == L'(')
                {
                    if (++depth > 32u)
                        return false;
                }
                if (c == L')')
                {
                    if (! depth)
                        break;
                    --depth;
                }
                ++i;
            }
            if (depth)
                return false;
            destination = Decode(std::wstring_view(_text).substr(first, i - first));
        }
        const auto afterDestination = i;
        SkipSpace(_text, i);
        if (i > afterDestination && i < _text.size() && (_text[i] == L'\'' || _text[i] == L'"' || _text[i] == L'('))
        {
            const auto open = _text[i++], close = open == L'(' ? L')' : open;
            first = i;
            while (i < _text.size() && _text[i] != close)
            {
                if (open == L'(' && _text[i] == L'(')
                    return false;
                if (_text[i] == L'\\' && i + 1u < _text.size() && AsciiPunctuation(_text[i + 1u]))
                    ++i;
                ++i;
            }
            if (i == _text.size())
                return false;
            title = Decode(std::wstring_view(_text).substr(first, i - first));
            ++i;
            SkipSpace(_text, i);
        }
        if (i == _text.size() || _text[i] != L')')
            return false;
        end = i + 1u;
        return true;
    }
    std::size_t FootnoteFor(std::wstring_view rawLabel) const
    {
        const auto label = FoldLabel(rawLabel);
        const auto found =
            std::lower_bound(_footnotes.begin(), _footnotes.end(), label, [](const auto& note, const auto& wanted) { return note.label < wanted; });
        return found != _footnotes.end() && found->label == label ? static_cast<std::size_t>(found - _footnotes.begin()) : MarkdownNoNode;
    }
    bool Footnote(const Bracket& bracket, std::size_t end)
    {
        if (_profile != MarkdownProfile::RichMarkdown || bracket.image || bracket.nested || ! _brackets.empty())
            return false;
        const auto range = Range(bracket.offset, end);
        const auto raw   = _source.substr(range.first, range.last - range.first);
        if (raw.size() < 4u || ! raw.starts_with(L"[^") || raw.back() != L']')
            return false;
        const auto label = raw.substr(2u, raw.size() - 3u);
        if (! ValidFootnoteLabel(label))
            return false;
        const auto binding = FootnoteFor(label);
        Emphasis(bracket.delimiter);
        if (_capacityExceeded)
            return true;
        auto& node  = _nodes[bracket.node];
        node.kind   = Kind::FootnoteReference;
        node.source = range;
        node.value.assign(raw);
        node.firstChild = node.nextSibling = MarkdownNoNode;
        _footnoteReferences.push_back({bracket.node, binding, 0u});
        _tail        = bracket.node;
        _lastLiteral = MarkdownNoNode;
        _cursor      = end;
        if (binding == MarkdownNoNode)
            _diagnostics.push_back({MarkdownDiagnosticCode::UndefinedFootnote, range, _blockCursor});
        return true;
    }
    bool CloseBracket()
    {
        if (_brackets.empty())
            return false;
        const auto bracket = _brackets.back();
        _brackets.pop_back();
        if (! bracket.active)
            return false;
        auto end = _cursor + 1u;
        std::wstring destination(0u, L' '), title(0u, L' ');
        bool found = InlineLink(end, destination, title);
        if (! found)
        {
            end                        = _cursor + 1u;
            const Reference* reference = nullptr;
            bool explicitLabel         = false;
            if (end < _text.size() && _text[end] == L'[')
            {
                auto last = end + 1u;
                while (last < _text.size() && _text[last] != L']' && _text[last] != L'[')
                {
                    if (_text[last] == L'\\' && last + 1u < _text.size() && AsciiPunctuation(_text[last + 1u]))
                        ++last;
                    ++last;
                }
                if (last < _text.size() && _text[last] == L']')
                {
                    explicitLabel    = last > end + 1u;
                    const auto label = explicitLabel ? std::wstring_view(_text).substr(end + 1u, last - end - 1u)
                                                     : std::wstring_view(_text).substr(bracket.offset + (bracket.image ? 2u : 1u),
                                                                                       _cursor - bracket.offset - (bracket.image ? 2u : 1u));
                    reference        = Lookup(label);
                    if (reference)
                        end = last + 1u;
                }
            }
            if (! reference && ! explicitLabel && ! bracket.nested)
                reference =
                    Lookup(std::wstring_view(_text).substr(bracket.offset + (bracket.image ? 2u : 1u), _cursor - bracket.offset - (bracket.image ? 2u : 1u)));
            if (reference)
            {
                destination = reference->destination;
                title       = reference->title;
                found       = true;
            }
        }
        if (! found && Footnote(bracket, _cursor + 1u))
            return true;
        if (! _brackets.empty())
            _brackets.back().nested = true;
        if (! found)
            return false;
        Emphasis(bracket.delimiter);
        if (_capacityExceeded)
            return true;
        const auto first = _nodes[bracket.node].nextSibling;
        if (first != MarkdownNoNode)
        {
            _nodes[_tail].nextSibling = MarkdownNoNode;
            _previous[first]          = MarkdownNoNode;
        }
        auto& node = _nodes[bracket.node];
        node.kind  = bracket.image ? Kind::Image : Kind::Link;
        node.value.clear();
        node.destination = destination;
        node.title       = title;
        node.firstChild  = first;
        node.nextSibling = MarkdownNoNode;
        node.source      = Range(bracket.offset, end);
        _tail            = bracket.node;
        if (! bracket.image)
            for (auto& earlier : _brackets)
                if (! earlier.image)
                    earlier.active = false;
        _cursor = end;
        return true;
    }
    static bool Email(std::wstring_view text, bool extended, std::size_t* work = nullptr) noexcept
    {
        std::size_t at = 0u;
        while (at < text.size() && text[at] != L'@')
        {
            if (work)
                ++*work;
            if (! AsciiLetter(text[at]) && ! Digit(text[at]) &&
                std::wstring_view(extended ? L".-_+" : L".!#$%&'*+-/=?^_`{|}~").find(text[at]) == std::wstring_view::npos)
                return false;
            ++at;
        }
        if (! at || at + 1u >= text.size())
            return false;
        std::size_t label = 0u, dots = 0u;
        for (auto i = at + 1u; i <= text.size(); ++i)
        {
            if (work)
                ++*work;
            if (i == text.size() || text[i] == L'.')
            {
                if (! label || label > 63u || text[i - 1u] == L'-')
                    return false;
                label = 0u;
                if (i < text.size())
                    ++dots;
            }
            else
            {
                if ((! AsciiLetter(text[i]) && ! Digit(text[i]) && text[i] != L'-' && ! (extended && text[i] == L'_')) || (! label && text[i] == L'-'))
                    return false;
                ++label;
            }
        }
        return ! extended || (dots && (AsciiLetter(text.back()) || Digit(text.back())));
    }
    std::optional<std::size_t> AngleAutolinkEnd(std::size_t start,
                                                std::optional<std::size_t> knownEnd = std::nullopt,
                                                std::size_t* work                   = nullptr) const noexcept
    {
        auto end = knownEnd.value_or(std::wstring::npos);
        if (! knownEnd)
        {
            for (auto i = start + 1u; i < _text.size(); ++i)
            {
                if (work)
                    ++*work;
                const auto c = _text[i];
                if (c == L'>')
                {
                    end = i;
                    break;
                }
                if (c <= L' ' || c == L'<' || c == 127)
                    return std::nullopt;
            }
        }
        if (end == std::wstring::npos)
            return std::nullopt;
        const auto content = std::wstring_view(_text).substr(start + 1u, end - start - 1u);
        if (work)
            *work += std::min(content.size(), size_t{33u});
        const auto colon = content.substr(0u, 33u).find(L':');
        bool uri         = colon >= 2u && colon <= 32u && colon != std::wstring_view::npos && AsciiLetter(content.front());
        if (uri)
        {
            for (std::size_t i = 1u; i < colon; ++i)
            {
                if (work)
                    ++*work;
                if (! AsciiLetter(content[i]) && ! Digit(content[i]) && content[i] != L'+' && content[i] != L'.' && content[i] != L'-')
                {
                    uri = false;
                    break;
                }
            }
            for (const auto c : content)
            {
                if (work)
                    ++*work;
                if (c <= L' ' || c == L'<' || c == 127)
                {
                    uri = false;
                    break;
                }
            }
        }
        const bool email = ! uri && Email(content, false, work);
        if (! uri && ! email)
            return std::nullopt;
        return end;
    }
    bool AngleAutolink()
    {
        const auto found = AngleAutolinkEnd(_cursor, std::nullopt, &_autolinkInspectionUnits);
        if (! found)
            return false;
        const auto end     = found.value();
        const auto content = std::wstring_view(_text).substr(_cursor + 1u, end - _cursor - 1u);
        const bool email   = content.find(L':') == std::wstring_view::npos;
        const auto link    = Append(Kind::Link, _cursor, end + 1u);
        if (link == MarkdownNoNode)
            return true;
        _nodes[link].destination = email ? std::wstring(L"mailto:") + std::wstring(content) : std::wstring(content);
        const auto child         = _nodes.size();
        if (_blocks.size() >= MarkdownSyntax::MaximumNodeRecords || Staged() >= MarkdownSyntax::MaximumNodeRecords - _blocks.size())
        {
            _capacityExceeded = true;
            return true;
        }
        _nodes.emplace_back();
        _previous.push_back(MarkdownNoNode);
        _nodes[child].kind      = Kind::Text;
        _nodes[child].source    = Range(_cursor + 1u, end);
        _nodes[child].value     = content;
        _nodes[link].firstChild = child;
        for (auto& bracket : _brackets)
            if (! bracket.image)
                bracket.active = false;
        _cursor = end + 1u;
        return true;
    }
    struct HtmlLookahead
    {
        std::size_t angle{}, comment{}, processing{}, cdata{};
    };
    std::size_t FindHtmlTerminator(std::size_t start, std::wstring_view suffix, std::size_t first, std::size_t slot) const noexcept
    {
        // Last-occurrence facts reject impossible searches in constant work;
        // successful searches consume the complete resulting HTML node.
        if (_htmlLast[slot] == std::wstring::npos || _htmlLast[slot] < start + first)
            return std::wstring::npos;
        const auto input = std::wstring_view(_text).substr(start);
        const auto end   = input.find(suffix, first);
        AccountMarkdownWork(&MarkdownWorkCounts::htmlSearchedUnits,
                            first > input.size()             ? 0u
                            : end == std::wstring_view::npos ? input.size() - first
                                                             : end - first + suffix.size());
        return end;
    }
    std::size_t HtmlEnd(std::size_t start, const HtmlLookahead* lookahead = nullptr, std::size_t* work = nullptr) const noexcept
    {
        const auto input = std::wstring_view(_text).substr(start);
        const auto tick  = [&]() noexcept
        {
            if (work)
                ++*work;
        };
        const auto skipSpace = [&](std::size_t& at) noexcept
        {
            while (at < input.size() && GrammarSpace(input[at]))
            {
                tick();
                ++at;
            }
        };
        auto terminated = [&](std::wstring_view prefix, std::wstring_view suffix)
        {
            if (! input.starts_with(prefix))
                return std::size_t{};
            auto end = std::wstring_view::npos;
            if (lookahead)
            {
                const auto absolute = suffix == L"-->" ? lookahead->comment : suffix == L"?>" ? lookahead->processing : lookahead->cdata;
                if (absolute != std::wstring::npos && absolute >= start + prefix.size())
                    end = absolute - start;
            }
            else
                end = FindHtmlTerminator(start, suffix, prefix.size(), suffix == L"-->" ? 1u : suffix == L"?>" ? 2u : 3u);
            return end == std::wstring_view::npos ? 0u : end + suffix.size();
        };
        if (input.starts_with(L"<!--"))
        {
            const auto length = terminated(L"<!--", L"-->");
            if (input.starts_with(L"<!-->"))
                return 5u;
            if (input.starts_with(L"<!--->"))
                return 6u;
            return length;
        }
        if (input.starts_with(L"<?"))
            return terminated(L"<?", L"?>");
        if (input.starts_with(L"<![CDATA["))
            return terminated(L"<![CDATA[", L"]]>");
        if (_profile != MarkdownProfile::Gfm029 && input.size() > 2u && input[1] == L'!' && AsciiLetter(input[2]))
        {
            const auto end = lookahead ? (lookahead->angle == std::wstring::npos ? std::wstring_view::npos : lookahead->angle - start)
                                       : FindHtmlTerminator(start, L">", 3u, 0u);
            return end == std::wstring_view::npos ? 0u : end + 1u;
        }
        if (input.size() > 2u && input[1] == L'!' && input[2] >= L'A' && input[2] <= L'Z')
        {
            auto i = 3u;
            while (i < input.size() && input[i] >= L'A' && input[i] <= L'Z')
            {
                tick();
                ++i;
            }
            if (i == input.size() || ! GrammarSpace(input[i]))
                return 0u;
            const auto end = lookahead ? (lookahead->angle == std::wstring::npos ? std::wstring_view::npos : lookahead->angle - start)
                                       : FindHtmlTerminator(start, L">", i, 0u);
            return end == std::wstring_view::npos ? 0u : end + 1u;
        }
        std::size_t i      = 1u;
        const bool closing = i < input.size() && input[i] == L'/';
        if (closing)
            ++i;
        if (i == input.size() || ! AsciiLetter(input[i]))
            return 0u;
        ++i;
        while (i < input.size() && (AsciiLetter(input[i]) || Digit(input[i]) || input[i] == L'-'))
        {
            tick();
            ++i;
        }
        while (i < input.size())
        {
            tick();
            const auto first = i;
            skipSpace(i);
            if (i == input.size())
                return 0u;
            if (input[i] == L'>')
                return i + 1u;
            if (! closing && input[i] == L'/' && i + 1u < input.size() && input[i + 1u] == L'>')
                return i + 2u;
            if (closing || i == first || ! (AsciiLetter(input[i]) || input[i] == L'_' || input[i] == L':'))
                return 0u;
            ++i;
            while (i < input.size() && (AsciiLetter(input[i]) || Digit(input[i]) || std::wstring_view(L"_.:-").find(input[i]) != std::wstring_view::npos))
            {
                tick();
                ++i;
            }
            const auto afterName = i;
            skipSpace(i);
            if (i == input.size() || input[i] != L'=')
            {
                i = afterName;
                continue;
            }
            ++i;
            skipSpace(i);
            if (i == input.size())
                return 0u;
            if (input[i] == L'\'' || input[i] == L'"')
            {
                const auto quote = input[i++];
                while (i < input.size() && input[i] != quote)
                {
                    tick();
                    ++i;
                }
                if (i == input.size())
                    return 0u;
                ++i;
            }
            else
            {
                const auto attributeStart = i;
                while (i < input.size() && ! GrammarSpace(input[i]) && input[i] != L'>')
                {
                    tick();
                    if (std::wstring_view(L"\"'=<>`").find(input[i]) != std::wstring_view::npos)
                        return 0u;
                    ++i;
                }
                if (i == attributeStart)
                    return 0u;
            }
        }
        return 0u;
    }
    bool ExtendedAutolink()
    {
        const auto rest     = std::wstring_view(_text).substr(_cursor, _inlineLimit - _cursor);
        const bool boundary = _autolinkBoundary;
        const auto starts   = [&](std::wstring_view scheme)
        {
            if (rest.size() < scheme.size())
                return false;
            for (std::size_t i = 0; i < scheme.size(); ++i)
            {
                auto c = rest[i];
                if (c >= L'A' && c <= L'Z')
                    c = static_cast<wchar_t>(c + 32);
                if (c != scheme[i])
                    return false;
            }
            return true;
        };
        std::size_t scheme = 0u;
        if (boundary)
        {
            if (starts(L"https://"))
                scheme = 8u;
            else if (starts(L"http://"))
                scheme = 7u;
            else if (starts(L"ftp://"))
                scheme = 6u;
            else if (starts(L"www."))
                scheme = 4u;
        }
        std::size_t length = 0u;
        bool email         = false;
        if (scheme)
        {
            const auto hostFirst  = _cursor + (scheme == 4u ? 0u : scheme);
            const auto hostEnd    = std::min(_inlineLimit, _autolinkFacts[hostFirst].hostEnd);
            const auto domainEnd  = std::max(hostFirst, _autolinkFacts[hostEnd].nonDotEnd);
            const auto dot        = _autolinkFacts[domainEnd].previousDot;
            const auto prior      = dot == MarkdownNoNode ? dot : _autolinkFacts[dot].previousDot;
            const auto lastLabels = prior == MarkdownNoNode ? hostFirst : std::max(hostFirst, prior + 1u);
            ++_autolinkInspectionUnits;
            if (domainEnd == hostFirst || dot == MarkdownNoNode || dot < hostFirst || _text[hostFirst] == L'.' ||
                _autolinkFacts[hostFirst].nextDoubleDot < domainEnd - 1u || _autolinkFacts[domainEnd].underscores != _autolinkFacts[lastLabels].underscores)
                return false;
            length = hostEnd - _cursor;
            while (length < rest.size() && ! White(Scalar(rest, length)) && rest[length] != L'<')
                ++length;
            while (length && std::wstring_view(L"?!.,:*_~").find(rest[length - 1u]) != std::wstring_view::npos)
                --length;
            if (length && rest[length - 1u] == L')')
            {
                std::size_t opens = 0u, closes = 0u;
                for (std::size_t i = 0; i < length; ++i)
                {
                    if (rest[i] == L'(')
                        ++opens;
                    else if (rest[i] == L')')
                        ++closes;
                }
                while (length && rest[length - 1u] == L')' && closes > opens)
                {
                    --length;
                    --closes;
                }
            }
            if (length && rest[length - 1u] == L';')
            {
                auto i = length - 1u;
                while (i && (AsciiLetter(rest[i - 1u]) || Digit(rest[i - 1u])))
                    --i;
                if (i && rest[i - 1u] == L'&' && i < length - 1u)
                    length = i - 1u;
            }
        }
        else
        {
            if (! _emailBoundary)
                return false;
            auto end = std::min(_inlineLimit, _autolinkFacts[_cursor].emailEnd);
            end      = std::max(_cursor, _autolinkFacts[end].nonDotEnd);
            if (_underscoreOpen)
                end = std::max(_cursor, _autolinkFacts[end].nonUnderscoreEnd);
            const auto at = _autolinkFacts[_cursor].nextAt;
            ++_autolinkInspectionUnits;
            if (at == MarkdownNoNode || at <= _cursor || at + 1u >= end || _autolinkFacts[at + 1u].nextAt < end)
                return false;
            // The suffix index already proves the complete local-part alphabet.
            // Validate the domain once per '@', rather than rescanning every
            // failed suffix of a long a@a@... candidate.
            if (! Email(std::wstring_view(_text).substr(at - 1u, end - at + 1u), true))
                return false;
            length = end - _cursor;
            email  = true;
        }
        if (! length)
            return false;
        const auto link = Append(Kind::Link, _cursor, _cursor + length);
        if (link == MarkdownNoNode)
            return true;
        _nodes[link].destination = email          ? std::wstring(L"mailto:") + std::wstring(rest.substr(0u, length))
                                   : scheme == 4u ? std::wstring(L"http://") + std::wstring(rest.substr(0u, length))
                                                  : std::wstring(rest.substr(0u, length));
        const auto child         = _nodes.size();
        if (_blocks.size() >= MarkdownSyntax::MaximumNodeRecords || Staged() >= MarkdownSyntax::MaximumNodeRecords - _blocks.size())
        {
            _capacityExceeded = true;
            return true;
        }
        _nodes.emplace_back();
        _previous.push_back(MarkdownNoNode);
        _nodes[child].kind      = Kind::Text;
        _nodes[child].value     = rest.substr(0u, length);
        _nodes[child].source    = Range(_cursor, _cursor + length);
        _nodes[link].firstChild = child;
        _cursor += length;
        return true;
    }
    std::size_t NormalizedOffset(std::size_t raw) const noexcept
    {
        return static_cast<std::size_t>(std::lower_bound(_offsets.begin(), _offsets.end(), raw) - _offsets.begin());
    }
    void DelimiterHistory(std::size_t raw, std::optional<MarkdownRange> consumed = std::nullopt)
    {
        // Source-ordered cursor: autolinks retire only delimiter runs they own.
        // Runs outside a URL remain on the ordinary emphasis stack. The small
        // underscore balance permits _mail@example.org_ without accepting an
        // otherwise invalid email domain ending in an unmatched underscore.
        while (_autolinkDelimiter != MarkdownNoNode && _nodes[_delimiters[_autolinkDelimiter].node].source.last <= raw)
        {
            const auto index     = _autolinkDelimiter;
            const auto delimiter = _delimiters[index];
            const auto range     = _nodes[delimiter.node].source;
            _autolinkDelimiter   = delimiter.next;
            if (consumed && range.first >= consumed->first && range.last <= consumed->last)
            {
                RemoveDelimiter(index);
                continue;
            }
            if (delimiter.marker != L'_')
                continue;
            auto remaining = delimiter.length;
            if (delimiter.close)
            {
                const auto used = std::min(_underscoreOpen, remaining);
                _underscoreOpen -= used;
                remaining -= used;
            }
            if (delimiter.open && remaining)
            {
                _underscoreOpen += remaining;
                if (range.last == raw)
                    _emailBoundary = true;
            }
        }
    }
    void DecodeAutolinkSuffix()
    {
        auto& deferred = _autolinkRegion->deferred;
        if (deferred == MarkdownNoNode)
            return;
        auto& node       = _nodes[deferred];
        const auto first = NormalizedOffset(node.source.first), last = NormalizedOffset(node.source.last);
        node.value = Decode(std::wstring_view(_text).substr(first, last - first));
        _autolinkDecodedUnits += last - first;
        deferred = MarkdownNoNode;
    }
    void AutolinkSplice(std::size_t link, std::size_t first, std::size_t last, std::size_t& current, std::size_t parent)
    {
        const auto range = _nodes[link].source;
        if (_autolinkRegion->deferred != MarkdownNoNode && _nodes[_autolinkRegion->deferred].source.last <= range.first)
            DecodeAutolinkSuffix();
        while (_nodes[current].source.last <= range.first)
            current = _nodes[current].nextSibling;
        const auto beginNode = current;
        // A deferred node containing this URL is about to become a final prefix
        // or be unlinked. It must not be decoded again at region completion.
        if (_autolinkRegion->deferred == beginNode)
            _autolinkRegion->deferred = MarkdownNoNode;
        auto endNode = current;
        while (_nodes[endNode].source.last < range.last)
            endNode = _nodes[endNode].nextSibling;
        const auto oldFirst = _nodes[beginNode].source.first, oldLast = _nodes[endNode].source.last;
        const auto before = _previous[beginNode], after = _nodes[endNode].nextSibling;
        auto prefix = before;
        if (oldFirst < range.first)
        {
            _nodes[beginNode].value = Decode(std::wstring_view(_text).substr(NormalizedOffset(oldFirst), first - NormalizedOffset(oldFirst)));
            _autolinkDecodedUnits += first - NormalizedOffset(oldFirst);
            _nodes[beginNode].source.last = range.first;
            prefix                        = beginNode;
        }
        if (prefix != MarkdownNoNode)
            _nodes[prefix].nextSibling = link;
        else if (parent != MarkdownNoNode)
            _nodes[parent].firstChild = link;
        else
            _blocks[_blockCursor].firstInline = link;
        _previous[link] = prefix;
        current         = after;
        if (range.last < oldLast)
        {
            const auto end = NormalizedOffset(oldLast);
            // Defer this suffix until it becomes a final prefix or the region
            // finishes. Decoding the complete remaining suffix after each URL
            // made a paragraph of small links quadratic.
            current = Append(Kind::Text, last, end);
            if (current == MarkdownNoNode)
                return;
            _autolinkRegion->deferred = current;
        }
        _nodes[_tail].nextSibling = after;
        if (after != MarkdownNoNode)
            _previous[after] = _tail;
    }
    void PrepareAutolinkFactsUnit()
    {
        if (! _autolinkFactsPhase)
        {
            _autolinkFacts.assign(_text.size() + 1u, AutolinkFacts{});
            _autolinkFactsCursor = 0u;
            _autolinkFactsPhase  = 1u;
        }
        else if (_autolinkFactsPhase == 1u)
        {
            if (_autolinkFactsCursor == _text.size())
            {
                _autolinkFacts.back().hostEnd = _autolinkFacts.back().emailEnd = _text.size();
                _autolinkFactsPhase                                            = 2u;
                return;
            }
            const auto at         = _autolinkFactsCursor++;
            const auto& prior     = _autolinkFacts[at];
            auto& next            = _autolinkFacts[at + 1u];
            next.previousDot      = _text[at] == L'.' ? at : prior.previousDot;
            next.nonDotEnd        = _text[at] == L'.' ? prior.nonDotEnd : at + 1u;
            next.nonUnderscoreEnd = _text[at] == L'_' ? prior.nonUnderscoreEnd : at + 1u;
            next.underscores      = prior.underscores + (_text[at] == L'_' ? 1u : 0u);
            ++_autolinkInspectionUnits;
        }
        else if (_autolinkFactsCursor)
        {
            const auto at       = --_autolinkFactsCursor;
            auto& facts         = _autolinkFacts[at];
            const auto& next    = _autolinkFacts[at + 1u];
            const auto scalar   = Scalar(_text, IsUtf16TrailSurrogate(_text[at]) && at ? at - 1u : at);
            const bool host     = Contains(MarkdownUnicodeData::kAlphanumeric, scalar) || scalar == L'_' || scalar == L'-' || scalar == L'.';
            const auto c        = _text[at];
            const bool email    = AsciiLetter(c) || Digit(c) || std::wstring_view(L".-_+@").find(c) != std::wstring_view::npos;
            facts.hostEnd       = host ? next.hostEnd : at;
            facts.emailEnd      = email ? next.emailEnd : at;
            facts.nextAt        = c == L'@' ? at : next.nextAt;
            facts.nextDoubleDot = c == L'.' && at + 1u < _text.size() && _text[at + 1u] == L'.' ? at : next.nextDoubleDot;
            ++_autolinkInspectionUnits;
        }
        else
            _autolinkFactsPhase = 3u;
    }
    std::optional<std::size_t> ResolveAutolinks(AutolinkVisit visit)
    {
        // Brackets/code/HTML already own their contents. Outside them, scan a
        // whole contiguous text region BEFORE emphasis: www.example.org*a*
        // belongs to the URL through its interior '*a', with only the trailing
        // '*' excluded. Splice links while preserving untouched delimiter nodes.
        if (! _autolinkRegion)
            _autolinkRegion.emplace(AutolinkRegion{visit, visit.node, MarkdownNoNode, visit.node});
        auto& region = _autolinkRegion.value();
        if (! region.scanning)
        {
            const auto next = _nodes[region.end].nextSibling;
            if (next != MarkdownNoNode && _nodes[next].kind == Kind::Text)
            {
                region.end = next;
                ++_autolinkInspectionUnits;
                return std::nullopt;
            }
            region.next      = next;
            const auto first = NormalizedOffset(_nodes[visit.node].source.first);
            _inlineFirst = _cursor = first;
            _inlineLimit           = NormalizedOffset(_nodes[region.end].source.last);
            _autolinkBoundary      = ! first || White(Scalar(_text, StepToPreviousCodePoint(_text, first))) ||
                                     std::wstring_view(L"*_~(").find(_text[first - 1u]) != std::wstring_view::npos;
            _emailBoundary         = true;
            region.scanning        = true;
            return std::nullopt;
        }
        if (_cursor < _inlineLimit)
        {
            ++_autolinkInspectionUnits;
            DelimiterHistory(_offsets[_cursor]);
            const auto at = _cursor;
            _root = _tail = _lastLiteral = MarkdownNoNode;
            if (ExtendedAutolink())
            {
                if (_capacityExceeded)
                    return std::nullopt;
                const auto link = _root;
                AutolinkSplice(link, at, _cursor, region.current, visit.parent);
                if (_capacityExceeded)
                    return std::nullopt;
                DelimiterHistory(_nodes[link].source.last, _nodes[link].source);
                _autolinkBoundary = _emailBoundary = false;
                return std::nullopt;
            }
            MarkdownLexeme entity;
            std::uint32_t previous = Scalar(_text, _cursor);
            if (_text[_cursor] == L'&' && _referenceDecoder(_text, _cursor, entity))
            {
                previous = entity.valueLength >= 2u && IsUtf16TrailSurrogate(entity.value[entity.valueLength - 1u])
                               ? Scalar(std::wstring_view(entity.value.data(), entity.valueLength), entity.valueLength - 2u)
                               : entity.value[entity.valueLength - 1u];
                _cursor  = entity.source.last;
            }
            else if (_text[_cursor] == L'\\' && _cursor + 1u < _inlineLimit && AsciiPunctuation(_text[_cursor + 1u]))
            {
                previous = _text[_cursor + 1u];
                _cursor += 2u;
            }
            else
                _cursor = StepToNextCodePoint(_text, _cursor);
            _autolinkBoundary =
                White(previous) || (previous < 128u && std::wstring_view(L"*_~(").find(static_cast<wchar_t>(previous)) != std::wstring_view::npos);
            _emailBoundary = previous >= 128u || (! AsciiLetter(static_cast<wchar_t>(previous)) && ! Digit(static_cast<wchar_t>(previous)) &&
                                                  std::wstring_view(L".-_+").find(static_cast<wchar_t>(previous)) == std::wstring_view::npos);
            return std::nullopt;
        }
        DecodeAutolinkSuffix();
        const auto next = region.next;
        _autolinkRegion.reset();
        return next;
    }
    void Scan()
    {
        const auto first = _cursor;
        const auto c     = _text[first];
        if (_profile == MarkdownProfile::RichMarkdown && std::wstring_view(_text).substr(first, 2u) == L"$$")
        {
            _cursor += 2u;
            Append(Kind::Text, first, _cursor, std::wstring_view(_text).substr(first, 2u));
            return;
        }
        if (_profile == MarkdownProfile::RichMarkdown && (c == L'$' || (c == L'\\' && first + 1u < _text.size() && _text[first + 1u] == L'(')))
        {
            const bool dollars = c == L'$';
            const auto width   = dollars && std::wstring_view(_text).substr(first, 2u) != L"$$" ? 1u : 2u;
            const bool display = width == 2u && (dollars || _text[first + 1u] == L'[');
            const auto closer  = dollars ? (width == 1u ? L"$" : L"$$") : _text[first + 1u] == L'(' ? L"\\)" : L"\\]";
            const auto content = first + width;
            if (content < _text.size() && (! dollars || display || ! SourceSpace(_text[content])))
            {
                bool endedLine         = false;
                std::size_t* probeWork = nullptr;
#if DXUI_ENABLE_DIAGNOSTICS
                probeWork = &testMathOwnershipProbeWork;
#endif
                const auto limit         = std::min(_text.size(), content + MathMaximumSourceUnits + 1u);
                const auto slot          = dollars ? (display ? 7u : 6u) : (display ? 5u : 4u);
                const auto candidate     = _htmlNext[content + 1u][slot];
                const auto newline       = _htmlNext[content][8];
                const bool endsAtNewline = ! display && newline != std::numeric_limits<std::uint32_t>::max() && newline < limit;
                const bool noCandidate =
                    candidate == std::numeric_limits<std::uint32_t>::max() || candidate >= limit || (endsAtNewline && candidate >= newline);
                // A failed opener below capacity, or before a physical newline,
                // cannot own an opaque fallback. The shared index proves the
                // absence of any eligible closer without rescanning its suffix.
                if (noCandidate && (endsAtNewline || _text.size() - content <= MathMaximumSourceUnits))
                    endedLine = true;
                for (auto end = content; ! endedLine && end < limit; ++end)
                {
                    if (probeWork)
                        ++*probeWork;
                    if (! display && _text[end] == L'\n')
                    {
                        endedLine = true;
                        break;
                    }
                    if (_text[end] == L'`' || _text[end] == L'<')
                    {
                        HtmlLookahead htmlLookahead{std::wstring::npos, std::wstring::npos, std::wstring::npos, std::wstring::npos};
                        if (_text[end] == L'<')
                        {
                            const auto& next  = _htmlNext[end];
                            const auto offset = [](std::uint32_t value) -> std::size_t
                            { return value == std::numeric_limits<std::uint32_t>::max() ? std::wstring::npos : value; };
                            htmlLookahead = {offset(next[0]), offset(next[1]), offset(next[2]), offset(next[3])};
                            if (probeWork)
                                *probeWork += 4u;
                        }
                        std::size_t slashes{};
                        for (auto index = end; index > content && _text[index - 1u] == L'\\'; --index)
                            ++slashes;
                        if (slashes % 2u == 0u && ((_text[end] == L'`' && _backtickNext[end] != MarkdownNoNode) ||
                                                   (_text[end] == L'<' && htmlLookahead.angle != std::wstring::npos &&
                                                    (HtmlEnd(end, &htmlLookahead, probeWork) || AngleAutolinkEnd(end, htmlLookahead.angle, probeWork)))))
                        {
                            endedLine = true; // Let the regular inline scanner own this opaque token.
                            break;
                        }
                    }
                    if (std::wstring_view(_text).substr(end, width) != closer)
                        continue;
                    std::size_t slashes{};
                    for (auto index = end; index > content && _text[index - 1u] == L'\\'; --index)
                        ++slashes;
                    if (slashes % 2u || end == content ||
                        (dollars && ! display && (SourceSpace(_text[end - 1u]) || (end + width < _text.size() && Digit(_text[end + width])))))
                        continue;
                    _cursor = end + width;
                    Append(Kind::Math, first, _cursor, std::wstring_view(_text).substr(content, end - content));
                    return;
                }
                if (! endedLine && _text.size() - content > MathMaximumSourceUnits)
                {
                    // Conservative capacity fallback: retain the remaining line
                    // (display: leaf) as one opaque source owner. Do not resume
                    // Markdown emphasis/link parsing inside oversized notation.
                    const auto end = display ? _text.size() : _text.find(L'\n', content + MathMaximumSourceUnits + 1u);
                    _cursor        = end == std::wstring::npos ? _text.size() : end;
                    Append(Kind::Math, first, _cursor, std::wstring_view(_text).substr(content, _cursor - content));
                    return;
                }
            }
        }
        if (c == L'`')
        {
            auto end = first;
            while (end < _text.size() && _text[end] == L'`')
                ++end;
            const auto close = _backtickNext[first];
            if (close != MarkdownNoNode)
            {
                auto value = _text.substr(end, close - end);
                std::replace(value.begin(), value.end(), L'\n', L' ');
                if (value.size() >= 2u && value.front() == L' ' && value.back() == L' ' && value.find_first_not_of(L' ') != std::wstring::npos)
                    value = value.substr(1u, value.size() - 2u);
                _cursor = close + end - first;
                Append(Kind::Code, first, _cursor, value);
                return;
            }
            _cursor = end;
            Append(Kind::Text, first, end, std::wstring_view(_text).substr(first, end - first));
            return;
        }
        if (c == L'<' && AngleAutolink())
            return;
        if (c == L'<')
            if (const auto length = HtmlEnd(_cursor))
            {
                _cursor += length;
                const auto node = Append(Kind::Html, first, _cursor, std::wstring_view(_text).substr(first, length));
                if (node == MarkdownNoNode)
                    return;
                FilterHtml(node);
                return;
            }
        if (c == L'\\' && first + 1u < _text.size())
        {
            if (AsciiPunctuation(_text[first + 1u]))
            {
                _cursor += 2u;
                Append(Kind::Text, first, _cursor, std::wstring_view(_text).substr(first + 1u, 1u));
                return;
            }
            if (_text[first + 1u] == L'\n')
            {
                _cursor += 2u;
                Append(Kind::HardBreak, first, _cursor);
                return;
            }
        }
        if (c == L'&')
        {
            MarkdownLexeme entity;
            if (_referenceDecoder(_text, first, entity))
            {
                _cursor = entity.source.last;
                Append(Kind::Text, first, _cursor, std::wstring_view(entity.value.data(), entity.valueLength));
                return;
            }
        }
        if (c == L'\n')
        {
            auto start = first;
            while (start && (_text[start - 1u] == L' ' || _text[start - 1u] == L'\t'))
                --start;
            auto spaces = first;
            while (spaces && _text[spaces - 1u] == L' ')
                --spaces;
            if (_tail != MarkdownNoNode && _nodes[_tail].kind == Kind::Text && _nodes[_tail].source.last > _offsets[start])
            {
                auto& node      = _nodes[_tail];
                const auto trim = node.source.last - std::max(node.source.first, _offsets[start]);
                node.value.resize(node.value.size() - trim);
                node.source.last -= trim;
                if (node.value.empty())
                    Remove(_tail);
            }
            ++_cursor;
            Append(first - spaces >= 2u ? Kind::HardBreak : Kind::SoftBreak, start, _cursor);
            return;
        }
        const bool wikiImage = c == L'!' && first + 2u < _text.size() && _text[first + 1u] == L'[' && _text[first + 2u] == L'[';
        const bool wikiLink  = c == L'[' && first + 1u < _text.size() && _text[first + 1u] == L'[';
        if ((wikiImage || wikiLink) && HasMarkdownExtension(_extensions, MarkdownExtensions::WikiLinks))
        {
            const auto body = first + (wikiImage ? 3u : 2u);
            if (_wikiClose != MarkdownNoNode && _wikiClose < body)
                _wikiClose = MarkdownNoNode;
            const auto limit = (std::min)(_text.size(), body + (std::min)(std::size_t{4098u}, _text.size() - body));
            if (_wikiClose == MarkdownNoNode && _wikiScanned < limit)
            {
                const auto start = (std::max)(body, _wikiScanned ? _wikiScanned - 1u : 0u);
                auto found       = MarkdownNoNode;
                for (auto at = start; at + 1u < limit; ++at)
                {
                    ++_wikiInspectionUnits;
                    if (_text[at] == L']' && _text[at + 1u] == L']')
                    {
                        found = at;
                        break;
                    }
                }
                if (found != MarkdownNoNode)
                {
                    _wikiClose   = found;
                    _wikiScanned = _wikiClose + 2u;
                }
                else
                    _wikiScanned = limit;
            }
            if (_wikiClose != MarkdownNoNode && _wikiClose >= body && _wikiClose - body <= 4096u)
            {
                auto separator = _wikiClose;
                bool valid     = body < _wikiClose;
                for (auto at = body; valid && at < _wikiClose; ++at)
                {
                    ++_wikiInspectionUnits;
                    const auto value = _text[at];
                    if (value == L'[' || value == L']' || value == L'\\' || value == L'`' || value == L'<' || value < 0x20u || value == 0x7fu)
                        valid = false;
                    else if (value == L'|')
                    {
                        if (separator != _wikiClose)
                            valid = false;
                        else
                            separator = at;
                    }
                }
                const auto labelFirst = separator == _wikiClose ? body : separator + 1u;
                valid                 = valid && separator > body && labelFirst < _wikiClose && separator - body <= 2048u && _wikiClose - labelFirst <= 2048u;
                if (valid && _sourceRegion && _sourceRegion->kind == MarkdownSourceRegionKind::Inline)
                {
                    const auto envelope = Range(first, _wikiClose + 2u);
                    valid               = envelope.last <= _sourceRegion->source.first || envelope.first >= _sourceRegion->source.last;
                }
                if (valid)
                {
                    const auto owner = Append(wikiImage ? Kind::Image : Kind::Link, first, _wikiClose + 2u, {}, false);
                    if (owner == MarkdownNoNode)
                        return;
                    _nodes[owner].wiki = true;
                    _nodes[owner].destination.assign(_text.data() + body, separator - body);
                    const auto child = Append(Kind::Text, labelFirst, _wikiClose, std::wstring_view(_text).substr(labelFirst, _wikiClose - labelFirst), false);
                    if (child == MarkdownNoNode)
                        return;
                    Remove(child);
                    _previous[child]         = MarkdownNoNode;
                    _nodes[owner].firstChild = child;
                    _cursor                  = _wikiClose + 2u;
                    if (! wikiImage)
                        for (auto& earlier : _brackets)
                            if (! earlier.image)
                                earlier.active = false;
                    return;
                }
            }
        }
        if (c == L'[' || (c == L'!' && first + 1u < _text.size() && _text[first + 1u] == L'['))
        {
            const bool image = c == L'!';
            _cursor += image ? 2u : 1u;
            const auto node = Append(Kind::Text, first, _cursor, std::wstring_view(_text).substr(first, _cursor - first), false);
            if (node == MarkdownNoNode)
                return;
            _brackets.push_back({node, first, _delimiterTail, image});
            return;
        }
        if (c == L']' && CloseBracket())
            return;
        if (c == L'*' || c == L'_' ||
            (c == L'~' &&
             (HasMarkdownExtension(_extensions, MarkdownExtensions::Strikethrough) || HasMarkdownExtension(_extensions, MarkdownExtensions::Subscript))) ||
            (c == L'^' && HasMarkdownExtension(_extensions, MarkdownExtensions::Superscript)) ||
            (c == L'=' && HasMarkdownExtension(_extensions, MarkdownExtensions::Highlight)) ||
            (c == L'+' && HasMarkdownExtension(_extensions, MarkdownExtensions::Inserted)))
        {
            while (_cursor < _text.size() && _text[_cursor] == c)
                ++_cursor;
            const auto length = _cursor - first;
            const auto node   = Append(Kind::Text, first, _cursor, std::wstring_view(_text).substr(first, length), false);
            if (node == MarkdownNoNode)
                return;
            if ((c == L'~' && ! ((length == 2u && HasMarkdownExtension(_extensions, MarkdownExtensions::Strikethrough)) ||
                                 (length == 1u && HasMarkdownExtension(_extensions, MarkdownExtensions::Subscript)))) ||
                (c == L'^' && length != 1u) || ((c == L'=' || c == L'+') && length != 2u))
                return;
            const auto before = first ? Scalar(_text, StepToPreviousCodePoint(_text, first)) : 0u;
            const auto after  = _cursor < _text.size() ? Scalar(_text, _cursor) : 0u;
            const bool left   = ! White(after) && (! Punctuation(after) || White(before) || Punctuation(before));
            const bool right  = ! White(before) && (! Punctuation(before) || White(after) || Punctuation(after));
            const bool open   = c == L'_' ? left && (! right || Punctuation(before)) : left;
            const bool close  = c == L'_' ? right && (! left || Punctuation(after)) : right;
            const auto index  = _delimiters.size();
            _delimiters.push_back({node, length, _delimiterTail, MarkdownNoNode, c, open, close, length});
            if (_delimiterTail != MarkdownNoNode)
                _delimiters[_delimiterTail].next = index;
            else
                _delimiterHead = index;
            _delimiterTail = index;
            return;
        }
        _cursor = StepToNextCodePoint(_text, first);
        Append(Kind::Text, first, _cursor, std::wstring_view(_text).substr(first, _cursor - first));
    }

    bool ResolveFootnoteStep()
    {
        if (! _footnoteReferencesFinal)
        {
            if (_footnoteReferenceCursor < _footnoteReferences.size())
            {
                auto reference       = _footnoteReferences[_footnoteReferenceCursor++];
                reference.inlineNode = _previous[reference.inlineNode];
                if (reference.inlineNode != MarkdownNoNode)
                    _footnoteReferences[_footnoteReferenceCount++] = reference;
                return false;
            }
            _footnoteReferences.resize(_footnoteReferenceCount);
            std::sort(_footnoteReferences.begin(), _footnoteReferences.end(), [](const auto& a, const auto& b) { return a.inlineNode < b.inlineNode; });
            _footnoteReferencesFinal = true;
            return false;
        }
        if (_footnotes.empty())
            return true;
        if (! _footnoteWalkStarted)
        {
            _footnoteWalkStarted = true;
            _footnoteWalk.push_back({0u, false, false, false});
            return false;
        }
        if (! _footnoteWalk.empty())
        {
            const auto visit = _footnoteWalk.back();
            _footnoteWalk.pop_back();
            if (visit.inlineNode)
            {
                auto& node = _nodes[visit.node];
                if (node.nextSibling != MarkdownNoNode)
                    _footnoteWalk.push_back({node.nextSibling, true});
                if (node.kind == Kind::FootnoteReference)
                {
                    const auto reference =
                        std::lower_bound(_footnoteReferences.begin(), _footnoteReferences.end(), visit.node, [](const auto& binding, auto wanted) {
                        return binding.inlineNode < wanted;
                    });
                    if (reference == _footnoteReferences.end() || reference->inlineNode != visit.node)
                        DxUi::Failure::FailContract(E_UNEXPECTED);
                    if (reference->footnote != MarkdownNoNode)
                    {
                        auto& note = _footnotes[reference->footnote];
                        if (! note.ordinal)
                        {
                            note.ordinal = ++_footnoteOrdinal;
                            _reachableDefinitions.push_back(note.definition);
                        }
                        note.references.push_back(visit.node);
                        reference->occurrence = note.references.size();
                    }
                }
                if (node.firstChild != MarkdownNoNode)
                    _footnoteWalk.push_back({node.firstChild, true});
            }
            else
            {
                const auto& block = _blocks[visit.node];
                if (visit.followSibling && block.nextSibling != MarkdownNoNode)
                    _footnoteWalk.push_back({block.nextSibling});
                // Main content never gets numbered by a definition's original
                // source position. Only a newly reachable primary body enters
                // the queue, once, so chains and cycles have a finite traversal.
                if (block.block.kind == MarkdownBlockKind::FootnoteDefinition && ! visit.definitionRoot)
                    return false;
                if (block.firstChild != MarkdownNoNode)
                    _footnoteWalk.push_back({block.firstChild});
                if (block.firstInline != MarkdownNoNode)
                    _footnoteWalk.push_back({block.firstInline, true});
            }
            return false;
        }
        if (_footnoteQueueHead < _reachableDefinitions.size())
        {
            _footnoteWalk.push_back({_reachableDefinitions[_footnoteQueueHead++], false, true, false});
            return false;
        }
        return true;
    }

public:
    bool CapacityExceeded() const noexcept
    {
        return _capacityExceeded;
    }
    MarkdownInlines(std::wstring_view source,
                    std::wstring_view rawSource,
                    std::optional<MarkdownSourceRegion> sourceRegion,
                    MarkdownProfile profile,
                    MarkdownExtensions extensions,
                    std::vector<MarkdownNode>& blocks,
                    std::vector<MarkdownInline>& inlines,
                    std::vector<MarkdownFootnote>& footnotes,
                    std::vector<MarkdownFootnoteReference>& footnoteReferences,
                    std::vector<MarkdownDiagnostic>& diagnostics,
                    ReferenceDecoder decoder)
        : _source(source),
          _rawSource(rawSource),
          _sourceRegion(sourceRegion),
          _profile(profile),
          _extensions(extensions),
          _blocks(blocks),
          _nodes(inlines),
          _footnotes(footnotes),
          _footnoteReferences(footnoteReferences),
          _diagnostics(diagnostics),
          _referenceDecoder(decoder),
          _references(0u),
          _delimiters(0u),
          _brackets(0u),
          _autolinkWalk(0u),
          _autolinkFacts(0u),
          _footnoteWalk(0u),
          _reachableDefinitions(0u),
          _previous(0u),
          _offsets(0u),
          _backtickNext(0u),
          _htmlNext(0u),
          _text(0u, L' '),
          _effectiveReferences(0u),
          _leafStaging(0u),
          _copyStack(0u)
    {
    }
    // W7.2: the previous parse this one may copy unchanged leaves from. Call before the first Step.
    void EnableReuse(const MarkdownInlineReuse& reuse) noexcept
    {
        _reuse = reuse;
    }
    // Carried to the next parse with the publication (swapped out; the parser holds no further use for them).
    void TakeReuseState(std::vector<MarkdownReferenceValue>& references, std::vector<std::uint32_t>& leafStaging) noexcept
    {
        references.swap(_effectiveReferences);
        leafStaging.swap(_leafStaging);
    }
    [[nodiscard]] std::size_t AutolinkInspectionUnits() const noexcept
    {
        return _autolinkInspectionUnits;
    }
    [[nodiscard]] std::size_t AutolinkDecodedUnits() const noexcept
    {
        return _autolinkDecodedUnits;
    }
    [[nodiscard]] std::size_t WikiInspectionUnits() const noexcept
    {
        return _wikiInspectionUnits;
    }
    bool Step()
    {
        if (! _referencesComplete)
        {
            if (_referenceCursor < _blocks.size())
                CollectReference();
            else if (! _referencesSorted)
            {
                std::sort(_references.begin(), _references.end(), [](const auto& a, const auto& b) {
                    return a.label != b.label ? a.label < b.label : a.source < b.source;
                });
                std::sort(_footnotes.begin(),
                          _footnotes.end(),
                          [&](const auto& a, const auto& b)
                {
                    if (a.label != b.label)
                        return a.label < b.label;
                    const auto first = _blocks[a.definition].block.source.first, second = _blocks[b.definition].block.source.first;
                    return first != second ? first < second : a.definition < b.definition;
                });
                _referencesSorted = true;
            }
            else if (_footnoteUniqueCursor < _footnotes.size())
            {
                const auto index = _footnoteUniqueCursor++;
                const auto& note = _footnotes[index];
                if (_footnoteUniqueCount && _footnotes[_footnoteUniqueCount - 1u].label == note.label)
                    _diagnostics.push_back({MarkdownDiagnosticCode::DuplicateFootnoteDefinition, _blocks[note.definition].label, note.definition});
                else
                {
                    if (index != _footnoteUniqueCount)
                        _footnotes[_footnoteUniqueCount] = note;
                    ++_footnoteUniqueCount;
                }
            }
            else
            {
                _footnotes.resize(_footnoteUniqueCount);
                DecideReuse();
                _referencesComplete = true;
            }
            return false;
        }
        if (_leafActive)
        {
            if (_autolinksActive)
            {
                if (_autolinkFactsPhase < 3u)
                {
                    PrepareAutolinkFactsUnit();
                    return false;
                }
                if (_autolinkRegion || ! _autolinkWalk.empty())
                {
                    const auto visit = _autolinkRegion ? _autolinkRegion->visit : _autolinkWalk.back();
                    if (! _autolinkRegion)
                        _autolinkWalk.pop_back();
                    auto next        = _nodes[visit.node].nextSibling;
                    const auto child = _nodes[visit.node].firstChild;
                    const auto kind  = _nodes[visit.node].kind;
                    if (kind == Kind::Text)
                    {
                        const auto resolved = ResolveAutolinks(visit);
                        if (! resolved)
                            return false;
                        next = resolved.value();
                    }
                    if (next != MarkdownNoNode)
                        _autolinkWalk.push_back({next, visit.parent});
                    if (child != MarkdownNoNode && (kind == Kind::Emphasis || kind == Kind::Strong || kind == Kind::Strikethrough || kind == Kind::Highlight ||
                                                    kind == Kind::Superscript || kind == Kind::Subscript || kind == Kind::Inserted))
                        _autolinkWalk.push_back({child, visit.node});
                }
                else
                {
                    _root = _blocks[_blockCursor].firstInline;
                    _tail = _root;
                    if (_tail != MarkdownNoNode)
                        while (_nodes[_tail].nextSibling != MarkdownNoNode)
                            _tail = _nodes[_tail].nextSibling;
                    Emphasis(MarkdownNoNode);
                    _blocks[_blockCursor].firstInline = _root;
                    FinishLeafStaging(Staged() - _leafStart);
                    ++_blockCursor;
                    _leafActive = _autolinksActive = false;
                }
            }
            else if (! _sourceRegionEmitted && _sourceRegion && _sourceRegion->kind == MarkdownSourceRegionKind::Inline &&
                     _offsets[_cursor] == _sourceRegion->source.first && _sourceRegion->source.last <= _offsets.back())
            {
                const auto first = _cursor;
                _cursor          = static_cast<std::size_t>(
                    std::lower_bound(_offsets.begin() + static_cast<std::ptrdiff_t>(_cursor), _offsets.end(), _sourceRegion->source.last) - _offsets.begin());
                // A zero-width source owner has a synthetic U+FFFC boundary in
                // this private scan buffer. Consume that sentinel here; if it
                // is scanned again as text it aliases the next real source
                // unit and can detach or corrupt the empty editable owner.
                if (_sourceRegion->source.first == _sourceRegion->source.last && _cursor == first && _cursor < _text.size() && _text[_cursor] == L'\ufffc')
                    ++_cursor;
                const auto index = Append(Kind::SourceEditor,
                                          first,
                                          _cursor,
                                          _rawSource.substr(_sourceRegion->source.first, _sourceRegion->source.last - _sourceRegion->source.first));
                if (index == MarkdownNoNode)
                    return false;
                _nodes[index].source = _sourceRegion->source;
                _sourceRegionEmitted = true;
            }
            else if (_cursor < _text.size())
                Scan();
            else
            {
                _blocks[_blockCursor].firstInline = _root;
                if (HasMarkdownExtension(_extensions, MarkdownExtensions::Autolinks))
                {
                    _autolinksActive    = true;
                    _autolinkFactsPhase = 0u;
                    _autolinkRegion.reset();
                    _autolinkWalk.clear();
                    _autolinkDelimiter = _delimiterHead;
                    _underscoreOpen    = 0u;
                    if (_root != MarkdownNoNode)
                        _autolinkWalk.push_back({_root, MarkdownNoNode});
                }
                else
                {
                    Emphasis(MarkdownNoNode);
                    _blocks[_blockCursor].firstInline = _root;
                    FinishLeafStaging(Staged() - _leafStart);
                    ++_blockCursor;
                    _leafActive = false;
                }
            }
            return false;
        }
        if (_blockCursor == _blocks.size())
        {
            if (! _compactComplete)
            {
                _compactComplete = Compact();
                return false;
            }
            return ResolveFootnoteStep();
        }
        const auto kind = _blocks[_blockCursor].block.kind;
        if (kind == MarkdownBlockKind::HtmlBlock || kind == MarkdownBlockKind::Paragraph || kind == MarkdownBlockKind::AtxHeading ||
            kind == MarkdownBlockKind::SetextHeading || kind == MarkdownBlockKind::TableCell)
            if (const auto old = ImportedOldBlock(); old != MarkdownNoNode && CopyLeaf(old))
                return false;
        if (kind == MarkdownBlockKind::HtmlBlock)
        {
            OpaqueHtmlBlock();
            return false;
        }
        if (kind == MarkdownBlockKind::Paragraph || kind == MarkdownBlockKind::AtxHeading || kind == MarkdownBlockKind::SetextHeading ||
            kind == MarkdownBlockKind::TableCell)
            StartLeaf();
        else
            ++_blockCursor;
        return false;
    }
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
