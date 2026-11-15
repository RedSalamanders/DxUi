#include <DxUi/Failure.h>

#include "DxUi.Internal.h"
#include "Support/MeasureCache.Tests.Hooks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <dwrite_1.h>
#include <exception>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace DxUi
{
namespace
{
[[nodiscard]] bool IsExtent(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f;
}

[[nodiscard]] bool SameRect(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

void ApplyMinimum(const LayoutConstraints& constraints, DesiredSize& result) noexcept
{
    result.sizeDip.width  = (std::max)(result.sizeDip.width, constraints.minimumDip.width);
    result.sizeDip.height = (std::max)(result.sizeDip.height, constraints.minimumDip.height);
    result.overflow       = result.overflow || (constraints.maximumWidthDip && result.sizeDip.width > *constraints.maximumWidthDip) ||
                            (constraints.maximumHeightDip && result.sizeDip.height > *constraints.maximumHeightDip);
}

// The first line's metrics (its baseline is the measured baseline), with no heap allocation for up to kStackLineMetrics
// lines; a layout with more lines reads them into a heap buffer, counted as text.lineMetricsHeapFallbacks.
inline constexpr UINT32 kStackLineMetrics = 16u;
[[nodiscard]] HRESULT ReadFirstLineMetrics(IDWriteTextLayout& layout, DWRITE_LINE_METRICS& first)
{
    std::array<DWRITE_LINE_METRICS, kStackLineMetrics> lines{};
    UINT32 lineCount = 0u;
    HRESULT hr       = layout.GetLineMetrics(lines.data(), kStackLineMetrics, &lineCount);
    if (SUCCEEDED(hr))
    {
        if (lineCount == 0u)
            return E_FAIL;
        first = lines.front();
        return S_OK;
    }
    if (hr != E_NOT_SUFFICIENT_BUFFER)
        return hr;
    if (lineCount == 0u || lineCount > kMaxIntrinsicTextUnits + 1u)
        return E_FAIL;
    ++Detail::Work().text.lineMetricsHeapFallbacks;
    std::vector<DWRITE_LINE_METRICS> heap(lineCount);
    hr = layout.GetLineMetrics(heap.data(), lineCount, &lineCount);
    if (FAILED(hr))
        return hr;
    first = heap.front();
    return S_OK;
}

// Measurement uses the same host formats as paint, but retains only numbers in its owning container. A missing
// public maximum is unbounded; DirectWrite requires a finite layout extent, so FLT_MAX is confined to this adapter.
[[nodiscard]] HRESULT MeasureText(const MeasureContext& context,
                                  const LayoutConstraints& constraints,
                                  std::wstring_view text,
                                  FontRole role,
                                  FlowDirection flow,
                                  bool wrap,
                                  float horizontalChrome,
                                  float verticalChrome,
                                  float baselineInset,
                                  float minimumHeight,
                                  DesiredSize& result,
                                  IDWriteTextFormat* configuredFormat = nullptr)
{
    if (text.size() > kMaxIntrinsicTextUnits)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    const size_t shapedUnits = text.empty() ? 2u : text.size();
    if (context.remainingTextUnits)
    {
        if (*context.remainingTextUnits < shapedUnits)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        *context.remainingTextUnits -= shapedUnits;
    }
    auto* const factory = context.host.GetWriteFactory();
    auto* const format  = configuredFormat ? configuredFormat : context.host.GetTextFormat(role);
    if (! factory || ! format)
        return E_FAIL;
    const bool empty = text.empty();
    if (empty)
        text = L"Ag";
    const float width =
        wrap && constraints.maximumWidthDip ? (std::max)(0.0f, *constraints.maximumWidthDip - horizontalChrome) : (std::numeric_limits<float>::max)();
    wil::com_ptr<IDWriteTextLayout> layout;
    HRESULT hr =
        CountedCreateTextLayout(factory, text.data(), static_cast<UINT32>(text.size()), format, width, (std::numeric_limits<float>::max)(), layout.put());
    if (FAILED(hr) || ! layout)
        return FAILED(hr) ? hr : E_FAIL;
    hr = layout->SetWordWrapping(wrap && ! empty ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (FAILED(hr))
        return hr;
    hr = layout->SetReadingDirection(ResolveReadingDirection(flow));
    if (FAILED(hr))
        return hr;
    hr = layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (FAILED(hr))
        return hr;
    hr = layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (FAILED(hr))
        return hr;
    DWRITE_TEXT_METRICS metrics{};
    hr = layout->GetMetrics(&metrics);
    if (FAILED(hr))
        return hr;
    DWRITE_LINE_METRICS firstLine{};
    hr = ReadFirstLineMetrics(*layout, firstLine);
    if (FAILED(hr))
        return hr;
    // A wrapped line's trailing space hangs past the width it wrapped to, so wrapped text is as wide as its ink; one line
    // keeps its trailing space, which separates it from what follows.
    const float textWidth = wrap ? metrics.width : metrics.widthIncludingTrailingWhitespace;
    result.sizeDip        = D2D1::SizeF((empty ? 0.0f : textWidth) + horizontalChrome, (std::max)(minimumHeight, metrics.height + verticalChrome));
    result.baselineDip    = baselineInset + firstLine.baseline;
    ApplyMinimum(constraints, result);
    return S_OK;
}

[[nodiscard]] float InteractiveHeight(const Control& control) noexcept
{
    return control.GetDensity() == Density::Compact ? 24.0f : 32.0f;
}

[[nodiscard]] bool ValidOptions(const FlexChildOptions& options) noexcept
{
    return AreValidLayoutConstraints(options.constraints) && IsExtent(options.grow) &&
           (! options.explicitSizeDip || (IsExtent(options.explicitSizeDip->width) && IsExtent(options.explicitSizeDip->height))) &&
           (options.alignment == LayoutAlignment::Start || options.alignment == LayoutAlignment::Center || options.alignment == LayoutAlignment::End ||
            options.alignment == LayoutAlignment::Stretch);
}

// A line consumes each returned edge as its next anchor, so an outward correction cannot overlap later children.
HRESULT AdvanceLayoutEdge(float anchor,
                          float extent,
                          bool reverse,
                          const std::optional<float>& maximum,
                          float& edge,
                          const std::optional<float>& intrinsicMinimum = std::nullopt) noexcept
{
    const float required = intrinsicMinimum ? *intrinsicMinimum : extent;
    if (! std::isfinite(anchor) || ! IsExtent(extent) || ! IsExtent(required) || required > extent)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    float staged = reverse ? anchor - extent : anchor + extent;
    double span  = reverse ? static_cast<double>(anchor) - staged : static_cast<double>(staged) - anchor;
    if (! std::isfinite(span) || span < 0.0 || span > (std::numeric_limits<float>::max)() || (required > 0.0f && span <= 0.0))
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    if (span < required)
    {
        staged = std::nextafter(staged, reverse ? -(std::numeric_limits<float>::infinity)() : (std::numeric_limits<float>::infinity)());
        span   = reverse ? static_cast<double>(anchor) - staged : static_cast<double>(staged) - anchor;
    }
    // Grow and Stretch are optional additions above an intrinsic minimum. Floor their endpoint within the
    // requested allocation/cap instead of making that extra space an impossible authoritative minimum.
    if (required < extent && span > extent)
    {
        const float inward      = std::nextafter(staged, anchor);
        const double inwardSpan = reverse ? static_cast<double>(anchor) - inward : static_cast<double>(inward) - anchor;
        if (inwardSpan >= required)
        {
            staged = inward;
            span   = inwardSpan;
        }
    }
    // An intrinsic extent may already exceed a maximum. A maximum that actually caps this extent cannot be exceeded
    // to repair endpoint rounding by more than one representable step of the endpoint: a fixed extent (its minimum
    // equal to its maximum) at an inexact anchor has no endpoint exactly at its span. That step is accepted only while
    // it is far below the extent, so coordinates too coarse for the extent still fail.
    bool exceedsMaximum = maximum && *maximum >= extent && span > *maximum;
    if (exceedsMaximum && std::isfinite(staged))
    {
        const double step = std::abs(static_cast<double>(staged) - std::nextafter(staged, anchor));
        exceedsMaximum    = span - *maximum > step || step > static_cast<double>(extent) / 1024.0;
    }
    if (! std::isfinite(staged) || ! std::isfinite(span) || span < required || span > (std::numeric_limits<float>::max)() || exceedsMaximum)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    edge = staged;
    return S_OK;
}

#if defined(_DEBUG)
// Debug verification of measure-cache hits (Detail::SetMeasureCacheVerificationForTest): off unless a test enables it.
thread_local bool g_verifyMeasureCacheHits = false;

[[nodiscard]] bool SameDesired(const DesiredSize& a, const DesiredSize& b) noexcept
{
    return a.sizeDip.width == b.sizeDip.width && a.sizeDip.height == b.sizeDip.height && a.baselineDip == b.baselineDip && a.overflow == b.overflow;
}
#endif

thread_local Detail::MeasureProbeObserver g_measureProbeObserver = nullptr;
thread_local void* g_measureProbeContext                         = nullptr;
} // namespace

namespace Detail
{
MeasureProbeObserver SetMeasureProbeObserverForTest(MeasureProbeObserver observer, void* context, void** previousContext) noexcept
{
    const MeasureProbeObserver previous = g_measureProbeObserver;
    if (previousContext)
        *previousContext = g_measureProbeContext;
    g_measureProbeObserver = observer;
    g_measureProbeContext  = context;
    return previous;
}

bool SetMeasureCacheVerificationForTest(bool enabled) noexcept
{
#if defined(_DEBUG)
    g_verifyMeasureCacheHits = enabled;
    return true;
#else
    static_cast<void>(enabled);
    return false;
#endif
}
} // namespace Detail

// The measure cache of one control (Specs/UI/UI_ControlsAndLayout.md, "Measure cache"). It holds results for one
// context, the control's measure revision with the host, its typography revision, DPI and the control's resolved flow and
// density; a context change empties every slot, so the slots key only the constraints. Slots are kept most recently used
// first. A slot filled while the control declared MeasureDependsOnMaximum() == false matches any maximum and recomputes
// only its overflow flag. The slots follow the 32-byte context in one allocation: two for a control whose size does not
// depend on the maximum (its keys differ only by minimum), four for any other, which a container asks once while it is
// measured and once while it is arranged, at each of two widths when a width returns (receipt W6.2).
struct Control::MeasureCache final
{
    static constexpr uint8_t kMaximumIndependentSlots = 2u;
    static constexpr uint8_t kMaximumDependentSlots   = 4u;
    static constexpr uint8_t kValid                   = 1u;
    static constexpr uint8_t kOverflow                = 2u;
    static constexpr uint8_t kMaximumIndependent      = 4u;
    static constexpr uint8_t kNotImplemented          = 8u;
    struct Slot final
    {
        float minimumWidth  = 0.0f;
        float minimumHeight = 0.0f;
        float maximumWidth  = -1.0f; // -1: unbounded (valid maxima are nonnegative)
        float maximumHeight = -1.0f;
        float width         = 0.0f;
        float height        = 0.0f;
        float baseline      = -1.0f; // -1: none
        uint8_t flags       = 0u;
    };
    const ControlHost* host = nullptr;
    uint64_t revision       = 0u;
    uint64_t typography     = 0u;
    float dpi               = 0.0f;
    FlowDirection flow      = FlowDirection::LeftToRight;
    Density density         = Density::Standard;
    uint8_t capacity        = 0u;

    // One allocation: the context, then `slots` value-initialized slots. Throws std::bad_alloc like operator new.
    [[nodiscard]] static MeasureCache* Create(uint8_t slots)
    {
        void* const storage = ::operator new(Bytes(slots));
        auto* const cache   = new (storage) MeasureCache();
        cache->capacity     = slots;
        for (uint8_t index = 0u; index < slots; ++index)
            new (static_cast<std::byte*>(storage) + sizeof(MeasureCache) + index * sizeof(Slot)) Slot();
        return cache;
    }
    [[nodiscard]] static constexpr size_t Bytes(uint8_t slots) noexcept
    {
        return sizeof(MeasureCache) + slots * sizeof(Slot);
    }
    [[nodiscard]] std::span<Slot> Slots() noexcept
    {
        return {std::launder(reinterpret_cast<Slot*>(reinterpret_cast<std::byte*>(this) + sizeof(MeasureCache))), capacity};
    }
    [[nodiscard]] std::span<const Slot> Slots() const noexcept
    {
        return {std::launder(reinterpret_cast<const Slot*>(reinterpret_cast<const std::byte*>(this) + sizeof(MeasureCache))), capacity};
    }
    [[nodiscard]] static float Maximum(const std::optional<float>& maximum) noexcept
    {
        return maximum ? *maximum : -1.0f;
    }
    [[nodiscard]] bool Holds(const ControlHost* h, uint64_t r, uint64_t t, float d, FlowDirection f, Density n) const noexcept
    {
        return host == h && revision == r && typography == t && dpi == d && flow == f && density == n;
    }
    void Reset(const ControlHost* h, uint64_t r, uint64_t t, float d, FlowDirection f, Density n) noexcept
    {
        host       = h;
        revision   = r;
        typography = t;
        dpi        = d;
        flow       = f;
        density    = n;
        std::ranges::fill(Slots(), Slot{});
    }
    // The slot answering these constraints, moved to the front; nullptr when none does.
    [[nodiscard]] const Slot* Find(const LayoutConstraints& constraints, bool maximumIndependent) noexcept
    {
        const std::span<Slot> slots = Slots();
        for (size_t index = 0u; index < slots.size(); ++index)
        {
            const Slot& slot = slots[index];
            if ((slot.flags & kValid) == 0u || slot.minimumWidth != constraints.minimumDip.width || slot.minimumHeight != constraints.minimumDip.height)
                continue;
            const bool anyMaximum = maximumIndependent && (slot.flags & kMaximumIndependent) != 0u;
            if (! anyMaximum && (slot.maximumWidth != Maximum(constraints.maximumWidthDip) || slot.maximumHeight != Maximum(constraints.maximumHeightDip)))
                continue;
            std::rotate(slots.begin(), slots.begin() + static_cast<std::ptrdiff_t>(index), slots.begin() + static_cast<std::ptrdiff_t>(index) + 1);
            return &slots.front();
        }
        return nullptr;
    }
    // Stores a result in front; returns whether it displaced a valid slot.
    bool Store(const LayoutConstraints& constraints, const DesiredSize* result, bool maximumIndependent) noexcept
    {
        const std::span<Slot> slots = Slots();
        const bool evicted          = (slots.back().flags & kValid) != 0u;
        std::rotate(slots.begin(), slots.end() - 1, slots.end());
        Slot& slot         = slots.front();
        slot.minimumWidth  = constraints.minimumDip.width;
        slot.minimumHeight = constraints.minimumDip.height;
        slot.maximumWidth  = Maximum(constraints.maximumWidthDip);
        slot.maximumHeight = Maximum(constraints.maximumHeightDip);
        slot.width         = result ? result->sizeDip.width : 0.0f;
        slot.height        = result ? result->sizeDip.height : 0.0f;
        slot.baseline      = result && result->baselineDip ? *result->baselineDip : -1.0f;
        slot.flags =
            static_cast<uint8_t>(kValid | (result ? (result->overflow ? kOverflow : 0u) : kNotImplemented) | (maximumIndependent ? kMaximumIndependent : 0u));
        return evicted;
    }
    [[nodiscard]] static DesiredSize Result(const Slot& slot, const LayoutConstraints& constraints) noexcept
    {
        DesiredSize result{.sizeDip = D2D1::SizeF(slot.width, slot.height)};
        if (slot.baseline >= 0.0f)
            result.baselineDip = slot.baseline;
        // A maximum-independent result exceeds the maximum exactly when its size does; any other slot was filled at these
        // very constraints.
        result.overflow = (slot.flags & kMaximumIndependent) != 0u ? (constraints.maximumWidthDip && slot.width > *constraints.maximumWidthDip) ||
                                                                         (constraints.maximumHeightDip && slot.height > *constraints.maximumHeightDip)
                                                                   : (slot.flags & kOverflow) != 0u;
        return result;
    }
};

void Control::MeasureCacheDeleter::operator()(MeasureCache* cache) const noexcept
{
    // Context and slots are trivially destructible: the storage Create took is released as one block.
    ::operator delete(static_cast<void*>(cache));
}

size_t Control::GetMeasureCacheEntries() const noexcept
{
    if (! _measureCache)
        return 0u;
    return static_cast<size_t>(std::ranges::count_if(std::as_const(*_measureCache).Slots(),
                                                     [](const MeasureCache::Slot& slot) noexcept { return (slot.flags & MeasureCache::kValid) != 0u; }));
}

size_t Control::GetMeasureCacheBytes() const noexcept
{
    return _measureCache ? MeasureCache::Bytes(_measureCache->capacity) : 0u;
}

void Control::DropMeasureCache() const noexcept
{
    static_assert(sizeof(_measureCache) == sizeof(void*), "the empty deleter must add nothing to every control");
    static_assert(std::is_trivially_destructible_v<MeasureCache> && std::is_trivially_destructible_v<MeasureCache::Slot> &&
                  alignof(MeasureCache::Slot) <= alignof(MeasureCache) && sizeof(MeasureCache) % alignof(MeasureCache::Slot) == 0u);
    static_assert(MeasureCache::Bytes(MeasureCache::kMaximumIndependentSlots) == 96u && MeasureCache::Bytes(MeasureCache::kMaximumDependentSlots) == 160u,
                  "a measured control's cache: a 32-byte context and two or four 32-byte slots (W6.2 receipt)");
    _measureCache.reset();
}
bool Control::MeasureDependsOnMaximum() const noexcept
{
    return true;
}

bool AreValidLayoutConstraints(const LayoutConstraints& constraints) noexcept
{
    return IsExtent(constraints.minimumDip.width) && IsExtent(constraints.minimumDip.height) &&
           (! constraints.maximumWidthDip || (IsExtent(*constraints.maximumWidthDip) && *constraints.maximumWidthDip >= constraints.minimumDip.width)) &&
           (! constraints.maximumHeightDip || (IsExtent(*constraints.maximumHeightDip) && *constraints.maximumHeightDip >= constraints.minimumDip.height));
}

HRESULT Control::Measure(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const noexcept
{
    return MeasureCounted(context, constraints, result, nullptr);
}

HRESULT Control::MeasureCounted(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result, bool* executed) const noexcept
{
    const Failure::Scope outerFailureScope;
    if (executed)
        *executed = false;
    if (! AreValidLayoutConstraints(constraints))
        return E_INVALIDARG;
    if (IsRenderStageActiveForDebug())
    {
        EmitRenderMutationBlockedForDebug();
        return E_UNEXPECTED;
    }
    if (_host && _host != &context.host)
        return E_INVALIDARG;
    // Resolve the host's lazy device-independent typography before capturing a measurement epoch. This is
    // opt-in preparation; unsupported controls still return E_NOTIMPL if the host has no font resources.
    static_cast<void>(context.host.GetWriteFactory());
    const auto lifetime      = GetLifetimeToken();
    const auto* const parent = _parent;
    const auto* const host   = _host;
    const auto revision      = _measureRevision;
    const auto geometry      = context.host.GetInteractionRevision();
    const auto focus         = context.host._focusRequestRevision;
    const auto typography    = context.host.GetTypographyRevision();
    const auto bounds        = GetBounds();
    // The cache context: everything a library measurement reads besides the control's own revisioned inputs.
    const float dpi               = context.host.GetDpi();
    const FlowDirection flow      = GetFlowDirection();
    const Density density         = GetDensity();
    const bool maximumIndependent = ! MeasureDependsOnMaximum();
    auto& work                    = Detail::Work().layout;
    const auto observe            = [&](bool ran) noexcept
    {
        if (const Detail::MeasureProbeObserver observer = g_measureProbeObserver)
            observer({this, &context.host, revision, typography, dpi, flow, density, constraints, ran}, g_measureProbeContext);
    };
    if (MeasureCache* const cache = _measureCache.get(); cache && cache->Holds(&context.host, revision, typography, dpi, flow, density))
    {
        if (const MeasureCache::Slot* const slot = cache->Find(constraints, maximumIndependent))
        {
            ++work.measureCacheHits;
            observe(false);
            const bool notImplemented = (slot->flags & MeasureCache::kNotImplemented) != 0u;
            const DesiredSize cached  = MeasureCache::Result(*slot, constraints);
#if defined(_DEBUG)
            if (g_verifyMeasureCacheHits)
            {
                // The rule (InvalidateMeasure on every input change) checked: an unbudgeted re-run must agree with the hit.
                DesiredSize fresh;
                HRESULT verified = E_FAIL;
                try
                {
                    verified = MeasureOverride(MeasureContext{context.host}, constraints, fresh);
                }
                catch (const std::bad_alloc&)
                {
                    Failure::FailAllocation();
                }
                catch (const std::exception&)
                {
                    Failure::FailUnexpected();
                }
                if (lifetime.expired())
                    return E_ABORT;
                if (SUCCEEDED(verified))
                    ApplyMinimum(constraints, fresh);
                if (notImplemented ? verified != E_NOTIMPL : (FAILED(verified) || ! SameDesired(fresh, cached)))
                    ++work.measureCacheVerifyMismatches;
            }
#endif
            if (notImplemented)
                return E_NOTIMPL;
            result = cached;
            return S_OK;
        }
    }
    ++work.measureCacheMisses;
    observe(true);
    DesiredSize staged;
    HRESULT hr = E_FAIL;
    try
    {
        ++work.measureOverrideExecutions;
        if (executed)
            *executed = true;
        hr = MeasureOverride(context, constraints, staged);
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    catch (const std::exception&)
    {
        Failure::FailUnexpected();
    }
    if (lifetime.expired())
        return E_ABORT;
    if (_parent != parent || _host != host || _measureRevision != revision || context.host.GetInteractionRevision() != geometry ||
        context.host._focusRequestRevision != focus || context.host.GetTypographyRevision() != typography || ! SameRect(GetBounds(), bounds))
        return E_ABORT;
    if (FAILED(hr) && hr != E_NOTIMPL)
        return hr;
    if (SUCCEEDED(hr))
    {
        if (! IsExtent(staged.sizeDip.width) || ! IsExtent(staged.sizeDip.height) ||
            (staged.baselineDip && (! IsExtent(*staged.baselineDip) || *staged.baselineDip > staged.sizeDip.height)))
            return E_INVALIDARG;
        ApplyMinimum(constraints, staged);
    }
    // Only a result whose context and revision held through the measurement reaches here: store it.
    try
    {
        const uint8_t capacity = maximumIndependent ? MeasureCache::kMaximumIndependentSlots : MeasureCache::kMaximumDependentSlots;
        if (! _measureCache || _measureCache->capacity < capacity)
            _measureCache.reset(MeasureCache::Create(capacity));
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    if (! _measureCache->Holds(&context.host, revision, typography, dpi, flow, density))
        _measureCache->Reset(&context.host, revision, typography, dpi, flow, density);
    if (_measureCache->Store(constraints, SUCCEEDED(hr) ? &staged : nullptr, maximumIndependent))
        ++work.measureCacheEvictions;
    if (FAILED(hr))
        return hr;
    result = staged;
    return hr;
}

uint64_t Control::GetMeasureRevision() const noexcept
{
    return _measureRevision;
}

HRESULT Control::MeasureOverride(const MeasureContext& /*context*/, const LayoutConstraints& /*constraints*/, DesiredSize& /*result*/) const
{
    return E_NOTIMPL;
}

HRESULT Label::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    const Failure::Scope outerFailureScope;
    if (_typographyOverride)
    {
        const size_t shapedUnits = _text.empty() ? 2u : _text.size();
        if (_text.size() > kMaxIntrinsicTextUnits || (context.remainingTextUnits && *context.remainingTextUnits < shapedUnits))
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        if (context.remainingTextUnits)
            *context.remainingTextUnits -= shapedUnits;
        auto* const factory = context.host.GetWriteFactory();
        auto* const format  = context.host.GetTextFormat(_fontRole,
                                                         _typographyOverride->fontSizeDip,
                                                         _typographyOverride->fontWeight,
                                                         DWRITE_TEXT_ALIGNMENT_LEADING,
                                                         DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
                                                         _multiline,
                                                         ResolveReadingDirection(GetFlowDirection()));
        if (! factory || ! format)
            return E_FAIL;
        const std::wstring_view text = _text.empty() ? std::wstring_view(L"Ag") : std::wstring_view(_text);
        const float width            = _multiline && constraints.maximumWidthDip ? *constraints.maximumWidthDip : (std::numeric_limits<float>::max)();
        wil::com_ptr<IDWriteTextLayout> layout;
        HRESULT hr =
            CountedCreateTextLayout(factory, text.data(), static_cast<UINT32>(text.size()), format, width, (std::numeric_limits<float>::max)(), layout.put());
        if (FAILED(hr) || ! layout)
            return FAILED(hr) ? hr : E_FAIL;
        const DWRITE_TEXT_RANGE range{0u, static_cast<UINT32>(text.size())};
        const float halfSpacing = _typographyOverride->characterSpacingDip * 0.5f;
        if (halfSpacing != 0.0f)
        {
            wil::com_ptr<IDWriteTextLayout1> layout1;
            hr = layout->QueryInterface(IID_PPV_ARGS(layout1.put()));
            if (FAILED(hr))
                return hr;
            hr = layout1->SetCharacterSpacing(halfSpacing, halfSpacing, 0.0f, range);
            if (FAILED(hr))
                return hr;
        }
        DWRITE_TEXT_METRICS metrics{};
        hr = layout->GetMetrics(&metrics);
        if (FAILED(hr))
            return hr;
        DWRITE_LINE_METRICS firstLine{};
        hr = ReadFirstLineMetrics(*layout, firstLine);
        if (FAILED(hr))
            return hr;
        result.sizeDip     = D2D1::SizeF((_text.empty() ? 0.0f : (_multiline ? metrics.width : metrics.widthIncludingTrailingWhitespace)), metrics.height);
        result.baselineDip = firstLine.baseline;
        ApplyMinimum(constraints, result);
        return S_OK;
    }
    return MeasureText(context, constraints, _text, _fontRole, GetFlowDirection(), _multiline, 0.0f, 0.0f, 0.0f, 0.0f, result);
}

// Single-line text lays out at an unbounded width: only the overflow flag reads the maximum.
bool Label::MeasureDependsOnMaximum() const noexcept
{
    return _multiline;
}

bool Button::MeasureDependsOnMaximum() const noexcept
{
    return IsMultiline() && GetVariant() == ButtonVariant::Standard;
}

bool Toggle::MeasureDependsOnMaximum() const noexcept
{
    return false;
}

bool Checkbox::MeasureDependsOnMaximum() const noexcept
{
    return IsMultiline();
}

bool RadioButton::MeasureDependsOnMaximum() const noexcept
{
    return false;
}

bool TextField::MeasureDependsOnMaximum() const noexcept
{
    return _multiline;
}

bool ComboBox::MeasureDependsOnMaximum() const noexcept
{
    return false;
}

HRESULT Button::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    const auto variant = GetVariant();
    float chrome       = 24.0f;
    if (variant == ButtonVariant::DropDown)
        chrome += kButtonDropDownChevronWidthDip;
    else if (variant == ButtonVariant::Split)
        chrome += kButtonSplitDropDownSegmentDip;
    else if (variant == ButtonVariant::Selector)
        chrome += kButtonSelectorChevronWidthDip;
    const bool decorated = variant == ButtonVariant::Standard || variant == ButtonVariant::DropDown || variant == ButtonVariant::Repeat;
    if (decorated && ! GetLeadingIcon().empty())
        chrome += 28.0f;
    if (decorated && ! GetShortcutText().empty())
    {
        DesiredSize hint;
        const HRESULT measured = MeasureText(context, {}, GetShortcutText(), FontRole::Small, GetFlowDirection(), false, 10.0f, 0.0f, 0.0f, 20.0f, hint);
        if (FAILED(measured))
            return measured;
        chrome += hint.sizeDip.width + 10.0f;
    }
    const bool icon            = variant == ButtonVariant::IconOnly || variant == ButtonVariant::Disclosure;
    const bool wrap            = IsMultiline() && variant == ButtonVariant::Standard;
    const bool overrideContent = GetContentFontSizeDip().has_value() && ! GetDisclosureExpanded().has_value();
    const auto role =
        icon ? (variant == ButtonVariant::IconOnly && overrideContent && ! context.host.HasFluentIconFont() ? FontRole::Body : FontRole::Icon) : FontRole::Body;
    const HRESULT hr = MeasureText(context,
                                   constraints,
                                   GetText(),
                                   role,
                                   GetFlowDirection(),
                                   wrap,
                                   icon ? 16.0f : chrome,
                                   16.0f,
                                   8.0f,
                                   InteractiveHeight(*this),
                                   result,
                                   overrideContent ? ResolveContentTextFormat(context.host, role, wrap) : nullptr);
    if (SUCCEEDED(hr) && icon)
    {
        result.sizeDip.width = (std::max)(result.sizeDip.width, InteractiveHeight(*this));
        ApplyMinimum(constraints, result);
    }
    return hr;
}

HRESULT Toggle::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    const auto measure = [&](std::wstring_view text, DesiredSize& size)
    {
        return MeasureText(context,
                           constraints,
                           text,
                           FontRole::Body,
                           GetFlowDirection(),
                           false,
                           kToggleTrackWidthDip + kToggleRowPaddingXDip + kToggleTextTrackGapDip + kToggleTrackTrailingPaddingDip + 2.0f,
                           6.0f,
                           3.0f,
                           InteractiveHeight(*this),
                           size);
    };
    if (! GetText().empty())
        return measure(GetText(), result);
    if (_uncheckedText.empty() && _checkedText.empty())
    {
        result.sizeDip = D2D1::SizeF(kToggleTrackWidthDip + 16.0f, (std::max)(InteractiveHeight(*this), kToggleTrackHeightDip + 12.0f));
        ApplyMinimum(constraints, result);
        return S_OK;
    }
    DesiredSize unchecked;
    DesiredSize checked;
    HRESULT hr = measure(_uncheckedText, unchecked);
    if (FAILED(hr))
        return hr;
    hr = measure(_checkedText, checked);
    if (FAILED(hr))
        return hr;
    result                = unchecked;
    result.sizeDip.width  = (std::max)(unchecked.sizeDip.width, checked.sizeDip.width);
    result.sizeDip.height = (std::max)(unchecked.sizeDip.height, checked.sizeDip.height);
    result.overflow       = unchecked.overflow || checked.overflow;
    return S_OK;
}

HRESULT Checkbox::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    return MeasureText(context, constraints, GetText(), FontRole::Body, GetFlowDirection(), IsMultiline(), 36.0f, 6.0f, 3.0f, InteractiveHeight(*this), result);
}

HRESULT RadioButton::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    // Paint draws the label from 35 DIP after the leading edge (inset, circle and gap) to 5 DIP before the trailing one.
    return MeasureText(context, constraints, GetText(), FontRole::Body, GetFlowDirection(), false, 40.0f, 6.0f, 3.0f, InteractiveHeight(*this), result);
}

HRESULT TextField::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    const float vertical = IsCompactDensity() ? 2.0f : 4.0f;
    const float top      = _hasExplicitVerticalTextPadding ? _textPaddingTopDip : vertical;
    const float bottom   = _hasExplicitVerticalTextPadding ? _textPaddingBottomDip : vertical;
    // Reserve the potential trailing action even while it is hidden, so editing/focus cannot resize the field.
    const bool action = ! _multiline && (_clearButtonEnabled || (_masked && _passwordRevealMode == PasswordRevealMode::Peek));
    const float right = action ? (std::max)(38.0f, _textPaddingRightDip) : _textPaddingRightDip;
    std::shared_ptr<const std::wstring> display;
    if (_multiline && UsesMaskedDisplayText())
    {
        if (_text->size() > kMaxIntrinsicTextUnits)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        display = GetDisplayText();
    }
    const std::wstring_view text = _multiline ? (display ? std::wstring_view(*display) : GetText()) : std::wstring_view{};
    return MeasureText(context,
                       constraints,
                       text,
                       _fontRole,
                       GetFlowDirection(),
                       _multiline,
                       _textPaddingLeftDip + right,
                       top + bottom,
                       top,
                       InteractiveHeight(*this),
                       result);
}

