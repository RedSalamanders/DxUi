#include "../../src/Controls/TsfFocus.h"

#include <atomic>
#include <cstdio>

namespace
{
// Stack-owned COM doubles: Release balances the helper's temporary references but never
// deletes the fixture. No COM activation, HWND operation or desktop interaction occurs.
class FocusDocument final : public ITfDocumentMgr
{
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITfDocumentMgr))
            return E_NOINTERFACE;
        *result = static_cast<ITfDocumentMgr*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE CreateContext(TfClientId, DWORD, IUnknown*, ITfContext**, TfEditCookie*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Push(ITfContext*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Pop(DWORD) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetTop(ITfContext**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetBase(ITfContext**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumContexts(IEnumTfContexts**) noexcept override
    {
        return E_NOTIMPL;
    }

    std::atomic<ULONG> references{1u};
};

class FocusManager final : public ITfThreadMgr
{
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITfThreadMgr))
            return E_NOINTERFACE;
        *result = static_cast<ITfThreadMgr*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE Activate(TfClientId*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Deactivate() noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CreateDocumentMgr(ITfDocumentMgr**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumDocumentMgrs(IEnumTfDocumentMgrs**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE IsThreadFocus(BOOL*) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetFunctionProvider(REFCLSID, ITfFunctionProvider**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFunctionProviders(IEnumTfFunctionProviders**) noexcept override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetGlobalCompartment(ITfCompartmentMgr**) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetFocus(ITfDocumentMgr** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (++focusReads == failFocusRead)
            return E_FAIL;
        return focus ? focus.copy_to(result) : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE SetFocus(ITfDocumentMgr* document) noexcept override
    {
        ++focusWrites;
        focus = document;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE AssociateFocus(HWND, ITfDocumentMgr* document, ITfDocumentMgr** previous) noexcept override
    {
        ++associationWrites;
        if (! previous)
            return E_POINTER;
        *previous = nullptr;
        if (failAssociation)
            return E_FAIL;
        const HRESULT result = association.copy_to(previous);
        if (FAILED(result))
            return result;
        association = document;
        focus       = postAssociationFocus;
        if (invalidateCurrent && current)
            *current = false;
        return S_OK;
    }

    wil::com_ptr_nothrow<ITfDocumentMgr> focus;
    wil::com_ptr_nothrow<ITfDocumentMgr> association;
    wil::com_ptr_nothrow<ITfDocumentMgr> postAssociationFocus;
    unsigned focusReads        = 0u;
    unsigned focusWrites       = 0u;
    unsigned associationWrites = 0u;
    unsigned failFocusRead     = 0u;
    bool failAssociation       = false;
    bool invalidateCurrent     = false;
    bool* current              = nullptr;
    std::atomic<ULONG> references{1u};
};

struct FocusFixture final
{
    FocusDocument retiring;
    FocusDocument foreign;
    FocusDocument previous;
    FocusDocument third;
    bool current = true;
    FocusManager manager;

    FocusFixture()
    {
        manager.focus       = &foreign;
        manager.association = &retiring;
        manager.current     = &current;
    }
    void Restore(ITfDocumentMgr* previousAssociation)
    {
        // This non-null sentinel is passed only to the COM double above; no Windows API sees it.
        const HWND sentinel = reinterpret_cast<HWND>(static_cast<uintptr_t>(1u));
        DxUi::Internal::RestoreThreadManagerFocusAssociation(&manager, sentinel, &retiring, previousAssociation, [this]() noexcept { return current; });
    }
};
} // namespace

bool TestTsfFocusAssociationRestorationControls()
{
    unsigned cases   = 0u;
    unsigned failed  = 0u;
    const auto check = [&](bool passed, const char* description)
    {
        ++cases;
        if (! passed)
            ++failed;
        std::printf("%s TSF association control: %s\n", passed ? "PASS" : "FAIL", description);
    };
    {
        FocusFixture fixture;
        fixture.manager.postAssociationFocus = nullptr;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.focus.get() == &fixture.foreign && fixture.manager.focusWrites == 1u,
              "nonnull previous association with cleared post-focus preserves the independently focused document");
    }
    {
        FocusFixture fixture;
        fixture.manager.postAssociationFocus = &fixture.previous;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.focus.get() == &fixture.foreign && fixture.manager.focusWrites == 1u,
              "selecting the restored previous association preserves independent focus");
    }
    {
        FocusFixture fixture;
        fixture.manager.postAssociationFocus = nullptr;
        fixture.Restore(nullptr);
        check(fixture.manager.focus.get() == &fixture.foreign && fixture.manager.focusWrites == 1u,
              "null association with cleared post-focus preserves independent focus");
    }
    {
        FocusFixture fixture;
        fixture.manager.postAssociationFocus = &fixture.third;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.focus.get() == &fixture.third && fixture.manager.focusWrites == 0u,
              "a different nonnull document selected during association is never overwritten");
    }
    {
        FocusFixture fixture;
        fixture.manager.postAssociationFocus = nullptr;
        fixture.manager.invalidateCurrent    = true;
        fixture.Restore(&fixture.previous);
        check(! fixture.current && ! fixture.manager.focus && fixture.manager.focusWrites == 0u,
              "a newer host transition is never overwritten even when focus is null");
    }
    {
        FocusFixture fixture;
        fixture.manager.failFocusRead        = 1u;
        fixture.manager.postAssociationFocus = &fixture.previous;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.focus.get() == &fixture.previous && fixture.manager.focusWrites == 0u,
              "a failed before-focus read does not invent an external document to restore");
    }
    {
        FocusFixture fixture;
        fixture.manager.failFocusRead        = 2u;
        fixture.manager.postAssociationFocus = &fixture.previous;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.focus.get() == &fixture.previous && fixture.manager.focusWrites == 0u,
              "a failed after-focus read prevents an unverified overwrite");
    }
    {
        FocusFixture fixture;
        fixture.manager.failAssociation = true;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.focus.get() == &fixture.foreign && fixture.manager.focusWrites == 0u, "a failed association change performs no focus write");
    }
    {
        FocusFixture fixture;
        fixture.current = false;
        fixture.Restore(&fixture.previous);
        check(fixture.manager.associationWrites == 0u && fixture.manager.focusReads == 0u && fixture.manager.focusWrites == 0u,
              "an already superseded transition performs no COM operations");
    }
    {
        FocusFixture fixture;
        fixture.manager.focus                = &fixture.retiring;
        fixture.manager.postAssociationFocus = nullptr;
        fixture.Restore(&fixture.previous);
        check(! fixture.manager.focus && fixture.manager.focusWrites == 0u, "the retiring document is not resurrected as independent focus");
    }
    {
        FocusFixture fixture;
        fixture.manager.focus                = nullptr;
        fixture.manager.postAssociationFocus = nullptr;
        fixture.Restore(&fixture.previous);
        check(! fixture.manager.focus && fixture.manager.focusWrites == 0u, "an initially empty focus is not replaced with an association");
    }
    {
        FocusFixture fixture;
        bool everyCyclePreservedOwnership = true;
        for (unsigned cycle = 0u; cycle < 64u; ++cycle)
        {
            ITfDocumentMgr* const independent    = cycle % 2u == 0u ? &fixture.foreign : &fixture.third;
            ITfDocumentMgr* const previous       = cycle % 3u == 0u ? nullptr : &fixture.previous;
            fixture.manager.focus                = independent;
            fixture.manager.association          = &fixture.retiring;
            fixture.manager.postAssociationFocus = cycle % 4u == 0u ? previous : nullptr;
            const unsigned writesBefore          = fixture.manager.focusWrites;
            fixture.Restore(previous);
            everyCyclePreservedOwnership =
                everyCyclePreservedOwnership && fixture.manager.focus.get() == independent && fixture.manager.focusWrites == writesBefore + 1u;
            DxUi::Internal::ClearThreadManagerFocusIfOwnedBy(&fixture.manager, &fixture.retiring);
            everyCyclePreservedOwnership =
                everyCyclePreservedOwnership && fixture.manager.focus.get() == independent && fixture.manager.focusWrites == writesBefore + 1u;
            fixture.manager.focus = &fixture.retiring;
            DxUi::Internal::ClearThreadManagerFocusIfOwnedBy(&fixture.manager, &fixture.retiring);
            everyCyclePreservedOwnership = everyCyclePreservedOwnership && ! fixture.manager.focus && fixture.manager.focusWrites == writesBefore + 2u;
        }
        fixture.manager.focus.reset();
        fixture.manager.association.reset();
        fixture.manager.postAssociationFocus.reset();
        const bool referencesBalanced = fixture.retiring.references.load() == 1u && fixture.foreign.references.load() == 1u &&
                                        fixture.previous.references.load() == 1u && fixture.third.references.load() == 1u;
        check(everyCyclePreservedOwnership && referencesBalanced,
              "64 alternating replacement/retirement cycles preserve foreign focus, clear only owned focus and balance COM references");
    }
    std::printf("TSF association controls: %u of %u passed; controlled ordering, not hosted root-cause attribution.\n", cases - failed, cases);
    return failed == 0u;
}
