#include <DxUi/Failure.h>
#include <DxUi/RichSemantic.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <iterator>
#include <limits>

namespace DxUi::RichText
{
std::vector<MarkdownRange> FindInSource(const SourceSnapshot& snapshot, std::wstring_view query, SourceSearchOptions options)
{
    const Failure::Scope failureScope;
    std::vector<MarkdownRange> result(0u);
    if (TryFindInSource(snapshot, query, result, options) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return result;
}
SourceResult TryFindInSource(const SourceSnapshot& snapshot, std::wstring_view query, std::vector<MarkdownRange>& output, SourceSearchOptions options)
{
    const Failure::Scope failureScope;
    if (! IsValidSourceUtf16(query))
        return SourceResult::InvalidSource;
    std::vector<MarkdownRange> result(0u);
    if (query.empty() || query.size() > snapshot.Length())
    {
        output.swap(result);
        return SourceResult::Success;
    }
    const auto source = snapshot.Text();
    const auto word   = [](wchar_t ch) noexcept
    {
        if (ch == L'_' || (ch >= 0xD800 && ch <= 0xDFFF))
            return true;
        WORD type = 0u;
        if (GetStringTypeW(CT_CTYPE1, &ch, 1, &type) && (type & (C1_ALPHA | C1_DIGIT)) != 0u)
            return true;
        return GetStringTypeW(CT_CTYPE3, &ch, 1, &type) && (type & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) != 0u;
    };
    size_t cursor = 0u;
    while (source.size() - cursor >= query.size())
    {
        const int found = FindStringOrdinal(FIND_FROMSTART,
                                            source.data() + cursor,
                                            static_cast<int>(source.size() - cursor),
                                            query.data(),
                                            static_cast<int>(query.size()),
                                            ! options.matchCase);
        if (found < 0)
            break;
        const auto first = cursor + static_cast<size_t>(found);
        const auto last  = first + query.size();
        if (IsEditBoundary(source, first) && IsEditBoundary(source, last) &&
            (! options.wholeWord || ((first == 0u || ! word(source[first - 1u])) && (last == source.size() || ! word(source[last])))))
        {
            result.push_back({first, last});
            cursor = last;
        }
        else
            cursor = first + 1u;
    }
    output.swap(result);
    return SourceResult::Success;
}
namespace
{
constexpr auto MissingAnchor = std::numeric_limits<std::size_t>::max();
bool Atomic(DisplaySpanKind kind) noexcept
{
    return kind == DisplaySpanKind::Transformed || kind == DisplaySpanKind::Object;
}
bool ValidRange(std::wstring_view text, MarkdownRange range) noexcept
{
    return range.first <= range.last && IsEditBoundary(text, range.first) && IsEditBoundary(text, range.last);
}
bool ValidAffinity(MappingAffinity affinity) noexcept
{
    return affinity == MappingAffinity::Leading || affinity == MappingAffinity::Trailing;
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
    std::shared_ptr<const SourceDisplayMap> result;
    if (TryBuild(source, display, spans, result) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return result;
}
SourceResult SourceDisplayMap::TryBuild(std::wstring_view source,
                                        std::wstring_view display,
                                        std::span<const DisplaySpan> spans,
                                        std::shared_ptr<const SourceDisplayMap>& output)
{
    const Failure::Scope failureScope;
    if (source.size() > Document::MaximumUnits || display.size() > MaximumDisplayUnits || spans.size() > MaximumSpans)
        return SourceResult::CapacityExceeded;
    if (! IsValidSourceUtf16(source) || ! IsValidSourceUtf16(display))
        return SourceResult::InvalidSource;
    // Reject malformed spans and display partitions before allocating the map.
    std::size_t displayEnd{};
    for (const auto& span : spans)
    {
        if (! ValidRange(source, span.source) || ! ValidRange(display, span.display) || span.display.first != displayEnd ||
            span.display.first == span.display.last)
            return SourceResult::InvalidRange;
        displayEnd = span.display.last;
        switch (span.kind)
        {
            case DisplaySpanKind::Generated:
                if (span.primary || span.source.first != span.source.last)
                    return SourceResult::InvalidRange;
                break;
            case DisplaySpanKind::Source:
                if (source.substr(span.source.first, span.source.last - span.source.first) !=
                    display.substr(span.display.first, span.display.last - span.display.first))
                    return SourceResult::InvalidRange;
                [[fallthrough]];
            case DisplaySpanKind::Transformed:
            case DisplaySpanKind::Object:
                if (span.source.first == span.source.last)
                    return SourceResult::InvalidRange;
                break;
            default: return SourceResult::InvalidRange;
        }
    }
    if (displayEnd != display.size())
        return SourceResult::InvalidRange;
    SourceResult result = SourceResult::Success;
    std::shared_ptr<const SourceDisplayMap> staged(new SourceDisplayMap(source, display, spans, result));
    if (result != SourceResult::Success)
        return result;
    output.swap(staged);
    return SourceResult::Success;
}
SourceDisplayMap::SourceDisplayMap(std::wstring_view source, std::wstring_view display, std::span<const DisplaySpan> spans, SourceResult& result)
    : source_(source),
      display_(display),
      spans_(spans.begin(), spans.end()),
      sourceLeading_(source.size() + 1u, MissingAnchor),
      sourceTrailing_(source.size() + 1u, MissingAnchor)
{
    std::vector<std::size_t> primary(0u);
    primary.reserve(spans.size());
    for (std::size_t i = 0u; i < spans_.size(); ++i)
    {
        const auto& span = spans_[i];
        if (span.primary)
            primary.push_back(i);
    }
    std::sort(primary.begin(), primary.end(), [&](auto a, auto b) { return spans_[a].source.first < spans_[b].source.first; });
    std::size_t sourceEnd = 0u;
    for (const auto index : primary)
    {
        const auto& span = spans_[index];
        if (span.source.first < sourceEnd)
        {
            result = SourceResult::InvalidRange;
            return;
        }
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
        {
            result = SourceResult::InvalidRange;
            return;
        }
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
SourceResult SourceDisplayMap::TryToDisplay(std::size_t offset, MappingAffinity affinity, std::size_t& output) const noexcept
{
    if (! ValidAffinity(affinity) || ! ValidRange(source_, {offset, offset}))
        return SourceResult::InvalidRange;
    output = (affinity == MappingAffinity::Leading ? sourceLeading_ : sourceTrailing_)[offset];
    return SourceResult::Success;
}
std::size_t SourceDisplayMap::ToDisplay(std::size_t offset, MappingAffinity affinity) const
{
    std::size_t output{};
    if (TryToDisplay(offset, affinity, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return output;
}
SourceResult SourceDisplayMap::TryToSource(std::size_t offset, MappingAffinity affinity, std::size_t& output) const noexcept
{
    if (! ValidAffinity(affinity) || ! ValidRange(display_, {offset, offset}))
        return SourceResult::InvalidRange;
    if (spans_.empty())
        output = affinity == MappingAffinity::Leading ? source_.size() : 0u;
    else
    {
        auto found = std::upper_bound(spans_.begin(), spans_.end(), offset, [](auto value, const auto& span) { return value < span.display.last; });
        if (found == spans_.end())
            found = std::prev(spans_.end());
        else if (affinity == MappingAffinity::Trailing && offset == found->display.first && found != spans_.begin())
            --found;
        if (found->kind == DisplaySpanKind::Generated)
            output = found->source.first;
        else if (Atomic(found->kind))
            output = offset == found->display.first         ? found->source.first
                     : offset == found->display.last        ? found->source.last
                     : affinity == MappingAffinity::Leading ? found->source.first
                                                            : found->source.last;
        else
            output = found->source.first + offset - found->display.first;
    }
    return SourceResult::Success;
}
std::size_t SourceDisplayMap::ToSource(std::size_t offset, MappingAffinity affinity) const
{
    std::size_t output{};
    if (TryToSource(offset, affinity, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return output;
}
SourceResult SourceDisplayMap::TryDisplayRanges(MarkdownRange source, std::vector<MarkdownRange>& output) const
{
    const Failure::Scope failureScope;
    if (! ValidRange(source_, source))
        return SourceResult::InvalidRange;
    std::vector<MarkdownRange> result(0u);
    if (source.first != source.last)
        for (const auto& span : spans_)
        {
            const auto first = std::max(source.first, span.source.first), last = std::min(source.last, span.source.last);
            if (span.kind == DisplaySpanKind::Generated || first >= last)
                continue;
            AppendRange(result,
                        Atomic(span.kind) ? span.display
                                          : MarkdownRange{span.display.first + first - span.source.first, span.display.first + last - span.source.first});
        }
    output.swap(result);
    return SourceResult::Success;
}
std::vector<MarkdownRange> SourceDisplayMap::DisplayRanges(MarkdownRange source) const
{
    const Failure::Scope failureScope;
    std::vector<MarkdownRange> output(0u);
    if (TryDisplayRanges(source, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return std::vector<MarkdownRange>(output);
}
SourceResult SourceDisplayMap::TrySourceRanges(MarkdownRange display, std::vector<MarkdownRange>& output) const
{
    const Failure::Scope failureScope;
    if (! ValidRange(display_, display))
        return SourceResult::InvalidRange;
    std::vector<MarkdownRange> ranges(0u);
    if (display.first != display.last)
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
    output.swap(result);
    return SourceResult::Success;
}
std::vector<MarkdownRange> SourceDisplayMap::SourceRanges(MarkdownRange display) const
{
    const Failure::Scope failureScope;
    std::vector<MarkdownRange> output(0u);
    if (TrySourceRanges(display, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return std::vector<MarkdownRange>(output);
}
SourceResult SourceDisplayMap::TrySnapDisplayBoundary(std::size_t offset, MappingAffinity affinity, std::size_t& output) const noexcept
{
    if (! ValidAffinity(affinity) || offset > display_.size())
        return SourceResult::InvalidRange;
    const auto found = std::upper_bound(spans_.begin(), spans_.end(), offset, [](auto position, const auto& span) { return position < span.display.last; });
    if (found != spans_.end() && Atomic(found->kind) && offset > found->display.first)
        output = affinity == MappingAffinity::Leading ? found->display.first : found->display.last;
    else
    {
        while (! IsEditBoundary(display_, offset))
            offset = affinity == MappingAffinity::Leading ? offset - 1u : offset + 1u;
        output = offset;
    }
    return SourceResult::Success;
}
std::size_t SourceDisplayMap::SnapDisplayBoundary(std::size_t offset, MappingAffinity affinity) const
{
    std::size_t output{};
    if (TrySnapDisplayBoundary(offset, affinity, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return output;
}

SourceResult TryApplySourceEdits(
    Document& document, const SourceSnapshot& expectedSource, std::uint64_t expectedRevision, std::span<const SourceEdit> edits, Selection after, bool& changed)
{
    changed = false;
    if (document.Revision() != expectedRevision || ! document.Snapshot().IsSameRevision(expectedSource))
        return SourceResult::Stale;
    if (edits.size() > Document::MaximumUnits)
        return SourceResult::CapacityExceeded;
    std::size_t removed = 0u, inserted = 0u, previousEnd = 0u, previousStart = 0u;
    bool first = true;
    for (const auto& edit : edits)
    {
        if (edit.source.first > edit.source.last || ! expectedSource.IsBoundary(edit.source.first) || ! expectedSource.IsBoundary(edit.source.last))
            return SourceResult::InvalidRange;
        if (! first && (edit.source.first < previousEnd || edit.source.first == previousStart))
            return SourceResult::InvalidRange;
        if (edit.replacement.size() > Document::MaximumUnits - inserted)
            return SourceResult::CapacityExceeded;
        if (! IsValidSourceUtf16(edit.replacement))
            return SourceResult::InvalidSource;
        inserted += edit.replacement.size();
        removed += edit.source.last - edit.source.first;
        previousStart = edit.source.first;
        previousEnd   = edit.source.last;
        first         = false;
    }
    if (inserted > Document::MaximumUnits - (document.Length() - removed))
        return SourceResult::CapacityExceeded;
    const auto resultLength = document.Length() - removed + inserted;
    if (after.anchor > resultLength || after.caret > resultLength)
        return SourceResult::InvalidRange;
    if (edits.empty())
    {
        return document.TrySetSelection(after);
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
    return document.TryReplace(begin, end, replacement, after, changed);
}
bool ApplySourceEdits(
    Document& document, const SourceSnapshot& expectedSource, std::uint64_t expectedRevision, std::span<const SourceEdit> edits, Selection after)
{
    bool changed = false;
    if (TryApplySourceEdits(document, expectedSource, expectedRevision, edits, after, changed) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return changed;
}
} // namespace DxUi::RichText
