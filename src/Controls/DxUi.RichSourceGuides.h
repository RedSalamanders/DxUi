#pragma once
#include "DxUi.PreparedLayoutAccess.h"
#include "DxUi.RichEditorLayout.h"
#include <DxUi/RichDocument.h>
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
// Source-only decorations. Work is staged privately; every advance consumes at
// most four DirectWrite layouts and 256 source units. Draw only borrows the
// finished immutable number layouts and mark coordinates.
class RichSourceGuides final
{
public:
    struct Window
    {
        std::size_t first{}, last{};
        float top{}, bottom{}, left{}, right{};
        bool operator==(const Window&) const = default;
    };
    struct Number
    {
        Number() noexcept = default;
        Number(std::unique_ptr<PreparedLayout> value, float position) noexcept : layout(std::move(value)), y(position)
        {
        }
        Number(Number&&) noexcept            = default;
        Number& operator=(Number&&) noexcept = default;
        Number(const Number&)                = delete;
        Number& operator=(const Number&)     = delete;
        std::unique_ptr<PreparedLayout> layout;
        float y{};
    };
    struct Mark
    {
        float x{}, y{}, width{}, height{};
        wchar_t kind{};
    };
    RichSourceGuides() : _numbers(0u), _marks(0u)
    {
    }
    RichSourceGuides(const RichSourceGuides&)            = delete;
    RichSourceGuides& operator=(const RichSourceGuides&) = delete;
    RichSourceGuides(RichSourceGuides&&)                 = delete;
    RichSourceGuides& operator=(RichSourceGuides&&)      = delete;
    void Clear() noexcept
    {
        _next.reset();
        _numbers.clear();
        _marks.clear();
        _revision = _generation = 0u;
        _published              = false;
    }
    void CancelPending() noexcept
    {
        _next.reset();
    }
    bool HasPublication(std::uint64_t generation, bool numbers, bool marks) const noexcept
    {
        // Old viewport decoration remains valid while scrolling. A replacement
        // geometry makes its positions unknown, so omit it until publication.
        return _published && _generation == generation && _numbersEnabled == numbers && _marksEnabled == marks;
    }
    bool Covers(float top, float bottom, float left, float right) const noexcept
    {
        return _published && _window.top <= top && _window.bottom >= bottom && _window.left <= left && _window.right >= right;
    }
    bool Current(std::uint64_t revision, std::uint64_t generation, bool numbers, bool marks) const noexcept
    {
        return ! _next && _revision == revision && _generation == generation && _numbersEnabled == numbers && _marksEnabled == marks;
    }
    bool Pending() const noexcept
    {
        return static_cast<bool>(_next);
    }
    std::size_t LastWork() const noexcept
    {
        return _lastWork;
    }
    std::size_t LastLayouts() const noexcept
    {
        return _lastLayouts;
    }
    void Begin(const SourceSnapshot& source,
               std::uint64_t revision,
               std::uint64_t generation,
               bool numbers,
               bool marks,
               const LayoutOptions& options,
               float gutterWidth,
               std::optional<Window> window = std::nullopt)
    {
        const Failure::Scope failureScope;
        auto desired = window.value_or(
            Window{0u, source.Length(), 0.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()});
        desired.first = (std::min)(desired.first, source.Length());
        desired.last  = (std::clamp)(desired.last, desired.first, source.Length());
        while (desired.first && ! source.IsBoundary(desired.first))
            --desired.first;
        while (desired.last < source.Length() && ! source.IsBoundary(desired.last))
            ++desired.last;
        if (_next && _next->revision == revision && _next->generation == generation && _next->numbers == numbers && _next->marks == marks &&
            _next->window == desired)
            return;
        auto staged                   = std::make_unique<Stage>();
        staged->source                = source;
        staged->window                = desired;
        staged->cursor                = desired.first;
        const auto location           = source.LocationAt(desired.first).value_or(SourceLocation{});
        staged->line                  = location.line + 1u;
        staged->numberAt              = desired.first - location.column;
        staged->revision              = revision;
        staged->generation            = generation;
        staged->numbers               = numbers;
        staged->marks                 = marks;
        staged->options               = options;
        staged->options.widthDip      = std::max(1.0f, gutterWidth - 8.0f);
        staged->options.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        staged->options.wrap          = false;
        _next.swap(staged);
    }
    HRESULT Advance(IDWriteFactory* factory, const RichEditorLayout& geometry)
    {
        const Failure::Scope failureScope;
        _lastWork = _lastLayouts = 0u;
        if (! _next)
            return S_OK;
        auto& stage   = *_next;
        auto& work    = _lastWork;
        auto& layouts = _lastLayouts;
        while (work < 256u)
        {
            if (stage.numberNeeded)
            {
                if (stage.numbers)
                {
                    CaretGeometry caret{};
                    RETURN_IF_FAILED(geometry.TrySourceCaret(stage.numberAt, MarkdownNoNode, caret));
                    if (caret.y + caret.height > stage.window.top && caret.y < stage.window.bottom)
                    {
                        if (layouts == 4u)
                            return S_FALSE;
                        Document number{std::to_wstring(stage.line)};
                        auto projection = Project(number.Snapshot(), number.Revision(), ViewMode::Source);
                        auto label      = std::make_unique<PreparedLayout>();
                        // A by-value call would invoke Projection's implicit
                        // noexcept move, whose Debug STL proxies may allocate.
                        RETURN_IF_FAILED(PreparedLayoutAccess::Prepare(*label, factory, projection, stage.options));
                        stage.stagedNumbers.emplace_back(std::move(label), caret.y);
                        ++layouts;
                    }
                }
                stage.numberNeeded = false;
                ++work;
                continue;
            }
            if (stage.cursor == stage.window.last)
            {
                _numbers.swap(stage.stagedNumbers);
                _marks.swap(stage.stagedMarks);
                _revision       = stage.revision;
                _generation     = stage.generation;
                _numbersEnabled = stage.numbers;
                _marksEnabled   = stage.marks;
                _window         = stage.window;
                _published      = true;
                _next.reset();
                return S_OK;
            }
            const auto required = (std::min)(std::size_t{2u}, stage.window.last - stage.cursor);
            if (stage.units.empty() || stage.cursor < stage.unitsFirst || stage.cursor + required > stage.unitsFirst + stage.units.size())
            {
                auto last = stage.cursor + (std::min)(std::size_t{258u}, stage.window.last - stage.cursor);
                while (last < stage.source.Length() && ! stage.source.IsBoundary(last))
                    ++last;
                std::wstring units(0u, L' ');
                const auto slice = stage.source.TrySlice(stage.cursor, last, units);
                if (slice != SourceResult::Success)
                    return slice == SourceResult::CapacityExceeded ? E_BOUNDS : E_INVALIDARG;
                stage.units.swap(units);
                stage.unitsFirst = stage.cursor;
            }
            const auto local = stage.cursor - stage.unitsFirst;
            const auto ch    = stage.units[local];
            const auto first = stage.cursor;
            const bool eol   = ch == L'\r' || ch == L'\n';
            const auto consumed =
                (ch == L'\r' && first + 1u < stage.window.last && stage.units[local + 1u] == L'\n') || (ch >= 0xd800 && ch <= 0xdbff) ? 2u : 1u;
            if (consumed > 256u - work)
                return S_FALSE;
            const auto nextCursor = first + consumed;
            if (stage.marks && (ch == L' ' || ch == L'\t' || eol))
            {
                CaretGeometry caret{}, next{};
                RETURN_IF_FAILED(geometry.TrySourceCaret(first, MarkdownNoNode, caret));
                RETURN_IF_FAILED(geometry.TrySourceCaret(nextCursor, MarkdownNoNode, next));
                const auto width = ! eol && next.y == caret.y ? std::abs(next.x - caret.x) : 0.0f;
                if (caret.y + caret.height > stage.window.top && caret.y < stage.window.bottom && caret.x >= stage.window.left - 8.0f &&
                    caret.x <= stage.window.right + 8.0f)
                    stage.stagedMarks.push_back({caret.x, caret.y, width, caret.height, eol ? L'\n' : ch});
            }
            stage.cursor = nextCursor; // No fallible work follows this source unit's publication.
            if (eol)
            {
                ++stage.line;
                stage.numberNeeded = true;
                stage.numberAt     = nextCursor;
            }
            work += consumed;
        }
        return S_FALSE;
    }
    std::span<const Number> Numbers() const noexcept
    {
        return _numbers;
    }
    std::span<const Mark> Marks() const noexcept
    {
        return _marks;
    }

private:
    struct Stage
    {
        Stage() : units(0u, L' '), stagedNumbers(0u), stagedMarks(0u)
        {
        }
        SourceSnapshot source;
        std::wstring units;
        Window window;
        LayoutOptions options;
        std::vector<Number> stagedNumbers;
        std::vector<Mark> stagedMarks;
        std::uint64_t revision{}, generation{};
        std::size_t cursor{}, numberAt{}, unitsFirst{}, line = 1u;
        bool numbers{}, marks{}, numberNeeded = true;
    };
    std::unique_ptr<Stage> _next;
    std::vector<Number> _numbers;
    std::vector<Mark> _marks;
    std::uint64_t _revision{}, _generation{};
    bool _numbersEnabled{}, _marksEnabled{};
    bool _published{};
    Window _window;
    std::size_t _lastWork{}, _lastLayouts{};
};
} // namespace DxUi::RichText::Detail