HRESULT ComboBox::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    return MeasureText(context,
                       constraints,
                       {},
                       FontRole::Body,
                       GetFlowDirection(),
                       false,
                       16.0f + kButtonSelectorChevronWidthDip,
                       8.0f,
                       4.0f,
                       InteractiveHeight(*this),
                       result);
}

HRESULT NumericStepper::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    if (! _field)
        return E_UNEXPECTED;
    const float chrome =
        kButtonWidthDip + kGapDip + (_labelWidthDip > 0.0f ? _labelWidthDip + kGapDip : 0.0f) + (_unitWidthDip > 0.0f ? _unitWidthDip + kGapDip : 0.0f);
    LayoutConstraints fieldConstraints = constraints;
    fieldConstraints.minimumDip.width  = (std::max)(0.0f, constraints.minimumDip.width - chrome);
    if (constraints.maximumWidthDip)
        fieldConstraints.maximumWidthDip = (std::max)(fieldConstraints.minimumDip.width, *constraints.maximumWidthDip - chrome);
    const HRESULT hr = _field->Measure(context, fieldConstraints, result);
    if (FAILED(hr))
        return hr;
    result.sizeDip.width += chrome;
    ApplyMinimum(constraints, result);
    return S_OK;
}

FlexPanel::FlexPanel()
{
    EnableIntrinsicLayout();
}

