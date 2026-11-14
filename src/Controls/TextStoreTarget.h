#pragma once

#include "DxUi.Internal.h"
#include <DxUi/DxUi.h>
#include <DxUi/Failure.h>
#include <DxUi/TextInputServices.h>
#include <cstdint>
#include <inputscope.h>
#include <limits>
#include <memory>
#include <msctf.h>
#include <optional>
#include <span>
#include <string_view>
#include <textstor.h>
#include <vector>

namespace DxUi
{
// One GUID_PROP_ATTRIBUTE range of a TSF composition, resolved to its TF_DISPLAYATTRIBUTE::bAttr.
struct TextStoreAttributeSpan
{
    size_t start              = 0u;
    size_t end                = 0u;
    TF_DA_ATTR_INFO attribute = TF_ATTR_OTHER;
};
// The conversion target and clause boundaries of one composition, in document ACPs. Boundaries follow the IMM
// GCS_COMPCLAUSE convention: the composition start, every interior clause edge and the composition end.
struct TextStoreCompositionAttributes
{
    std::optional<size_t> targetStart;
    std::optional<size_t> targetEnd;
    std::vector<size_t> clauseBoundaries;
};
// GUID_PROP_INPUTSCOPE. InputScope.h declares it with DEFINE_GUID and no import library defines it.
inline constexpr GUID kTextStoreInputScopeProperty = {0x1713dd5a, 0x68e7, 0x4a5b, {0x9a, 0xf6, 0x59, 0x2a, 0x59, 0x5c, 0x77, 0x8d}};
// The most clause boundaries one composition publishes, matching the embedded snapshot ceiling.
inline constexpr size_t kMaximumTextStoreClauseBoundaries = 256u;
// The TS_TEXTCHANGE for replacing `before` with `after`: the common prefix and suffix are excluded, and neither cut
// splits a surrogate pair or a CRLF pair in either text.
[[nodiscard]] TS_TEXTCHANGE ComputeTextStoreTextChange(std::wstring_view before, std::wstring_view after) noexcept;
// Pure mapping from display-attribute spans to the conversion target (the first contiguous run of target spans) and
// clause boundaries. Spans are clipped to the composition; empty, reversed or out-of-order spans are ignored. `result`
// keeps its capacity so a composition reuses it; growth beyond that capacity may throw std::bad_alloc.
void ResolveTextStoreCompositionAttributes(size_t compositionStart,
                                           size_t compositionEnd,
                                           std::span<const TextStoreAttributeSpan> spans,
                                           TextStoreCompositionAttributes& result);
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
    struct Policy
    {
        bool readOnly  = false;
        bool masked    = false;
        bool multiline = false;
        // Reported through ITfInputScope and GUID_PROP_INPUTSCOPE; a masked target reports IS_PASSWORD instead.
        InputScope inputScope = IS_DEFAULT;
    };
    virtual ~TextStoreTarget()                                                 = default;
    virtual void Disconnect() noexcept                                         = 0;
    [[nodiscard]] virtual bool ReadState(TextInputState& state) const noexcept = 0;
    // Metadata does not grant text access. Native password controls override
    // this without exporting their secret; ordinary targets keep the old read path.
    [[nodiscard]] virtual bool ReadPolicy(Policy& policy) const noexcept
    {
        TextInputState state;
        if (! ReadState(state))
            return false;
        policy = {state.readOnly, state.masked, state.multiline};
        return true;
    }
    [[nodiscard]] virtual bool ApplyState(const TextInputState& state, bool notifyChange) noexcept = 0;
    // The store reads and stages through these. A target that keeps shared text overrides them so a read shares the
    // document and only an edit allocates; the defaults adapt ReadState/ApplyState with one copy each.
    [[nodiscard]] virtual bool ReadSharedState(SharedTextInputState& state) const noexcept
    {
        try
        {
            TextInputState copy;
            if (! ReadState(copy))
            {
                state = {};
                return false;
            }
            state = {MakeSharedText(std::move(copy.text)),
                     copy.selectionAnchorIndex,
                     copy.caretIndex,
                     copy.firstVisibleLine,
                     copy.readOnly,
                     copy.masked,
                     copy.multiline};
            return true;
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }
    [[nodiscard]] virtual bool ApplySharedState(const SharedTextInputState& state, bool notifyChange) noexcept
    {
        try
        {
            TextInputState copy{std::wstring(state.View()),
                                state.selectionAnchorIndex,
                                state.caretIndex,
                                state.firstVisibleLine,
                                state.readOnly,
                                state.masked,
                                state.multiline};
            return ApplyState(copy, notifyChange);
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }
    [[nodiscard]] virtual D2D1_RECT_F ResolveTextViewportBounds() const noexcept                       = 0;
    [[nodiscard]] virtual HWND GetHwnd() const noexcept                                                = 0;
    virtual HRESULT GetAcpFromScreenPoint(const POINT* point, LONG* acp) const noexcept                = 0;
    virtual HRESULT GetRangeScreenRect(LONG start, LONG end, RECT* rect, BOOL* clipped) const noexcept = 0;
    virtual HRESULT GetScreenRect(RECT* rect) const noexcept                                           = 0;
    // Native controls enforce their own document limits; application snapshots may impose a smaller ceiling.
    [[nodiscard]] virtual size_t GetMaximumDocumentTextUnits() const noexcept
    {
        return (std::numeric_limits<size_t>::max)();
    }
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
    // Publishes the composition's conversion target and clauses read from GUID_PROP_ATTRIBUTE. Targets without
    // composition metadata ignore it.
    virtual bool SetCompositionAttributes(const TextStoreCompositionAttributes&) noexcept
    {
        return false;
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
// Advises the store as the context's ITfTextEditSink so it can read GUID_PROP_ATTRIBUTE after each edit session.
// Failure leaves composition attributes unpublished and is not an activation failure.
HRESULT AdviseTextStoreEditSink(ITfContext* context, ITextStoreACP* store) noexcept;
void UnadviseTextStoreEditSink(ITfContext* context, IUnknown* store) noexcept;
// Mark an activated production store for a bounded posted reconciliation after TSF callbacks finish.
void ScheduleTextStoreReconciliation(ITextStoreACP* store) noexcept;
#if DXUI_ENABLE_DIAGNOSTICS
void DebugGetTextStoreNotificationCountsForTest(ITextStoreACP* store, uint64_t& textChanges, uint64_t& selectionChanges, uint64_t& layoutChanges) noexcept;
#endif
} // namespace DxUi
