#pragma once
// Counting COM wrappers around the Direct2D factory, device and device context, and (through CountingWriteFactory) the
// DirectWrite factory, for test executables only (Specs/Core/Core_PerformanceAndResources.md, "Work counters"). Installed
// through the private creation seam Detail::FactoryInterposer, they observe what actually reaches Direct2D and DirectWrite,
// independently of DxUi's source: every successful Create... by resource kind, and every Direct2D draw call, which they add
// to Detail::Work().frame.drawCalls (the only place that counter is incremented). While a test installs a CallTicks record
// (Support.Tests.CallTicks.h, hardware attribution only), they also time draw calls, BeginDraw, EndDraw and Flush.
//
// Pass-through rules. Every method forwards to the real object with the caller's arguments unchanged and returns its result.
// Objects are wrapped only where DxUi itself holds them: the factory (ID2D1Factory1, the highest version DxUi requests), the
// device (ID2D1Device) and each device context (up to ID2D1DeviceContext7, the highest version DxUi queries). Everything they
// create is the real object, so no wrapper is ever handed to Direct2D, DirectWrite, DirectComposition or an effect. The
// wrappers report each other where Direct2D would report the real objects (a context's GetDevice and GetFactory, a device's
// GetFactory), so a resource created through them is still seen. QueryInterface answers a wrapped version with the wrapper
// itself (one COM identity) only when the real object implements that version; any other interface is answered by the real
// object and counted in escapedInterfaces, since calls through it are no longer seen. A wrapper holds one reference to its
// real object and releases it when it is destroyed, so the real object's lifetime is the wrapper's.
#include "Support.Tests.CallTicks.h"
#include "Support.Tests.CountingWriteFactory.h"
#include "Support/NativeRendering.h"
#include "Support/WorkCounters.h"

#include <array>
#include <d2d1_3.h>
#include <new>
#include <span>
#include <wil/com.h>

