#pragma once
#include <DxUi/Configuration.h>
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <cstddef>
#include <vector>

namespace DxUi::RichText::Detail
{
// Viewport-first v0 document preparation (W7.3). The document region an owner shows, in document DIPs, given to each
// advance through PreparedDocumentLayoutAccess::Advance: the chunks that meet it (with one viewport height of overscan
// above and below) are laid and published (a partial publication) before any other chunk. The public AdvancePreparation
// takes no viewport: it lays in document order and publishes only the complete result.
struct DocumentViewport
{
    static constexpr std::size_t NoFocus = static_cast<std::size_t>(-1);
    float top{}, bottom{};
    bool active{};
    // A source position the owner will reveal when the preparation completes (an edit's caret): the viewport's height is
    // centered on its estimated place instead of on [top, bottom].
    std::size_t focus = NoFocus;
    [[nodiscard]] static DocumentViewport Of(float top, float bottom, std::size_t focus = NoFocus) noexcept
    {
        return {top, bottom, std::isfinite(top) && std::isfinite(bottom) && bottom >= top, focus};
    }
};
// Library-internal access to PreparedDocumentLayout's viewport-first preparation. A partial publication is not Ready():
// every query refuses it as a pending preparation would; only DrawPartial (laid chunks only) and PartialSourceTop (a
// position in a laid chunk, else E_PENDING) read it.
struct PreparedDocumentLayoutAccess
{
    static LayoutAdvance Advance(PreparedDocumentLayout& layout, const LayoutWorkBudget& budget, const DocumentViewport& viewport);
    [[nodiscard]] static bool Partial(const PreparedDocumentLayout& layout) noexcept;
    // PreparedDocumentLayout::TryVisibleRange (W7.6): a chunk whose caret boxes are stacked is searched for the lines
    // meeting the window; `stepping` visits every line from the chunk's first as the query did before, the reference
    // of the tests.
    static HRESULT VisibleRange(const PreparedDocumentLayout& owner, float top, float bottom, std::pair<std::size_t, std::size_t>& output, bool stepping);
    static HRESULT DrawPartial(
        const PreparedDocumentLayout& layout, ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom);
    static HRESULT PartialSourceTop(const PreparedDocumentLayout& layout, std::size_t source, float& y);
    // The display offsets where the published chunks start (empty for whole geometry): tests bound DirectWrite's own
    // single-precision accumulation inside each chunk with them.
    [[nodiscard]] static std::vector<std::size_t> ChunkDisplayStarts(const PreparedDocumentLayout& layout);
    // What the layout keeps alive: its graphs (the publication, the last complete publication a partial one keeps, the
    // partial publication a preparation began from, the staged graph) and the distinct chunk and whole layouts they hold.
    struct Retention
    {
        std::size_t graphs{}, chunkLayouts{}, wholeLayouts{}, largestGraphChunks{};
    };
    [[nodiscard]] static Retention Retained(const PreparedDocumentLayout& layout);
};
#if DXUI_ENABLE_DIAGNOSTICS
// The incremental-versus-fresh oracle's "same chunking" (W7.3): while testRichRecordChunkPlan is set, every completed v0
// document publication outside a fresh preparation records its chunk source starts and source length here, and a fresh
// preparation (DxUi::Detail::SetFreshPreparationForTest) of a source of that length cuts its chunks at exactly those
// starts. Negative control: testRichChunkOffsetError is added to the placement of every chunk after the first in a
// preparation that is not fresh, so the oracle must report a difference.
inline thread_local bool testRichRecordChunkPlan{};
inline thread_local std::vector<std::size_t> testRichPublishedChunkStarts{};
inline thread_local std::size_t testRichPublishedSourceLength{};
inline thread_local float testRichChunkOffsetError{};
#endif
enum class LayoutOptionsUse
{
    Text,
    Semantic
};
// v0 text has no math, semantic palette or code-copy action. Preserve its
// compatibility policy while sharing the full set of common layout inputs.
inline bool SameLayoutOptions(const LayoutOptions& a, const LayoutOptions& b, LayoutOptionsUse use) noexcept
{
    return a.tableOverflow == b.tableOverflow && a.tabStopDip == b.tabStopDip && a.widthDip == b.widthDip && a.fontSizeDip == b.fontSizeDip && a.dpi == b.dpi &&
           a.codeFontSizeDip == b.codeFontSizeDip && a.lineHeightMultiplier == b.lineHeightMultiplier && a.headingScaleStep == b.headingScaleStep &&
           a.fontFamily == b.fontFamily && a.codeFontFamily == b.codeFontFamily && a.headingFontSizeDip == b.headingFontSizeDip &&
           a.headingLineHeightMultiplier == b.headingLineHeightMultiplier && a.paragraphSpacingAfterLines == b.paragraphSpacingAfterLines &&
           a.listItemSpacingDip == b.listItemSpacingDip && a.semanticSpacing == b.semanticSpacing && a.locale == b.locale &&
           a.readingDirection == b.readingDirection && a.textAlignment == b.textAlignment && a.wrap == b.wrap &&
           (use == LayoutOptionsUse::Text ||
            (a.autoParagraphDirection == b.autoParagraphDirection && a.markdownPresentation == b.markdownPresentation && a.codeCopyAction == b.codeCopyAction &&
             a.mathFontFamily == b.mathFontFamily && a.diagramLabelWidthDip == b.diagramLabelWidthDip && a.mathAbsoluteUnitScale == b.mathAbsoluteUnitScale &&
             a.strikeCompletedTasks == b.strikeCompletedTasks));
}
} // namespace DxUi::RichText::Detail