HRESULT FlexPanel::SetOrientation(StackOrientation orientation) noexcept
{
    if (orientation != StackOrientation::Horizontal && orientation != StackOrientation::Vertical)
        return E_INVALIDARG;
    Assign(_orientation, orientation, Affects::Layout, [this]() noexcept { ++_layoutRevision; });
    return S_OK;
}

StackOrientation FlexPanel::GetOrientation() const noexcept
{
    return _orientation;
}

HRESULT FlexPanel::SetGap(float gapDip) noexcept
{
    if (! IsExtent(gapDip))
        return E_INVALIDARG;
    Assign(_gapDip, gapDip, Affects::Layout, [this]() noexcept { ++_layoutRevision; });
    return S_OK;
}

float FlexPanel::GetGap() const noexcept
{
    return _gapDip;
}

HRESULT FlexPanel::SetPadding(float leftDip, float topDip, float rightDip, float bottomDip) noexcept
{
    if (! IsExtent(leftDip) || ! IsExtent(topDip) || ! IsExtent(rightDip) || ! IsExtent(bottomDip) || ! IsExtent(leftDip + rightDip) ||
        ! IsExtent(topDip + bottomDip))
        return E_INVALIDARG;
    Assign(_padding, D2D1::RectF(leftDip, topDip, rightDip, bottomDip), Affects::Layout, [this]() noexcept { ++_layoutRevision; });
    return S_OK;
}