// COM wrappers have one identity and are never copied or moved; their bases delete those operations.
// Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace NativeWorkTests
{
// What the Direct2D wrappers of one installation saw, on the owner thread. Kinds follow Detail::ResourceKind; only created
// counts are kept. Draw calls go to Detail::Work().frame.drawCalls instead, beside the record's other frame counters.
struct Direct2DWork final
{
    DxUi::Detail::ResourceWork seen;
    uint64_t otherCreations    = 0u;      // Objects of kinds the counted helpers do not count (a device, a mesh, a render target...).
    const char* lastOther      = nullptr; // The method that made the latest of them, for diagnostics.
    uint64_t unwrappedTargets  = 0u;      // Render targets created through a wrapper and returned unwrapped: their work is unseen.
    uint64_t escapedInterfaces = 0u;      // Interfaces the real object answered outside the wrapped versions.
    uint64_t wrappedFactories  = 0u;
    uint64_t wrappedDevices    = 0u;
    uint64_t wrappedContexts   = 0u;
    uint64_t liveWrappers      = 0u; // Wrappers not yet destroyed: 0 once the host released everything it created.
};

namespace Detail
{
inline void NoteDraw() noexcept
{
    ++DxUi::Detail::Work().frame.drawCalls;
}
// One forwarded draw call: counted, and timed into CallTicks::drawCalls while a test installs a CallTicks record (W0.7).
struct DrawCall final
{
    DrawCall() noexcept
    {
        NoteDraw();
    }
    TimedCall<&CallTicks::drawCalls> timed;
};
template <typename Interface> inline void NoteCreated(Direct2DWork& work, HRESULT hr, DxUi::Detail::ResourceKind kind, Interface** object) noexcept
{
    if (SUCCEEDED(hr) && object && *object)
        ++DxUi::Detail::Tally(work.seen, kind).created;
}
template <typename Interface> inline void NoteOther(Direct2DWork& work, HRESULT hr, const char* method, Interface** object) noexcept
{
    if (SUCCEEDED(hr) && object && *object)
    {
        ++work.otherCreations;
        work.lastOther = method;
    }
}
template <typename Interface> inline void NoteTarget(Direct2DWork& work, HRESULT hr, const char* method, Interface** object) noexcept
{
    if (SUCCEEDED(hr) && object && *object)
        ++work.unwrappedTargets;
    NoteOther(work, hr, method, object);
}

// Reference counting shared by the wrappers. Interface is the most derived wrapped interface; every wrapped version is one of
// its bases in a single inheritance chain, so each has this object's address.
template <typename Interface> class CountingObject : public Interface
{
public:
    CountingObject()                                 = default;
    CountingObject(const CountingObject&)            = delete;
    CountingObject& operator=(const CountingObject&) = delete;
    CountingObject(CountingObject&&)                 = delete;
    CountingObject& operator=(CountingObject&&)      = delete;
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return static_cast<ULONG>(InterlockedIncrement(&_references));
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto remaining = static_cast<ULONG>(InterlockedDecrement(&_references));
        if (remaining == 0u)
            delete this;
        return remaining;
    }

protected:
    virtual ~CountingObject() = default;
    // Answers a wrapped version with this object, or else lets the real object answer and counts the escape.
    HRESULT Answer(bool wrapped, IUnknown* real, Direct2DWork& work, REFIID iid, void** object) noexcept
    {
        if (! object)
            return E_POINTER;
        *object = nullptr;
        if (wrapped)
        {
            *object = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        const HRESULT hr = real->QueryInterface(iid, object);
        if (SUCCEEDED(hr))
            ++work.escapedInterfaces;
        return hr;
    }

private:
    LONG _references = 1;
};
} // namespace Detail

class CountingFactory;
class CountingDevice;

// ID2D1Factory1: geometries and stroke styles, devices (wrapped), render targets (returned unwrapped, counted).
class CountingFactory final : public Detail::CountingObject<ID2D1Factory1>
{
public:
    CountingFactory(ID2D1Factory1* real, Direct2DWork& work) noexcept : _real(real), _work(work)
    {
        ++_work.wrappedFactories;
        ++_work.liveWrappers;
    }
    ~CountingFactory() override
    {
        --_work.liveWrappers;
    }
    [[nodiscard]] ID2D1Factory1* Real() const noexcept
    {
        return _real.get();
    }
    [[nodiscard]] Direct2DWork& Work() const noexcept
    {
        return _work;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        const bool wrapped = iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1Factory) || iid == __uuidof(ID2D1Factory1);
        return Answer(wrapped, _real.get(), _work, iid, object);
    }
    // ID2D1Factory.
    HRESULT STDMETHODCALLTYPE ReloadSystemMetrics() noexcept override
    {
        return _real->ReloadSystemMetrics();
    }
#pragma warning(push)
#pragma warning(disable : 4996) // GetDesktopDpi is deprecated; the wrapper still forwards it.
    void STDMETHODCALLTYPE GetDesktopDpi(FLOAT* dpiX, FLOAT* dpiY) noexcept override
    {
        Base()->GetDesktopDpi(dpiX, dpiY);
    }
#pragma warning(pop)
    HRESULT STDMETHODCALLTYPE CreateRectangleGeometry(const D2D1_RECT_F* rectangle, ID2D1RectangleGeometry** geometry) noexcept override
    {
        const HRESULT hr = Base()->CreateRectangleGeometry(rectangle, geometry);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, geometry);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateRoundedRectangleGeometry(const D2D1_ROUNDED_RECT* rectangle, ID2D1RoundedRectangleGeometry** geometry) noexcept override
    {
        const HRESULT hr = Base()->CreateRoundedRectangleGeometry(rectangle, geometry);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, geometry);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateEllipseGeometry(const D2D1_ELLIPSE* ellipse, ID2D1EllipseGeometry** geometry) noexcept override
    {
        const HRESULT hr = Base()->CreateEllipseGeometry(ellipse, geometry);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, geometry);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateGeometryGroup(D2D1_FILL_MODE fillMode,
                                                  ID2D1Geometry** geometries,
                                                  UINT32 count,
                                                  ID2D1GeometryGroup** group) noexcept override
    {
        const HRESULT hr = Base()->CreateGeometryGroup(fillMode, geometries, count, group);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, group);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateTransformedGeometry(ID2D1Geometry* source,
                                                        const D2D1_MATRIX_3X2_F* transform,
                                                        ID2D1TransformedGeometry** geometry) noexcept override
    {
        const HRESULT hr = Base()->CreateTransformedGeometry(source, transform, geometry);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, geometry);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreatePathGeometry(ID2D1PathGeometry** geometry) noexcept override
    {
        const HRESULT hr = Base()->CreatePathGeometry(geometry);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, geometry);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateStrokeStyle(const D2D1_STROKE_STYLE_PROPERTIES* properties,
                                                const FLOAT* dashes,
                                                UINT32 dashesCount,
                                                ID2D1StrokeStyle** strokeStyle) noexcept override
    {
        const HRESULT hr = Base()->CreateStrokeStyle(properties, dashes, dashesCount, strokeStyle);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::strokeStyle, strokeStyle);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateDrawingStateBlock(const D2D1_DRAWING_STATE_DESCRIPTION* description,
                                                      IDWriteRenderingParams* parameters,
                                                      ID2D1DrawingStateBlock** block) noexcept override
    {
        const HRESULT hr = Base()->CreateDrawingStateBlock(description, parameters, block);
        Detail::NoteOther(_work, hr, "CreateDrawingStateBlock", block);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateWicBitmapRenderTarget(IWICBitmap* target,
                                                          const D2D1_RENDER_TARGET_PROPERTIES* properties,
                                                          ID2D1RenderTarget** renderTarget) noexcept override
    {
        const HRESULT hr = Base()->CreateWicBitmapRenderTarget(target, properties, renderTarget);
        Detail::NoteTarget(_work, hr, "CreateWicBitmapRenderTarget", renderTarget);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateHwndRenderTarget(const D2D1_RENDER_TARGET_PROPERTIES* properties,
                                                     const D2D1_HWND_RENDER_TARGET_PROPERTIES* hwndProperties,
                                                     ID2D1HwndRenderTarget** renderTarget) noexcept override
    {
        const HRESULT hr = Base()->CreateHwndRenderTarget(properties, hwndProperties, renderTarget);
        Detail::NoteTarget(_work, hr, "CreateHwndRenderTarget", renderTarget);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateDxgiSurfaceRenderTarget(IDXGISurface* surface,
                                                            const D2D1_RENDER_TARGET_PROPERTIES* properties,
                                                            ID2D1RenderTarget** renderTarget) noexcept override
    {
        const HRESULT hr = Base()->CreateDxgiSurfaceRenderTarget(surface, properties, renderTarget);
        Detail::NoteTarget(_work, hr, "CreateDxgiSurfaceRenderTarget", renderTarget);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateDCRenderTarget(const D2D1_RENDER_TARGET_PROPERTIES* properties, ID2D1DCRenderTarget** renderTarget) noexcept override
    {
        const HRESULT hr = Base()->CreateDCRenderTarget(properties, renderTarget);
        Detail::NoteTarget(_work, hr, "CreateDCRenderTarget", renderTarget);
        return hr;
    }
    // ID2D1Factory1.
    HRESULT STDMETHODCALLTYPE CreateDevice(IDXGIDevice* dxgiDevice, ID2D1Device** device) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateStrokeStyle(const D2D1_STROKE_STYLE_PROPERTIES1* properties,
                                                const FLOAT* dashes,
                                                UINT32 dashesCount,
                                                ID2D1StrokeStyle1** strokeStyle) noexcept override
    {
        const HRESULT hr = _real->CreateStrokeStyle(properties, dashes, dashesCount, strokeStyle);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::strokeStyle, strokeStyle);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreatePathGeometry(ID2D1PathGeometry1** geometry) noexcept override
    {
        const HRESULT hr = _real->CreatePathGeometry(geometry);
        Detail::NoteCreated(_work, hr, DxUi::Detail::ResourceKind::geometry, geometry);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateDrawingStateBlock(const D2D1_DRAWING_STATE_DESCRIPTION1* description,
                                                      IDWriteRenderingParams* parameters,
                                                      ID2D1DrawingStateBlock1** block) noexcept override
    {
        const HRESULT hr = _real->CreateDrawingStateBlock(description, parameters, block);
        Detail::NoteOther(_work, hr, "CreateDrawingStateBlock", block);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateGdiMetafile(IStream* stream, ID2D1GdiMetafile** metafile) noexcept override
    {
        const HRESULT hr = _real->CreateGdiMetafile(stream, metafile);
        Detail::NoteOther(_work, hr, "CreateGdiMetafile", metafile);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE RegisterEffectFromStream(REFCLSID classId,
                                                       IStream* propertyXml,
                                                       const D2D1_PROPERTY_BINDING* bindings,
                                                       UINT32 bindingsCount,
                                                       const PD2D1_EFFECT_FACTORY effectFactory) noexcept override
    {
        return _real->RegisterEffectFromStream(classId, propertyXml, bindings, bindingsCount, effectFactory);
    }
    HRESULT STDMETHODCALLTYPE RegisterEffectFromString(REFCLSID classId,
                                                       PCWSTR propertyXml,
                                                       const D2D1_PROPERTY_BINDING* bindings,
                                                       UINT32 bindingsCount,
                                                       const PD2D1_EFFECT_FACTORY effectFactory) noexcept override
    {
        return _real->RegisterEffectFromString(classId, propertyXml, bindings, bindingsCount, effectFactory);
    }
    HRESULT STDMETHODCALLTYPE UnregisterEffect(REFCLSID classId) noexcept override
    {
        return _real->UnregisterEffect(classId);
    }
    HRESULT STDMETHODCALLTYPE GetRegisteredEffects(CLSID* effects,
                                                   UINT32 effectsCount,
                                                   UINT32* effectsReturned,
                                                   UINT32* effectsRegistered) const noexcept override
    {
        return _real->GetRegisteredEffects(effects, effectsCount, effectsReturned, effectsRegistered);
    }
    HRESULT STDMETHODCALLTYPE GetEffectProperties(REFCLSID effectId, ID2D1Properties** properties) const noexcept override
    {
        return _real->GetEffectProperties(effectId, properties);
    }

private:
    // The version-0 methods, called through the base interface so that version-1 overloads do not hide them.
    [[nodiscard]] ID2D1Factory* Base() const noexcept
    {
        return _real.get();
    }
    wil::com_ptr_nothrow<ID2D1Factory1> _real;
    Direct2DWork& _work;
};

// ID2D1Device: device contexts (wrapped). GetFactory reports the wrapped factory.
class CountingDevice final : public Detail::CountingObject<ID2D1Device>
{
public:
    CountingDevice(ID2D1Device* real, CountingFactory* factory) noexcept : _real(real), _factory(factory)
    {
        ++_factory->Work().wrappedDevices;
        ++_factory->Work().liveWrappers;
    }
    ~CountingDevice() override
    {
        --_factory->Work().liveWrappers;
    }
    [[nodiscard]] CountingFactory* Factory() const noexcept
    {
        return _factory.get();
    }
    [[nodiscard]] ID2D1Device* Real() const noexcept
    {
        return _real.get();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        const bool wrapped = iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1Resource) || iid == __uuidof(ID2D1Device);
        return Answer(wrapped, _real.get(), _factory->Work(), iid, object);
    }
    void STDMETHODCALLTYPE GetFactory(ID2D1Factory** factory) const noexcept override
    {
        if (factory)
            static_cast<void>(_factory.copy_to(factory));
    }
    HRESULT STDMETHODCALLTYPE CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS options, ID2D1DeviceContext** context) noexcept override;
    HRESULT STDMETHODCALLTYPE CreatePrintControl(IWICImagingFactory* wicFactory,
                                                 IPrintDocumentPackageTarget* documentTarget,
                                                 const D2D1_PRINT_CONTROL_PROPERTIES* properties,
                                                 ID2D1PrintControl** printControl) noexcept override
    {
        const HRESULT hr = _real->CreatePrintControl(wicFactory, documentTarget, properties, printControl);
        Detail::NoteOther(_factory->Work(), hr, "CreatePrintControl", printControl);
        return hr;
    }
    void STDMETHODCALLTYPE SetMaximumTextureMemory(UINT64 maximumInBytes) noexcept override
    {
        _real->SetMaximumTextureMemory(maximumInBytes);
    }
    UINT64 STDMETHODCALLTYPE GetMaximumTextureMemory() const noexcept override
    {
        return _real->GetMaximumTextureMemory();
    }
    void STDMETHODCALLTYPE ClearResources(UINT32 millisecondsSinceUse) noexcept override
    {
        _real->ClearResources(millisecondsSinceUse);
    }

private:
    wil::com_ptr_nothrow<ID2D1Device> _real;
    wil::com_ptr_nothrow<CountingFactory> _factory;
};

inline HRESULT STDMETHODCALLTYPE CountingFactory::CreateDevice(IDXGIDevice* dxgiDevice, ID2D1Device** device) noexcept
{
    if (! device)
        return _real->CreateDevice(dxgiDevice, device);
    *device = nullptr;
    wil::com_ptr_nothrow<ID2D1Device> created;
    const HRESULT hr = _real->CreateDevice(dxgiDevice, created.put());
    if (FAILED(hr))
        return hr;
    Detail::NoteOther(_work, hr, "CreateDevice", created.addressof());
    auto* const wrapper = new (std::nothrow) CountingDevice(created.get(), this);
    if (! wrapper)
        return E_OUTOFMEMORY;
    *device = wrapper;
    return S_OK;
}

// ID2D1DeviceContext through ID2D1DeviceContext7. Counts creations by kind and every draw call; GetDevice and GetFactory
// report the wrappers. A version the real context does not implement is refused by QueryInterface, so its methods here are
// never reached.
class CountingContext final : public Detail::CountingObject<ID2D1DeviceContext7>
{
public:
    CountingContext(ID2D1DeviceContext* real, CountingDevice* device) noexcept : _real(real), _device(device)
    {
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real1.put())));
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real2.put())));
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real3.put())));
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real4.put())));
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real5.put())));
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real6.put())));
        static_cast<void>(real->QueryInterface(IID_PPV_ARGS(_real7.put())));
        ++Work().wrappedContexts;
        ++Work().liveWrappers;
    }
    ~CountingContext() override
    {
        --Work().liveWrappers;
    }
    [[nodiscard]] ID2D1DeviceContext* Real() const noexcept
    {
        return _real.get();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        const bool wrapped = iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1Resource) || iid == __uuidof(ID2D1RenderTarget) ||
                             iid == __uuidof(ID2D1DeviceContext) || (iid == __uuidof(ID2D1DeviceContext1) && _real1) ||
                             (iid == __uuidof(ID2D1DeviceContext2) && _real2) || (iid == __uuidof(ID2D1DeviceContext3) && _real3) ||
                             (iid == __uuidof(ID2D1DeviceContext4) && _real4) || (iid == __uuidof(ID2D1DeviceContext5) && _real5) ||
                             (iid == __uuidof(ID2D1DeviceContext6) && _real6) || (iid == __uuidof(ID2D1DeviceContext7) && _real7);
        return Answer(wrapped, _real.get(), Work(), iid, object);
    }

    // ID2D1Resource.
    void STDMETHODCALLTYPE GetFactory(ID2D1Factory** factory) const noexcept override
    {
        if (factory)
        {
            *factory = _device->Factory();
            (*factory)->AddRef();
        }
    }

    // ID2D1RenderTarget: creation.
    HRESULT STDMETHODCALLTYPE
    CreateBitmap(D2D1_SIZE_U size, const void* source, UINT32 pitch, const D2D1_BITMAP_PROPERTIES* properties, ID2D1Bitmap** bitmap) noexcept override
    {
        return Created(Target()->CreateBitmap(size, source, pitch, properties, bitmap), DxUi::Detail::ResourceKind::bitmap, bitmap);
    }
    HRESULT STDMETHODCALLTYPE CreateBitmapFromWicBitmap(IWICBitmapSource* source,
                                                        const D2D1_BITMAP_PROPERTIES* properties,
                                                        ID2D1Bitmap** bitmap) noexcept override
    {
        return Created(Target()->CreateBitmapFromWicBitmap(source, properties, bitmap), DxUi::Detail::ResourceKind::bitmap, bitmap);
    }
    HRESULT STDMETHODCALLTYPE CreateSharedBitmap(REFIID iid, void* data, const D2D1_BITMAP_PROPERTIES* properties, ID2D1Bitmap** bitmap) noexcept override
    {
        return Created(Target()->CreateSharedBitmap(iid, data, properties, bitmap), DxUi::Detail::ResourceKind::bitmap, bitmap);
    }
    HRESULT STDMETHODCALLTYPE CreateBitmapBrush(ID2D1Bitmap* bitmap,
                                                const D2D1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties,
                                                const D2D1_BRUSH_PROPERTIES* brushProperties,
                                                ID2D1BitmapBrush** brush) noexcept override
    {
        return Created(Target()->CreateBitmapBrush(bitmap, bitmapBrushProperties, brushProperties, brush), DxUi::Detail::ResourceKind::brush, brush);
    }
    HRESULT STDMETHODCALLTYPE CreateSolidColorBrush(const D2D1_COLOR_F* color,
                                                    const D2D1_BRUSH_PROPERTIES* properties,
                                                    ID2D1SolidColorBrush** brush) noexcept override
    {
        return Created(Target()->CreateSolidColorBrush(color, properties, brush), DxUi::Detail::ResourceKind::brush, brush);
    }
    HRESULT STDMETHODCALLTYPE CreateGradientStopCollection(const D2D1_GRADIENT_STOP* stops,
                                                           UINT32 count,
                                                           D2D1_GAMMA gamma,
                                                           D2D1_EXTEND_MODE extendMode,
                                                           ID2D1GradientStopCollection** collection) noexcept override
    {
        return Created(
            Target()->CreateGradientStopCollection(stops, count, gamma, extendMode, collection), DxUi::Detail::ResourceKind::gradientStops, collection);
    }
    HRESULT STDMETHODCALLTYPE CreateLinearGradientBrush(const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearProperties,
                                                        const D2D1_BRUSH_PROPERTIES* brushProperties,
                                                        ID2D1GradientStopCollection* stops,
                                                        ID2D1LinearGradientBrush** brush) noexcept override
    {
        return Created(Target()->CreateLinearGradientBrush(linearProperties, brushProperties, stops, brush), DxUi::Detail::ResourceKind::brush, brush);
    }
    HRESULT STDMETHODCALLTYPE CreateRadialGradientBrush(const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialProperties,
                                                        const D2D1_BRUSH_PROPERTIES* brushProperties,
                                                        ID2D1GradientStopCollection* stops,
                                                        ID2D1RadialGradientBrush** brush) noexcept override
    {
        return Created(Target()->CreateRadialGradientBrush(radialProperties, brushProperties, stops, brush), DxUi::Detail::ResourceKind::brush, brush);
    }
    HRESULT STDMETHODCALLTYPE CreateCompatibleRenderTarget(const D2D1_SIZE_F* desiredSize,
                                                           const D2D1_SIZE_U* desiredPixelSize,
                                                           const D2D1_PIXEL_FORMAT* desiredFormat,
                                                           D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS options,
                                                           ID2D1BitmapRenderTarget** target) noexcept override
    {
        const HRESULT hr = Target()->CreateCompatibleRenderTarget(desiredSize, desiredPixelSize, desiredFormat, options, target);
        Detail::NoteTarget(Work(), hr, "CreateCompatibleRenderTarget", target);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateLayer(const D2D1_SIZE_F* size, ID2D1Layer** layer) noexcept override
    {
        return Created(Target()->CreateLayer(size, layer), DxUi::Detail::ResourceKind::layer, layer);
    }
    HRESULT STDMETHODCALLTYPE CreateMesh(ID2D1Mesh** mesh) noexcept override
    {
        return Other(Target()->CreateMesh(mesh), "CreateMesh", mesh);
    }

    // ID2D1RenderTarget: drawing. Each call counts one draw call.
    void STDMETHODCALLTYPE
    DrawLine(D2D1_POINT_2F point0, D2D1_POINT_2F point1, ID2D1Brush* brush, FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawLine(point0, point1, brush, strokeWidth, strokeStyle);
    }
    void STDMETHODCALLTYPE DrawRectangle(const D2D1_RECT_F* rect, ID2D1Brush* brush, FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawRectangle(rect, brush, strokeWidth, strokeStyle);
    }
    void STDMETHODCALLTYPE FillRectangle(const D2D1_RECT_F* rect, ID2D1Brush* brush) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->FillRectangle(rect, brush);
    }
    void STDMETHODCALLTYPE DrawRoundedRectangle(const D2D1_ROUNDED_RECT* rect,
                                                ID2D1Brush* brush,
                                                FLOAT strokeWidth,
                                                ID2D1StrokeStyle* strokeStyle) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawRoundedRectangle(rect, brush, strokeWidth, strokeStyle);
    }
    void STDMETHODCALLTYPE FillRoundedRectangle(const D2D1_ROUNDED_RECT* rect, ID2D1Brush* brush) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->FillRoundedRectangle(rect, brush);
    }
    void STDMETHODCALLTYPE DrawEllipse(const D2D1_ELLIPSE* ellipse, ID2D1Brush* brush, FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawEllipse(ellipse, brush, strokeWidth, strokeStyle);
    }
    void STDMETHODCALLTYPE FillEllipse(const D2D1_ELLIPSE* ellipse, ID2D1Brush* brush) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->FillEllipse(ellipse, brush);
    }
    void STDMETHODCALLTYPE DrawGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
    }
    void STDMETHODCALLTYPE FillGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, ID2D1Brush* opacityBrush) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->FillGeometry(geometry, brush, opacityBrush);
    }
    void STDMETHODCALLTYPE FillMesh(ID2D1Mesh* mesh, ID2D1Brush* brush) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->FillMesh(mesh, brush);
    }
    void STDMETHODCALLTYPE FillOpacityMask(
        ID2D1Bitmap* mask, ID2D1Brush* brush, D2D1_OPACITY_MASK_CONTENT content, const D2D1_RECT_F* destination, const D2D1_RECT_F* source) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->FillOpacityMask(mask, brush, content, destination, source);
    }
    void STDMETHODCALLTYPE DrawBitmap(ID2D1Bitmap* bitmap,
                                      const D2D1_RECT_F* destination,
                                      FLOAT opacity,
                                      D2D1_BITMAP_INTERPOLATION_MODE interpolationMode,
                                      const D2D1_RECT_F* source) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawBitmap(bitmap, destination, opacity, interpolationMode, source);
    }
    // DrawText lays its string out on every call: one implicit text layout, as CountedDrawText counts it.
    void STDMETHODCALLTYPE DrawText(const WCHAR* string,
                                    UINT32 length,
                                    IDWriteTextFormat* format,
                                    const D2D1_RECT_F* layoutRect,
                                    ID2D1Brush* brush,
                                    D2D1_DRAW_TEXT_OPTIONS options,
                                    DWRITE_MEASURING_MODE measuringMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        ++Work().seen.implicitTextLayout.created;
        Target()->DrawText(string, length, format, layoutRect, brush, options, measuringMode);
    }
    void STDMETHODCALLTYPE DrawTextLayout(D2D1_POINT_2F origin, IDWriteTextLayout* layout, ID2D1Brush* brush, D2D1_DRAW_TEXT_OPTIONS options) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawTextLayout(origin, layout, brush, options);
    }
    void STDMETHODCALLTYPE DrawGlyphRun(D2D1_POINT_2F origin,
                                        const DWRITE_GLYPH_RUN* run,
                                        ID2D1Brush* brush,
                                        DWRITE_MEASURING_MODE measuringMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->DrawGlyphRun(origin, run, brush, measuringMode);
    }
    void STDMETHODCALLTYPE Clear(const D2D1_COLOR_F* color) noexcept override
    {
        const Detail::DrawCall drawCall;
        Target()->Clear(color);
    }

    // ID2D1RenderTarget: state, forwarded.
    void STDMETHODCALLTYPE SetTransform(const D2D1_MATRIX_3X2_F* transform) noexcept override
    {
        Target()->SetTransform(transform);
    }
    void STDMETHODCALLTYPE GetTransform(D2D1_MATRIX_3X2_F* transform) const noexcept override
    {
        Target()->GetTransform(transform);
    }
    void STDMETHODCALLTYPE SetAntialiasMode(D2D1_ANTIALIAS_MODE mode) noexcept override
    {
        Target()->SetAntialiasMode(mode);
    }
    D2D1_ANTIALIAS_MODE STDMETHODCALLTYPE GetAntialiasMode() const noexcept override
    {
        return Target()->GetAntialiasMode();
    }
    void STDMETHODCALLTYPE SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE mode) noexcept override
    {
        Target()->SetTextAntialiasMode(mode);
    }
    D2D1_TEXT_ANTIALIAS_MODE STDMETHODCALLTYPE GetTextAntialiasMode() const noexcept override
    {
        return Target()->GetTextAntialiasMode();
    }
    void STDMETHODCALLTYPE SetTextRenderingParams(IDWriteRenderingParams* parameters) noexcept override
    {
        Target()->SetTextRenderingParams(parameters);
    }
    void STDMETHODCALLTYPE GetTextRenderingParams(IDWriteRenderingParams** parameters) const noexcept override
    {
        Target()->GetTextRenderingParams(parameters);
    }
    void STDMETHODCALLTYPE SetTags(D2D1_TAG tag1, D2D1_TAG tag2) noexcept override
    {
        Target()->SetTags(tag1, tag2);
    }
    void STDMETHODCALLTYPE GetTags(D2D1_TAG* tag1, D2D1_TAG* tag2) const noexcept override
    {
        Target()->GetTags(tag1, tag2);
    }
    void STDMETHODCALLTYPE PushLayer(const D2D1_LAYER_PARAMETERS* parameters, ID2D1Layer* layer) noexcept override
    {
        Target()->PushLayer(parameters, layer);
    }
    void STDMETHODCALLTYPE PopLayer() noexcept override
    {
        Target()->PopLayer();
    }
    HRESULT STDMETHODCALLTYPE Flush(D2D1_TAG* tag1, D2D1_TAG* tag2) noexcept override
    {
        const TimedCall<&CallTicks::flush> timed;
        return Target()->Flush(tag1, tag2);
    }
    void STDMETHODCALLTYPE SaveDrawingState(ID2D1DrawingStateBlock* block) const noexcept override
    {
        Target()->SaveDrawingState(block);
    }
    void STDMETHODCALLTYPE RestoreDrawingState(ID2D1DrawingStateBlock* block) noexcept override
    {
        Target()->RestoreDrawingState(block);
    }
    void STDMETHODCALLTYPE PushAxisAlignedClip(const D2D1_RECT_F* clip, D2D1_ANTIALIAS_MODE mode) noexcept override
    {
        Target()->PushAxisAlignedClip(clip, mode);
    }
    void STDMETHODCALLTYPE PopAxisAlignedClip() noexcept override
    {
        Target()->PopAxisAlignedClip();
    }
    void STDMETHODCALLTYPE BeginDraw() noexcept override
    {
        const TimedCall<&CallTicks::beginDraw> timed;
        Target()->BeginDraw();
    }
    HRESULT STDMETHODCALLTYPE EndDraw(D2D1_TAG* tag1, D2D1_TAG* tag2) noexcept override
    {
        const TimedCall<&CallTicks::endDraw> timed;
        return Target()->EndDraw(tag1, tag2);
    }
    D2D1_PIXEL_FORMAT STDMETHODCALLTYPE GetPixelFormat() const noexcept override
    {
        return Target()->GetPixelFormat();
    }
    void STDMETHODCALLTYPE SetDpi(FLOAT dpiX, FLOAT dpiY) noexcept override
    {
        Target()->SetDpi(dpiX, dpiY);
    }
    void STDMETHODCALLTYPE GetDpi(FLOAT* dpiX, FLOAT* dpiY) const noexcept override
    {
        Target()->GetDpi(dpiX, dpiY);
    }
    D2D1_SIZE_F STDMETHODCALLTYPE GetSize() const noexcept override
    {
        return Target()->GetSize();
    }
    D2D1_SIZE_U STDMETHODCALLTYPE GetPixelSize() const noexcept override
    {
        return Target()->GetPixelSize();
    }
    UINT32 STDMETHODCALLTYPE GetMaximumBitmapSize() const noexcept override
    {
        return Target()->GetMaximumBitmapSize();
    }
    BOOL STDMETHODCALLTYPE IsSupported(const D2D1_RENDER_TARGET_PROPERTIES* properties) const noexcept override
    {
        return Target()->IsSupported(properties);
    }

    // ID2D1DeviceContext: creation.
    HRESULT STDMETHODCALLTYPE
    CreateBitmap(D2D1_SIZE_U size, const void* source, UINT32 pitch, const D2D1_BITMAP_PROPERTIES1* properties, ID2D1Bitmap1** bitmap) noexcept override
    {
        return Created(_real->CreateBitmap(size, source, pitch, properties, bitmap), DxUi::Detail::ResourceKind::bitmap, bitmap);
    }
    HRESULT STDMETHODCALLTYPE CreateBitmapFromWicBitmap(IWICBitmapSource* source,
                                                        const D2D1_BITMAP_PROPERTIES1* properties,
                                                        ID2D1Bitmap1** bitmap) noexcept override
    {
        return Created(_real->CreateBitmapFromWicBitmap(source, properties, bitmap), DxUi::Detail::ResourceKind::bitmap, bitmap);
    }
    HRESULT STDMETHODCALLTYPE CreateColorContext(D2D1_COLOR_SPACE space,
                                                 const BYTE* profile,
                                                 UINT32 profileSize,
                                                 ID2D1ColorContext** colorContext) noexcept override
    {
        return Other(_real->CreateColorContext(space, profile, profileSize, colorContext), "CreateColorContext", colorContext);
    }
    HRESULT STDMETHODCALLTYPE CreateColorContextFromFilename(PCWSTR filename, ID2D1ColorContext** colorContext) noexcept override
    {
        return Other(_real->CreateColorContextFromFilename(filename, colorContext), "CreateColorContextFromFilename", colorContext);
    }
    HRESULT STDMETHODCALLTYPE CreateColorContextFromWicColorContext(IWICColorContext* wicColorContext, ID2D1ColorContext** colorContext) noexcept override
    {
        return Other(_real->CreateColorContextFromWicColorContext(wicColorContext, colorContext), "CreateColorContextFromWicColorContext", colorContext);
    }
    // The surface bitmap of an embedded preparation: the real context creates it from the caller's DXGI surface.
    HRESULT STDMETHODCALLTYPE CreateBitmapFromDxgiSurface(IDXGISurface* surface,
                                                          const D2D1_BITMAP_PROPERTIES1* properties,
                                                          ID2D1Bitmap1** bitmap) noexcept override
    {
        return Created(_real->CreateBitmapFromDxgiSurface(surface, properties, bitmap), DxUi::Detail::ResourceKind::bitmap, bitmap);
    }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFCLSID effectId, ID2D1Effect** effect) noexcept override
    {
        return Created(_real->CreateEffect(effectId, effect), DxUi::Detail::ResourceKind::effect, effect);
    }
    HRESULT STDMETHODCALLTYPE CreateGradientStopCollection(const D2D1_GRADIENT_STOP* stops,
                                                           UINT32 count,
                                                           D2D1_COLOR_SPACE preInterpolationSpace,
                                                           D2D1_COLOR_SPACE postInterpolationSpace,
                                                           D2D1_BUFFER_PRECISION bufferPrecision,
                                                           D2D1_EXTEND_MODE extendMode,
                                                           D2D1_COLOR_INTERPOLATION_MODE interpolationMode,
                                                           ID2D1GradientStopCollection1** collection) noexcept override
    {
        return Created(_real->CreateGradientStopCollection(
                           stops, count, preInterpolationSpace, postInterpolationSpace, bufferPrecision, extendMode, interpolationMode, collection),
                       DxUi::Detail::ResourceKind::gradientStops,
                       collection);
    }
    HRESULT STDMETHODCALLTYPE CreateImageBrush(ID2D1Image* image,
                                               const D2D1_IMAGE_BRUSH_PROPERTIES* imageBrushProperties,
                                               const D2D1_BRUSH_PROPERTIES* brushProperties,
                                               ID2D1ImageBrush** brush) noexcept override
    {
        return Created(_real->CreateImageBrush(image, imageBrushProperties, brushProperties, brush), DxUi::Detail::ResourceKind::brush, brush);
    }
    HRESULT STDMETHODCALLTYPE CreateBitmapBrush(ID2D1Bitmap* bitmap,
                                                const D2D1_BITMAP_BRUSH_PROPERTIES1* bitmapBrushProperties,
                                                const D2D1_BRUSH_PROPERTIES* brushProperties,
                                                ID2D1BitmapBrush1** brush) noexcept override
    {
        return Created(_real->CreateBitmapBrush(bitmap, bitmapBrushProperties, brushProperties, brush), DxUi::Detail::ResourceKind::brush, brush);
    }
    HRESULT STDMETHODCALLTYPE CreateCommandList(ID2D1CommandList** commandList) noexcept override
    {
        return Created(_real->CreateCommandList(commandList), DxUi::Detail::ResourceKind::commandList, commandList);
    }

    // ID2D1DeviceContext: queries and state, forwarded.
    BOOL STDMETHODCALLTYPE IsDxgiFormatSupported(DXGI_FORMAT format) const noexcept override
    {
        return _real->IsDxgiFormatSupported(format);
    }
    BOOL STDMETHODCALLTYPE IsBufferPrecisionSupported(D2D1_BUFFER_PRECISION precision) const noexcept override
    {
        return _real->IsBufferPrecisionSupported(precision);
    }
    HRESULT STDMETHODCALLTYPE GetImageLocalBounds(ID2D1Image* image, D2D1_RECT_F* bounds) const noexcept override
    {
        return _real->GetImageLocalBounds(image, bounds);
    }
    HRESULT STDMETHODCALLTYPE GetImageWorldBounds(ID2D1Image* image, D2D1_RECT_F* bounds) const noexcept override
    {
        return _real->GetImageWorldBounds(image, bounds);
    }
    HRESULT STDMETHODCALLTYPE GetGlyphRunWorldBounds(D2D1_POINT_2F origin,
                                                     const DWRITE_GLYPH_RUN* run,
                                                     DWRITE_MEASURING_MODE measuringMode,
                                                     D2D1_RECT_F* bounds) const noexcept override
    {
        return _real->GetGlyphRunWorldBounds(origin, run, measuringMode, bounds);
    }
    // The wrapped device, so that a context DxUi creates from it is wrapped too.
    void STDMETHODCALLTYPE GetDevice(ID2D1Device** device) const noexcept override
    {
        if (device)
            static_cast<void>(_device.copy_to(device));
    }
    void STDMETHODCALLTYPE SetTarget(ID2D1Image* image) noexcept override
    {
        _real->SetTarget(image);
    }
    void STDMETHODCALLTYPE GetTarget(ID2D1Image** image) const noexcept override
    {
        _real->GetTarget(image);
    }
    void STDMETHODCALLTYPE SetRenderingControls(const D2D1_RENDERING_CONTROLS* controls) noexcept override
    {
        _real->SetRenderingControls(controls);
    }
    void STDMETHODCALLTYPE GetRenderingControls(D2D1_RENDERING_CONTROLS* controls) const noexcept override
    {
        _real->GetRenderingControls(controls);
    }
    void STDMETHODCALLTYPE SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND blend) noexcept override
    {
        _real->SetPrimitiveBlend(blend);
    }
    D2D1_PRIMITIVE_BLEND STDMETHODCALLTYPE GetPrimitiveBlend() const noexcept override
    {
        return _real->GetPrimitiveBlend();
    }
    void STDMETHODCALLTYPE SetUnitMode(D2D1_UNIT_MODE mode) noexcept override
    {
        _real->SetUnitMode(mode);
    }
    D2D1_UNIT_MODE STDMETHODCALLTYPE GetUnitMode() const noexcept override
    {
        return _real->GetUnitMode();
    }
    void STDMETHODCALLTYPE PushLayer(const D2D1_LAYER_PARAMETERS1* parameters, ID2D1Layer* layer) noexcept override
    {
        _real->PushLayer(parameters, layer);
    }
    HRESULT STDMETHODCALLTYPE InvalidateEffectInputRectangle(ID2D1Effect* effect, UINT32 input, const D2D1_RECT_F* rectangle) noexcept override
    {
        return _real->InvalidateEffectInputRectangle(effect, input, rectangle);
    }
    HRESULT STDMETHODCALLTYPE GetEffectInvalidRectangleCount(ID2D1Effect* effect, UINT32* count) noexcept override
    {
        return _real->GetEffectInvalidRectangleCount(effect, count);
    }
    HRESULT STDMETHODCALLTYPE GetEffectInvalidRectangles(ID2D1Effect* effect, D2D1_RECT_F* rectangles, UINT32 count) noexcept override
    {
        return _real->GetEffectInvalidRectangles(effect, rectangles, count);
    }
    HRESULT STDMETHODCALLTYPE GetEffectRequiredInputRectangles(ID2D1Effect* effect,
                                                               const D2D1_RECT_F* imageRectangle,
                                                               const D2D1_EFFECT_INPUT_DESCRIPTION* descriptions,
                                                               D2D1_RECT_F* rectangles,
                                                               UINT32 count) noexcept override
    {
        return _real->GetEffectRequiredInputRectangles(effect, imageRectangle, descriptions, rectangles, count);
    }

    // ID2D1DeviceContext: drawing.
    void STDMETHODCALLTYPE DrawGlyphRun(D2D1_POINT_2F origin,
                                        const DWRITE_GLYPH_RUN* run,
                                        const DWRITE_GLYPH_RUN_DESCRIPTION* description,
                                        ID2D1Brush* brush,
                                        DWRITE_MEASURING_MODE measuringMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real->DrawGlyphRun(origin, run, description, brush, measuringMode);
    }
    void STDMETHODCALLTYPE DrawImage(ID2D1Image* image,
                                     const D2D1_POINT_2F* offset,
                                     const D2D1_RECT_F* rectangle,
                                     D2D1_INTERPOLATION_MODE interpolationMode,
                                     D2D1_COMPOSITE_MODE compositeMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real->DrawImage(image, offset, rectangle, interpolationMode, compositeMode);
    }
    void STDMETHODCALLTYPE DrawGdiMetafile(ID2D1GdiMetafile* metafile, const D2D1_POINT_2F* offset) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real->DrawGdiMetafile(metafile, offset);
    }
    void STDMETHODCALLTYPE DrawBitmap(ID2D1Bitmap* bitmap,
                                      const D2D1_RECT_F* destination,
                                      FLOAT opacity,
                                      D2D1_INTERPOLATION_MODE interpolationMode,
                                      const D2D1_RECT_F* source,
                                      const D2D1_MATRIX_4X4_F* perspective) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real->DrawBitmap(bitmap, destination, opacity, interpolationMode, source, perspective);
    }
    void STDMETHODCALLTYPE FillOpacityMask(ID2D1Bitmap* mask, ID2D1Brush* brush, const D2D1_RECT_F* destination, const D2D1_RECT_F* source) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real->FillOpacityMask(mask, brush, destination, source);
    }

    // ID2D1DeviceContext1.
    HRESULT STDMETHODCALLTYPE CreateFilledGeometryRealization(ID2D1Geometry* geometry,
                                                              FLOAT tolerance,
                                                              ID2D1GeometryRealization** realization) noexcept override
    {
        return Other(_real1->CreateFilledGeometryRealization(geometry, tolerance, realization), "CreateFilledGeometryRealization", realization);
    }
    HRESULT STDMETHODCALLTYPE CreateStrokedGeometryRealization(
        ID2D1Geometry* geometry, FLOAT tolerance, FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, ID2D1GeometryRealization** realization) noexcept override
    {
        return Other(_real1->CreateStrokedGeometryRealization(geometry, tolerance, strokeWidth, strokeStyle, realization),
                     "CreateStrokedGeometryRealization",
                     realization);
    }
    void STDMETHODCALLTYPE DrawGeometryRealization(ID2D1GeometryRealization* realization, ID2D1Brush* brush) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real1->DrawGeometryRealization(realization, brush);
    }

    // ID2D1DeviceContext2.
    HRESULT STDMETHODCALLTYPE CreateInk(const D2D1_INK_POINT* startPoint, ID2D1Ink** ink) noexcept override
    {
        return Other(_real2->CreateInk(startPoint, ink), "CreateInk", ink);
    }
    HRESULT STDMETHODCALLTYPE CreateInkStyle(const D2D1_INK_STYLE_PROPERTIES* properties, ID2D1InkStyle** inkStyle) noexcept override
    {
        return Other(_real2->CreateInkStyle(properties, inkStyle), "CreateInkStyle", inkStyle);
    }
    HRESULT STDMETHODCALLTYPE CreateGradientMesh(const D2D1_GRADIENT_MESH_PATCH* patches, UINT32 count, ID2D1GradientMesh** mesh) noexcept override
    {
        return Other(_real2->CreateGradientMesh(patches, count, mesh), "CreateGradientMesh", mesh);
    }
    HRESULT STDMETHODCALLTYPE CreateImageSourceFromWic(IWICBitmapSource* source,
                                                       D2D1_IMAGE_SOURCE_LOADING_OPTIONS options,
                                                       D2D1_ALPHA_MODE alphaMode,
                                                       ID2D1ImageSourceFromWic** imageSource) noexcept override
    {
        return Other(_real2->CreateImageSourceFromWic(source, options, alphaMode, imageSource), "CreateImageSourceFromWic", imageSource);
    }
    HRESULT STDMETHODCALLTYPE CreateLookupTable3D(D2D1_BUFFER_PRECISION precision,
                                                  const UINT32* extents,
                                                  const BYTE* data,
                                                  UINT32 dataCount,
                                                  const UINT32* strides,
                                                  ID2D1LookupTable3D** table) noexcept override
    {
        return Other(_real2->CreateLookupTable3D(precision, extents, data, dataCount, strides, table), "CreateLookupTable3D", table);
    }
    HRESULT STDMETHODCALLTYPE CreateImageSourceFromDxgi(IDXGISurface** surfaces,
                                                        UINT32 surfaceCount,
                                                        DXGI_COLOR_SPACE_TYPE colorSpace,
                                                        D2D1_IMAGE_SOURCE_FROM_DXGI_OPTIONS options,
                                                        ID2D1ImageSource** imageSource) noexcept override
    {
        return Other(_real2->CreateImageSourceFromDxgi(surfaces, surfaceCount, colorSpace, options, imageSource), "CreateImageSourceFromDxgi", imageSource);
    }
    HRESULT STDMETHODCALLTYPE GetGradientMeshWorldBounds(ID2D1GradientMesh* mesh, D2D1_RECT_F* bounds) const noexcept override
    {
        return _real2->GetGradientMeshWorldBounds(mesh, bounds);
    }
    void STDMETHODCALLTYPE DrawInk(ID2D1Ink* ink, ID2D1Brush* brush, ID2D1InkStyle* inkStyle) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real2->DrawInk(ink, brush, inkStyle);
    }
    void STDMETHODCALLTYPE DrawGradientMesh(ID2D1GradientMesh* mesh) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real2->DrawGradientMesh(mesh);
    }
    void STDMETHODCALLTYPE DrawGdiMetafile(ID2D1GdiMetafile* metafile, const D2D1_RECT_F* destination, const D2D1_RECT_F* source) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real2->DrawGdiMetafile(metafile, destination, source);
    }
    HRESULT STDMETHODCALLTYPE CreateTransformedImageSource(ID2D1ImageSource* imageSource,
                                                           const D2D1_TRANSFORMED_IMAGE_SOURCE_PROPERTIES* properties,
                                                           ID2D1TransformedImageSource** transformed) noexcept override
    {
        return Other(_real2->CreateTransformedImageSource(imageSource, properties, transformed), "CreateTransformedImageSource", transformed);
    }

    // ID2D1DeviceContext3.
    HRESULT STDMETHODCALLTYPE CreateSpriteBatch(ID2D1SpriteBatch** spriteBatch) noexcept override
    {
        return Other(_real3->CreateSpriteBatch(spriteBatch), "CreateSpriteBatch", spriteBatch);
    }
    void STDMETHODCALLTYPE DrawSpriteBatch(ID2D1SpriteBatch* spriteBatch,
                                           UINT32 startIndex,
                                           UINT32 spriteCount,
                                           ID2D1Bitmap* bitmap,
                                           D2D1_BITMAP_INTERPOLATION_MODE interpolationMode,
                                           D2D1_SPRITE_OPTIONS options) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real3->DrawSpriteBatch(spriteBatch, startIndex, spriteCount, bitmap, interpolationMode, options);
    }

    // ID2D1DeviceContext4.
    HRESULT STDMETHODCALLTYPE CreateSvgGlyphStyle(ID2D1SvgGlyphStyle** style) noexcept override
    {
        return Other(_real4->CreateSvgGlyphStyle(style), "CreateSvgGlyphStyle", style);
    }
    void STDMETHODCALLTYPE DrawText(const WCHAR* string,
                                    UINT32 length,
                                    IDWriteTextFormat* format,
                                    const D2D1_RECT_F* layoutRect,
                                    ID2D1Brush* brush,
                                    ID2D1SvgGlyphStyle* svgGlyphStyle,
                                    UINT32 colorPaletteIndex,
                                    D2D1_DRAW_TEXT_OPTIONS options,
                                    DWRITE_MEASURING_MODE measuringMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        ++Work().seen.implicitTextLayout.created;
        _real4->DrawText(string, length, format, layoutRect, brush, svgGlyphStyle, colorPaletteIndex, options, measuringMode);
    }
    void STDMETHODCALLTYPE DrawTextLayout(D2D1_POINT_2F origin,
                                          IDWriteTextLayout* layout,
                                          ID2D1Brush* brush,
                                          ID2D1SvgGlyphStyle* svgGlyphStyle,
                                          UINT32 colorPaletteIndex,
                                          D2D1_DRAW_TEXT_OPTIONS options) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real4->DrawTextLayout(origin, layout, brush, svgGlyphStyle, colorPaletteIndex, options);
    }
    void STDMETHODCALLTYPE DrawColorBitmapGlyphRun(DWRITE_GLYPH_IMAGE_FORMATS format,
                                                   D2D1_POINT_2F origin,
                                                   const DWRITE_GLYPH_RUN* run,
                                                   DWRITE_MEASURING_MODE measuringMode,
                                                   D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION snap) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real4->DrawColorBitmapGlyphRun(format, origin, run, measuringMode, snap);
    }
    void STDMETHODCALLTYPE DrawSvgGlyphRun(D2D1_POINT_2F origin,
                                           const DWRITE_GLYPH_RUN* run,
                                           ID2D1Brush* brush,
                                           ID2D1SvgGlyphStyle* svgGlyphStyle,
                                           UINT32 colorPaletteIndex,
                                           DWRITE_MEASURING_MODE measuringMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real4->DrawSvgGlyphRun(origin, run, brush, svgGlyphStyle, colorPaletteIndex, measuringMode);
    }
    HRESULT STDMETHODCALLTYPE GetColorBitmapGlyphImage(DWRITE_GLYPH_IMAGE_FORMATS format,
                                                       D2D1_POINT_2F origin,
                                                       IDWriteFontFace* fontFace,
                                                       FLOAT emSize,
                                                       UINT16 glyphIndex,
                                                       BOOL isSideways,
                                                       const D2D1_MATRIX_3X2_F* worldTransform,
                                                       FLOAT dpiX,
                                                       FLOAT dpiY,
                                                       D2D1_MATRIX_3X2_F* glyphTransform,
                                                       ID2D1Image** glyphImage) noexcept override
    {
        return Other(
            _real4->GetColorBitmapGlyphImage(format, origin, fontFace, emSize, glyphIndex, isSideways, worldTransform, dpiX, dpiY, glyphTransform, glyphImage),
            "GetColorBitmapGlyphImage",
            glyphImage);
    }
    HRESULT STDMETHODCALLTYPE GetSvgGlyphImage(D2D1_POINT_2F origin,
                                               IDWriteFontFace* fontFace,
                                               FLOAT emSize,
                                               UINT16 glyphIndex,
                                               BOOL isSideways,
                                               const D2D1_MATRIX_3X2_F* worldTransform,
                                               ID2D1Brush* brush,
                                               ID2D1SvgGlyphStyle* svgGlyphStyle,
                                               UINT32 colorPaletteIndex,
                                               D2D1_MATRIX_3X2_F* glyphTransform,
                                               ID2D1CommandList** glyphImage) noexcept override
    {
        return Other(_real4->GetSvgGlyphImage(
                         origin, fontFace, emSize, glyphIndex, isSideways, worldTransform, brush, svgGlyphStyle, colorPaletteIndex, glyphTransform, glyphImage),
                     "GetSvgGlyphImage",
                     glyphImage);
    }

    // ID2D1DeviceContext5.
    HRESULT STDMETHODCALLTYPE CreateSvgDocument(IStream* stream, D2D1_SIZE_F viewportSize, ID2D1SvgDocument** document) noexcept override
    {
        return Other(_real5->CreateSvgDocument(stream, viewportSize, document), "CreateSvgDocument", document);
    }
    void STDMETHODCALLTYPE DrawSvgDocument(ID2D1SvgDocument* document) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real5->DrawSvgDocument(document);
    }
    HRESULT STDMETHODCALLTYPE CreateColorContextFromDxgiColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace, ID2D1ColorContext1** colorContext) noexcept override
    {
        return Other(_real5->CreateColorContextFromDxgiColorSpace(colorSpace, colorContext), "CreateColorContextFromDxgiColorSpace", colorContext);
    }
    HRESULT STDMETHODCALLTYPE CreateColorContextFromSimpleColorProfile(const D2D1_SIMPLE_COLOR_PROFILE* profile,
                                                                       ID2D1ColorContext1** colorContext) noexcept override
    {
        return Other(_real5->CreateColorContextFromSimpleColorProfile(profile, colorContext), "CreateColorContextFromSimpleColorProfile", colorContext);
    }

    // ID2D1DeviceContext6.
    void STDMETHODCALLTYPE BlendImage(ID2D1Image* image,
                                      D2D1_BLEND_MODE blendMode,
                                      const D2D1_POINT_2F* offset,
                                      const D2D1_RECT_F* rectangle,
                                      D2D1_INTERPOLATION_MODE interpolationMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real6->BlendImage(image, blendMode, offset, rectangle, interpolationMode);
    }

    // ID2D1DeviceContext7.
    DWRITE_PAINT_FEATURE_LEVEL STDMETHODCALLTYPE GetPaintFeatureLevel() noexcept override
    {
        return _real7->GetPaintFeatureLevel();
    }
    void STDMETHODCALLTYPE DrawPaintGlyphRun(
        D2D1_POINT_2F origin, const DWRITE_GLYPH_RUN* run, ID2D1Brush* brush, UINT32 colorPaletteIndex, DWRITE_MEASURING_MODE measuringMode) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real7->DrawPaintGlyphRun(origin, run, brush, colorPaletteIndex, measuringMode);
    }
    void STDMETHODCALLTYPE DrawGlyphRunWithColorSupport(D2D1_POINT_2F origin,
                                                        const DWRITE_GLYPH_RUN* run,
                                                        const DWRITE_GLYPH_RUN_DESCRIPTION* description,
                                                        ID2D1Brush* brush,
                                                        ID2D1SvgGlyphStyle* svgGlyphStyle,
                                                        UINT32 colorPaletteIndex,
                                                        DWRITE_MEASURING_MODE measuringMode,
                                                        D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION snap) noexcept override
    {
        const Detail::DrawCall drawCall;
        _real7->DrawGlyphRunWithColorSupport(origin, run, description, brush, svgGlyphStyle, colorPaletteIndex, measuringMode, snap);
    }

