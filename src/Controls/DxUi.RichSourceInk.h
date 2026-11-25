#pragma once
#include <DxUi/Failure.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <exception>
#include <memory>
#include <new>
#include <utility>

namespace DxUi::RichText::Detail
{
// Presentation of admitted syntax only. Each Advance shares one 256-step budget
// across node/marker walks, inline marker/link scans, merge ordering and event
// publication. No partial ranges escape; source/SDK geometry remains owned by
// the original projection. No parsing or allocation is performed by Draw.
class RichSourceInk final
{
public:
    RichSourceInk() : _events(0u), _mergeBuffer(0u), _ranges(0u)
    {
    }
    RichSourceInk(const RichSourceInk&)            = delete;
    RichSourceInk& operator=(const RichSourceInk&) = delete;
    void Clear() noexcept
    {
        ResetStaging();
        _ranges.clear();
        _publishedSyntax.reset();
        _publishedWindow = {};
        _published       = false;
    }
    void CancelPending() noexcept
    {
        ResetStaging();
    }
    void ResetStaging() noexcept
    {
        _syntax.reset();
        _window = {};
        _events.clear();
        _mergeBuffer.clear();
        if (_stagedRanges)
            _stagedRanges->clear();
        _node = _inlineIndex = _event = _previous = 0u;
        _counts                                   = {};
        _inlinePhase                              = InlinePhase::None;
        _childCursor = _scanPosition = _markerCount = _linkLast = _destinationFirst = _destinationPosition = _destinationDepth = 0u;
        _destinationAngle = _destinationEscaped = false;
        _sortWidth = _mergeStart = _mergeLeft = _mergeMiddle = _mergeRight = _mergeLeftCursor = _mergeRightCursor = _mergeOutput = 0u;
        _mergeRunActive = _mergePickReady = _mergePickLeft = false;
        _sorted = _complete = _overflow = false;
        _lastWork                       = 0u;
    }
    template <typename Map> bool Advance(const std::shared_ptr<const MarkdownSyntax>& syntax, Map&& map, std::optional<MarkdownRange> window = std::nullopt)
    {
        const Failure::Scope failureScope;
        bool complete{};
        const HRESULT result = TryAdvance(syntax, std::forward<Map>(map), complete, window);
        if (FAILED(result))
            Failure::FailContract(result);
        return complete;
    }
    template <typename Map>
    [[nodiscard]] HRESULT TryAdvance(const std::shared_ptr<const MarkdownSyntax>& syntax,
                                     Map&& map,
                                     bool& complete,
                                     std::optional<MarkdownRange> window = std::nullopt)
    {
        const Failure::Scope failureScope;
        const auto size = syntax ? syntax->Source().size() : 0u;
        if (window && (window->first > window->last || window->last > size))
            return E_INVALIDARG;
        _lastWork               = 0u;
        HRESULT mappingResult   = S_OK;
        const bool nextComplete = AdvanceCore(syntax, std::forward<Map>(map), window, mappingResult);
        if (FAILED(mappingResult))
        {
            ResetStaging();
            return mappingResult;
        }
        complete = nextComplete;
        return S_OK;
    }
    std::span<const TextInkRange> Ranges() const noexcept
    {
        return _ranges;
    }
    bool Pending() const noexcept
    {
        return _syntax && ! _complete;
    }
    bool Current(const std::shared_ptr<const MarkdownSyntax>& syntax) const noexcept
    {
        return syntax ? _published && _publishedSyntax.lock() == syntax : ! _published && ! _syntax;
    }
    bool Covers(std::size_t first, std::size_t last) const noexcept
    {
        return _published && first <= last && _publishedWindow.first <= first && _publishedWindow.last >= last;
    }
    size_t LastWork() const noexcept
    {
        return _lastWork;
    }
    static constexpr size_t WorkPerTurn = 256u;

private:
    enum class InlinePhase
    {
        None,
        MarkerRun,
        LinkChildren,
        LinkWhitespace,
        LinkBody,
        Finished
    };
    struct Event
    {
        size_t position;
        TextInkRole role;
        int delta;
    };
    static constexpr size_t MaximumEvents = 65536u;
    std::shared_ptr<const MarkdownSyntax> _syntax;
    MarkdownRange _window;
    // Publication retains its own ranges, but need not keep an old syntax tree
    // alive after its owner retires it while a replacement window is staging.
    std::weak_ptr<const MarkdownSyntax> _publishedSyntax;
    MarkdownRange _publishedWindow;
    std::vector<Event> _events;
    std::vector<Event> _mergeBuffer;
    std::vector<TextInkRange> _ranges;
    std::unique_ptr<std::vector<TextInkRange>> _stagedRanges;
    size_t _node{}, _inlineIndex{}, _event{}, _previous{};
    std::array<int, 3> _counts{};
    InlinePhase _inlinePhase{InlinePhase::None};
    size_t _childCursor{}, _scanPosition{}, _markerCount{}, _linkLast{}, _destinationFirst{}, _destinationPosition{}, _destinationDepth{};
    wchar_t _marker{};
    bool _destinationAngle{}, _destinationEscaped{};
    size_t _sortWidth{}, _mergeStart{}, _mergeLeft{}, _mergeMiddle{}, _mergeRight{}, _mergeLeftCursor{}, _mergeRightCursor{}, _mergeOutput{};
    bool _mergeRunActive{}, _mergePickReady{}, _mergePickLeft{};
    bool _sorted{}, _complete{}, _overflow{}, _published{};
    size_t _lastWork{};

