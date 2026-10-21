#pragma once
#include <DxUi/RichDocument.h>
#include <optional>
#include <stdexcept>

namespace DxUi::RichText::Detail
{
struct MarkdownSourceCorrelation
{
    std::size_t first{}, previousEnd{}, currentEnd{};
    bool discardPrevious{};
};
// Snapshot ownership/ancestry has already established the trusted metadata.
// Honor it before touching source units. String-only callers still need the
// exact legacy common-prefix/suffix comparison for node-identity correlation.
// This does not remove source materialization or other bounded staging scans.
template <typename PreviousUnit, typename CurrentUnit>
MarkdownSourceCorrelation CorrelateMarkdownSource(
    std::size_t previousLength, std::size_t currentLength, std::optional<SourceChangeRange> change, PreviousUnit&& previousUnit, CurrentUnit&& currentUnit)
{
    if (change)
    {
        if (change->kind == SourceChangeKind::TrackedEdits)
        {
            if (change->begin > change->previousEnd || change->begin > change->currentEnd || change->previousEnd > previousLength ||
                change->currentEnd > currentLength)
                throw std::invalid_argument("Invalid tracked source correlation interval");
            return {change->begin, change->previousEnd, change->currentEnd, false};
        }
        if (change->kind == SourceChangeKind::FullReplacement)
            return {0u, previousLength, currentLength, true};
        if (change->kind == SourceChangeKind::Unchanged)
        {
            if (previousLength != currentLength)
                throw std::invalid_argument("Unchanged source correlation requires equal lengths");
            return {previousLength, previousLength, currentLength, false};
        }
        throw std::invalid_argument("Unknown source correlation metadata");
    }
    MarkdownSourceCorrelation result{0u, previousLength, currentLength, false};
    while (result.first < previousLength && result.first < currentLength && previousUnit(result.first) == currentUnit(result.first))
        ++result.first;
    while (result.previousEnd > result.first && result.currentEnd > result.first &&
           previousUnit(result.previousEnd - 1u) == currentUnit(result.currentEnd - 1u))
    {
        --result.previousEnd;
        --result.currentEnd;
    }
    return result;
}
} // namespace DxUi::RichText::Detail