private:
    [[nodiscard]] Direct2DWork& Work() const noexcept
    {
        return _device->Factory()->Work();
    }
    // The version-0 methods, called through the base interface so that later overloads do not hide them.
    [[nodiscard]] ID2D1RenderTarget* Target() const noexcept
    {
        return _real.get();
    }
    template <typename Interface> HRESULT Created(HRESULT hr, DxUi::Detail::ResourceKind kind, Interface** object) const noexcept
    {
        Detail::NoteCreated(Work(), hr, kind, object);
        return hr;
    }
    template <typename Interface> HRESULT Other(HRESULT hr, const char* method, Interface** object) const noexcept
    {
        Detail::NoteOther(Work(), hr, method, object);
        return hr;
    }

    wil::com_ptr_nothrow<ID2D1DeviceContext> _real;
    wil::com_ptr_nothrow<ID2D1DeviceContext1> _real1;
    wil::com_ptr_nothrow<ID2D1DeviceContext2> _real2;
    wil::com_ptr_nothrow<ID2D1DeviceContext3> _real3;
    wil::com_ptr_nothrow<ID2D1DeviceContext4> _real4;
    wil::com_ptr_nothrow<ID2D1DeviceContext5> _real5;
    wil::com_ptr_nothrow<ID2D1DeviceContext6> _real6;
    wil::com_ptr_nothrow<ID2D1DeviceContext7> _real7;
    wil::com_ptr_nothrow<CountingDevice> _device;
};

