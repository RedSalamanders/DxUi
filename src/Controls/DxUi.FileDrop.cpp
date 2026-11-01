#include <DxUi/FileDrop.h>
#include <atomic>
#include <cstdint>
#include <exception>
#include <new>
#include <shellapi.h>
#include <utility>
#include <wil/resource.h>
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)

namespace DxUi
{
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
    bool offered{}, dropping{};
};

// This COM identity is intentionally neither copyable nor movable.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class RichFileDropTarget final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDropTarget>
{
public:
    explicit RichFileDropTarget(std::shared_ptr<RichFileDropAdapter::State> state) noexcept : _state(std::move(state))
    {
    }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) noexcept override
    try
    {
        if (! effect)
            return E_POINTER;
        const DWORD allowed = *effect;
        *effect             = DROPEFFECT_NONE;
        if (! OnThread())
            return RPC_E_WRONG_THREAD;
        _state->offered = false;
        if (! _state->attached || _state->dropping || ! data)
            return S_OK;
        wil::com_ptr<IDropTarget> keepAlive = this;
        const auto generation               = _state->generation;
        FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        const HRESULT offered = data->QueryGetData(&format);
        if (! _state->attached || _state->generation != generation || offered != S_OK)
            return S_OK;
        _state->offered = true;
        return Preview(keys, point, allowed, effect);
    }
    catch (const std::bad_alloc&)
    {
        if (effect)
            *effect = DROPEFFECT_NONE;
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        if (effect)
            *effect = DROPEFFECT_NONE;
        return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD keys, POINTL point, DWORD* effect) noexcept override
    {
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
        _state->offered = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) noexcept override
    {
        if (! effect)
            return E_POINTER;
        const DWORD allowed = *effect;
        *effect             = DROPEFFECT_NONE;
        if (! OnThread())
            return RPC_E_WRONG_THREAD;
        if (! _state->attached || _state->dropping)
            return S_OK;
        const bool offered = std::exchange(_state->offered, false);
        if (! offered || ! data || ! Accepts(keys, allowed))
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
        if (receiver->Drop(point, *data, guard))
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
        if (! _state->attached || ! _state->offered || _state->dropping || ! Accepts(keys, allowed))
            return S_OK;
        const auto generation = _state->generation;
        auto receiver         = _state->receiver.lock();
        if (receiver && receiver->Preview(point) && _state->attached && _state->generation == generation)
            *effect = DROPEFFECT_COPY;
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
    return E_OUTOFMEMORY;
}
void RichFileDropAdapter::Detach() noexcept
{
    if (_state && _state->thread != GetCurrentThreadId())
        return;
    if (_state)
    {
        _state->attached = false;
        _state->offered  = false;
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
