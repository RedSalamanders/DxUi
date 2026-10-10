#pragma once

#include <wil/com.h>

#include <msctf.h>

namespace DxUi::Internal
{
[[nodiscard]] inline bool HasSameComIdentity(IUnknown* left, IUnknown* right) noexcept
{
    if (! left || ! right)
        return false;
    wil::com_ptr_nothrow<IUnknown> leftIdentity;
    wil::com_ptr_nothrow<IUnknown> rightIdentity;
    return SUCCEEDED(left->QueryInterface(IID_PPV_ARGS(leftIdentity.put()))) && leftIdentity &&
           SUCCEEDED(right->QueryInterface(IID_PPV_ARGS(rightIdentity.put()))) && rightIdentity && leftIdentity.get() == rightIdentity.get();
}

inline void ClearThreadManagerFocusIfOwnedBy(ITfThreadMgr* threadMgr, ITfDocumentMgr* documentMgr) noexcept
{
    if (! threadMgr || ! documentMgr)
        return;
    wil::com_ptr_nothrow<ITfDocumentMgr> focusedDocument;
    if (SUCCEEDED(threadMgr->GetFocus(focusedDocument.put())) && focusedDocument && HasSameComIdentity(focusedDocument.get(), documentMgr))
        static_cast<void>(threadMgr->SetFocus(nullptr));
}

template <typename IsCurrent>
void RestoreThreadManagerFocusAssociation(
    ITfThreadMgr* threadMgr, HWND hwnd, ITfDocumentMgr* retiringDocument, ITfDocumentMgr* previousAssociation, IsCurrent&& isCurrent) noexcept
{
    if (! threadMgr || ! hwnd || ! isCurrent())
        return;
    wil::com_ptr_nothrow<ITfDocumentMgr> focusBefore;
    const HRESULT focusResult = threadMgr->GetFocus(focusBefore.put());
    if (! isCurrent())
        return;
    const bool preserveExternalFocus = SUCCEEDED(focusResult) && focusBefore && ! HasSameComIdentity(focusBefore.get(), retiringDocument);
    wil::com_ptr_nothrow<ITfDocumentMgr> replacedAssociation;
    if (FAILED(threadMgr->AssociateFocus(hwnd, previousAssociation, replacedAssociation.put())) || ! preserveExternalFocus || ! isCurrent())
        return;
    // Updating the association of the focused HWND can synchronously select that association.
    // Restore a different document only if no newer host transition or different document won in its callback.
    wil::com_ptr_nothrow<ITfDocumentMgr> focusAfter;
    if (FAILED(threadMgr->GetFocus(focusAfter.put())) || ! isCurrent())
        return;
    if (previousAssociation ? HasSameComIdentity(focusAfter.get(), previousAssociation) : ! focusAfter)
        static_cast<void>(threadMgr->SetFocus(focusBefore.get()));
}
} // namespace DxUi::Internal
