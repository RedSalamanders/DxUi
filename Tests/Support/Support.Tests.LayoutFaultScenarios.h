#pragma once
#include "Support.Tests.CountingWriteFactory.h"
#include "Support.Tests.FaultTextLayout.h"
#include <DxUi/RichTextLayout.h>
#include <atomic>
#include <d2d1_1.h>

namespace RichLayoutFaultTests
{
using namespace DxUi::RichText;
struct Fixture
{
    NativeTextTests::LayoutFaultState fault;
    wil::com_ptr<IDWriteFactory> native;
    Microsoft::WRL::ComPtr<NativeTextTests::CountingWriteFactory> factory;
    wil::com_ptr<ID2D1Factory> d2d;
    wil::com_ptr<ID2D1DCRenderTarget> target;
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    Projection projection;
    LayoutOptions options;
    PreparedLayout layout;
    bool Initialize()
    {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(native.put()))))
            return false;
        factory = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(native.get());
        if (! factory)
            return false;
        factory->wrapContext = &fault;
        factory->wrapLayout  = NativeTextTests::WrapFaultLayout;
        Document doc(L"link plain");
        projection = Project(doc.Snapshot(), doc.Revision(), ViewMode::Rich);
        projection.runs.clear();
        projection.runs.push_back({0u, 4u, Link, 0u});
        projection.runs.push_back({4u, 6u, Regular, 0u});
        options.widthDip    = 420.f;
        options.fontSizeDip = 24.f;
        if (FAILED(layout.TryPrepare(factory.Get(), projection, options)))
            return false;
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.put())))
            return false;
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        if (FAILED(d2d->CreateDCRenderTarget(&properties, target.put())))
            return false;
        if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), brush.put())))
            return false;
        fault.effectCalls = fault.underlineCalls = 0u;
        return true;
    }
};
inline bool PreparationPreservesPublishedState()
{
    Fixture f;
    if (! f.Initialize())
        return false;
    const auto generation = f.layout.Generation();
    const auto original   = f.layout.Content().source;
    const auto caret      = f.layout.Caret(4u);
    const auto height     = f.layout.Height();
    Document changed(L"replacement text");
    const auto replacement  = Project(changed.Snapshot(), changed.Revision(), ViewMode::Rich);
    f.fault.failLineMetrics = true;
    for (const HRESULT failure : {E_FAIL, E_OUTOFMEMORY})
    {
        f.fault.failure           = failure;
        const auto beforeFailures = f.fault.lineFailures;
        const auto hr             = f.layout.TryPrepare(f.factory.Get(), replacement, f.options);
        const auto after          = f.layout.Caret(4u);
        if (hr != failure || f.fault.lineFailures != beforeFailures + 1u || ! f.layout.Ready() || f.layout.Generation() != generation ||
            f.layout.Content().source != original || f.layout.Height() != height || after.x != caret.x || after.y != caret.y || after.height != caret.height)
            return false;
    }
    f.fault.failLineMetrics = false;
    return SUCCEEDED(f.layout.TryPrepare(f.factory.Get(), replacement, f.options)) && f.layout.Generation() == generation + 1u &&
           f.layout.Content().source == changed.Text();
}
inline bool SetterFailureRestores(bool effect)
{
    Fixture f;
    if (! f.Initialize())
        return false;
    LinkDrawing links;
    links.underline = false;
    if (effect)
    {
        links.emphasis       = MarkdownRange{0u, 4u};
        links.emphasisBrush  = f.brush.get();
        f.fault.failEffectAt = 0u;
    }
    else
        f.fault.failUnderlineAt = 0u;
    const auto hr = f.layout.TryDraw(f.target.get(), f.brush.get(), {}, &links);
    if (hr != f.fault.failure || (effect ? f.fault.effectCalls != 2u : f.fault.underlineCalls != 2u))
        return false;
    wil::com_ptr<IUnknown> stored;
    BOOL underline = FALSE;
    return SUCCEEDED(f.fault.lastLayout->GetDrawingEffect(0u, stored.put(), nullptr)) && ! stored &&
           SUCCEEDED(f.fault.lastLayout->GetUnderline(0u, &underline, nullptr)) && underline;
}
inline std::atomic<unsigned> rollbackOriginMarker{};
__declspec(noinline) inline void LayoutRollbackFailureOrigin(Fixture& f, bool effect)
{
    rollbackOriginMarker.store(effect ? 1u : 2u, std::memory_order_relaxed);
    LinkDrawing links;
    links.underline = false;
    if (effect)
    {
        links.emphasis        = MarkdownRange{0u, 4u};
        links.emphasisBrush   = f.brush.get();
        f.fault.failEffectAt  = 0u;
        f.fault.failEffectAt2 = 1u;
    }
    else
    {
        f.fault.failUnderlineAt  = 0u;
        f.fault.failUnderlineAt2 = 1u;
    }
    (void)f.layout.TryDraw(f.target.get(), f.brush.get(), {}, &links);
    rollbackOriginMarker.store(3u, std::memory_order_relaxed); // Unreachable; prevents a tail call.
}
template <bool Effect> void RollbackFailureChild()
{
    Fixture f;
    if (! f.Initialize())
        return;
    LayoutRollbackFailureOrigin(f, Effect);
}
} // namespace RichLayoutFaultTests
