#include "Controls/DxUi.Internal.h"
#include "Support/Diagnostics.h"
#include <DxUi/Embedded.h>
#include <DxUi/FrameRuntime.h>
#include <DxUiCompositePS.h>
#include <DxUiCompositeVS.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <wil/result.h>

namespace DxUi
{
void EmbeddedKeyDispatchReceipt::Reset() noexcept
{
    _embeddedHost = nullptr;
    _controlHost  = nullptr;
    _owner        = nullptr;
    _root         = nullptr;
    _ownerLifetime.reset();
    _rootLifetime.reset();
    _attachmentRevision     = 0u;
    _focusOwnerEpoch        = 0u;
    _sequence               = 0u;
    _virtualKey             = 0u;
    _ownerSupportsTextInput = false;
    _allowCharacter         = false;
    _valid                  = false;
}
EmbeddedKeyDispatchReceipt::EmbeddedKeyDispatchReceipt(EmbeddedKeyDispatchReceipt&& other) noexcept
{
    *this = std::move(other);
}
EmbeddedKeyDispatchReceipt& EmbeddedKeyDispatchReceipt::operator=(EmbeddedKeyDispatchReceipt&& other) noexcept
{
    if (this == &other)
        return *this;
    Reset();
    _embeddedHost           = other._embeddedHost;
    _controlHost            = other._controlHost;
    _owner                  = other._owner;
    _root                   = other._root;
    _ownerLifetime          = std::move(other._ownerLifetime);
    _rootLifetime           = std::move(other._rootLifetime);
    _attachmentRevision     = other._attachmentRevision;
    _focusOwnerEpoch        = other._focusOwnerEpoch;
    _sequence               = other._sequence;
    _virtualKey             = other._virtualKey;
    _ownerSupportsTextInput = other._ownerSupportsTextInput;
    _allowCharacter         = other._allowCharacter;
    _valid                  = other._valid;
    other.Reset();
    return *this;
}
struct GraphicsDevice::State
{
    DWORD thread = GetCurrentThreadId();
    wil::com_ptr_nothrow<ID3D11Device> device;
    wil::com_ptr_nothrow<ID2D1Factory1> factory;
    wil::com_ptr_nothrow<ID2D1Device> d2d;
    wil::com_ptr_nothrow<IDWriteFactory> write;
    wil::com_ptr_nothrow<ID3D11VertexShader> vs;
    wil::com_ptr_nothrow<ID3D11PixelShader> ps;
    wil::com_ptr_nothrow<ID3D11BlendState> blend;
    wil::com_ptr_nothrow<ID3D11RasterizerState> raster;
    wil::com_ptr_nothrow<ID3D11DepthStencilState> depth;
    wil::com_ptr_nothrow<ID3D11SamplerState> sampler;
};
GraphicsDevice::GraphicsDevice() : _state(std::make_unique<State>())
{
}
GraphicsDevice::~GraphicsDevice() = default;
ID3D11Device* GraphicsDevice::GetDevice() const noexcept
{
    return _state->device.get();
}
HRESULT GraphicsDevice::Create(ID3D11Device* device, std::shared_ptr<GraphicsDevice>& result) noexcept
{
    if (! device || ! (device->GetCreationFlags() & D3D11_CREATE_DEVICE_BGRA_SUPPORT))
        return E_INVALIDARG;
    try
    {
        std::shared_ptr<GraphicsDevice> created(new GraphicsDevice);
        auto& s  = *created->_state;
        s.device = device;
        D2D1_FACTORY_OPTIONS options{};
        RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, s.factory.put_void()));
        wil::com_ptr_nothrow<IDXGIDevice> dxgi;
        RETURN_IF_FAILED(device->QueryInterface(IID_PPV_ARGS(dxgi.put())));
        RETURN_IF_FAILED(s.factory->CreateDevice(dxgi.get(), s.d2d.put()));
        RETURN_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), s.write.put_unknown()));
        RETURN_IF_FAILED(device->CreateVertexShader(g_DxUiCompositeVS, sizeof(g_DxUiCompositeVS), nullptr, s.vs.put()));
        RETURN_IF_FAILED(device->CreatePixelShader(g_DxUiCompositePS, sizeof(g_DxUiCompositePS), nullptr, s.ps.put()));
        D3D11_BLEND_DESC blend{};
        auto& target                 = blend.RenderTarget[0];
        target.BlendEnable           = TRUE;
        target.SrcBlend              = D3D11_BLEND_ONE;
        target.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOp               = D3D11_BLEND_OP_ADD;
        target.SrcBlendAlpha         = D3D11_BLEND_ONE;
        target.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        RETURN_IF_FAILED(device->CreateBlendState(&blend, s.blend.put()));
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode        = D3D11_FILL_SOLID;
        raster.CullMode        = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;
        RETURN_IF_FAILED(device->CreateRasterizerState(&raster, s.raster.put()));
        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable   = FALSE;
        depth.StencilEnable = FALSE;
        RETURN_IF_FAILED(device->CreateDepthStencilState(&depth, s.depth.put()));
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD                                         = D3D11_FLOAT32_MAX;
        RETURN_IF_FAILED(device->CreateSamplerState(&sampler, s.sampler.put()));
        result = std::move(created);
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}
struct EmbeddedHost::State
{
    std::shared_ptr<GraphicsDevice> graphics;
    EmbeddedCallbacks callbacks{};
    wil::com_ptr_nothrow<ID3D11Texture2D> texture;
    wil::com_ptr_nothrow<ID3D11ShaderResourceView> view;
    wil::com_ptr_nothrow<ID2D1Bitmap1> bitmap;
    UINT width = 0, height = 0;
    float dpi    = 96;
    bool visible = true, dirty = true, coherent = false;
    bool interactionLayoutAvailable      = false;
    bool zeroSized                       = false;
    bool animationSuspended              = false;
    bool preparationRequested            = true; // Initial dirty state already requires the caller's first Prepare.
    bool recoveryWakePending             = false;
    bool preparing                       = false;
    bool prepareCallActive               = false;
    bool contentPending                  = false;
    uint64_t contentInteractionRevision  = 0u;
    uint64_t revision                    = 0;
    uint64_t preparedInteractionRevision = 0;
    EmbeddedStatistics stats{};
};
EmbeddedHost::EmbeddedHost() = default;
EmbeddedHost::~EmbeddedHost()
{
    Detach();
}
HRESULT EmbeddedHost::Attach(std::shared_ptr<GraphicsDevice> graphics, EmbeddedCallbacks callbacks) noexcept
{
    if (_detaching || ! graphics || graphics->_state->thread != GetCurrentThreadId())
        return E_INVALIDARG;
    if (_state)
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    try
    {
        _state = std::make_unique<State>();
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    ++_attachmentRevision;
    _state->callbacks = callbacks;
    // A new attachment brings its own application clock: its first tick sets the baseline. The timeline continues from the
    // clock the host read before, so a transition stamped then neither waits for it nor jumps.
    _host._animationTimelineMs     = (std::max)(_host._animationTimelineMs, _host.GetAnimationTickMs());
    _host._animationTimelineRebase = true;
    _host._embedded                = true;
    _host._embeddedContext         = this;
    _host._embeddedInvalidate      = &InvalidateThunk;
    _host._attachmentOwnerThreadId = GetCurrentThreadId();
    const auto hr                  = ReplaceDevice(std::move(graphics));
    if (FAILED(hr))
        Detach();
    else
        // Attach starts dirty: the caller supplies the first Prepare. Resource
        // replacement released its surface state, not an application wakeup.
        _state->preparationRequested = true;
    return hr;
}
void EmbeddedHost::Detach() noexcept
{
    if (! _state || _detaching)
        return;
    _detaching                 = true;
    const auto finishDetach    = wil::scope_exit([this]() noexcept { _detaching = false; });
    State* const attachedState = _state.get();
    ++_attachmentRevision;
    ++_operationRevision;
    DisconnectAccessibility();
    CancelPointer();
    CancelTextInput();
    _host._embeddedInvalidate = nullptr;
    _host._embeddedContext    = nullptr;
    _host.Detach();
    _host._embedded                   = false;
    _host._embeddedAnimationRequested = false;
    _textInputRevision += attachedState->revision + 1;
    _state.reset();
}
HRESULT EmbeddedHost::ReplaceDevice(std::shared_ptr<GraphicsDevice> graphics) noexcept
{
    if (! _state || _detaching || _replacingDevice || ! graphics || graphics->_state->thread != GetCurrentThreadId())
        return E_INVALIDARG;
    State* const attachedState        = _state.get();
    const uint64_t attachmentRevision = _attachmentRevision;
    const uint64_t operationRevision  = ++_operationRevision;
    _replacingDevice                  = true;
    const auto finishReplacement      = wil::scope_exit([this]() noexcept { _replacingDevice = false; });
    const auto attachmentIsCurrent    = [&]() noexcept { return _state.get() == attachedState && _attachmentRevision == attachmentRevision && ! _detaching; };
    const auto operationIsCurrent     = [&]() noexcept { return attachmentIsCurrent() && _operationRevision == operationRevision; };
    ReleaseSurface();
    if (! operationIsCurrent())
        return E_ABORT;
    auto& s = *attachedState;
    auto& g = *graphics->_state;
    s.graphics.reset();
    _host.DiscardDeviceResources();
    if (! operationIsCurrent())
        return E_ABORT;
    _host._d3dDevice     = g.device.get();
    _host._d2dFactory    = g.factory.get();
    _host._d2dDevice     = g.d2d.get();
    _host._dwriteFactory = g.write.get();
    ++_host._typographyRevision;
    // Text measured through the old factory no longer counts: the next preparation lays the content out again, as a DPI
    // change does, instead of keeping the old geometry until some other change asks for layout.
    if (_host._root)
        ControlModelQueryAccess::RequestIntrinsicLayoutOfTree(*_host._root);
    g.device->GetImmediateContext(_host._d3dContext.put());
    RETURN_IF_FAILED(g.d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _host._d2dContext.put()));
    _host._d2dContext->SetUnitMode(D2D1_UNIT_MODE_DIPS);
    _host._d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    if (! _host.EnsureDeviceIndependentResources())
        return E_FAIL;
    if (! operationIsCurrent())
        return E_ABORT;
    _host.RecreateBrushCache();
    s.graphics = std::move(graphics);
    if (s.prepareCallActive)
    {
        // This frame belongs to the retired resource epoch. Queue one owner
        // wake for the replacement; the retiring frame must not retry it.
        s.recoveryWakePending  = true;
        s.preparationRequested = false;
    }
    else
        s.preparationRequested = false;
    return S_OK;
}
void EmbeddedHost::InvalidateThunk(void* context) noexcept
{
    static_cast<EmbeddedHost*>(context)->MarkDirty();
}
void EmbeddedHost::MarkDirty() noexcept
{
    if (! _state)
        return;
    auto& s = *_state;
    s.dirty = true;
    ++s.revision;
    RequestPreparation();
}
void EmbeddedHost::RequestPreparation() noexcept
{
    if (! _state || ! _state->visible || _state->zeroSized || _state->preparing || _state->preparationRequested || ! _state->callbacks.requestPreparation)
        return;
    // Dirty pixels and an outstanding notification are different states. A
    // cooperative turn can consume a wakeup while the surface remains dirty.
    _state->preparationRequested = true;
    _state->callbacks.requestPreparation(_state->callbacks.context);
}
void EmbeddedHost::ReleaseSurface() noexcept
{
    if (! _state)
        return;
    State* const attachedState        = _state.get();
    const uint64_t attachmentRevision = _attachmentRevision;
    const uint64_t operationRevision  = _operationRevision;
    const auto operationIsCurrent     = [&]() noexcept
    { return _state.get() == attachedState && _attachmentRevision == attachmentRevision && _operationRevision == operationRevision && ! _detaching; };
    DisconnectAccessibility();
    if (! operationIsCurrent())
        return;
    CancelPointer();
    if (! operationIsCurrent())
        return;
    CancelTextInput();
    if (! operationIsCurrent())
        return;
    auto& s = *attachedState;
    // Drop the borrowed D2D target before the bitmap so no context reference keeps the texture alive.
    if (_host._d2dContext)
        _host._d2dContext->SetTarget(nullptr);
    s.bitmap.reset();
    s.view.reset();
    s.texture.reset();
    s.width = s.height           = 0;
    s.stats.surfaceBytes         = 0;
    s.coherent                   = false;
    s.interactionLayoutAvailable = false;
    s.contentPending             = false;
    s.preparationRequested       = false;
    s.dirty                      = true;
    // This is deliberately the final operation: visual cancellation can invoke
    // arbitrary control callbacks, including detaching this EmbeddedHost.
    _host.CancelPreparedVisualContent();
}
void EmbeddedHost::SetVisible(bool visible) noexcept
{
    if (! _state || _detaching || _state->visible == visible)
        return;
    State* const attachedState        = _state.get();
    const uint64_t attachmentRevision = _attachmentRevision;
    const uint64_t operationRevision  = ++_operationRevision;
    const auto attachmentIsCurrent    = [&]() noexcept { return _state.get() == attachedState && _attachmentRevision == attachmentRevision && ! _detaching; };
    const auto operationIsCurrent     = [&]() noexcept { return attachmentIsCurrent() && _operationRevision == operationRevision; };
    attachedState->visible            = visible;
    if (! visible)
    {
        attachedState->recoveryWakePending = false;
        ReleaseSurface();
        if (! operationIsCurrent())
            return;
        // Nothing paints while the view is hidden, so its controls return what painting rebuilds (a Grid's text layouts).
        if (_host._root)
            _host._root->OnHidden();
        if (! operationIsCurrent())
            return;
        _state->animationSuspended        = _host._embeddedAnimationRequested;
        _host._embeddedAnimationRequested = false;
    }
    else
    {
        _host._embeddedAnimationRequested = _host._embeddedAnimationRequested || _state->animationSuspended;
        _state->animationSuspended        = false;
        _host._animationTimelineRebase    = true;
        _state->dirty                     = true;
        if (_state->prepareCallActive)
            _state->recoveryWakePending = true;
        else
            RequestPreparation();
    }
}
bool EmbeddedHost::NeedsPreparation() const noexcept
{
    return _state && _state->visible && ! _state->zeroSized && _state->dirty;
}
bool EmbeddedHost::NeedsAnimation() const noexcept
{
    return _state && _state->visible && ! _state->zeroSized && _host._embeddedAnimationRequested;
}
bool EmbeddedHost::AdvanceAnimation(uint64_t tick) noexcept
{
    if (! NeedsAnimation())
        return false;
    // The timeline counts the time between ticks the view asked for: the first tick after a pause sets the baseline,
    // and a tick that goes back adds nothing.
    if (! _host._animationTimelineRebase && tick > _host._lastSuppliedAnimationTickMs)
    {
        const uint64_t deltaMs     = tick - _host._lastSuppliedAnimationTickMs;
        const uint64_t remainingMs = (std::numeric_limits<uint64_t>::max)() - _host._animationTimelineMs;
        _host._animationTimelineMs = deltaMs > remainingMs ? (std::numeric_limits<uint64_t>::max)() : _host._animationTimelineMs + deltaMs;
    }
    _host._lastSuppliedAnimationTickMs = tick;
    _host._animationTimelineRebase     = false;
    const uint64_t nowTickMs           = _host._animationTimelineMs;
    try
    {
        _host.ValidateSupplementalTooltipTarget();
        // Ticks dirty the view only through control invalidation; an idle tick leaves a clean surface clean.
        const bool rootTicking            = _host._root && _host._root->Tick(_host, nowTickMs);
        const bool tooltipTicking         = _host._tooltipLayer.Tick(_host, nowTickMs);
        _host._embeddedAnimationRequested = rootTicking || tooltipTicking;
        return _host._embeddedAnimationRequested;
    }
    catch (const std::exception&)
    {
        _host._embeddedAnimationRequested = false;
        return false;
    }
}
HRESULT EmbeddedHost::Prepare(UINT width, UINT height, float dpi) noexcept
{
    if (! _state || ! _state->graphics || _detaching || _replacingDevice)
        return E_UNEXPECTED;
    State* const attachedState        = _state.get();
    const uint64_t attachmentRevision = _attachmentRevision;
    const uint64_t operationRevision  = _operationRevision;
    const auto attachmentIsCurrent    = [&]() noexcept { return _state.get() == attachedState && _attachmentRevision == attachmentRevision; };
    const auto preparationIsCurrent   = [&]() noexcept
    { return attachmentIsCurrent() && _operationRevision == operationRevision && ! _detaching && ! _replacingDevice; };
    auto& s                  = *attachedState;
    const auto graphicsOwner = s.graphics;
    if (! graphicsOwner)
        return E_UNEXPECTED;
    auto& g = *graphicsOwner->_state;
    if (g.thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    if (s.prepareCallActive)
        return E_UNEXPECTED;
    s.prepareCallActive = true;
    // Keep the recursive-call guard set while the scheduling callback runs.
    // The callback may detach or replace this attachment, so only clear the
    // captured state when its attachment identity still matches on unwind.
    const auto finishPrepareCall = wil::scope_exit([&]() noexcept
    {
        if (_state.get() == attachedState && _attachmentRevision == attachmentRevision)
            attachedState->prepareCallActive = false;
    });
    bool requestAfterPreparation = false;
    auto observedRevision        = s.revision;
    s.preparing                  = true;
    const auto finishPreparation = wil::scope_exit([&]
    {
        if (! attachmentIsCurrent())
            return;
        s.preparing = false;
        // Never invoke application scheduling while a control paint or layout
        // callback is still on the stack. A new reentrant request is distinct
        // from automatically retrying the same failed work.
        const bool recoveryWake            = s.recoveryWakePending;
        s.recoveryWakePending              = false;
        const bool currentVisibleOperation = attachmentIsCurrent() && ! _detaching && ! _replacingDevice && s.visible && ! s.zeroSized;
        const bool request =
            currentVisibleOperation && (recoveryWake || (preparationIsCurrent() && (requestAfterPreparation || s.revision != observedRevision)));
        if (request)
            RequestPreparation();
    });
    s.preparationRequested       = false;
    s.contentPending             = false;
    const bool wasZeroSized      = s.zeroSized;
    s.zeroSized                  = ! width || ! height;
    if (s.zeroSized != wasZeroSized)
        _host._animationTimelineRebase = true;
    if (! s.visible || s.zeroSized)
    {
        // Neither state holds a surface; the next visible sized preparation allocates one.
        ReleaseSurface();
        if (! preparationIsCurrent())
            return E_ABORT;
        if (s.zeroSized && ! wasZeroSized && _host._root)
            _host._root->OnHidden();
        if (! preparationIsCurrent())
            return E_ABORT;
        return S_FALSE;
    }
    if (! std::isfinite(dpi) || dpi < 48 || dpi > 768 || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        s.coherent                   = false;
        s.interactionLayoutAvailable = false;
        CancelPointer();
        return E_INVALIDARG;
    }
    const uint64_t bytes = uint64_t(width) * height * 4;
    if (bytes > 64ull * 1024 * 1024)
    {
        s.coherent                   = false;
        s.interactionLayoutAvailable = false;
        CancelPointer();
        return E_OUTOFMEMORY;
    }
    const bool geometryChanged = s.preparedInteractionRevision != _host._interactionRevision;
    const bool targetChanged   = s.width && (width != s.width || height != s.height || dpi != s.dpi);
    // Sibling pane layout during a splitter drag changes the interaction revision. The captured control is still
    // the drag target, so that revision must not snap the drag back. A new view size, or a capture whose control
    // is gone, hidden or disabled, still cancels.
    if (targetChanged || (geometryChanged && ! CapturedDragContinues()))
    {
        CancelPointer();
        if (! preparationIsCurrent())
            return E_ABORT;
    }
    if (! s.dirty && s.coherent && width == s.width && height == s.height && dpi == s.dpi)
        return S_FALSE;
    s.coherent                   = false;
    s.interactionLayoutAvailable = false;
    s.dirty                      = true;
    try
    {
        FrameStage preparationStage = FrameStage::Idle;
        FrameStageScope updateStage(preparationStage, FrameStage::Update);
        if (width != s.width || height != s.height || ! s.texture)
        {
            wil::com_ptr_nothrow<ID3D11Texture2D> texture;
            wil::com_ptr_nothrow<ID3D11ShaderResourceView> view;
            wil::com_ptr_nothrow<ID2D1Bitmap1> bitmap;
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width            = width;
            desc.Height           = height;
            desc.MipLevels        = 1;
            desc.ArraySize        = 1;
            desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage            = D3D11_USAGE_DEFAULT;
            desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            RETURN_IF_FAILED(g.device->CreateTexture2D(&desc, nullptr, texture.put()));
            RETURN_IF_FAILED(g.device->CreateShaderResourceView(texture.get(), nullptr, view.put()));
            wil::com_ptr_nothrow<IDXGISurface> surface;
            RETURN_IF_FAILED(texture->QueryInterface(IID_PPV_ARGS(surface.put())));
            auto properties = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(desc.Format, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
            RETURN_IF_FAILED(_host._d2dContext->CreateBitmapFromDxgiSurface(surface.get(), &properties, bitmap.put()));
            _host._d2dContext->SetTarget(nullptr);
            s.stats.replacementPeakBytes = std::max(s.stats.replacementPeakBytes, s.stats.surfaceBytes + bytes);
            s.texture                    = std::move(texture);
            s.view                       = std::move(view);
            s.bitmap                     = std::move(bitmap);
            s.stats.surfaceBytes         = bytes;
            ++s.stats.surfaceAllocations;
            s.width  = width;
            s.height = height;
        }
        const bool dpiChanged = s.dpi != dpi;
        s.dpi                 = dpi;
        _host._dpi            = static_cast<UINT>(dpi);
        _host._widthPx        = width;
        _host._heightPx       = height;
        _host._d2dContext->SetDpi(dpi, dpi);
        _host._d2dContext->SetTarget(s.bitmap.get());
        if (_host._root)
        {
            if (dpiChanged)
            {
                _host._root->OnHostDpiChanged(_host);
                if (! preparationIsCurrent())
                    return E_ABORT;
            }
            if (_host._root)
            {
                _host._root->SetBounds(D2D1::RectF(0, 0, float(width) * 96 / dpi, float(height) * 96 / dpi));
                if (! preparationIsCurrent())
                    return E_ABORT;
            }
        }
        _host.TrimCaches();
        if (! preparationIsCurrent())
            return E_ABORT;
        observedRevision       = s.revision;
        const bool layoutReady = _host.PreparePendingPageLayout();
        if (! preparationIsCurrent())
            return E_ABORT;
        if (! layoutReady)
        {
            s.contentPending             = _host._contentPreparationContinues;
            s.contentInteractionRevision = _host._interactionRevision;
            if (s.contentPending)
            {
                requestAfterPreparation = true;
                return S_FALSE;
            }
            // The finish guard schedules a changed revision after callbacks unwind.
            // A failure of unchanged content stays dirty without an automatic retry.
            return E_ABORT;
        }
        ID3D11ShaderResourceView* empty = nullptr;
        _host._d3dContext->PSSetShaderResources(0, 1, &empty);
        _host._d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
        observedRevision                                 = s.revision;
        const auto revision                              = s.revision;
        const auto interactionRevision                   = _host._interactionRevision;
        wil::com_ptr_nothrow<ID2D1DeviceContext> dcOwner = _host._d2dContext;
        auto* dc                                         = dcOwner.get();
        if (! dc)
            return E_UNEXPECTED;
        FrameStageScope renderStage(preparationStage, FrameStage::Render);
        dc->BeginDraw();
        auto finish = wil::scope_exit([&] { dc->EndDraw(); });
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        dc->Clear(D2D1::ColorF(0, 0, 0, 0));
        Control* const paintRoot    = _host._root.get();
        const auto rootLifetime     = paintRoot ? GetControlLifetimeToken(*paintRoot) : std::weak_ptr<int>{};
        const auto treeStillCurrent = [&]() noexcept
        {
            return preparationIsCurrent() && (! paintRoot || ! rootLifetime.expired()) && _host._root.get() == paintRoot &&
                   _host._interactionRevision == interactionRevision;
        };
        if (_host._root)
        {
            _host._root->Paint(_host);
            if (! treeStillCurrent())
                return E_ABORT;
            _host._root->PaintOverlay(_host);
            if (! treeStillCurrent())
                return E_ABORT;
        }
        if (_host._tooltipLayer.HasTooltip())
        {
            _host._tooltipLayer.Paint(_host);
            if (! treeStillCurrent())
                return E_ABORT;
        }
        finish.release();
        RETURN_IF_FAILED(dc->EndDraw());
        if (! preparationIsCurrent())
            return E_ABORT;
        s.preparedInteractionRevision = interactionRevision;
        s.coherent                    = interactionRevision == _host._interactionRevision;
        s.interactionLayoutAvailable  = true;
        s.dirty                       = s.revision != revision;
        ++s.stats.preparations;
        _host.PublishPreparedContent();
        if (! preparationIsCurrent())
            return E_ABORT;
        s.coherent              = s.coherent && interactionRevision == _host._interactionRevision;
        s.dirty                 = s.revision != revision;
        requestAfterPreparation = s.dirty;
        if (! s.coherent)
            return E_ABORT;
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        return E_FAIL;
    }
}
HRESULT EmbeddedHost::PrepareInteraction(UINT width, UINT height, float dpi) noexcept
{
    if (! _state || ! _state->graphics || _detaching || _replacingDevice)
        return E_UNEXPECTED;
    State* const attachedState        = _state.get();
    const uint64_t attachmentRevision = _attachmentRevision;
    const uint64_t operationRevision  = _operationRevision;
    const auto attachmentIsCurrent    = [&]() noexcept { return _state.get() == attachedState && _attachmentRevision == attachmentRevision; };
    const auto preparationIsCurrent   = [&]() noexcept
    { return attachmentIsCurrent() && _operationRevision == operationRevision && ! _detaching && ! _replacingDevice; };
    auto& s = *attachedState;
    if (s.graphics->_state->thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    if (! std::isfinite(dpi) || dpi < 48 || dpi > 768)
    {
        s.coherent                   = false;
        s.interactionLayoutAvailable = false;
        CancelPointer();
        return E_INVALIDARG;
    }
    // Layout may be acknowledged between frames, but never rehabilitate a failed paint, an unpainted device,
    // or geometry with a different pixel-to-DIP mapping. Those need the ordinary full preparation boundary.
    if (! s.visible || s.zeroSized || ! s.interactionLayoutAvailable || ! s.bitmap || ! width || ! height || width != s.width || height != s.height ||
        dpi != s.dpi)
    {
        s.coherent                   = false;
        s.interactionLayoutAvailable = false;
        CancelPointer();
        return S_FALSE;
    }
    if (s.coherent && s.preparedInteractionRevision == _host._interactionRevision)
        return S_FALSE;
    const auto revision = _host._interactionRevision;
    if (! CapturedDragContinues())
    {
        CancelPointer();
        if (! preparationIsCurrent())
            return E_ABORT;
    }
    // A cancellation callback may itself change the tree or bounds. The caller has not arranged that state yet.
    if (revision != _host._interactionRevision)
    {
        s.coherent = false;
        return HRESULT_FROM_WIN32(ERROR_RETRY);
    }
    s.preparedInteractionRevision = revision;
    s.coherent                    = true;
    // The cached pixels/text geometry and accessibility snapshot still belong to the previous frame.
    // Do not clear dirty or publish either; the application's next frame must call Prepare.
    s.dirty = true;
    return S_OK;
}
HRESULT EmbeddedHost::Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) noexcept
{
    if (! _state || ! context)
        return E_INVALIDARG;
    auto& s = *_state;
    if (! s.visible || ! s.coherent)
        return S_FALSE;
    auto& g = *s.graphics->_state;
    if (g.thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return E_INVALIDARG;
    wil::com_ptr_nothrow<ID3D11Device> device;
    context->GetDevice(device.put());
    if (device.get() != g.device.get())
        return E_INVALIDARG;
    if (! std::isfinite(viewport.TopLeftX) || ! std::isfinite(viewport.TopLeftY) || ! std::isfinite(viewport.MinDepth) || ! std::isfinite(viewport.MaxDepth) ||
        viewport.MinDepth < 0 || viewport.MaxDepth > 1 || viewport.MinDepth > viewport.MaxDepth)
        return E_INVALIDARG;
    if (! std::isfinite(viewport.Width) || ! std::isfinite(viewport.Height) || viewport.Width <= 0 || viewport.Height <= 0)
        return E_INVALIDARG;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(g.raster.get());
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g.vs.get(), nullptr, 0);
    context->PSSetShader(g.ps.get(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->OMSetDepthStencilState(g.depth.get(), 0);
    context->OMSetBlendState(g.blend.get(), nullptr, 0xffffffff);
    auto* view    = s.view.get();
    auto* sampler = g.sampler.get();
    context->PSSetShaderResources(0, 1, &view);
    context->PSSetSamplers(0, 1, &sampler);
    context->SetPredication(nullptr, FALSE);
    context->Draw(3, 0);
    view = nullptr;
    context->PSSetShaderResources(0, 1, &view);
    ++s.stats.composites;
    return S_OK;
}
bool EmbeddedHost::CapturedDragContinues() const noexcept
{
    const Control* const captured = _host._capturedControl;
    // Resolve the capture through the live tree before touching it: a removed control may already be destroyed.
    // Ancestors count as well, so hiding or disabling a containing panel cancels like the control itself.
    if (! IsControlEffectivelyInteractive(_host._root.get(), captured))
        return false;
    const D2D1_RECT_F bounds     = captured->GetBounds();
    const D2D1_RECT_F capturedAt = _host._capturedBounds;
    return bounds.left == capturedAt.left && bounds.top == capturedAt.top && bounds.right == capturedAt.right && bounds.bottom == capturedAt.bottom;
}

void EmbeddedHost::CancelPointer(bool clearPendingDoubleClick) noexcept
{
    auto* captured         = _host._capturedControl;
    _host._capturedControl = nullptr;
    try
    {
        // Hidden/disabled controls still need their draft canceled before ordinary interaction pruning. Tree
        // membership is resolved without dereferencing a possibly removed captured pointer.
        if (captured && IsControlInTree(_host._root.get(), captured))
            captured->OnCaptureLost(_host);
        // A host commonly releases OS capture after every successful Up, which produces a Cancel notification even
        // though no press remains active. Keep that completed click as a candidate for the next double-click. A real
        // cancellation of an active press or an internal suspension/device-loss reset invalidates the candidate.
        if (clearPendingDoubleClick)
            _host.ClearPendingPointerDoubleClick();
        _host.PruneStaleInteractionState();
    }
    catch (const std::exception&)
    { /* Capture is already cleared. */
    }
}
bool EmbeddedHost::InputIsCoherent(bool allowDirty) noexcept
{
    if (! _state || ! _state->visible || ! _state->coherent)
        return false;
    if (_state->preparedInteractionRevision != _host._interactionRevision)
    {
        _state->coherent = false;
        CancelPointer();
        return false;
    }
    // Paint-dirty means the cached surface needs another Prepare before Composite.
    // Hit-testing still uses live control bounds; only a changed interaction revision
    // (bounds, tree, visibility, enabled) invalidates geometry. Pixel snapshots pass
    // allowDirty=false so they do not publish a stale surface.
    return allowDirty || ! _state->dirty;
}
bool EmbeddedHost::TextInputIsAvailable() noexcept
{
    if (InputIsCoherent(true))
        return true;
    if (! _state || ! _state->visible || _state->zeroSized || ! _state->contentPending || _state->contentInteractionRevision != _host._interactionRevision)
        return false;
    auto* target = _host.GetFocusControl();
    return target && IsControlInTree(_host._root.get(), target) && target->GetHost() == &_host && target->IsEnabled() && target->IsVisible() &&
           target->HasFocus() && target->AcceptsTextInputDuringPreparation();
}
bool EmbeddedHost::KeyboardInputIsAvailable() noexcept
{
    if (! _state || ! _state->visible || _state->zeroSized)
        return false;
    // Pending rich preparation keeps its source-only permission and revision checks.
    if (_state->contentPending)
        return TextInputIsAvailable();
    return _state->interactionLayoutAvailable;
}
bool EmbeddedHost::DispatchPointer(const PointerEvent& event) noexcept
{
    // Pointer hit-testing is valid on a paint-dirty view. Requiring a clean surface
    // here dropped the Down that followed a hover/focus Invalidate, so a touch looked
    // like "first contact focuses, second contact activates."
    if (! InputIsCoherent(true))
        return false;
    try
    {
        _host.PruneStaleInteractionState();
        if (! InputIsCoherent(true))
            return false;
        if (event.action == PointerAction::Down || event.action == PointerAction::Move || event.action == PointerAction::Up ||
            event.action == PointerAction::Wheel)
        {
            _host.SetInputModality(InputModality::Pointer);
            _host.SetPointerDevice(event.device);
        }
        const auto point = D2D1::Point2F(event.xPixels * 96 / _state->dpi, event.yPixels * 96 / _state->dpi);
        {
            UINT inspectionMessage = WM_MOUSEMOVE;
            switch (event.action)
            {
                case PointerAction::Down: inspectionMessage = WM_LBUTTONDOWN; break;
                case PointerAction::Up: inspectionMessage = WM_LBUTTONUP; break;
                case PointerAction::Wheel: inspectionMessage = WM_MOUSEWHEEL; break;
                case PointerAction::Cancel: inspectionMessage = WM_CAPTURECHANGED; break;
                case PointerAction::Leave: inspectionMessage = WM_MOUSELEAVE; break;
                case ::DxUi::PointerAction::Move:
                default: break;
            }
            if (_host.HandleTooltipInspectionPointer(inspectionMessage, point, event.wheelDelta))
            {
                return true;
            }
        }
        if (event.action == PointerAction::Cancel)
        {
            CancelPointer(_host._capturedControl != nullptr);
            return true;
        }
        if (event.action == PointerAction::Leave)
        {
            _host.UpdateHover(D2D1::Point2F(-1e6f, -1e6f), event.modifiers);
            return true;
        }
        // A captured control receives its event once through the dispatch below. A synthetic hover Move here
        // would advance the same draft twice and could mutate pane geometry before the actual captured event.
        if (! _host._capturedControl)
        {
            _host.UpdateHover(point, event.modifiers);
            if (! InputIsCoherent(true))
                return false;
        }
        auto* target = _host._capturedControl ? _host._capturedControl : _host.HitTestControl(point);
        if (! InputIsCoherent(true) || ! target)
            return false;
        switch (event.action)
        {
            case PointerAction::Down:
            {
                const auto lifetime    = target->GetLifetimeToken();
                const bool doubleClick = _host.ShouldTreatPointerDownAsDoubleClick(target, point);
                const bool handled =
                    doubleClick ? target->OnMouseDoubleClick(_host, point, false, event.modifiers) : target->OnMouseDown(_host, point, false, event.modifiers);
                Control* const live = (! lifetime.expired() && target->GetHost() == &_host) ? target : nullptr;
                // Like the native host, capture a handled press after the callback; the callback may remove its control.
                if (handled && live && ! _host.IsTooltipInspectionActive())
                {
                    _host.CaptureMouse(live);
                    if (doubleClick)
                    {
                        _host.ClearPendingPointerDoubleClick();
                    }
                    else
                    {
                        _host.RememberPointerDownDip(live, point);
                    }
                    _host.PruneStaleInteractionState();
                }
                else if (doubleClick)
                {
                    _host.ClearPendingPointerDoubleClick();
                }
                return handled;
            }
            case PointerAction::Move: return target->OnMouseMove(_host, point, event.modifiers);
            case PointerAction::Up:
            {
                auto release = wil::scope_exit([&] { _host.ReleaseMouseCapture(); });
                return target->OnMouseUp(_host, point, false, event.modifiers);
            }
            case PointerAction::Wheel: return target->OnMouseWheel(_host, point, event.wheelDelta, event.modifiers);
            case ::DxUi::PointerAction::Cancel:
            case ::DxUi::PointerAction::Leave:
            default: return false;
        }
    }
    catch (const std::exception&)
    {
        CancelPointer();
        return false;
    }
}
bool EmbeddedHost::DispatchKey(UINT key, bool down, UINT modifiers) noexcept
{
    return DispatchKeyImpl(key, down, modifiers, nullptr);
}
bool EmbeddedHost::DispatchKey(UINT key, bool down, UINT modifiers, EmbeddedKeyDispatchReceipt& receipt) noexcept
{
    receipt.Reset();
    return DispatchKeyImpl(key, down, modifiers, &receipt);
}
bool EmbeddedHost::DispatchKeyImpl(UINT key, bool down, UINT modifiers, EmbeddedKeyDispatchReceipt* receipt) noexcept
{
    uint64_t issuedReceiptSequence = 0u;
    // Overlapping use of the same out-object cannot identify which caller's raw key should translate.
    // Invalidate it on every outer exit, including a callback failure; independent receipts stay valid.
    const auto invalidateOverlappedReceipt = wil::scope_exit([this, receipt, &issuedReceiptSequence]() noexcept
    {
        if (receipt && (receipt->_embeddedHost != this || receipt->_controlHost != &_host || receipt->_sequence != issuedReceiptSequence))
            receipt->Reset();
    });
    ControlHost::KeyCharacterDispatch keyDispatch{.previous = _host._keyCharacterDispatch, .virtualKey = key, .messageRoute = down};
    _host._keyCharacterDispatch = &keyDispatch;
    const auto restoreDispatch  = wil::scope_exit([this, &keyDispatch]() noexcept { _host._keyCharacterDispatch = keyDispatch.previous; });

    // Capture raw identities before pruning or input-modality callbacks. Only take a token from a control proven to
    // belong to the current tree, so stale focus pointers are never dereferenced while issuing the receipt.
    if (receipt && down && (key == VK_RETURN || key == VK_SPACE) && _state)
    {
        ++_keyDispatchSequence;
        if (_keyDispatchSequence == 0u)
            ++_keyDispatchSequence;
        issuedReceiptSequence            = _keyDispatchSequence;
        Control* const initialTarget     = _host.GetFocusControl();
        Control* const initialRoot       = _host._root.get();
        const bool ownerIsInTree         = initialTarget && IsControlInTree(initialRoot, initialTarget);
        receipt->_embeddedHost           = this;
        receipt->_controlHost            = &_host;
        receipt->_owner                  = ownerIsInTree ? initialTarget : nullptr;
        receipt->_root                   = initialRoot;
        receipt->_ownerLifetime          = ownerIsInTree ? initialTarget->GetLifetimeToken() : std::weak_ptr<int>{};
        receipt->_rootLifetime           = initialRoot ? initialRoot->GetLifetimeToken() : std::weak_ptr<int>{};
        receipt->_attachmentRevision     = _textInputRevision;
        receipt->_focusOwnerEpoch        = _host._keyboardOwnerEpoch;
        receipt->_sequence               = issuedReceiptSequence;
        receipt->_virtualKey             = key;
        receipt->_ownerSupportsTextInput = ownerIsInTree && initialTarget->SupportsTextInput();
        receipt->_valid                  = true;
    }

    if (! KeyboardInputIsAvailable())
        return false;
    try
    {
        _host.PruneStaleInteractionState();
        if (! KeyboardInputIsAvailable())
            return false;
        _host.SetInputModality(InputModality::Keyboard);
        auto* target = _host.GetFocusControl();
        if (down && _host.HandleTooltipInspectionKey(key))
        {
            return true;
        }
        if (down && key == VK_ESCAPE && _host._nativeTextInputImeComposing)
            return ApplyTextInput(_textInputRevision + _state->revision, {}, EmbeddedTextInputAction::Cancel) == S_OK;
        if (down && key == VK_TAB)
        {
            // A semantic editor owns list/cell Tab before ordinary host focus
            // traversal, just as under WM_KEYDOWN. A declined callback may also
            // retire/rebind its owner: never traverse a successor in that frame.
            auto* tabTarget     = _host.GetFocusControl();
            const auto lifetime = tabTarget ? tabTarget->GetLifetimeToken() : std::weak_ptr<int>{};
            if (tabTarget && tabTarget->OnKeyDown(_host, key, modifiers))
                return true;
            if (tabTarget && (lifetime.expired() || _host.GetFocusControl() != tabTarget || tabTarget->GetHost() != &_host || ! tabTarget->IsEnabled() ||
                              ! tabTarget->IsVisible()))
                return true;
            return _host.HandleTabNavigation((modifiers & MK_SHIFT) != 0);
        }
        target = _host.GetFocusControl();
        if (! target)
            return false;

        const bool handled = down ? target->OnKeyDown(_host, key, modifiers) : target->OnKeyUp(_host, key, modifiers);
        if (receipt && issuedReceiptSequence != 0u && receipt->_valid && receipt->_embeddedHost == this && receipt->_controlHost == &_host &&
            receipt->_sequence == issuedReceiptSequence)
        {
            if (! handled && ! receipt->_ownerLifetime.expired() && target == _host.GetFocusControl() && target->GetHost() == &_host &&
                _host._keyboardOwnerEpoch == receipt->_focusOwnerEpoch && target->SupportsTextInput())
                keyDispatch.allowCharacter = true;
            receipt->_allowCharacter = keyDispatch.allowCharacter;
        }
        return handled;
    }
    catch (const std::bad_alloc&)
    {
        std::terminate();
    }
    catch (const std::exception&)
    {
        // Noexcept input boundary: a failed key route must not authorize a later translated character.
        if (receipt && issuedReceiptSequence != 0u && receipt->_valid && receipt->_embeddedHost == this && receipt->_controlHost == &_host &&
            receipt->_sequence == issuedReceiptSequence)
            receipt->_allowCharacter = false;
        Debug::Warning(L"DxUi::EmbeddedHost: key dispatch failed; translated character permission withheld.");
        return false;
    }
}
bool EmbeddedHost::DispatchCharacter(wchar_t character, UINT modifiers) noexcept
{
    if (! KeyboardInputIsAvailable())
        return false;
    try
    {
        _host.PruneStaleInteractionState();
        if (! KeyboardInputIsAvailable())
            return false;
        auto* target = _host.GetFocusControl();
        return target && target->OnChar(_host, character, modifiers);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool EmbeddedHost::ShouldTranslateCharacter(const EmbeddedKeyDispatchReceipt& receipt) noexcept
{
    if (! receipt._valid || receipt._embeddedHost != this || receipt._controlHost != &_host || ! receipt._allowCharacter || ! receipt._ownerSupportsTextInput ||
        ! _state || ! _state->visible || _state->zeroSized || receipt._attachmentRevision != _textInputRevision)
        return false;

    Control* const owner = receipt._owner;
    Control* const root  = receipt._root;
    if (! owner || receipt._ownerLifetime.expired() || ! root || receipt._rootLifetime.expired() || _host._root.get() != root ||
        _host._keyboardOwnerEpoch != receipt._focusOwnerEpoch || _host.GetFocusControl() != owner || owner->GetHost() != &_host || ! owner->HasFocus() ||
        ! owner->SupportsTextInput() || ! IsControlEffectivelyInteractive(root, owner))
        return false;
    if (_state->coherent && _state->preparedInteractionRevision == _host._interactionRevision)
        return true;
    return _state->contentPending && _state->contentInteractionRevision == _host._interactionRevision && IsControlInTree(_host._root.get(), owner) &&
           owner->IsEnabled() && owner->IsVisible() && owner->HasFocus() && owner->AcceptsTextInputDuringPreparation();
}
bool EmbeddedHost::DispatchCharacter(wchar_t character, UINT modifiers, EmbeddedKeyDispatchReceipt& receipt) noexcept
{
    if (! receipt._valid)
        return false;

    const wchar_t expectedCharacter = receipt._virtualKey == VK_RETURN ? L'\r' : L' ';
    const bool receiptBelongsHere   = receipt._embeddedHost == this && receipt._controlHost == &_host;
    // A foreign view or an unrelated character does not consume the caller's token; it may still be delivered to
    // the correct view/message. A matching attempt below consumes it before any user callback can re-enter.
    if (! receiptBelongsHere || character != expectedCharacter)
        return false;

    const bool currentAttachment      = _state && receipt._attachmentRevision == _textInputRevision;
    const auto owner                  = receipt._owner;
    const auto ownerLifetime          = receipt._ownerLifetime;
    const auto root                   = receipt._root;
    const auto rootLifetime           = receipt._rootLifetime;
    const uint64_t ownerEpoch         = receipt._focusOwnerEpoch;
    const uint64_t attachmentRevision = receipt._attachmentRevision;
    const bool ownerSupportsTextInput = receipt._ownerSupportsTextInput;
    const bool allowCharacter         = receipt._allowCharacter;
    receipt.Reset();

    if (! currentAttachment)
        return true;

    // A matching character belongs to its key event even if that callback retired its owner, replaced the tree,
    // detached this view, or moved focus. Never retarget it to a successor editor.
    if (! _state || _textInputRevision != attachmentRevision || _host._root.get() != root || (root && rootLifetime.expired()) ||
        _host._keyboardOwnerEpoch != ownerEpoch)
        return true;
    if (! owner)
        return true;
    if (ownerLifetime.expired() || _host.GetFocusControl() != owner || owner->GetHost() != &_host)
        return true;

    if (! allowCharacter)
        return true;
    if (! ownerSupportsTextInput || ! TextInputIsAvailable())
        return true;

    try
    {
        _host.PruneStaleInteractionState();
        if (! _state || _textInputRevision != attachmentRevision || (root && rootLifetime.expired()) || _host._root.get() != root ||
            _host._keyboardOwnerEpoch != ownerEpoch || ownerLifetime.expired() || _host.GetFocusControl() != owner || owner->GetHost() != &_host ||
            ! owner->SupportsTextInput() || ! owner->HasFocus() || ! IsControlEffectivelyInteractive(root, owner))
            return true;
        return owner->OnChar(_host, character, modifiers);
    }
    catch (const std::bad_alloc&)
    {
        std::terminate();
    }
    catch (const std::exception&)
    {
        // Noexcept input boundary: the matching receipt remains consumed when a control callback fails.
        Debug::Warning(L"DxUi::EmbeddedHost: correlated character dispatch failed; receipt consumed.");
        return false;
    }
}
namespace
{
bool ValidEmbeddedTextState(const NativeTextInputState& state, bool preview) noexcept
{
    const auto length = state.text.size();
    if (length > 65536 || state.compositionClauseBoundaries.size() > 256 || state.caretIndex > length || state.firstVisibleLine > length ||
        (state.selectionAnchorIndex && *state.selectionAnchorIndex > length))
        return false;
    const auto validRange = [length](std::optional<size_t> first, std::optional<size_t> last) noexcept
    { return first.has_value() == last.has_value() && (! first || (*first <= *last && *last <= length)); };
    if (! validRange(state.compositionStartIndex, state.compositionEndIndex) || ! validRange(state.conversionTargetStartIndex, state.conversionTargetEndIndex))
        return false;
    if (! preview)
        return ! state.compositionStartIndex && ! state.conversionTargetStartIndex && ! state.compositionCursorIndex &&
               state.compositionClauseBoundaries.empty();
    if (! state.compositionStartIndex)
        return false;
    const auto first = *state.compositionStartIndex, last = *state.compositionEndIndex;
    if ((state.conversionTargetStartIndex && (*state.conversionTargetStartIndex < first || *state.conversionTargetEndIndex > last)) ||
        (state.compositionCursorIndex && (*state.compositionCursorIndex < first || *state.compositionCursorIndex > last)))
        return false;
    size_t previous = first;
    for (const auto boundary : state.compositionClauseBoundaries)
    {
        if (boundary < previous || boundary > last)
            return false;
        previous = boundary;
    }
    return true;
}
} // namespace
HRESULT EmbeddedHost::ReadTextInput(EmbeddedTextInputSnapshot& snapshot) noexcept
{
    snapshot = {};
    if (! _state || ! _state->graphics)
        return E_UNEXPECTED;
    if (_state->graphics->_state->thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    if (! TextInputIsAvailable())
        return S_FALSE;
    _host.PruneStaleInteractionState();
    auto* control = _host.GetFocusControl();
    if (! control || control != _host._nativeTextInputControl || ! control->SupportsTextInput())
        return S_FALSE;
    try
    {
        // SetText/selection changes may originate outside input dispatch. Read the control, not an old cache.
        TextInputState actual;
        if (! _host.TryReadTextInputState(control, actual))
            return S_FALSE;
        if (_host._nativeTextInputImeComposing && actual.text != _host._nativeTextInputStateCache.text)
            _host.ClearNativeTextInputCompositionState();
        _host.SyncNativeTextInputSession(control);
        if (! _host.TryReadNativeTextInputState(control, snapshot.state))
            return S_FALSE;
        if (snapshot.state.text.size() > 65536)
        {
            snapshot = {};
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }
        if (! _state->dirty)
        {
            snapshot.caretBoundsDip    = control->GetTextInputCaretRect(_host, snapshot.state.caretIndex);
            snapshot.viewportBoundsDip = control->GetTextInputViewportRect();
        }
        snapshot.revision = _textInputRevision + _state->revision;
        snapshot.focusId  = _host._nativeTextInputFocusId;
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        snapshot = {};
        return E_OUTOFMEMORY;
    }
}
HRESULT EmbeddedHost::TransformClipboardPaste(uint64_t revision, uint64_t focusId, const TextClipboardContent& content, std::wstring& replacement) noexcept
try
{
    EmbeddedTextInputSnapshot current;
    const auto result = ReadTextInput(current);
    if (result != S_OK)
        return result;
    if (current.revision != revision || current.focusId != focusId)
        return HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
    if (current.state.readOnly)
        return E_ACCESSDENIED;
    if (current.state.compositionStartIndex || current.state.compositionEndIndex)
        return S_FALSE;
    auto* editor = dynamic_cast<RichTextEditor*>(_host.GetFocusControl());
    if (! editor)
        return E_NOTIMPL;
    return editor->TryPrepareClipboardPaste(content, replacement) ? S_OK : S_FALSE;
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
catch (const std::length_error&)
{
    return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
}
catch (const std::exception&)
{
    return E_INVALIDARG;
}
HRESULT EmbeddedHost::ApplyTextInput(uint64_t revision, const NativeTextInputState& state, EmbeddedTextInputAction action) noexcept
{
    if (! _state || ! _state->graphics)
        return E_UNEXPECTED;
    if (_state->graphics->_state->thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    if (action != EmbeddedTextInputAction::Preview && action != EmbeddedTextInputAction::Commit && action != EmbeddedTextInputAction::Cancel)
        return E_INVALIDARG;
    if (! TextInputIsAvailable())
        return S_FALSE;
    _host.PruneStaleInteractionState();
    if (! revision || revision != _textInputRevision + _state->revision)
        return HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
    auto* control = _host.GetFocusControl();
    if (! control || control != _host._nativeTextInputControl || ! control->SupportsTextInput())
        return S_FALSE;
    const auto lifetime  = control->GetLifetimeToken();
    const auto stillLive = [&]() noexcept
    { return ! lifetime.expired() && control == _host.GetFocusControl() && control == _host._nativeTextInputControl && control->GetHost() == &_host; };
    try
    {
        TextInputState actual;
        if (! _host.TryReadTextInputState(control, actual))
            return S_FALSE;
        if (action != EmbeddedTextInputAction::Cancel && ! ValidEmbeddedTextState(state, action == EmbeddedTextInputAction::Preview))
            return E_INVALIDARG;
        if (action != EmbeddedTextInputAction::Cancel && actual.readOnly && (action == EmbeddedTextInputAction::Preview || actual.text != state.text))
            return E_ACCESSDENIED;
        if (_host._nativeTextInputImeComposing && actual.text != _host._nativeTextInputStateCache.text)
            _host.ClearNativeTextInputCompositionState();
        const auto previous = _host._nativeTextInputStateCache;
        if (action == EmbeddedTextInputAction::Cancel)
        {
            if (! _host._nativeTextInputImeComposing)
                return S_FALSE;
            const auto base = _host._nativeTextInputImeBaseState;
            _host.ClearNativeTextInputCompositionState();
            if (base && ! control->ImportTextInputState(_host, *base, false))
                return stillLive() ? E_FAIL : S_OK;
        }
        else
        {
            TextInputState edit       = actual;
            edit.text                 = state.text;
            edit.selectionAnchorIndex = state.selectionAnchorIndex;
            edit.caretIndex           = state.caretIndex;
            edit.firstVisibleLine     = state.firstVisibleLine;
            if (action == EmbeddedTextInputAction::Preview)
            {
                // Allocate the ownership marker before importing the preview so cancellation can identify
                // exactly which text it owns even after the host's synchronized cache has changed.
                std::wstring previewText = edit.text;
                if (! _host._nativeTextInputImeComposing)
                    _host._nativeTextInputImeBaseState = actual;
                _host._nativeTextInputImeComposing                = true;
                _host._nativeTextInputCompositionStartIndex       = state.compositionStartIndex;
                _host._nativeTextInputCompositionEndIndex         = state.compositionEndIndex;
                _host._nativeTextInputConversionTargetStartIndex  = state.conversionTargetStartIndex;
                _host._nativeTextInputConversionTargetEndIndex    = state.conversionTargetEndIndex;
                _host._nativeTextInputCompositionCursorIndex      = state.compositionCursorIndex;
                _host._nativeTextInputCompositionClauseBoundaries = state.compositionClauseBoundaries;
                if (! control->ImportTextInputState(_host, edit, false))
                    return stillLive() ? E_FAIL : S_OK;
                if (! stillLive())
                    return S_OK;
                _host._nativeTextInputImePreviewText = std::move(previewText);
            }
            else
            {
                // Compare/notify against the pre-composition value even when the final text equals its preview.
                const auto base = _host._nativeTextInputImeBaseState;
                _host.ClearNativeTextInputCompositionState();
                if (base && ! control->ImportTextInputState(_host, *base, false))
                    return stillLive() ? E_FAIL : S_OK;
                if (! stillLive())
                    return S_OK;
                if (! control->ImportTextInputState(_host, edit, true))
                    return stillLive() ? E_FAIL : S_OK;
            }
        }
        ++_state->revision;
        // An application callback may remove/replace the root or focus another control.
        if (stillLive())
        {
            _host.SyncNativeTextInputSession(control);
            _host.RaiseNativeTextInputAccessibilityEvents(previous);
        }
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        _state->coherent                   = false;
        _state->interactionLayoutAvailable = false;
        MarkDirty();
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        // Consumer callbacks can throw after changing text. Suppress interaction until the next prepare
        // publishes a coherent control state; never let an exception escape this noexcept input boundary.
        _host.ClearNativeTextInputCompositionState();
        _state->coherent                   = false;
        _state->interactionLayoutAvailable = false;
        MarkDirty();
        return E_FAIL;
    }
}
HRESULT EmbeddedHost::HitTestTextInput(uint64_t revision, D2D1_POINT_2F point, size_t& index) noexcept
{
    index = 0;
    if (! std::isfinite(point.x) || ! std::isfinite(point.y))
        return E_INVALIDARG;
    EmbeddedTextInputSnapshot snapshot;
    const HRESULT read = ReadTextInput(snapshot);
    if (read != S_OK)
        return read;
    if (! revision || revision != snapshot.revision)
        return HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
    if (! InputIsCoherent(false))
        return S_FALSE;
    const auto hit = _host.GetFocusControl()->TryHitTestTextInputPoint(_host, point);
    if (! hit || *hit > snapshot.state.text.size())
        return S_FALSE;
    index = *hit;
    return S_OK;
}
HRESULT EmbeddedHost::GetTextInputRangeBounds(uint64_t revision, size_t start, size_t end, D2D1_RECT_F& bounds, bool& clipped) noexcept
{
    bounds  = {};
    clipped = true;
    EmbeddedTextInputSnapshot snapshot;
    const HRESULT read = ReadTextInput(snapshot);
    if (read != S_OK)
        return read;
    if (! revision || revision != snapshot.revision)
        return HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);
    if (start > end || end > snapshot.state.text.size())
        return E_INVALIDARG;
    if (! InputIsCoherent(false) || ! snapshot.viewportBoundsDip)
        return S_FALSE;
    const auto viewport   = *snapshot.viewportBoundsDip;
    bool found            = false;
    clipped               = false;
    const auto accumulate = [&](const D2D1_RECT_F& rect) noexcept
    {
        const D2D1_RECT_F visible{(std::max)(rect.left, viewport.left),
                                  (std::max)(rect.top, viewport.top),
                                  (std::min)(rect.right, viewport.right),
                                  (std::min)(rect.bottom, viewport.bottom)};
        clipped = clipped || visible.left != rect.left || visible.top != rect.top || visible.right != rect.right || visible.bottom != rect.bottom;
        if (visible.left >= visible.right || visible.top >= visible.bottom)
            return;
        if (! found)
        {
            bounds = visible;
            found  = true;
        }
        else
        {
            bounds.left   = (std::min)(bounds.left, visible.left);
            bounds.top    = (std::min)(bounds.top, visible.top);
            bounds.right  = (std::max)(bounds.right, visible.right);
            bounds.bottom = (std::max)(bounds.bottom, visible.bottom);
        }
    };
    try
    {
        auto* control = _host.GetFocusControl();
        if (start == end)
        {
            const auto caret = control->TryGetTextInputCaretRect(_host, start);
            if (! caret)
            {
                clipped = true;
                return S_FALSE;
            }
            accumulate(*caret);
        }
        else
        {
            const auto rects = control->TryGetTextInputRangeRects(_host, start, end);
            if (! rects)
            {
                clipped = true;
                return S_FALSE;
            }
            for (const auto& rect : *rects)
                accumulate(rect);
        }
        if (! found)
            clipped = true;
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        bounds  = {};
        clipped = true;
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        bounds  = {};
        clipped = true;
        return E_FAIL;
    }
}
void EmbeddedHost::CancelTextInput() noexcept
{
    if (! _state || (_state->graphics && _state->graphics->_state->thread != GetCurrentThreadId()))
        return;
    ++_state->revision;
    _host.DeactivateTextInput();
    _host.SetFocusControl(nullptr);
}
EmbeddedStatistics EmbeddedHost::GetStatistics() const noexcept
{
    if (! _state)
        return {};
    auto stats              = _state->stats;
    stats.cachedBrushes     = _host._brushCache.size();
    stats.cachedTextFormats = _host._configuredTextFormats.size();
    return stats;
}
} // namespace DxUi