    template <typename Map>
    bool AdvanceCore(const std::shared_ptr<const MarkdownSyntax>& syntax, Map&& map, std::optional<MarkdownRange> window, HRESULT& mappingResult)
    {
        const auto size    = syntax ? syntax->Source().size() : 0u;
        const auto desired = window.value_or(MarkdownRange{0u, size});
        if (! syntax)
        {
            Clear();
            return true;
        }
        if (_published && _publishedSyntax.lock() == syntax && _publishedWindow.first <= desired.first && _publishedWindow.last >= desired.last)
        {
            ResetStaging();
            return true;
        }
        if (_syntax != syntax || desired.first < _window.first || desired.last > _window.last)
        {
            ResetStaging();
            _syntax = syntax;
            _window = desired;
        }
        if (_complete)
            return true;
        if (! _stagedRanges)
            _stagedRanges = std::make_unique<std::vector<TextInkRange>>(0u);
        const auto& nodes   = _syntax->Nodes();
        const auto& inlines = _syntax->Inlines();
        while (_lastWork < WorkPerTurn)
        {
            if (_overflow)
            {
                // Capacity fallback is the exact ordinary source view, never a
                // partially colored/stale revision or a repeated failed request.
                _events.clear();
                _mergeBuffer.clear();
                _ranges.clear();
                _stagedRanges->clear();
                _publishedSyntax = _syntax;
                _publishedWindow = _window;
                _published       = true;
                _complete        = true;
                return true;
            }
            if (_node < nodes.size())
            {
                AddNode(nodes[_node++]);
                ++_lastWork;
                continue;
            }
            if (_inlineIndex < inlines.size())
            {
                const auto& item = inlines[_inlineIndex];
                if (_inlinePhase == InlinePhase::None)
                {
                    StartInline(item);
                    ++_lastWork;
                    continue;
                }
                if (_inlinePhase == InlinePhase::Finished)
                {
                    ++_inlineIndex;
                    _inlinePhase = InlinePhase::None;
                    continue;
                }
                AdvanceInline(item);
                continue;
            }
            if (! _sorted)
            {
                AdvanceMerge();
                continue;
            }
            if (_event < _events.size())
            {
                const auto next = _events[_event].position;
                for (size_t role = _counts.size(); role > 0u; --role)
                    if (_counts[role - 1u] != 0)
                    {
                        std::size_t first{}, last{};
                        if constexpr (requires { map(_previous, first); })
                        {
                            mappingResult = map(_previous, first);
                            if (FAILED(mappingResult))
                                return false;
                            mappingResult = map(next, last);
                            if (FAILED(mappingResult))
                                return false;
                        }
                        else
                        {
                            first = map(_previous);
                            last  = map(next);
                        }
                        Append({first, last}, static_cast<TextInkRole>(role - 1u));
                        break;
                    }
                _previous         = next;
                const auto& event = _events[_event++];
                _counts[static_cast<size_t>(event.role)] += event.delta;
                ++_lastWork;
                continue;
            }
            Publish();
            break;
        }
        if (_node == nodes.size() && _inlineIndex == inlines.size() && _sorted && _event == _events.size())
            if (! _complete)
                Publish();
        return _complete;
    }
    void Publish() noexcept
    {
        _ranges.swap(*_stagedRanges);
        _publishedSyntax = _syntax;
        _publishedWindow = _window;
        _published       = true;
        _complete        = true;
    }
    void Add(MarkdownRange range, TextInkRole role)
    {
        range.first = (std::max)(range.first, _window.first);
        range.last  = (std::min)(range.last, _window.last);
        if (range.first >= range.last || range.last > _syntax->Source().size() || _overflow)
            return;
        if (_events.size() > MaximumEvents - 2u)
        {
            _overflow = true;
            return;
        }
        _events.push_back({range.first, role, 1});
        _events.push_back({range.last, role, -1});
    }
    void Append(MarkdownRange range, TextInkRole role)
    {
        if (range.first == range.last)
            return;
        auto& ranges = *_stagedRanges;
        if (! ranges.empty() && ranges.back().role == role && ranges.back().display.last == range.first)
            ranges.back().display.last = range.last;
        else
            ranges.push_back({range, role});
    }
    void AddNode(const MarkdownNode& node)
    {
        const auto& block = node.block;
        if (block.source.last <= _window.first || block.source.first >= _window.last)
            return;
        switch (block.kind)
        {
            case MarkdownBlockKind::AtxHeading:
            case MarkdownBlockKind::SetextHeading:
                Add({block.source.first, block.content.first}, TextInkRole::Accent);
                Add({block.content.last, block.source.last}, TextInkRole::Accent);
                break;
            case MarkdownBlockKind::ListItem: Add(block.info, TextInkRole::Accent); break;
            case MarkdownBlockKind::BlockQuote:
            case MarkdownBlockKind::FencedCode:
            case MarkdownBlockKind::FrontMatter:
                Add({block.source.first, block.content.first}, TextInkRole::Subtle);
                Add({block.content.last, block.source.last}, TextInkRole::Subtle);
                break;
            case MarkdownBlockKind::ThematicBreak: Add(block.source, TextInkRole::Subtle); break;
            case MarkdownBlockKind::ReferenceDefinition:
                Add(block.source, TextInkRole::Subtle);
                Add(node.destination, TextInkRole::Link);
                break;
            case ::DxUi::RichText::MarkdownBlockKind::BlankLine:
            case ::DxUi::RichText::MarkdownBlockKind::Document:
            case ::DxUi::RichText::MarkdownBlockKind::FootnoteDefinition:
            case ::DxUi::RichText::MarkdownBlockKind::HtmlBlock:
            case ::DxUi::RichText::MarkdownBlockKind::IndentedCode:
            case ::DxUi::RichText::MarkdownBlockKind::List:
            case ::DxUi::RichText::MarkdownBlockKind::Paragraph:
            case ::DxUi::RichText::MarkdownBlockKind::SourceEditor:
            case ::DxUi::RichText::MarkdownBlockKind::Table:
            case ::DxUi::RichText::MarkdownBlockKind::TableCell:
            case ::DxUi::RichText::MarkdownBlockKind::TableRow:
            default: break;
        }
    }
    void StartInline(const MarkdownInline& item)
    {
        const auto source = _syntax->Source();
        const auto range  = item.source;
        if (range.first >= range.last || range.last > source.size() || range.last <= _window.first || range.first >= _window.last)
        {
            _inlinePhase = InlinePhase::Finished;
            return;
        }
        if (item.kind == MarkdownInlineKind::Math && range.last - range.first >= 2u && source[range.first] == L'\\' &&
            (source[range.first + 1u] == L'(' || source[range.first + 1u] == L'['))
        {
            Add({range.first, range.first + 2u}, TextInkRole::Subtle);
            const auto close = source[range.first + 1u] == L'(' ? L')' : L']';
            if (range.last - range.first >= 4u && source[range.last - 2u] == L'\\' && source[range.last - 1u] == close)
                Add({range.last - 2u, range.last}, TextInkRole::Subtle);
            _inlinePhase = InlinePhase::Finished;
            return;
        }
        switch (item.kind)
        {
            case MarkdownInlineKind::Code:
            case MarkdownInlineKind::Emphasis:
            case MarkdownInlineKind::Strong:
            case MarkdownInlineKind::Strikethrough:
            case MarkdownInlineKind::Highlight:
            case MarkdownInlineKind::Superscript:
            case MarkdownInlineKind::Subscript:
            case MarkdownInlineKind::Inserted:
            case MarkdownInlineKind::SourceEditor:
            case MarkdownInlineKind::Math:
                _marker       = source[range.first];
                _markerCount  = 0u;
                _scanPosition = range.first;
                _inlinePhase  = InlinePhase::MarkerRun;
                break;
            case MarkdownInlineKind::Link:
            case MarkdownInlineKind::Image:
                _linkLast    = std::min(range.last, range.first + (item.kind == MarkdownInlineKind::Image ? 2u : 1u));
                _childCursor = item.firstChild;
                _inlinePhase = InlinePhase::LinkChildren;
                break;
            case MarkdownInlineKind::Html:
                Add(range, TextInkRole::Subtle);
                _inlinePhase = InlinePhase::Finished;
                break;
            case ::DxUi::RichText::MarkdownInlineKind::FootnoteReference:
            case ::DxUi::RichText::MarkdownInlineKind::HardBreak:
            case ::DxUi::RichText::MarkdownInlineKind::SoftBreak:
            case ::DxUi::RichText::MarkdownInlineKind::Text:
            default: _inlinePhase = InlinePhase::Finished; break;
        }
    }
    void FinishMarkerRun(const MarkdownInline& item)
    {
        if (_markerCount != 0u)
        {
            Add({item.source.first, item.source.first + _markerCount}, TextInkRole::Subtle);
            if (item.source.last >= item.source.first + _markerCount * 2u)
                Add({item.source.last - _markerCount, item.source.last}, TextInkRole::Subtle);
        }
        _inlinePhase = InlinePhase::Finished;
    }
    void FinishLinkLabel(const MarkdownInline& item)
    {
        const auto range  = item.source;
        const auto source = _syntax->Source();
        if (item.kind == MarkdownInlineKind::Link && source[range.first] != L'[' && source[range.first] != L'<')
        {
            Add(range, TextInkRole::Link);
            _inlinePhase = InlinePhase::Finished;
            return;
        }
        _linkLast = std::min(_linkLast, range.last);
        Add({range.first, std::min(range.last, range.first + (item.kind == MarkdownInlineKind::Image ? 2u : 1u))}, TextInkRole::Subtle);
        Add({_linkLast, range.last}, TextInkRole::Subtle);
        if (_linkLast + 1u < range.last && source[_linkLast] == L']' && source[_linkLast + 1u] == L'(')
        {
            _destinationPosition = _linkLast + 2u;
            _inlinePhase         = InlinePhase::LinkWhitespace;
            return;
        }
        if (source[range.first] == L'<' && source[range.last - 1u] == L'>')
            Add({range.first + 1u, range.last - 1u}, TextInkRole::Link);
        _inlinePhase = InlinePhase::Finished;
    }
    void FinishLinkDestination(size_t end)
    {
        Add({_destinationFirst, end}, TextInkRole::Link);
        _inlinePhase = InlinePhase::Finished;
    }
    void AdvanceInline(const MarkdownInline& item)
    {
        const auto source = _syntax->Source();
        switch (_inlinePhase)
        {
            case InlinePhase::MarkerRun:
                if (_scanPosition < item.source.last && source[_scanPosition] == _marker)
                {
                    ++_scanPosition;
                    ++_markerCount;
                    ++_lastWork;
                }
                else
                    FinishMarkerRun(item);
                return;
            case InlinePhase::LinkChildren:
                if (_childCursor != MarkdownNoNode)
                {
                    const auto& child = _syntax->Inlines()[_childCursor];
                    _linkLast         = std::max(_linkLast, child.source.last);
                    _childCursor      = child.nextSibling;
                    ++_lastWork;
                }
                else
                    FinishLinkLabel(item);
                return;
            case InlinePhase::LinkWhitespace:
                if (_destinationPosition < item.source.last && (source[_destinationPosition] == L' ' || source[_destinationPosition] == L'\t' ||
                                                                source[_destinationPosition] == L'\r' || source[_destinationPosition] == L'\n'))
                {
                    ++_destinationPosition;
                    ++_lastWork;
                    return;
                }
                _destinationAngle    = _destinationPosition < item.source.last && source[_destinationPosition] == L'<';
                _destinationFirst    = _destinationPosition + (_destinationAngle ? 1u : 0u);
                _destinationPosition = _destinationFirst;
                _destinationDepth    = 0u;
                _destinationEscaped  = false;
                _inlinePhase         = InlinePhase::LinkBody;
                return;
            case InlinePhase::LinkBody:
                if (_destinationPosition >= item.source.last)
                {
                    FinishLinkDestination(_destinationPosition);
                    return;
                }
                {
                    const auto value = source[_destinationPosition];
                    ++_lastWork;
                    if (_destinationEscaped)
                        _destinationEscaped = false;
                    else if (value == L'\\' && _destinationPosition + 1u < item.source.last)
                        _destinationEscaped = true;
                    else if (_destinationAngle && value == L'>')
                    {
                        FinishLinkDestination(_destinationPosition);
                        return;
                    }
                    else if (! _destinationAngle)
                    {
                        if ((value == L')' && _destinationDepth == 0u) || value == L' ' || value == L'\t' || value == L'\r' || value == L'\n')
                        {
                            FinishLinkDestination(_destinationPosition);
                            return;
                        }
                        if (value == L'(')
                            ++_destinationDepth;
                        else if (value == L')' && _destinationDepth > 0u)
                            --_destinationDepth;
                    }
                    ++_destinationPosition;
                }
                return;
            case ::DxUi::RichText::Detail::RichSourceInk::InlinePhase::Finished:
            case ::DxUi::RichText::Detail::RichSourceInk::InlinePhase::None:
            default: _inlinePhase = InlinePhase::Finished; return;
        }
    }
    void AdvanceMerge()
    {
        const auto count = _events.size();
        if (count < 2u)
        {
            _sorted = true;
            return;
        }
        if (_sortWidth == 0u)
        {
            _mergeBuffer.reserve(count);
            _sortWidth = 1u;
        }
        if (! _mergeRunActive)
        {
            if (_mergeStart >= count)
            {
                _events.swap(_mergeBuffer);
                if (_sortWidth >= count - _sortWidth)
                {
                    _sorted = true;
                    return;
                }
                _sortWidth *= 2u;
                _mergeStart = 0u;
                _mergeBuffer.clear();
                return;
            }
            _mergeLeft        = _mergeStart;
            _mergeMiddle      = std::min(count, _mergeLeft + _sortWidth);
            _mergeRight       = std::min(count, _mergeMiddle + _sortWidth);
            _mergeLeftCursor  = _mergeLeft;
            _mergeRightCursor = _mergeMiddle;
            _mergeOutput      = _mergeLeft;
            _mergeRunActive   = true;
        }
        if (_mergePickReady)
        {
            _mergeBuffer.push_back(_events[_mergePickLeft ? _mergeLeftCursor++ : _mergeRightCursor++]);
            ++_mergeOutput;
            _mergePickReady = false;
            ++_lastWork;
        }
        else if (_mergeLeftCursor < _mergeMiddle && _mergeRightCursor < _mergeRight)
        {
            _mergePickLeft  = _events[_mergeLeftCursor].position <= _events[_mergeRightCursor].position;
            _mergePickReady = true;
            ++_lastWork;
        }
        else if (_mergeLeftCursor < _mergeMiddle || _mergeRightCursor < _mergeRight)
        {
            const bool left = _mergeLeftCursor < _mergeMiddle;
            _mergeBuffer.push_back(_events[left ? _mergeLeftCursor++ : _mergeRightCursor++]);
            ++_mergeOutput;
            ++_lastWork;
        }
        if (_mergeOutput == _mergeRight)
        {
            _mergeStart     = _mergeRight;
            _mergeRunActive = false;
        }
    }
};
} // namespace DxUi::RichText::Detail