HRESULT FlexPanel::SetWrap(bool wrap) noexcept
{
    Assign(_wrap, wrap, Affects::Layout, [this]() noexcept { ++_layoutRevision; });
    return S_OK;
}

bool FlexPanel::IsWrapping() const noexcept
{
    return _wrap;
}

HRESULT FlexPanel::SynchronizeRecords() const noexcept
{
    const Failure::Scope outerFailureScope;
    const auto children = GetChildren();
    // The cap counts live children: a slot TakeChild emptied holds nothing to lay out.
    if (static_cast<size_t>(std::ranges::count_if(children, [](const auto& child) noexcept { return child != nullptr; })) > kMaxLayoutChildren)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    // Even a pure nested measure replaces these reusable arrangement records. Retire any caller's scratch
    // before resizing/resetting it, including synchronization for an unsuccessful option lookup.
    ++_scratchRevision;
    try
    {
        _records.resize(children.size());
        _lines.resize(children.size());
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    for (size_t index = 0u; index < children.size(); ++index)
    {
        auto& record            = _records[index];
        const auto* const child = children[index].get();
        if (record.child != child || (child && record.lifetime.expired()))
        {
            record       = {};
            record.child = child;
            if (child)
                record.lifetime = GetControlLifetimeToken(*child);
        }
    }
    return S_OK;
}

size_t FlexPanel::CompactChildren() noexcept
{
    // A child's options live in the record at its index, which SynchronizeRecords matches by index: move each live child's
    // record to the child's new index before the slots close, so no option moves to another child or resets.
    const auto children = GetChildren();
    size_t kept         = 0u;
    for (size_t index = 0u; index < children.size(); ++index)
    {
        const Control* const child = children[index].get();
        if (! child)
            continue;
        if (kept < _records.size())
        {
            if (index < _records.size() && _records[index].child == child)
            {
                if (kept != index)
                    _records[kept] = std::move(_records[index]);
            }
            else
            {
                _records[kept] = {};
            }
        }
        ++kept;
    }
    if (kept < _records.size())
        _records.erase(_records.begin() + static_cast<std::ptrdiff_t>(kept), _records.end());
    ++_scratchRevision;
    return Panel::CompactChildren();
}

// Control::Assign's equality for a child's options (AssignEquality): every member.
[[nodiscard]] static bool SameAssignedValue(const FlexChildOptions& a, const FlexChildOptions& b) noexcept
{
    return AssignEquality::Same(a.constraints.minimumDip, b.constraints.minimumDip) &&
           AssignEquality::Same(a.constraints.maximumWidthDip, b.constraints.maximumWidthDip) &&
           AssignEquality::Same(a.constraints.maximumHeightDip, b.constraints.maximumHeightDip) && AssignEquality::Same(a.explicitSizeDip, b.explicitSizeDip) &&
           AssignEquality::Same(a.grow, b.grow) && a.alignment == b.alignment;
}

HRESULT FlexPanel::SetChildOptions(const Control* child, const FlexChildOptions& options) noexcept
{
    if (! child || ! ValidOptions(options))
        return E_INVALIDARG;
    HRESULT hr = SynchronizeRecords();
    if (FAILED(hr))
        return hr;
    for (auto& record : _records)
    {
        if (record.child == child)
        {
            Assign(record.options, options, Affects::Layout, [this]() noexcept { ++_layoutRevision; });
            return S_OK;
        }
    }
    return E_INVALIDARG;
}

HRESULT FlexPanel::BuildLayout(const MeasureContext& context,
                               const LayoutConstraints& constraints,
                               DesiredSize& result,
                               const std::optional<D2D1_RECT_F>& arrangementBounds) const noexcept
{
    const Failure::Scope outerFailureScope;
    if (_buildingLayout)
        return E_ABORT;
    const auto origin = arrangementBounds ? D2D1::Point2F(arrangementBounds->left, arrangementBounds->top) : D2D1::Point2F();
    if (! std::isfinite(origin.x) || ! std::isfinite(origin.y))
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    const auto lifetime = GetLifetimeToken();
    _buildingLayout     = true;
    const auto building = wil::scope_exit([this, lifetime]() noexcept
    {
        if (! lifetime.expired())
            _buildingLayout = false;
    });
    HRESULT hr          = SynchronizeRecords();
    if (FAILED(hr))
        return hr;
    size_t remainingTextUnits     = kMaxIntrinsicTextUnits;
    MeasureContext boundedContext = context;
    if (! boundedContext.remainingTextUnits)
        boundedContext.remainingTextUnits = &remainingTextUnits;
    const bool horizontal     = _orientation == StackOrientation::Horizontal;
    const float paddingWidth  = _padding.left + _padding.right;
    const float paddingHeight = _padding.top + _padding.bottom;
    const auto contentMaximum = [](const std::optional<float>& maximum, float padding) noexcept -> std::optional<float>
    { return maximum ? std::optional<float>((std::max)(0.0f, *maximum - padding)) : std::nullopt; };
    const auto widthMaximum        = contentMaximum(constraints.maximumWidthDip, paddingWidth);
    const auto heightMaximum       = contentMaximum(constraints.maximumHeightDip, paddingHeight);
    const auto mainMaximum         = horizontal ? widthMaximum : heightMaximum;
    const auto crossMaximum        = horizontal ? heightMaximum : widthMaximum;
    const auto revision            = GetMeasureRevision();
    const auto layoutRevision      = _layoutRevision;
    const auto scratchRevision     = _scratchRevision;
    const auto geometry            = context.host.GetInteractionRevision();
    const auto focus               = context.host._focusRequestRevision;
    const auto* const attachedHost = GetHost();
    const size_t count             = _records.size();
    size_t lineCount               = 0u;
    bool overflow                  = false;
    for (auto& line : _lines)
        line = {};
    for (size_t index = 0u; index < count; ++index)
    {
        const auto* const child     = GetChildren()[index].get();
        _records[index].visible     = child && child->IsVisible();
        _records[index].entryBounds = child ? child->GetBounds() : D2D1::RectF();
    }
    const auto childrenUnchanged = [&]() noexcept
    {
        if (_scratchRevision != scratchRevision || GetChildren().size() != count)
            return false;
        for (size_t index = 0u; index < count; ++index)
        {
            const auto* const child = GetChildren()[index].get();
            if (child != _records[index].child || (child && (_records[index].lifetime.expired() || child->IsVisible() != _records[index].visible ||
                                                             ! SameRect(child->GetBounds(), _records[index].entryBounds))))
                return false;
        }
        return true;
    };
    for (size_t index = 0u; index < count; ++index)
    {
        auto* const child       = GetChildren()[index].get();
        _records[index].visible = child && child->IsVisible();
        if (! _records[index].visible)
            continue;
        const auto childLifetime           = _records[index].lifetime;
        const auto options                 = _records[index].options;
        LayoutConstraints childConstraints = options.constraints;
        const auto boundAxis               = [](std::optional<float>& maximum, float minimum, const std::optional<float>& available) noexcept
        {
            if (available)
                maximum = (std::max)(minimum, maximum ? (std::min)(*maximum, *available) : *available);
        };
        boundAxis(childConstraints.maximumWidthDip, childConstraints.minimumDip.width, widthMaximum);
        boundAxis(childConstraints.maximumHeightDip, childConstraints.minimumDip.height, heightMaximum);
        DesiredSize measured;
        if (options.explicitSizeDip)
        {
            measured.sizeDip = *options.explicitSizeDip;
            ApplyMinimum(childConstraints, measured);
        }
        else
        {
            // The child's own measure cache answers an unchanged key; only an actual measurement runs callbacks.
            bool executed = false;
            hr            = ControlModelQueryAccess::MeasureCounted(*child, boundedContext, childConstraints, measured, executed);
            if (executed)
            {
                // The measurement can run callbacks that retire this panel: nothing of it is touched before the check.
                if (lifetime.expired())
                    return E_ABORT;
                ++_measureCount;
                if (GetMeasureRevision() != revision || _layoutRevision != layoutRevision || _scratchRevision != scratchRevision || GetHost() != attachedHost ||
                    context.host.GetInteractionRevision() != geometry || context.host._focusRequestRevision != focus || childLifetime.expired() ||
                    GetChildren().size() != count || GetChildren()[index].get() != child || ! child->IsVisible())
                    return E_ABORT;
                if (! childrenUnchanged())
                    return E_ABORT;
            }
            if (FAILED(hr))
                return hr;
        }
        auto& record      = _records[index];
        record.measured   = measured;
        overflow          = overflow || measured.overflow;
        const float main  = horizontal ? measured.sizeDip.width : measured.sizeDip.height;
        const float cross = horizontal ? measured.sizeDip.height : measured.sizeDip.width;
        if (lineCount == 0u)
            lineCount = 1u;
        auto* line        = &_lines[lineCount - 1u];
        const double next = static_cast<double>(line->extent) + (line->count != 0u ? _gapDip : 0.0f) + main;
        if (_wrap && mainMaximum && line->count != 0u && next > *mainMaximum)
            line = &_lines[lineCount++];
        const double extent = static_cast<double>(line->extent) + (line->count != 0u ? _gapDip : 0.0f) + main;
        if (extent > (std::numeric_limits<float>::max)())
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        line->extent      = static_cast<float>(extent);
        line->crossExtent = (std::max)(line->crossExtent, cross);
        ++line->count;
        record.line         = lineCount - 1u;
        record.arrangedMain = main;
    }
    double naturalMain  = 0.0;
    double naturalCross = 0.0;
    for (size_t lineIndex = 0u; lineIndex < lineCount; ++lineIndex)
    {
        naturalMain = (std::max)(naturalMain, static_cast<double>(_lines[lineIndex].extent));
        naturalCross += _lines[lineIndex].crossExtent + (lineIndex != 0u ? _gapDip : 0.0f);
    }
    const double desiredWidth  = (horizontal ? naturalMain : naturalCross) + paddingWidth;
    const double desiredHeight = (horizontal ? naturalCross : naturalMain) + paddingHeight;
    if (desiredWidth > (std::numeric_limits<float>::max)() || desiredHeight > (std::numeric_limits<float>::max)())
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    DesiredSize staged{.sizeDip = D2D1::SizeF(static_cast<float>(desiredWidth), static_cast<float>(desiredHeight)), .overflow = overflow};
    ApplyMinimum(constraints, staged);
    const bool reverseMain  = horizontal && IsRightToLeft();
    const bool reverseCross = ! horizontal && IsRightToLeft();
    float paddedX           = 0.0f;
    float paddedY           = 0.0f;
    hr                      = AdvanceLayoutEdge(origin.x, _padding.left, false, std::nullopt, paddedX);
    if (FAILED(hr))
        return hr;
    hr = AdvanceLayoutEdge(origin.y, _padding.top, false, std::nullopt, paddedY);
    if (FAILED(hr))
        return hr;
    const auto trailingEdge = [&](float start,
                                  const std::optional<float>& maximum,
                                  const std::optional<float>& parentFar,
                                  float padding,
                                  std::optional<float>& edge) noexcept -> HRESULT
    {
        if (! maximum)
            return S_OK;
        // The captured parent endpoint is a bound, not a minimum reconstructed from a rounded width.
        const float outer = parentFar ? *parentFar : start + *maximum;
        float inner       = 0.0f;
        HRESULT edgeHr    = AdvanceLayoutEdge(outer, padding, true, std::nullopt, inner);
        if (SUCCEEDED(edgeHr))
            edge = inner;
        return edgeHr;
    };
    std::optional<float> paddedRight;
    std::optional<float> paddedBottom;
    hr = trailingEdge(
        origin.x, constraints.maximumWidthDip, arrangementBounds ? std::optional<float>(arrangementBounds->right) : std::nullopt, _padding.right, paddedRight);
    if (FAILED(hr))
        return hr;
    hr = trailingEdge(origin.y,
                      constraints.maximumHeightDip,
                      arrangementBounds ? std::optional<float>(arrangementBounds->bottom) : std::nullopt,
                      _padding.bottom,
                      paddedBottom);
    if (FAILED(hr))
        return hr;
    const float mainOrigin   = horizontal ? paddedX : paddedY;
    const float crossOrigin  = horizontal ? paddedY : paddedX;
    const auto mainFar       = horizontal ? paddedRight : paddedBottom;
    const auto crossFar      = horizontal ? paddedBottom : paddedRight;
    const auto availableSpan = [](float nearEdge, float farEdge) noexcept
    {
        const double exact = (std::max)(0.0, static_cast<double>(farEdge) - nearEdge);
        float span         = static_cast<float>(exact);
        if (span > exact)
            span = std::nextafter(span, 0.0f);
        return span;
    };
    const float mainAllocationLimit = mainFar ? availableSpan(mainOrigin, *mainFar) : static_cast<float>(naturalMain);
    const float crossAllocation     = crossFar ? availableSpan(crossOrigin, *crossFar) : static_cast<float>(naturalCross);
    float crossCursor               = crossOrigin;
    if (reverseCross)
    {
        if (crossFar && naturalCross <= crossAllocation)
            crossCursor = *crossFar;
        else
        {
            hr = AdvanceLayoutEdge(crossOrigin, (std::max)(crossAllocation, static_cast<float>(naturalCross)), false, std::nullopt, crossCursor);
            if (FAILED(hr))
                return hr;
        }
    }
    for (size_t lineIndex = 0u; lineIndex < lineCount; ++lineIndex)
    {
        auto& line = _lines[lineIndex];
        // Redistribute only among children that can still grow. The number of passes is bounded by this line's size.
        if (mainMaximum && line.extent < mainAllocationLimit)
        {
            float remaining = mainAllocationLimit - line.extent;
            for (size_t pass = 0u; pass < line.count && remaining > 0.0f; ++pass)
            {
                double weights = 0.0;
                for (const auto& record : _records)
                {
                    const auto cap = horizontal ? record.options.constraints.maximumWidthDip : record.options.constraints.maximumHeightDip;
                    if (record.visible && record.line == lineIndex && record.options.grow > 0.0f && (! cap || record.arrangedMain < *cap))
                        weights += record.options.grow;
                }
                if (weights == 0.0)
                    break;
                float distributed = 0.0f;
                for (auto& record : _records)
                {
                    if (! record.visible || record.line != lineIndex || record.options.grow == 0.0f)
                        continue;
                    const auto cap = horizontal ? record.options.constraints.maximumWidthDip : record.options.constraints.maximumHeightDip;
                    float share    = static_cast<float>(static_cast<double>(remaining) * record.options.grow / weights);
                    if (cap)
                        share = (std::min)(share, (std::max)(0.0f, *cap - record.arrangedMain));
                    record.arrangedMain += share;
                    distributed += share;
                }
                if (distributed <= 0.0f)
                    break;
                remaining = (std::max)(0.0f, remaining - distributed);
                line.extent += distributed;
            }
        }
        const float lineCross      = ! _wrap && crossMaximum ? (std::max)(line.crossExtent, crossAllocation) : line.crossExtent;
        const float mainAllocation = mainMaximum ? (std::max)(line.extent, mainAllocationLimit) : line.extent;
        float mainCursor           = mainOrigin;
        if (reverseMain)
        {
            if (mainFar && line.extent <= mainAllocationLimit)
                mainCursor = *mainFar;
            else
            {
                hr = AdvanceLayoutEdge(mainOrigin, mainAllocation, false, std::nullopt, mainCursor);
                if (FAILED(hr))
                    return hr;
            }
        }
        float crossEnd = 0.0f;
        if (! _wrap && crossFar && line.crossExtent <= crossAllocation)
            crossEnd = reverseCross ? crossOrigin : *crossFar;
        else
        {
            hr = AdvanceLayoutEdge(crossCursor, lineCross, reverseCross, std::nullopt, crossEnd);
            if (FAILED(hr))
                return hr;
        }
        const float lineCrossEnd = crossEnd;
        size_t remaining         = line.count;
        for (auto& record : _records)
        {
            if (! record.visible || record.line != lineIndex)
                continue;
            const float natural = horizontal ? record.measured.sizeDip.height : record.measured.sizeDip.width;
            float crossSize     = natural;
            if (record.options.alignment == LayoutAlignment::Stretch)
            {
                crossSize      = (std::max)(crossSize, lineCross);
                const auto cap = horizontal ? record.options.constraints.maximumHeightDip : record.options.constraints.maximumWidthDip;
                if (cap)
                    crossSize = (std::max)(natural, (std::min)(crossSize, *cap));
            }
            const float extra           = (std::max)(0.0f, lineCross - crossSize);
            const float alignmentOffset = record.options.alignment == LayoutAlignment::Center ? extra * 0.5f
                                          : record.options.alignment == LayoutAlignment::End  ? extra
                                                                                              : 0.0f;
            float mainEnd               = 0.0f;
            float crossStart            = 0.0f;
            float childCrossEnd         = 0.0f;
            hr = AdvanceLayoutEdge(mainCursor,
                                   record.arrangedMain,
                                   reverseMain,
                                   horizontal ? record.options.constraints.maximumWidthDip : record.options.constraints.maximumHeightDip,
                                   mainEnd,
                                   horizontal ? record.measured.sizeDip.width : record.measured.sizeDip.height);
            if (FAILED(hr))
                return hr;
            const auto childCrossMaximum = horizontal ? record.options.constraints.maximumHeightDip : record.options.constraints.maximumWidthDip;
            if (record.options.alignment == LayoutAlignment::End)
            {
                childCrossEnd = lineCrossEnd;
                hr            = AdvanceLayoutEdge(childCrossEnd, crossSize, ! reverseCross, childCrossMaximum, crossStart, natural);
            }
            else
            {
                hr = AdvanceLayoutEdge(crossCursor, alignmentOffset, reverseCross, std::nullopt, crossStart);
                if (SUCCEEDED(hr))
                    hr = AdvanceLayoutEdge(crossStart, crossSize, reverseCross, childCrossMaximum, childCrossEnd, natural);
            }
            if (FAILED(hr))
                return hr;
            const float mainLow   = reverseMain ? mainEnd : mainCursor;
            const float mainHigh  = reverseMain ? mainCursor : mainEnd;
            const float crossLow  = reverseCross ? childCrossEnd : crossStart;
            const float crossHigh = reverseCross ? crossStart : childCrossEnd;
            record.bounds         = horizontal ? D2D1::RectF(mainLow, crossLow, mainHigh, crossHigh) : D2D1::RectF(crossLow, mainLow, crossHigh, mainHigh);
            crossEnd              = reverseCross ? (std::min)(crossEnd, childCrossEnd) : (std::max)(crossEnd, childCrossEnd);
            mainCursor            = mainEnd;
            if (--remaining != 0u)
            {
                hr = AdvanceLayoutEdge(mainCursor, _gapDip, reverseMain, std::nullopt, mainCursor);
                if (FAILED(hr))
                    return hr;
            }
        }
        crossCursor = crossEnd;
        if (lineIndex + 1u < lineCount)
        {
            hr = AdvanceLayoutEdge(crossCursor, _gapDip, reverseCross, std::nullopt, crossCursor);
            if (FAILED(hr))
                return hr;
        }
    }
    result = staged;
    return S_OK;
}

HRESULT FlexPanel::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& result) const
{
    return BuildLayout(context, constraints, result);
}

