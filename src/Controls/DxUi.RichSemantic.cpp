#include <DxUi/RichSemantic.h>
#include <limits>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
constexpr auto MissingAnchor = std::numeric_limits<std::size_t>::max();
bool Atomic(DisplaySpanKind kind) noexcept
{
    return kind == DisplaySpanKind::Transformed || kind == DisplaySpanKind::Object;
}
void CheckRange(std::wstring_view text, MarkdownRange range)
{
    if (range.first > range.last || ! IsEditBoundary(text, range.first) || ! IsEditBoundary(text, range.last))
        throw std::out_of_range("Invalid semantic coordinate range");
}
void CheckAffinity(MappingAffinity affinity)
{
    if (affinity != MappingAffinity::Leading && affinity != MappingAffinity::Trailing)
        throw std::invalid_argument("Invalid semantic mapping affinity");
}
void AppendRange(std::vector<MarkdownRange>& result, MarkdownRange range)
{
    if (range.first == range.last)
        return;
    if (! result.empty() && result.back().last >= range.first)
        result.back().last = std::max(result.back().last, range.last);
    else
        result.push_back(range);
}
} // namespace

std::shared_ptr<const SourceDisplayMap> SourceDisplayMap::Build(std::wstring_view source, std::wstring_view display, std::span<const DisplaySpan> spans)
{
    // Admission precedes copies and indexes, including multiplication/amplification.
    if (source.size() > Document::MaximumUnits || display.size() > MaximumDisplayUnits || spans.size() > MaximumSpans)
        throw std::length_error("Semantic coordinate capacity exceeded");
    if (! IsValidUtf16(source) || ! IsValidUtf16(display))
        throw std::invalid_argument("Invalid semantic UTF-16");
    return std::shared_ptr<const SourceDisplayMap>(new SourceDisplayMap(source, display, spans));
}
SourceDisplayMap::SourceDisplayMap(std::wstring_view source, std::wstring_view display, std::span<const DisplaySpan> spans)
    : source_(source),
      display_(display),
      spans_(spans.begin(), spans.end()),
      sourceLeading_(source.size() + 1u, MissingAnchor),
      sourceTrailing_(source.size() + 1u, MissingAnchor)
{
    std::vector<std::size_t> primary(0u);
    primary.reserve(spans.size());
    std::size_t displayEnd = 0u;
    for (std::size_t i = 0u; i < spans_.size(); ++i)
    {
        const auto& span = spans_[i];
        CheckRange(source, span.source);
        CheckRange(display, span.display);
        if (span.display.first != displayEnd || span.display.first == span.display.last)
            throw std::invalid_argument("Semantic spans must partition display");
        displayEnd = span.display.last;
        switch (span.kind)
        {
            case DisplaySpanKind::Generated:
                if (span.primary || span.source.first != span.source.last)
                    throw std::invalid_argument("Generated text has no source ownership");
                break;
            case DisplaySpanKind::Source:
                if (source.substr(span.source.first, span.source.last - span.source.first) !=
                    display.substr(span.display.first, span.display.last - span.display.first))
                    throw std::invalid_argument("Source-backed display spelling differs");
                [[fallthrough]];
            case DisplaySpanKind::Transformed:
            case DisplaySpanKind::Object:
                if (span.source.first == span.source.last)
                    throw std::invalid_argument("Source-bearing display has empty ownership");
                if (span.primary)
                    primary.push_back(i);
                break;
            default: throw std::invalid_argument("Invalid semantic span kind");
        }
    }
    if (displayEnd != display.size())
        throw std::invalid_argument("Semantic display coverage is incomplete");
    std::sort(primary.begin(), primary.end(), [&](auto a, auto b) { return spans_[a].source.first < spans_[b].source.first; });
    std::size_t sourceEnd = 0u;
    for (const auto index : primary)
    {
        const auto& span = spans_[index];
        if (span.source.first < sourceEnd)
            throw std::invalid_argument("Primary semantic source ownership overlaps");
        sourceEnd = span.source.last;
        for (auto position = span.source.first; position <= span.source.last; ++position)
        {
            const auto leading  = Atomic(span.kind) && position < span.source.last ? span.display.first : span.display.first + position - span.source.first;
            const auto trailing = Atomic(span.kind) && position > span.source.first ? span.display.last : span.display.first;
            // Source order gives the next primary ownership the leading side
            // of a shared boundary, and the previous ownership its trailing side.
            sourceLeading_[position] = Atomic(span.kind) && position == span.source.last ? span.display.last : leading;
            if (position != span.source.first || sourceTrailing_[position] == MissingAnchor)
                sourceTrailing_[position] = Atomic(span.kind) ? trailing : span.display.first + position - span.source.first;
        }
    }
    for (const auto& span : spans_)
    {
        if (span.primary || span.kind == DisplaySpanKind::Generated)
            continue;
        const auto match =
            std::lower_bound(primary.begin(), primary.end(), span.source.first, [&](auto index, auto first) { return spans_[index].source.first < first; });
        if (match == primary.end() || spans_[*match].source != span.source || spans_[*match].kind != span.kind)
            throw std::invalid_argument("Repeated semantic span needs an exact primary owner");
    }
    auto next = display.size();
    for (auto position = source.size() + 1u; position > 0u;)
    {
        --position;
        if (sourceLeading_[position] == MissingAnchor)
            sourceLeading_[position] = next;
        else
            next = sourceLeading_[position];
    }
    auto previous = std::size_t{0u};
    for (auto& anchor : sourceTrailing_)
    {
        if (anchor == MissingAnchor)
            anchor = previous;
        else
            previous = anchor;
    }
}
std::wstring_view SourceDisplayMap::Source() const noexcept
{
    return source_;
}
std::wstring_view SourceDisplayMap::Display() const noexcept
{
    return display_;
}
std::span<const DisplaySpan> SourceDisplayMap::Spans() const noexcept
{
    return spans_;
}
std::size_t SourceDisplayMap::ToDisplay(std::size_t sourceOffset, MappingAffinity affinity) const
{
    CheckAffinity(affinity);
    CheckRange(source_, {sourceOffset, sourceOffset});
    return (affinity == MappingAffinity::Leading ? sourceLeading_ : sourceTrailing_)[sourceOffset];
}
std::size_t SourceDisplayMap::ToSource(std::size_t displayOffset, MappingAffinity affinity) const
{
    CheckAffinity(affinity);
    CheckRange(display_, {displayOffset, displayOffset});
    if (spans_.empty())
        return affinity == MappingAffinity::Leading ? source_.size() : 0u;
    auto found = std::upper_bound(spans_.begin(), spans_.end(), displayOffset, [](auto offset, const auto& span) { return offset < span.display.last; });
    if (found == spans_.end())
        found = std::prev(spans_.end());
    else if (affinity == MappingAffinity::Trailing && displayOffset == found->display.first && found != spans_.begin())
        --found;
    if (found->kind == DisplaySpanKind::Generated)
        return found->source.first;
    if (Atomic(found->kind))
    {
        if (displayOffset == found->display.first)
            return found->source.first;
        if (displayOffset == found->display.last)
            return found->source.last;
        return affinity == MappingAffinity::Leading ? found->source.first : found->source.last;
    }
    return found->source.first + displayOffset - found->display.first;
}
std::vector<MarkdownRange> SourceDisplayMap::DisplayRanges(MarkdownRange source) const
{
    CheckRange(source_, source);
    std::vector<MarkdownRange> result(0u);
    if (source.first == source.last)
        return std::vector<MarkdownRange>(0u);
    for (const auto& span : spans_)
    {
        const auto first = std::max(source.first, span.source.first), last = std::min(source.last, span.source.last);
        if (span.kind == DisplaySpanKind::Generated || first >= last)
            continue;
        AppendRange(result,
                    Atomic(span.kind) ? span.display
                                      : MarkdownRange{span.display.first + first - span.source.first, span.display.first + last - span.source.first});
    }
    // Explicit fallible copy/prvalue elision avoids relying on NRVO: the MSVC
    // Debug vector move is noexcept but can allocate an iterator proxy.
    return std::vector<MarkdownRange>(result);
}
std::vector<MarkdownRange> SourceDisplayMap::SourceRanges(MarkdownRange display) const
{
    CheckRange(display_, display);
    std::vector<MarkdownRange> ranges(0u);
    if (display.first == display.last)
        return std::vector<MarkdownRange>(0u);
    for (const auto& span : spans_)
    {
        const auto first = std::max(display.first, span.display.first), last = std::min(display.last, span.display.last);
        if (span.kind == DisplaySpanKind::Generated || first >= last)
            continue;
        ranges.push_back(Atomic(span.kind) ? span.source
                                           : MarkdownRange{span.source.first + first - span.display.first, span.source.first + last - span.display.first});
    }
    std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<MarkdownRange> result(0u);
    result.reserve(ranges.size());
    for (const auto range : ranges)
        AppendRange(result, range);
    return std::vector<MarkdownRange>(result);
}
std::size_t SourceDisplayMap::SnapDisplayBoundary(std::size_t offset, MappingAffinity affinity) const
{
    CheckAffinity(affinity);
    if (offset > display_.size())
        throw std::out_of_range("Invalid display boundary");
    const auto found = std::upper_bound(spans_.begin(), spans_.end(), offset, [](auto position, const auto& span) { return position < span.display.last; });
    if (found != spans_.end() && Atomic(found->kind) && offset > found->display.first)
        return affinity == MappingAffinity::Leading ? found->display.first : found->display.last;
    while (! IsEditBoundary(display_, offset))
    {
        if (affinity == MappingAffinity::Leading)
            --offset;
        else
            ++offset;
    }
    return offset;
}

