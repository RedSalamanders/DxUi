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
    RichSourceGuides() : numbers_(0u), marks_(0u)
    {
    }
    RichSourceGuides(const RichSourceGuides&)            = delete;
    RichSourceGuides& operator=(const RichSourceGuides&) = delete;
    RichSourceGuides(RichSourceGuides&&)                 = delete;
    RichSourceGuides& operator=(RichSourceGuides&&)      = delete;
    void Clear() noexcept
    {
        next_.reset();
        numbers_.clear();
        marks_.clear();
        revision_ = generation_ = 0u;
        published_              = false;
    }
    void CancelPending() noexcept
    {
        next_.reset();
    }
    bool HasPublication(std::uint64_t generation, bool numbers, bool marks) const noexcept
    {
        // Old viewport decoration remains valid while scrolling. A replacement
        // geometry makes its positions unknown, so omit it until publication.
        return published_ && generation_ == generation && numbersEnabled_ == numbers && marksEnabled_ == marks;
    }
    bool Covers(float top, float bottom, float left, float right) const noexcept
    {
        return published_ && window_.top <= top && window_.bottom >= bottom && window_.left <= left && window_.right >= right;
    }
    bool Current(std::uint64_t revision, std::uint64_t generation, bool numbers, bool marks) const noexcept
    {
        return ! next_ && revision_ == revision && generation_ == generation && numbersEnabled_ == numbers && marksEnabled_ == marks;
    }
    bool Pending() const noexcept
    {
        return static_cast<bool>(next_);
    }
    std::size_t LastWork() const noexcept
    {
        return lastWork_;
    }
    std::size_t LastLayouts() const noexcept
    {
        return lastLayouts_;
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
        if (next_ && next_->revision == revision && next_->generation == generation && next_->numbers == numbers && next_->marks == marks &&
            next_->window == desired)
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
        next_.swap(staged);
    }
    HRESULT Advance(IDWriteFactory* factory, const RichEditorLayout& geometry)
    {
        const Failure::Scope failureScope;
        lastWork_ = lastLayouts_ = 0u;
        if (! next_)
            return S_OK;
        auto& stage   = *next_;
        auto& work    = lastWork_;
        auto& layouts = lastLayouts_;
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
                numbers_.swap(stage.stagedNumbers);
                marks_.swap(stage.stagedMarks);
                revision_       = stage.revision;
                generation_     = stage.generation;
                numbersEnabled_ = stage.numbers;
                marksEnabled_   = stage.marks;
                window_         = stage.window;
                published_      = true;
                next_.reset();
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
        return numbers_;
    }
    std::span<const Mark> Marks() const noexcept
    {
        return marks_;
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
    std::unique_ptr<Stage> next_;
    std::vector<Number> numbers_;
    std::vector<Mark> marks_;
    std::uint64_t revision_{}, generation_{};
    bool numbersEnabled_{}, marksEnabled_{};
    bool published_{};
    Window window_;
    std::size_t lastWork_{}, lastLayouts_{};
};
} // namespace DxUi::RichText::Detail
