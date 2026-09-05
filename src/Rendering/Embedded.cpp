#include "../Controls/DxUi.Internal.h"
#include <DxUi/Embedded.h>
#include <DxUiCompositePS.h>
#include <DxUiCompositeVS.h>
#include <algorithm>
#include <cmath>
#include <new>
#include <wil/result.h>

namespace DxUi
{
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
    bool zeroSized                       = false;
    bool animationSuspended              = false;
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
    if (! graphics || graphics->_state->thread != GetCurrentThreadId())
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
    _state->callbacks              = callbacks;
    _host._embedded                = true;
    _host._embeddedContext         = this;
    _host._embeddedInvalidate      = &InvalidateThunk;
    _host._attachmentOwnerThreadId = GetCurrentThreadId();
    const auto hr                  = ReplaceDevice(std::move(graphics));
    if (FAILED(hr))
        Detach();
    return hr;
}
void EmbeddedHost::Detach() noexcept
{
    if (! _state)
        return;
    CancelPointer();
    CancelTextInput();
    _host._embeddedInvalidate = nullptr;
    _host._embeddedContext    = nullptr;
    _host.Detach();
    _host._embedded                   = false;
    _host._embeddedAnimationRequested = false;
    _textInputRevision += _state->revision + 1;
    _state.reset();
}
HRESULT EmbeddedHost::ReplaceDevice(std::shared_ptr<GraphicsDevice> graphics) noexcept
{
    if (! _state || ! graphics || graphics->_state->thread != GetCurrentThreadId())
        return E_INVALIDARG;
    CancelPointer();
    CancelTextInput();
    auto& s    = *_state;
    auto& g    = *graphics->_state;
    s.coherent = false;
    s.dirty    = true;
    s.view.reset();
    s.bitmap.reset();
    s.texture.reset();
    s.width = s.height   = 0;
    s.stats.surfaceBytes = 0;
    s.graphics.reset();
    _host.DiscardDeviceResources();
    _host._d3dDevice     = g.device.get();
    _host._d2dFactory    = g.factory.get();
    _host._d2dDevice     = g.d2d.get();
    _host._dwriteFactory = g.write.get();
    g.device->GetImmediateContext(_host._d3dContext.put());
    RETURN_IF_FAILED(g.d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _host._d2dContext.put()));
    _host._d2dContext->SetUnitMode(D2D1_UNIT_MODE_DIPS);
    _host._d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    if (! _host.EnsureDeviceIndependentResources())
        return E_FAIL;
    _host.RecreateBrushCache();
    s.graphics = std::move(graphics);
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
    auto& s            = *_state;
    const bool changed = ! s.dirty;
    s.dirty            = true;
    ++s.revision;
    if (changed && s.visible && ! s.zeroSized && s.callbacks.requestPreparation)
        s.callbacks.requestPreparation(s.callbacks.context);
}
void EmbeddedHost::SetVisible(bool visible) noexcept
{
    if (! _state || _state->visible == visible)
        return;
    _state->visible = visible;
    if (! visible)
    {
        CancelPointer();
        CancelTextInput();
        _state->animationSuspended        = _host._embeddedAnimationRequested;
        _host._embeddedAnimationRequested = false;
    }
    else
    {
        _host._embeddedAnimationRequested = _host._embeddedAnimationRequested || _state->animationSuspended;
        _state->animationSuspended        = false;
        _state->dirty                     = true;
        if (! _state->zeroSized && _state->callbacks.requestPreparation)
            _state->callbacks.requestPreparation(_state->callbacks.context);
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
    try
    {
        _host._lastAnimationTickMs = tick;
        _host.ValidateSupplementalTooltipTarget();
        const bool rootTicking            = _host._root && _host._root->Tick(_host, tick);
        const bool tooltipTicking         = _host._tooltipLayer.Tick(_host, tick);
        _host._embeddedAnimationRequested = rootTicking || tooltipTicking;
        MarkDirty();
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
    if (! _state || ! _state->graphics)
        return E_UNEXPECTED;
    auto& s = *_state;
    auto& g = *s.graphics->_state;
    if (g.thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    s.zeroSized = ! width || ! height;
    if (! s.visible || s.zeroSized)
    {
        CancelPointer();
        CancelTextInput();
        s.coherent = false;
        return S_FALSE;
    }
    if (! std::isfinite(dpi) || dpi < 48 || dpi > 768 || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        s.coherent = false;
        CancelPointer();
        return E_INVALIDARG;
    }
    const uint64_t bytes = uint64_t(width) * height * 4;
    if (bytes > 64ull * 1024 * 1024)
    {
        s.coherent = false;
        CancelPointer();
        return E_OUTOFMEMORY;
    }
    if (s.preparedInteractionRevision != _host._interactionRevision || (s.width && (width != s.width || height != s.height || dpi != s.dpi)))
        CancelPointer();
    if (! s.dirty && s.coherent && width == s.width && height == s.height && dpi == s.dpi)
        return S_FALSE;
    s.coherent = false;
    try
    {
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
                _host._root->OnHostDpiChanged(_host);
            _host._root->SetBounds(D2D1::RectF(0, 0, float(width) * 96 / dpi, float(height) * 96 / dpi));
        }
        ID3D11ShaderResourceView* empty = nullptr;
        _host._d3dContext->PSSetShaderResources(0, 1, &empty);
        _host._d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
        const auto revision            = s.revision;
        const auto interactionRevision = _host._interactionRevision;
        auto* dc                       = _host._d2dContext.get();
        dc->BeginDraw();
        auto finish = wil::scope_exit([&] { dc->EndDraw(); });
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        dc->Clear(D2D1::ColorF(0, 0, 0, 0));
        if (_host._root)
        {
            _host._root->Paint(_host);
            _host._root->PaintOverlay(_host);
        }
        if (_host._tooltipLayer.HasTooltip())
            _host._tooltipLayer.Paint(_host);
        finish.release();
        RETURN_IF_FAILED(dc->EndDraw());
        s.preparedInteractionRevision = interactionRevision;
        s.coherent                    = interactionRevision == _host._interactionRevision;
        s.dirty                       = s.revision != revision;
        ++s.stats.preparations;
        if (s.dirty && s.callbacks.requestPreparation)
            s.callbacks.requestPreparation(s.callbacks.context);
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
void EmbeddedHost::CancelPointer() noexcept
{
    // Validate by traversing live children without dereferencing a possibly removed captured pointer.
    const auto contains = [](auto&& self, const Control* root, const Control* target) noexcept -> bool
    {
        if (! root)
            return false;
        if (root == target)
            return true;
        for (size_t i = 0; i < root->GetLogicalChildCount(); ++i)
            if (self(self, root->GetLogicalChild(i), target))
                return true;
        return false;
    };
    auto* captured         = _host._capturedControl;
    _host._capturedControl = nullptr;
    try
    {
        // Hidden/disabled controls still need their draft canceled before ordinary interaction pruning.
        if (captured && contains(contains, _host._root.get(), captured))
            captured->OnCaptureLost(_host);
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
    return allowDirty || ! _state->dirty;
}
bool EmbeddedHost::DispatchPointer(const PointerEvent& event) noexcept
{
    if (! InputIsCoherent(event.action == PointerAction::Cancel || event.action == PointerAction::Leave ||
                          (event.action != PointerAction::Down && event.action != PointerAction::Wheel && _host._capturedControl)))
        return false;
    try
    {
        _host.PruneStaleInteractionState();
        const auto point = D2D1::Point2F(event.xPixels * 96 / _state->dpi, event.yPixels * 96 / _state->dpi);
        if (event.action == PointerAction::Cancel)
        {
            CancelPointer();
            return true;
        }
        if (event.action == PointerAction::Leave)
        {
            _host.UpdateHover(D2D1::Point2F(-1e6f, -1e6f), event.modifiers);
            return true;
        }
        _host.UpdateHover(point, event.modifiers);
        auto* target = _host._capturedControl ? _host._capturedControl : _host.HitTestControl(point);
        if (! target)
            return false;
        switch (event.action)
        {
            case PointerAction::Down:
            {
                const auto lifetime = target->GetLifetimeToken();
                const bool handled  = target->OnMouseDown(_host, point, false, event.modifiers);
                // Like the native host, capture a handled press after the callback; the callback may remove its control.
                if (handled && ! lifetime.expired() && target->GetHost() == &_host)
                {
                    _host.CaptureMouse(target);
                    _host.PruneStaleInteractionState();
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
    if (! InputIsCoherent(true))
        return false;
    try
    {
        _host.PruneStaleInteractionState();
        _host.SetInputModality(InputModality::Keyboard);
        if (down && key == VK_ESCAPE && _host._nativeTextInputImeComposing)
            return ApplyTextInput(_textInputRevision + _state->revision, {}, EmbeddedTextInputAction::Cancel) == S_OK;
        if (down && key == VK_TAB)
            return _host.HandleTabNavigation((modifiers & MK_SHIFT) != 0);
        auto* target = _host.GetFocusControl();
        return target && (down ? target->OnKeyDown(_host, key, modifiers) : target->OnKeyUp(_host, key, modifiers));
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool EmbeddedHost::DispatchCharacter(wchar_t character, UINT modifiers) noexcept
{
    if (! InputIsCoherent(true))
        return false;
    try
    {
        _host.PruneStaleInteractionState();
        auto* target = _host.GetFocusControl();
        return target && target->OnChar(_host, character, modifiers);
    }
    catch (const std::exception&)
    {
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
    if (! InputIsCoherent(true))
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
        snapshot.caretBoundsDip    = control->GetTextInputCaretRect(_host, snapshot.state.caretIndex);
        snapshot.viewportBoundsDip = control->GetTextInputViewportRect();
        snapshot.revision          = _textInputRevision + _state->revision;
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        snapshot = {};
        return E_OUTOFMEMORY;
    }
}
HRESULT EmbeddedHost::ApplyTextInput(uint64_t revision, const NativeTextInputState& state, EmbeddedTextInputAction action) noexcept
{
    if (! _state || ! _state->graphics)
        return E_UNEXPECTED;
    if (_state->graphics->_state->thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    if (action != EmbeddedTextInputAction::Preview && action != EmbeddedTextInputAction::Commit && action != EmbeddedTextInputAction::Cancel)
        return E_INVALIDARG;
    if (! InputIsCoherent(true))
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
                return E_FAIL;
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
                    return E_FAIL;
            }
            else
            {
                // Compare/notify against the pre-composition value even when the final text equals its preview.
                const auto base = _host._nativeTextInputImeBaseState;
                _host.ClearNativeTextInputCompositionState();
                if (base && ! control->ImportTextInputState(_host, *base, false))
                    return E_FAIL;
                if (! stillLive())
                    return S_OK;
                if (! control->ImportTextInputState(_host, edit, true))
                    return E_FAIL;
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
        _state->coherent = false;
        MarkDirty();
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        // Consumer callbacks can throw after changing text. Suppress interaction until the next prepare
        // publishes a coherent control state; never let an exception escape this noexcept input boundary.
        _host.ClearNativeTextInputCompositionState();
        _state->coherent = false;
        MarkDirty();
        return E_FAIL;
    }
}
void EmbeddedHost::CancelTextInput() noexcept
{
    if (! _state || (_state->graphics && _state->graphics->_state->thread != GetCurrentThreadId()))
        return;
    ++_state->revision;
    _host.DeactivateTextInput(false);
    _host.SetFocusControl(nullptr);
}
EmbeddedStatistics EmbeddedHost::GetStatistics() const noexcept
{
    return _state ? _state->stats : EmbeddedStatistics{};
}
} // namespace DxUi
