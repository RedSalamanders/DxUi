#include <DxUi/Failure.h>

#include <DxUi/FileDrop.h>
#include <DxUi/RichDocument.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <shellapi.h>
#include <shlobj_core.h>
#include <utility>
#include <wil/resource.h>
#pragma comment(lib, "shell32.lib")
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)

namespace DxUi
{
// OLE keeps the source objects alive independently of the initiating control.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class RichTextDragData final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDataObject>
{
public:
    explicit RichTextDragData(std::wstring_view text) : text_(text)
    {
    }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) noexcept override
    {
        return format && format->cfFormat == CF_UNICODETEXT && format->dwAspect == DVASPECT_CONTENT && format->lindex == -1 && (format->tymed & TYMED_HGLOBAL)
                   ? S_OK
                   : DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) noexcept override
    {
        if (! medium)
            return E_POINTER;
        *medium = {};
        if (QueryGetData(format) != S_OK)
            return DV_E_FORMATETC;
        wil::unique_hglobal memory(GlobalAlloc(GMEM_MOVEABLE, (text_.size() + 1u) * sizeof(wchar_t)));
        if (! memory)
            return E_OUTOFMEMORY;
        void* destination = GlobalLock(memory.get());
        if (! destination)
            return E_OUTOFMEMORY;
        {
            const auto unlock = wil::scope_exit([&] noexcept { static_cast<void>(GlobalUnlock(memory.get())); });
            std::memcpy(destination, text_.c_str(), (text_.size() + 1u) * sizeof(wchar_t));
        }
        medium->tymed   = TYMED_HGLOBAL;
        medium->hGlobal = memory.release();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) noexcept override
    {
        return DATA_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* output) noexcept override
    {
        if (! output)
            return E_POINTER;
        output->ptd = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** output) noexcept override
    {
        if (! output)
            return E_POINTER;
        *output = nullptr;
        if (direction != DATADIR_GET)
            return E_NOTIMPL;
        FORMATETC format{CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        return SHCreateStdEnumFmtEtc(1u, &format, output);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) noexcept override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) noexcept override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) noexcept override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    std::wstring text_;
};
class RichTextDragSource final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDropSource>
{
public:
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) noexcept override
    {
        return escape ? DRAGDROP_S_CANCEL : (keys & MK_LBUTTON) ? S_OK : DRAGDROP_S_DROP;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) noexcept override
    {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
};
#pragma warning(pop)

HRESULT CreateRichTextDragData(std::wstring_view text, IDataObject** result) noexcept
try
{
    const Failure::Scope failureScope;
    if (! result)
        return E_POINTER;
    *result = nullptr;
    if (text.empty() || text.size() > RichText::Document::MaximumUnits || text.find(L'\0') != std::wstring_view::npos || ! RichText::IsValidSourceUtf16(text))
        return E_INVALIDARG;
    auto data = Microsoft::WRL::Make<RichTextDragData>(text);
    if (! data)
        return E_OUTOFMEMORY;
    *result = data.Detach();
    return S_OK;
}
catch (const std::bad_alloc&)
{
    Failure::FailAllocation();
}

HRESULT CreateRichTextDragSource(IDropSource** result) noexcept
{
    if (! result)
        return E_POINTER;
    *result     = nullptr;
    auto source = Microsoft::WRL::Make<RichTextDragSource>();
    if (! source)
        return E_OUTOFMEMORY;
    *result = source.Detach();
    return S_OK;
}

struct RichFileDropAdapter::State
{
    State()                        = default;
    State(const State&)            = delete;
    State& operator=(const State&) = delete;
    State(State&&)                 = delete;
    State& operator=(State&&)      = delete;
    std::weak_ptr<RichFileDropReceiver> receiver;
    DWORD thread{};
    std::uint64_t generation{};
    std::atomic<bool> attached = false;
    bool offered{}, offeredText{}, dropping{};
};