bool ApplySourceEdits(
    Document& document, const SourceSnapshot& expectedSource, std::uint64_t expectedRevision, std::span<const SourceEdit> edits, Selection after)
{
    if (document.Revision() != expectedRevision || ! document.Snapshot().IsSameRevision(expectedSource))
        throw std::invalid_argument("Stale semantic source transaction");
    if (edits.size() > Document::MaximumUnits)
        throw std::length_error("Too many semantic source edits");
    std::size_t removed = 0u, inserted = 0u, previousEnd = 0u, previousStart = 0u;
    bool first = true;
    for (const auto& edit : edits)
    {
        if (edit.source.first > edit.source.last || ! expectedSource.IsBoundary(edit.source.first) || ! expectedSource.IsBoundary(edit.source.last))
            throw std::out_of_range("Invalid semantic edit range");
        if (! first && (edit.source.first < previousEnd || edit.source.first == previousStart))
            throw std::invalid_argument("Semantic edits must be ordered and disjoint");
        if (edit.replacement.size() > Document::MaximumUnits - inserted)
            throw std::length_error("Semantic insertion capacity exceeded");
        if (! IsValidUtf16(edit.replacement))
            throw std::invalid_argument("Invalid semantic replacement UTF-16");
        inserted += edit.replacement.size();
        removed += edit.source.last - edit.source.first;
        previousStart = edit.source.first;
        previousEnd   = edit.source.last;
        first         = false;
    }
    if (inserted > Document::MaximumUnits - (document.Length() - removed))
        throw std::length_error("Semantic transaction source capacity exceeded");
    if (edits.empty())
    {
        document.SetSelection(after);
        return false;
    }
    const auto begin = edits.front().source.first, end = edits.back().source.last;
    std::wstring replacement(0u, L' ');
    replacement.reserve(end - begin - removed + inserted);
    auto cursor = begin;
    for (const auto& edit : edits)
    {
        replacement += expectedSource.Slice(cursor, edit.source.first);
        replacement += edit.replacement;
        cursor = edit.source.last;
    }
    // Replace validates the virtual prefix/envelope/suffix selection joins and
    // stages persistent pieces/history before its single nonthrow publication.
    return document.Replace(begin, end, replacement, after);
}
} // namespace DxUi::RichText