HRESULT FlexPanel::ApplyLayout(const ControlHost& host) noexcept
{
    const Failure::Scope outerFailureScope;
    if (GetHost() && GetHost() != &host)
        return E_INVALIDARG;
    if (IsRenderStageActiveForDebug())
        return E_UNEXPECTED;
    const auto lifetime            = GetLifetimeToken();
    const auto* const attachedHost = GetHost();
    const auto bounds              = GetBounds();
    const auto revision            = GetMeasureRevision();
    const auto focus               = host._focusRequestRevision;
    const auto pass                = ++_layoutRevision;
    if (! IsExtent(bounds.right - bounds.left) || ! IsExtent(bounds.bottom - bounds.top))
        return E_INVALIDARG;
    DesiredSize desired;
    HRESULT hr = BuildLayout({host}, LayoutConstraints::AtMost(bounds.right - bounds.left, bounds.bottom - bounds.top), desired, bounds);
    if (lifetime.expired())
        return E_ABORT;
    if (FAILED(hr))
        return hr;
    const auto scratchRevision = _scratchRevision;
    const size_t count         = _records.size();
    // BuildLayout stages absolute endpoints in line order. Compute their occupied extent before any extensible
    // SetBounds; represented corrections, growth and padding must be reflected in the published content/overflow.
    bool occupied         = false;
    bool geometryOverflow = false;
    D2D1_RECT_F occupiedBounds{};
    for (const auto& record : _records)
    {
        if (! record.visible)
            continue;
        geometryOverflow = geometryOverflow || static_cast<double>(record.bounds.left) - _padding.left < bounds.left ||
                           static_cast<double>(record.bounds.top) - _padding.top < bounds.top ||
                           static_cast<double>(record.bounds.right) + _padding.right > bounds.right ||
                           static_cast<double>(record.bounds.bottom) + _padding.bottom > bounds.bottom;
        if (! occupied)
            occupiedBounds = record.bounds;
        else
        {
            occupiedBounds.left   = (std::min)(occupiedBounds.left, record.bounds.left);
            occupiedBounds.top    = (std::min)(occupiedBounds.top, record.bounds.top);
            occupiedBounds.right  = (std::max)(occupiedBounds.right, record.bounds.right);
            occupiedBounds.bottom = (std::max)(occupiedBounds.bottom, record.bounds.bottom);
        }
        occupied = true;
    }
    D2D1_SIZE_F content = desired.sizeDip;
    if (occupied)
    {
        const double width  = static_cast<double>(occupiedBounds.right) - occupiedBounds.left + _padding.left + _padding.right;
        const double height = static_cast<double>(occupiedBounds.bottom) - occupiedBounds.top + _padding.top + _padding.bottom;
        if (width > (std::numeric_limits<float>::max)() || height > (std::numeric_limits<float>::max)())
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        content.width  = (std::max)(content.width, static_cast<float>(width));
        content.height = (std::max)(content.height, static_cast<float>(height));
    }
    auto& mutableHost = const_cast<ControlHost&>(host);
    mutableHost.BeginIntrinsicArrange(*this);
    for (size_t index = 0u; index < count; ++index)
    {
        if (! _records[index].visible)
            continue;
        auto* const child                   = GetChildren()[index].get();
        const auto childLifetime            = _records[index].lifetime;
        const auto childBounds              = _records[index].bounds;
        const auto* const previousBranch    = mutableHost._intrinsicArrangeBranch;
        const auto foreign                  = mutableHost._intrinsicArrangeForeignRevision;
        const auto geometry                 = mutableHost._interactionRevision;
        const auto acceptedBounds           = mutableHost._intrinsicArrangeBoundsRevision;
        mutableHost._intrinsicArrangeBranch = child;
        ++Detail::Work().layout.arrangedChildren;
        child->SetBounds(childBounds);
        mutableHost._intrinsicArrangeBranch = previousBranch;
        if (lifetime.expired())
            return E_ABORT;
        if (_layoutRevision != pass || _scratchRevision != scratchRevision || GetMeasureRevision() != revision || host._focusRequestRevision != focus ||
            GetHost() != attachedHost || ! SameRect(GetBounds(), bounds) || mutableHost._intrinsicArrangeForeignRevision != foreign ||
            childLifetime.expired() || GetChildren().size() != count ||
            mutableHost._interactionRevision != geometry + (mutableHost._intrinsicArrangeBoundsRevision - acceptedBounds) ||
            GetChildren()[index].get() != child || ! child->IsVisible() || ! SameRect(child->GetBounds(), childBounds))
            return E_ABORT;
        _records[index].entryBounds = childBounds;
        for (size_t check = 0u; check < count; ++check)
        {
            const auto* const current = GetChildren()[check].get();
            if (current != _records[check].child || (current && (_records[check].lifetime.expired() || current->IsVisible() != _records[check].visible ||
                                                                 ! SameRect(current->GetBounds(), _records[check].entryBounds))))
                return E_ABORT;
        }
    }
    _contentSize    = content;
    _layoutOverflow = desired.overflow || geometryOverflow || content.width > bounds.right - bounds.left || content.height > bounds.bottom - bounds.top;
    return S_OK;
}

