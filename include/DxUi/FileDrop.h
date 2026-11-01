#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <memory>
#include <objidl.h>
#include <oleidl.h>
#include <wil/com.h>
#include <windows.h>

namespace DxUi
{
struct RichFileDropGuard
{
    // Borrowed only for the current Drop callback; check immediately before
    // mutation after every external data-object call.
    bool (*current)(void*) noexcept = nullptr;
    void* context                   = nullptr;
};
// Application-owned router. Callbacks run only on the constructing UI thread.
// The receiver owns no window or host on the adapter's behalf. Drop must capture
// and revalidate its target around IDataObject calls, and return true only after
// one source transaction has committed.
class RichFileDropReceiver
{
public:
    virtual ~RichFileDropReceiver()                                                                  = default;
    virtual bool Preview(POINTL screenPoint) noexcept                                                = 0;
    virtual bool Drop(POINTL screenPoint, IDataObject& dataObject, RichFileDropGuard guard) noexcept = 0;
};

// Explicitly owned by the receiving application. Target() can be registered
// with OLE or forwarded from an existing application IDropTarget router. Detach
// makes retained COM references inert; revoke a registered target before the
// window or the adapter is destroyed. No receiver or HWND is owned here.
class RichFileDropAdapter final
{
public:
    RichFileDropAdapter();
    ~RichFileDropAdapter();
    RichFileDropAdapter(const RichFileDropAdapter&)            = delete;
    RichFileDropAdapter& operator=(const RichFileDropAdapter&) = delete;
    HRESULT Attach(std::weak_ptr<RichFileDropReceiver> receiver) noexcept;
    void Detach() noexcept;
    [[nodiscard]] IDropTarget* Target() const noexcept;

private:
    friend class RichFileDropTarget;
    struct State;
    std::shared_ptr<State> _state;
    wil::com_ptr<IDropTarget> _target;
};
} // namespace DxUi