inline HRESULT STDMETHODCALLTYPE CountingDevice::CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS options, ID2D1DeviceContext** context) noexcept
{
    if (! context)
        return _real->CreateDeviceContext(options, context);
    *context = nullptr;
    wil::com_ptr_nothrow<ID2D1DeviceContext> created;
    const HRESULT hr = _real->CreateDeviceContext(options, created.put());
    if (FAILED(hr))
        return hr;
    ++DxUi::Detail::Tally(_factory->Work().seen, DxUi::Detail::ResourceKind::deviceContext).created;
    auto* const wrapper = new (std::nothrow) CountingContext(created.get(), this);
    if (! wrapper)
        return E_OUTOFMEMORY;
    *context = wrapper;
    return S_OK;
}

// One installation of the wrappers on the calling thread: while it lives, every Direct2D factory and DirectWrite factory
// DxUi creates through the seam (GraphicsDevice::Create, the native host's shared resources) is wrapped. The DirectWrite
// wrapper is CountingWriteFactory, which forwards interfaces it does not wrap to the real factory and counts them.
class CountingWrappers final
{
public:
    CountingWrappers() noexcept : _previous(DxUi::Detail::g_factoryInterposer)
    {
        _interposer.context               = this;
        _interposer.d2dFactory            = &WrapD2DFactory;
        _interposer.writeFactory          = &WrapWriteFactory;
        DxUi::Detail::g_factoryInterposer = &_interposer;
    }
    CountingWrappers(const CountingWrappers&)            = delete;
    CountingWrappers& operator=(const CountingWrappers&) = delete;
    CountingWrappers(CountingWrappers&&)                 = delete;
    CountingWrappers& operator=(CountingWrappers&&)      = delete;
    ~CountingWrappers()
    {
        if (DxUi::Detail::g_factoryInterposer == &_interposer)
            DxUi::Detail::g_factoryInterposer = _previous;
    }

