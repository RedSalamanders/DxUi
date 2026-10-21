#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <span>

namespace DxUi::RichText
{
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
    // Fragment queries omit generated text, expand transformed/object spans
    // atomically, sort by the destination coordinate and merge touching ranges.
    // Repetition never creates duplicate source edits; reorder never selects
    // the unrelated source between two returned fragments. Empty ranges stay empty.
    std::vector<MarkdownRange> DisplayRanges(MarkdownRange source) const;
    std::vector<MarkdownRange> SourceRanges(MarkdownRange display) const;
    // Snap an arbitrary bounded ACP out of an atomic span or a scalar/CRLF join.
    // Logical grapheme movement is the separate TextElements/layout contract.
    std::size_t SnapDisplayBoundary(std::size_t displayOffset, MappingAffinity affinity) const;

private:
    SourceDisplayMap(std::wstring_view source, std::wstring_view display, std::span<const DisplaySpan> spans);
    std::wstring source_, display_;
    std::vector<DisplaySpan> spans_;
    std::vector<std::size_t> sourceLeading_, sourceTrailing_;
};

struct SourceEdit
{
    MarkdownRange source;
    std::wstring_view replacement; // Borrowed for this synchronous transaction.
};
// All ranges refer to expectedSource, in ascending disjoint order. Touching
// edits are legal; two insertions at the same boundary are ambiguous and rejected.
// A foreign/equal-number snapshot is stale even if its spelling happens to match.
// Stage one envelope, then delegate history/selection/publication to Document.
// Returns false for unchanged spelling (selection may still change); exceptions
// leave source, revision, selection and both histories untouched.
bool ApplySourceEdits(
    Document& document, const SourceSnapshot& expectedSource, std::uint64_t expectedRevision, std::span<const SourceEdit> edits, Selection after);
} // namespace DxUi::RichText