HRESULT FlexPanel::PrepareIntrinsicLayout(const MeasureContext& context) noexcept
{
    return ApplyLayout(context.host);
}

D2D1_SIZE_F FlexPanel::GetContentSize() const noexcept
{
    return _contentSize;
}

bool FlexPanel::HasLayoutOverflow() const noexcept
{
    return _layoutOverflow;
}

uint64_t FlexPanel::DebugGetMeasureCount() const noexcept
{
    return _measureCount;
}

void FlexPanel::ClearChildren() noexcept
{
    const Failure::Scope outerFailureScope;
    // Retire only the entry snapshot. Panel keeps children added by detach callbacks, including their newer
    // options and any completed layout, so never clear the live scratch after those callbacks return.
    _records.clear();
    _lines.clear();
    ++_scratchRevision;
    ++_layoutRevision;
    Panel::ClearChildren();
}

void FlexPanel::OnBoundsChanged() noexcept
{
    ++_layoutRevision;
    RequestIntrinsicLayout();
}

void FlexPanel::OnHidden() noexcept
{
    // A hidden container retires its children's numeric measurements; showing it measures them once more.
    for (const auto& child : GetChildren())
        if (child)
            ControlModelQueryAccess::DropMeasureCache(*child);
    RequestIntrinsicLayout();
    Panel::OnHidden();
}

void FlexPanel::PropagateHost(ControlHost* host) noexcept
{
    const auto lifetime = GetLifetimeToken();
    Panel::PropagateHost(host);
    if (! lifetime.expired())
        RequestIntrinsicLayout();
}
} // namespace DxUi