    static constexpr size_t kWriteFactoryCapacity = 8u;
    Direct2DWork direct2D;
    // The DirectWrite factories wrapped so far, in creation order; a factory beyond the capacity is wrapped but not
    // recorded, and counted in unrecordedWriteFactories (a test requires 0).
    std::array<Microsoft::WRL::ComPtr<NativeTextTests::CountingWriteFactory>, kWriteFactoryCapacity> writeFactories{};
    size_t writeFactoryCount          = 0u;
    uint64_t unrecordedWriteFactories = 0u;

    // What the wrappers saw so far, by resource kind: the Direct2D wrappers' tallies plus each DirectWrite factory's.
    [[nodiscard]] DxUi::Detail::ResourceWork Seen() const noexcept
    {
        DxUi::Detail::ResourceWork seen = direct2D.seen;
        for (size_t index = 0u; index < writeFactoryCount; ++index)
        {
            seen.textFormat.created += writeFactories[index]->createdFormats;
            seen.textLayout.created += writeFactories[index]->createdLayouts;
        }
        return seen;
    }
    [[nodiscard]] uint64_t OtherCreations() const noexcept
    {
        uint64_t total = direct2D.otherCreations;
        for (size_t index = 0u; index < writeFactoryCount; ++index)
            total += writeFactories[index]->otherCreations;
        return total;
    }
    [[nodiscard]] uint64_t EscapedInterfaces() const noexcept
    {
        uint64_t total = direct2D.escapedInterfaces;
        for (size_t index = 0u; index < writeFactoryCount; ++index)
            total += writeFactories[index]->escapedInterfaces;
        return total;
    }

private:
    static HRESULT WrapD2DFactory(void* context, ID2D1Factory1* created, ID2D1Factory1** result) noexcept
    {
        auto* const self    = static_cast<CountingWrappers*>(context);
        auto* const wrapper = new (std::nothrow) CountingFactory(created, self->direct2D);
        if (! wrapper)
            return E_OUTOFMEMORY;
        *result = wrapper;
        return S_OK;
    }
    static HRESULT WrapWriteFactory(void* context, IDWriteFactory* created, IDWriteFactory** result) noexcept
    {
        auto* const self = static_cast<CountingWrappers*>(context);
        auto wrapper     = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(created);
        if (! wrapper)
            return E_OUTOFMEMORY;
        wrapper->forwardUnknownInterfaces = true;
        if (self->writeFactoryCount < kWriteFactoryCapacity)
            self->writeFactories[self->writeFactoryCount++] = wrapper;
        else
            ++self->unrecordedWriteFactories;
        *result = wrapper.Detach();
        return S_OK;
    }