// This COM identity is intentionally neither copyable nor movable.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class RichFileDropTarget final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDropTarget>
{
public:
    explicit RichFileDropTarget(std::shared_ptr<RichFileDropAdapter::State> state) noexcept : _state(std::move(state))
    {
        const Failure::Scope outerFailureScope;
    }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) noexcept override
    try
    {
        const Failure::Scope failureScope;
        if (! effect)
            return E_POINTER;
        const DWORD allowed = *effect;
        *effect             = DROPEFFECT_NONE;
        if (! OnThread())
            return RPC_E_WRONG_THREAD;
        _state->offered     = false;
        _state->offeredText = false;
        if (! _state->attached || _state->dropping || ! data)
            return S_OK;
        wil::com_ptr<IDropTarget> keepAlive = this;
        const auto generation               = _state->generation;
        FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        const HRESULT offered = data->QueryGetData(&format);
        if (! _state->attached || _state->generation != generation)
            return S_OK;
        if (offered == S_OK)
            _state->offered = true;
        else
        {
            FORMATETC textFormat{CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            const HRESULT offeredText = data->QueryGetData(&textFormat);
            if (! _state->attached || _state->generation != generation || offeredText != S_OK)
                return S_OK;
            _state->offeredText = true;
        }
        return Preview(keys, point, allowed, effect);
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    catch (const std::exception&)
    {
        Failure::FailUnexpected();
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD keys, POINTL point, DWORD* effect) noexcept override
    {
        const Failure::Scope failureScope;
        if (! effect)
            return E_POINTER;
        const DWORD allowed = *effect;
        *effect             = DROPEFFECT_NONE;
        if (! OnThread())
            return RPC_E_WRONG_THREAD;
        wil::com_ptr<IDropTarget> keepAlive = this;
        return Preview(keys, point, allowed, effect);
    }
    HRESULT STDMETHODCALLTYPE DragLeave() noexcept override
    {
        if (! OnThread())
            return RPC_E_WRONG_THREAD;
        _state->offered     = false;
        _state->offeredText = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) noexcept override
    {
        const Failure::Scope failureScope;
        if (! effect)
            return E_POINTER;
        const DWORD allowed = *effect;
        *effect             = DROPEFFECT_NONE;
        if (! OnThread())
            return RPC_E_WRONG_THREAD;
        if (! _state->attached || _state->dropping)
            return S_OK;
        const bool offered      = std::exchange(_state->offered, false);
        const bool offeredText  = std::exchange(_state->offeredText, false);
        const bool textAccepted = offeredText && (allowed & DROPEFFECT_MOVE) && ! (keys & (MK_CONTROL | MK_RBUTTON)) && ! (GetKeyState(VK_MENU) & 0x8000);
        if ((! offered || ! Accepts(keys, allowed)) && ! textAccepted)
            return S_OK;
        if (! data)
            return S_OK;
        wil::com_ptr<IDropTarget> keepAlive = this;
        auto receiver                       = _state->receiver.lock();
        if (! receiver)
            return S_OK;
        _state->dropping  = true;
        const auto finish = wil::scope_exit([state = _state] noexcept { state->dropping = false; });
        struct GuardState
        {
            RichFileDropAdapter::State* state;
            std::uint64_t generation;
        } guardState{_state.get(), _state->generation};
        const RichFileDropGuard guard{[](void* context) noexcept
        {
            const auto& value = *static_cast<GuardState*>(context);
            return value.state->attached && value.state->generation == value.generation;
        },
                                      &guardState};
        // Receiver owns the prepared target snapshot and revalidates it after
        // the data object has been queried. No raw control is retained here.
        if (textAccepted && receiver->DropText(point, *data, guard))
            *effect = DROPEFFECT_MOVE;
        else if (offered && Accepts(keys, allowed) && receiver->Drop(point, *data, guard))
            *effect = DROPEFFECT_COPY;
        return S_OK;
    }

private:
    [[nodiscard]] bool OnThread() const noexcept
    {
        return _state->thread == GetCurrentThreadId();
    }
    [[nodiscard]] static bool Accepts(DWORD keys, DWORD allowed) noexcept
    {
        return (allowed & DROPEFFECT_COPY) && ! (keys & (MK_SHIFT | MK_RBUTTON)) && ! (GetKeyState(VK_MENU) & 0x8000);
    }
    HRESULT Preview(DWORD keys, POINTL point, DWORD allowed, DWORD* effect) noexcept
    {
        const bool textAccepted =
            _state->offeredText && (allowed & DROPEFFECT_MOVE) && ! (keys & (MK_CONTROL | MK_RBUTTON)) && ! (GetKeyState(VK_MENU) & 0x8000);
        if (! _state->attached || _state->dropping || ((! _state->offered || ! Accepts(keys, allowed)) && ! textAccepted))
            return S_OK;
        const auto generation = _state->generation;
        auto receiver         = _state->receiver.lock();
        if (receiver && _state->attached && _state->generation == generation)
        {
            const bool accepted = textAccepted ? receiver->PreviewText(point) : receiver->Preview(point);
            if (accepted && _state->attached && _state->generation == generation)
                *effect = textAccepted ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
        }
        return S_OK;
    }
    std::shared_ptr<RichFileDropAdapter::State> _state;
};
#pragma warning(pop)

RichFileDropAdapter::RichFileDropAdapter() = default;
RichFileDropAdapter::~RichFileDropAdapter()
{
    // Destruction on a foreign thread cannot touch the receiver's UI state.
    // Retained COM references nevertheless become inert before member release.
    if (_state && _state->thread != GetCurrentThreadId())
        _state->attached.store(false, std::memory_order_release);
    else
        Detach();
}
HRESULT RichFileDropAdapter::Attach(std::weak_ptr<RichFileDropReceiver> receiver) noexcept
try
{
    const Failure::Scope outerFailureScope;
    if (receiver.expired())
        return E_INVALIDARG;
    if (_state && _state->thread != GetCurrentThreadId())
        return RPC_E_WRONG_THREAD;
    APTTYPE apartment{};
    APTTYPEQUALIFIER qualifier{};
    const HRESULT apartmentResult = CoGetApartmentType(&apartment, &qualifier);
    if (FAILED(apartmentResult))
        return apartmentResult;
    if (apartment != APTTYPE_STA && apartment != APTTYPE_MAINSTA)
        return RPC_E_WRONG_THREAD;
    Detach();
    auto state      = std::make_shared<State>();
    state->thread   = GetCurrentThreadId();
    state->receiver = std::move(receiver);
    state->attached = true;
    auto target     = Microsoft::WRL::Make<RichFileDropTarget>(state);
    if (! target.Get())
        return E_OUTOFMEMORY;
    _target = target.Get();
    _state  = std::move(state);
    return S_OK;
}
catch (const std::bad_alloc&)
{
    Failure::FailAllocation();
}
void RichFileDropAdapter::Detach() noexcept
{
    if (_state && _state->thread != GetCurrentThreadId())
        return;
    if (_state)
    {
        _state->attached    = false;
        _state->offered     = false;
        _state->offeredText = false;
        ++_state->generation;
        _state->receiver.reset();
    }
    _target.reset();
    _state.reset();
}
IDropTarget* RichFileDropAdapter::Target() const noexcept
{
    return _target.get();
}
} // namespace DxUi
