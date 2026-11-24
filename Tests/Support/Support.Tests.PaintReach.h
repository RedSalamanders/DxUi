#pragma once
// The check of a declared painted extent (W4.10; docs/controls.md, "Painted extent"): a control painted alone on a
// transparent surface draws no pixel outside its bounds plus its reach (Control::GetPaintOverhang), snapped outward to the
// device pixels the extent touches. The library's own controls are checked with it in every state and at 96, 120, 144, 168
// and 192 DPI (Tests/Embedded/Embedded.Tests.PaintRegions.h); a control author can check a derived control the same way:
// attach an EmbeddedHost, make the control the only child of a root Panel, put it in each state it can reach (hover, press,
// focus with keyboard focus visible, disabled, its own options), prepare, read the surface with DebugReadSurfaceForTest and
// call InkOutsideExtent with its bounds inflated by its overhang.
#include <DxUi/DxUi.h>

#include <cmath>
#include <cstdint>
#include <format>
#include <string>

namespace PaintReach
{
// The first pixel with any coverage outside `extentDip` snapped outward, described, or an empty string when there is none.
// `bgraPixels` is premultiplied BGRA of `widthPx` x `heightPx` on a transparent surface.
[[nodiscard]] inline std::string InkOutsideExtent(const DxUi::WindowHostBitmapCapture& surface, const D2D1_RECT_F& extentDip, float dpi)
{
    const float scale = dpi / 96.0f;
    const LONG left   = static_cast<LONG>(std::floor(extentDip.left * scale));
    const LONG top    = static_cast<LONG>(std::floor(extentDip.top * scale));
    const LONG right  = static_cast<LONG>(std::ceil(extentDip.right * scale));
    const LONG bottom = static_cast<LONG>(std::ceil(extentDip.bottom * scale));
    for (UINT y = 0u; y < surface.heightPx; ++y)
        for (UINT x = 0u; x < surface.widthPx; ++x)
        {
            const uint8_t alpha = surface.bgraPixels[(static_cast<size_t>(y) * surface.widthPx + x) * 4u + 3u];
            if (alpha != 0u && (LONG(x) < left || LONG(x) >= right || LONG(y) < top || LONG(y) >= bottom))
                return std::format("pixel ({}, {}) alpha {} outside [{}, {}, {}, {})", x, y, alpha, left, top, right, bottom);
        }
    return {};
}

// How far the ink reaches beyond `bounds` on each side, in DIPs (negative: inside); false when nothing is inked.
[[nodiscard]] inline bool MeasureInkReach(const DxUi::WindowHostBitmapCapture& surface, const D2D1_RECT_F& bounds, float dpi, DxUi::PaintOverhang& reach)
{
    LONG minX = LONG_MAX, minY = LONG_MAX, maxX = LONG_MIN, maxY = LONG_MIN;
    for (UINT y = 0u; y < surface.heightPx; ++y)
        for (UINT x = 0u; x < surface.widthPx; ++x)
            if (surface.bgraPixels[(static_cast<size_t>(y) * surface.widthPx + x) * 4u + 3u] != 0u)
            {
                minX = (std::min)(minX, LONG(x));
                minY = (std::min)(minY, LONG(y));
                maxX = (std::max)(maxX, LONG(x) + 1);
                maxY = (std::max)(maxY, LONG(y) + 1);
            }
    if (minX == LONG_MAX)
        return false;
    const float scale = dpi / 96.0f;
    reach = {bounds.left - float(minX) / scale, bounds.top - float(minY) / scale, float(maxX) / scale - bounds.right, float(maxY) / scale - bounds.bottom};
    return true;
}
} // namespace PaintReach
