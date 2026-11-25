#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <span>

namespace DxUi::RichText
{
namespace Detail
{
struct SourceDisplayMapAccess;
} // namespace Detail
enum class MappingAffinity
{
    Leading,
    Trailing
};
enum class DisplaySpanKind
{
    Source,
    Transformed,
    Generated,
    Object
};
struct DisplaySpan
{
    MarkdownRange source, display;
    DisplaySpanKind kind = DisplaySpanKind::Source;
    // Repeated display occurrences must name an exact primary source span of
    // the same kind. Only that primary occurrence receives source caret hits.
    bool primary = true;
};

// Immutable coordinate data, independent of the v0 monotonic Projection and of
// any host/graphics object. Build during preparation, never during composition.
// Source/display offsets are UTF-16 scalar/CRLF boundaries, not grapheme stops.
class SourceDisplayMap
{
public:
    static constexpr std::size_t MaximumDisplayUnits = Document::MaximumUnits * 2u;
    static constexpr std::size_t MaximumSpans        = Document::MaximumUnits;
    [[nodiscard]] static SourceResult TryBuild(std::wstring_view source,
                                               std::wstring_view display,
                                               std::span<const DisplaySpan> spans,
                                               std::shared_ptr<const SourceDisplayMap>& output);
    static std::shared_ptr<const SourceDisplayMap> Build(std::wstring_view source, std::wstring_view display, std::span<const DisplaySpan> spans);
    SourceDisplayMap(const SourceDisplayMap&)            = delete;
    SourceDisplayMap& operator=(const SourceDisplayMap&) = delete;
    std::wstring_view Source() const noexcept;
    std::wstring_view Display() const noexcept;
    std::span<const DisplaySpan> Spans() const noexcept;
    // Hidden source uses the next primary span for Leading and the preceding
    // primary span for Trailing. At the outside edges use display end/start.
    std::size_t ToDisplay(std::size_t sourceOffset, MappingAffinity affinity = MappingAffinity::Leading) const;
    std::size_t ToSource(std::size_t displayOffset, MappingAffinity affinity = MappingAffinity::Leading) const;
    // Checked queries preserve output on an invalid coordinate, range or affinity.
    [[nodiscard]] SourceResult TryToDisplay(std::size_t sourceOffset, MappingAffinity affinity, std::size_t& output) const noexcept;
    [[nodiscard]] SourceResult TryToSource(std::size_t displayOffset, MappingAffinity affinity, std::size_t& output) const noexcept;
    // Fragment queries omit generated text, expand transformed/object spans
    // atomically, sort by the destination coordinate and merge touching ranges.
    // Repetition never creates duplicate source edits; reorder never selects
    // the unrelated source between two returned fragments. Empty ranges stay empty.
    std::vector<MarkdownRange> DisplayRanges(MarkdownRange source) const;
    std::vector<MarkdownRange> SourceRanges(MarkdownRange display) const;
    [[nodiscard]] SourceResult TryDisplayRanges(MarkdownRange source, std::vector<MarkdownRange>& output) const;
    [[nodiscard]] SourceResult TrySourceRanges(MarkdownRange display, std::vector<MarkdownRange>& output) const;
    // Snap an arbitrary bounded ACP out of an atomic span or a scalar/CRLF join.
    // Logical grapheme movement is the separate TextElements/layout contract.
    std::size_t SnapDisplayBoundary(std::size_t displayOffset, MappingAffinity affinity) const;
    [[nodiscard]] SourceResult TrySnapDisplayBoundary(std::size_t displayOffset, MappingAffinity affinity, std::size_t& output) const noexcept;

private:
    SourceDisplayMap(std::wstring_view source, std::wstring_view display, std::span<const DisplaySpan> spans, SourceResult& result);
    SourceDisplayMap(std::shared_ptr<const void> sourceOwner,
                     std::wstring_view source,
                     std::wstring&& display,
                     std::vector<DisplaySpan>&& spans,
                     SourceResult& result) noexcept;
    void IndexPrimary(SourceResult& result);
    void FindPrimary(std::size_t sourceOffset, const DisplaySpan*& previous, const DisplaySpan*& current, const DisplaySpan*& next) const noexcept;
    // Sparse representation: no per-unit anchors. Source is either an owned copy
    // (public TryBuild) or a view borrowed from an immutable owner the map keeps
    // alive (semantic publication). primaryOrder_ is empty while every span's
    // source start is nondecreasing in display order; otherwise it lists the
    // primary spans in source order. It is also kept when more than 16 non-primary
    // spans follow one another, so every source lookup is O(log n + 16) span visits.
    std::wstring _ownedSource, _display;
    std::wstring_view _source;
    std::shared_ptr<const void> _sourceOwner;
    std::vector<DisplaySpan> _spans;
    std::vector<std::uint32_t> _primaryOrder;
    friend struct Detail::SourceDisplayMapAccess;
};

struct SourceEdit
{
    MarkdownRange source;
    std::wstring_view replacement; // Borrowed for this synchronous transaction.
};
struct SourceSearchOptions
{
    bool matchCase = false;
    bool wholeWord = false;
};
// Non-overlapping exact-source matches, ordered by ACP. Case-insensitive
// matching uses Windows ordinal Unicode comparison, independent of locale.
// Empty queries return no matches; malformed UTF-16 is rejected.
// Checked search preserves output on malformed UTF-16.
[[nodiscard]] SourceResult TryFindInSource(const SourceSnapshot& source,
                                           std::wstring_view query,
                                           std::vector<MarkdownRange>& output,
                                           SourceSearchOptions options = {});
std::vector<MarkdownRange> FindInSource(const SourceSnapshot& source, std::wstring_view query, SourceSearchOptions options = {});
// All ranges refer to expectedSource, in ascending disjoint order. Touching
// edits are legal; two insertions at the same boundary are ambiguous and rejected.
// A foreign/equal-number snapshot is stale even if its spelling happens to match.
// Stage one envelope, then delegate history/selection/publication to Document.
// The checked result leaves source, revision, selection and histories unchanged
// on refusal. A successful no-op sets changed false and may change selection.
// The convenience wrapper treats refusal as a contract violation.
bool ApplySourceEdits(
    Document& document, const SourceSnapshot& expectedSource, std::uint64_t expectedRevision, std::span<const SourceEdit> edits, Selection after);
[[nodiscard]] SourceResult TryApplySourceEdits(Document& document,
                                               const SourceSnapshot& expectedSource,
                                               std::uint64_t expectedRevision,
                                               std::span<const SourceEdit> edits,
                                               Selection after,
                                               bool& changed);
} // namespace DxUi::RichText
