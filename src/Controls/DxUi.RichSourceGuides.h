#pragma once
#include "DxUi.RichEditorLayout.h"
#include <DxUi/RichDocument.h>
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <memory>
#include <vector>

namespace DxUi::RichText::Detail
{
// Source-only decorations. Work is staged privately; every advance consumes at
// most four DirectWrite layouts and 256 source units. Draw only borrows the
// finished immutable number layouts and mark coordinates.
class RichSourceGuides final
{
public:
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
    }
    bool Current(std::uint64_t revision, std::uint64_t generation, bool numbers, bool marks) const noexcept
    {
        return ! next_ && revision_ == revision && generation_ == generation && numbersEnabled_ == numbers && marksEnabled_ == marks;
    }
    void Begin(const SourceSnapshot& source, std::uint64_t revision, std::uint64_t generation, bool numbers, bool marks, const LayoutOptions& options)
    {
        if (next_ && next_->revision == revision && next_->generation == generation && next_->numbers == numbers && next_->marks == marks)
            return;
        auto staged                   = std::make_unique<Stage>();
        staged->source                = source.Slice(0u, source.Length());
        staged->revision              = revision;
        staged->generation            = generation;
        staged->numbers               = numbers;
        staged->marks                 = marks;
        staged->options               = options;
        staged->options.widthDip      = 36.0f;
        staged->options.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        staged->options.wrap          = false;
        next_.swap(staged);
    }
    bool Advance(IDWriteFactory* factory, const RichEditorLayout& geometry)
    {
        if (! next_)
            return true;
        auto& stage = *next_;
        std::size_t work{}, layouts{};
        while (work < 256u)
        {
            if (stage.numberNeeded)
            {
                if (stage.numbers)
                {
                    if (layouts == 4u)
                        return false;
                    Document number{std::to_wstring(stage.line)};
                    auto projection = Project(number.Snapshot(), number.Revision(), ViewMode::Source);
                    auto label      = std::make_unique<PreparedLayout>();
                    label->Prepare(factory, std::move(projection), stage.options);
                    const float position = geometry.SourceCaret(stage.cursor).y;
                    stage.stagedNumbers.emplace_back(std::move(label), position);
                    ++layouts;
                }
                stage.numberNeeded = false;
                ++work;
                continue;
            }
            if (stage.cursor == stage.source.size())
            {
                numbers_.swap(stage.stagedNumbers);
                marks_.swap(stage.stagedMarks);
                revision_       = stage.revision;
                generation_     = stage.generation;
                numbersEnabled_ = stage.numbers;
                marksEnabled_   = stage.marks;
                next_.reset();
                return true;
            }
            const auto ch         = stage.source[stage.cursor];
            const auto first      = stage.cursor;
            const bool eol        = ch == L'\r' || ch == L'\n';
            const auto nextCursor = first + (ch == L'\r' && first + 1u < stage.source.size() && stage.source[first + 1u] == L'\n' ? 2u : 1u);
            if (stage.marks && (ch == L' ' || ch == L'\t' || eol))
            {
                const auto caret = geometry.SourceCaret(first);
                const auto next  = geometry.SourceCaret(nextCursor);
                const auto width = ! eol && next.y == caret.y ? std::abs(next.x - caret.x) : 0.0f;
                stage.stagedMarks.push_back({caret.x, caret.y, width, caret.height, eol ? L'\n' : ch});
            }
            stage.cursor = nextCursor; // No fallible work follows this source unit's publication.
            if (eol)
            {
                ++stage.line;
                stage.numberNeeded = true;
            }
            ++work;
        }
        return false;
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
        Stage() : source(0u, L' '), stagedNumbers(0u), stagedMarks(0u)
        {
        }
        std::wstring source;
        LayoutOptions options;
        std::vector<Number> stagedNumbers;
        std::vector<Mark> stagedMarks;
        std::uint64_t revision{}, generation{};
        std::size_t cursor{}, line = 1u;
        bool numbers{}, marks{}, numberNeeded = true;
    };
    std::unique_ptr<Stage> next_;
    std::vector<Number> numbers_;
    std::vector<Mark> marks_;
    std::uint64_t revision_{}, generation_{};
    bool numbersEnabled_{}, marksEnabled_{};
};
} // namespace DxUi::RichText::Detail
