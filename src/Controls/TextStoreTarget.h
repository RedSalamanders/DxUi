#pragma once

#include <DxUi/DxUi.h>
#include <DxUi/TextInputServices.h>
#include <cstdint>
#include <memory>
#include <msctf.h>
#include <textstor.h>

namespace DxUi
{
// Private UI-thread adapter shared by native controls and application-side text services.
// The COM store retains the adapter; Disconnect invalidates borrowed application/control state.
struct TextStoreDispatch
{
    void* context                       = nullptr;
    bool (*requestLock)(void*) noexcept = nullptr;
    // Retains immutable per-store dispatch state when context is owned by the service.
    std::shared_ptr<void> owner;
    bool (*ownsClientSuccessor)(void*, const TextInputClient*, uint64_t) noexcept = nullptr;
};
class TextStoreTarget
{
public:
    virtual ~TextStoreTarget()                                                                         = default;
    virtual void Disconnect() noexcept                                                                 = 0;
    [[nodiscard]] virtual bool ReadState(TextInputState& state) const noexcept                         = 0;
    [[nodiscard]] virtual bool ApplyState(const TextInputState& state, bool notifyChange) noexcept     = 0;
    [[nodiscard]] virtual D2D1_RECT_F ResolveTextViewportBounds() const noexcept                       = 0;
    [[nodiscard]] virtual HWND GetHwnd() const noexcept                                                = 0;
    virtual HRESULT GetAcpFromScreenPoint(const POINT* point, LONG* acp) const noexcept                = 0;
    virtual HRESULT GetRangeScreenRect(LONG start, LONG end, RECT* rect, BOOL* clipped) const noexcept = 0;
    virtual HRESULT GetScreenRect(RECT* rect) const noexcept                                           = 0;
    virtual bool ScheduleLock() noexcept
    {
        return false;
    }
    virtual HRESULT BeginEdit(bool) noexcept
    {
        return S_OK;
    }
    virtual HRESULT EndEdit(bool) noexcept
    {
        return S_OK;
    }
    virtual HRESULT StartComposition(ITfCompositionView*, BOOL* accepted) noexcept
    {
        *accepted = TRUE;
        return S_OK;
    }
    virtual HRESULT UpdateComposition(ITfCompositionView*, ITfRange*) noexcept
    {
        return S_OK;
    }
    virtual HRESULT EndComposition(ITfCompositionView*) noexcept
    {
        return S_OK;
    }
};

[[nodiscard]] ITextStoreACP* CreateTextStore(std::shared_ptr<TextStoreTarget> target) noexcept;
[[nodiscard]] ITextStoreACP* CreateClientTextStore(HWND hwnd, std::shared_ptr<TextInputClient> client, TextStoreDispatch dispatch = {}) noexcept;
[[nodiscard]] ITextStoreACP* CreateClientTextStoreForFocus(HWND hwnd,
                                                           std::shared_ptr<TextInputClient> client,
                                                           uint64_t focusId,
                                                           TextStoreDispatch dispatch = {}) noexcept;
HRESULT DispatchPendingTextStoreLock(ITextStoreACP* store) noexcept;
// Consumes one posted deferred-lock notification. TS_E_NOLOCK means the owning lock is still unwinding; the store
// schedules the pending request once that lock is released rather than reposting from a nested message loop.
HRESULT DispatchDeferredTextStoreLock(ITextStoreACP* store, bool& messagePosted) noexcept;
void NotifyTextStoreChanged(ITextStoreACP* store) noexcept;
void NotifyTextStoreLayoutChanged(ITextStoreACP* store) noexcept;
// Mark an activated production store for a bounded posted reconciliation after TSF callbacks finish.
void ScheduleTextStoreReconciliation(ITextStoreACP* store) noexcept;
#if DXUI_ENABLE_DIAGNOSTICS
void DebugGetTextStoreNotificationCountsForTest(ITextStoreACP* store, uint64_t& textChanges, uint64_t& selectionChanges, uint64_t& layoutChanges) noexcept;
#endif
} // namespace DxUi
