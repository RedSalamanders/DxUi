#pragma once
#include <DxUi/RichTextLayout.h>

namespace DxUi::RichText::Detail
{
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
