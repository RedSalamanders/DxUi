#pragma once
#include <cstddef>
#include <utility>

namespace DxUi::RichText::Detail
{
// Private, opt-in logical-work accounting. These count searched raw source
// spans/loop iterations, not elapsed time or vectorized STL character loads.
// A null owner-thread sink adds no retained work state or public ABI fields.
struct MarkdownWorkCounts
{
    std::size_t htmlSearchedUnits{}, htmlIndexUnits{}, linkDestinationUnits{}, referenceFlattenUnits{};
};
inline thread_local MarkdownWorkCounts* markdownWorkSink{};
inline void AccountMarkdownWork(std::size_t MarkdownWorkCounts::* counter, std::size_t units = 1u) noexcept
{
    if (markdownWorkSink)
        markdownWorkSink->*counter += units;
}
class MarkdownWorkProbe
{
public:
    explicit MarkdownWorkProbe(MarkdownWorkCounts& counts) noexcept : previous_(std::exchange(markdownWorkSink, &counts))
    {
    }
    ~MarkdownWorkProbe()
    {
        markdownWorkSink = previous_;
    }
    MarkdownWorkProbe(const MarkdownWorkProbe&)            = delete;
    MarkdownWorkProbe& operator=(const MarkdownWorkProbe&) = delete;

private:
    MarkdownWorkCounts* previous_{};
};
} // namespace DxUi::RichText::Detail