    DxUi::Detail::FactoryInterposer _interposer{};
    const DxUi::Detail::FactoryInterposer* _previous = nullptr;
};

[[nodiscard]] constexpr const char* ResourceKindName(DxUi::Detail::ResourceKind kind) noexcept
{
    switch (kind)
    {
#define DXUI_COUNTING_KIND_NAME(name)                                                                                                                          \
    case DxUi::Detail::ResourceKind::name: return #name;
        DXUI_WORK_RESOURCE_KINDS(DXUI_COUNTING_KIND_NAME)
#undef DXUI_COUNTING_KIND_NAME
        case DxUi::Detail::ResourceKind::Count:
        default: break;
    }
    return "?";
}

// Created counts of a later reading minus an earlier one, kind by kind.
[[nodiscard]] constexpr DxUi::Detail::ResourceWork CreatedSince(const DxUi::Detail::ResourceWork& later, const DxUi::Detail::ResourceWork& earlier) noexcept
{
    DxUi::Detail::ResourceWork delta{};
    for (size_t index = 0u; index < static_cast<size_t>(DxUi::Detail::ResourceKind::Count); ++index)
    {
        const auto kind                          = static_cast<DxUi::Detail::ResourceKind>(index);
        DxUi::Detail::Tally(delta, kind).created = DxUi::Detail::Tally(later, kind).created - DxUi::Detail::Tally(earlier, kind).created;
    }
    return delta;
}

// The kinds the wrappers can see: every kind but D3D11 devices, which are created outside Direct2D and DirectWrite.
[[nodiscard]] constexpr bool WrappersSee(DxUi::Detail::ResourceKind kind) noexcept
{
    return kind != DxUi::Detail::ResourceKind::d3d11Device && kind != DxUi::Detail::ResourceKind::Count;
}

struct CreationMismatch final
{
    DxUi::Detail::ResourceKind kind = DxUi::Detail::ResourceKind::Count;
    uint64_t helpers                = 0u;
    uint64_t wrappers               = 0u;
};

// Compares, kind by kind, the creations the counted helpers reported with those the wrappers saw over the same interval.
// Writes the first mismatches into out and returns how many kinds differ.
[[nodiscard]] inline size_t CompareCreations(const DxUi::Detail::ResourceWork& helpers,
                                             const DxUi::Detail::ResourceWork& wrappers,
                                             std::span<CreationMismatch> out) noexcept
{
    size_t mismatches = 0u;
    for (size_t index = 0u; index < static_cast<size_t>(DxUi::Detail::ResourceKind::Count); ++index)
    {
        const auto kind = static_cast<DxUi::Detail::ResourceKind>(index);
        if (! WrappersSee(kind))
            continue;
        const uint64_t counted = DxUi::Detail::Tally(helpers, kind).created;
        const uint64_t seen    = DxUi::Detail::Tally(wrappers, kind).created;
        if (counted == seen)
            continue;
        if (mismatches < out.size())
            out[mismatches] = CreationMismatch{kind, counted, seen};
        ++mismatches;
    }
    return mismatches;
}
} // namespace NativeWorkTests
#pragma warning(pop)
