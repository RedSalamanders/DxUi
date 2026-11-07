#pragma once

#include <DxUi/DxUi.h>
#include <imm.h>
#include <inputscope.h>
#include <memory>
#include <textstor.h>
#include <wil/com.h>
#include <wil/resource.h>

namespace DxUi
{
// Existing private factory, not a packaged consumer API. The fixture avoids the
// implementation headers so it can be linked against the original SDK archive.
[[nodiscard]] ITextStoreACP* CreateNativeTextInputTextStore(ControlHost&, Control&, bool) noexcept;
} // namespace DxUi

namespace MaskedInputPolicyTests
{
struct Result
{
    HRESULT status        = E_PENDING;
    DWORD flags           = 0u;
    HRESULT inputScope    = E_PENDING;
    InputScope scope      = IS_DEFAULT;
    UINT scopeCount       = 0u;
    bool writeDenied      = false;
    bool sourcePreserved  = false;
    bool historyPreserved = false;
};

inline Result ObserveMaskedMetadata()
{
    DxUi::ControlHost host;
    auto field         = std::make_unique<DxUi::TextField>(L"Synthetic password");
    auto* const target = field.get();
    field->SetMasked(true);
    host.SetRoot(std::move(field));
    host.SetFocusControl(target, false);
    const bool typed     = target->OnChar(host, L'X', 0u);
    const auto caret     = target->GetCaretIndex();
    const auto selection = target->GetSelectionRange();
    Result result;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(DxUi::CreateNativeTextInputTextStore(host, *target, false));
    if (! store)
        return result;
    TS_STATUS status{};
    result.status = store->GetStatus(&status);
    result.flags  = status.dwDynamicFlags;
    wil::com_ptr_nothrow<ITfInputScope> scope;
    result.inputScope = store.query_to(scope.put());
    if (SUCCEEDED(result.inputScope))
    {
        wil::unique_cotaskmem_ptr<InputScope> values;
        result.inputScope = scope->GetInputScopes(wil::out_param(values), &result.scopeCount);
        if (SUCCEEDED(result.inputScope) && values && result.scopeCount == 1u)
            result.scope = values.get()[0];
    }
    HRESULT session         = E_PENDING;
    result.writeDenied      = store->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &session) == S_OK && session == E_ACCESSDENIED;
    result.sourcePreserved  = typed && target->IsMasked() && target->GetText() == L"Synthetic passwordX" && target->GetCaretIndex() == caret &&
                              target->GetSelectionRange() == selection;
    result.historyPreserved = target->OnKeyDown(host, 'Z', MK_CONTROL) && target->GetText() == L"Synthetic password" &&
                              target->OnKeyDown(host, 'Y', MK_CONTROL) && target->GetText() == L"Synthetic passwordX";
    return result;
}

struct ImeResult
{
    bool ready                = false;
    bool suppressed           = false;
    bool unmaskRestored       = false;
    bool blurRestored         = false;
    bool replacementPreserved = false;
    bool detachRestored       = false;
    bool siblingPreserved     = false;
    bool focusPreserved       = false;
};

inline HIMC ReadImeContext(HWND hwnd)
{
    const auto context = ImmGetContext(hwnd);
    if (context)
        static_cast<void>(ImmReleaseContext(hwnd, context));
    return context;
}

inline ImeResult ObserveMaskedImeAssociation()
{
    using ImeContext = wil::unique_any<HIMC, decltype(&ImmDestroyContext), ImmDestroyContext>;
    ImeContext custom(ImmCreateContext());
    ImeContext replacement(ImmCreateContext());
    ImeContext siblingContext(ImmCreateContext());
    const HWND foreground = GetForegroundWindow();
    const HWND focus      = GetFocus();
    wil::unique_hwnd window(CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                            L"STATIC",
                                            L"Synthetic masked input",
                                            WS_POPUP,
                                            -32000,
                                            -32000,
                                            160,
                                            80,
                                            nullptr,
                                            nullptr,
                                            GetModuleHandleW(nullptr),
                                            nullptr));
    wil::unique_hwnd sibling(CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                             L"STATIC",
                                             L"Synthetic plain input",
                                             WS_POPUP,
                                             -32000,
                                             -32000,
                                             160,
                                             80,
                                             nullptr,
                                             nullptr,
                                             GetModuleHandleW(nullptr),
                                             nullptr));
    ImeResult result;
    if (! custom || ! replacement || ! siblingContext || ! window || ! sibling)
        return result;
    const HIMC original        = ImmAssociateContext(window.get(), custom.get());
    const HIMC siblingOriginal = ImmAssociateContext(sibling.get(), siblingContext.get());
    const auto restore         = wil::scope_exit([&]() noexcept
    {
        static_cast<void>(ImmAssociateContext(window.get(), original));
        static_cast<void>(ImmAssociateContext(sibling.get(), siblingOriginal));
    });
    DxUi::ControlHost host;
    DxUi::ControlHost::AttachOptions options;
    options.forceWarp = true;
    if (! host.Attach(window.get(), options))
        return result;
    static_cast<void>(ShowWindow(window.get(), SW_SHOWNOACTIVATE));
    auto root          = std::make_unique<DxUi::Panel>();
    auto* const masked = root->AddChild<DxUi::TextField>(L"Synthetic secret");
    auto* const plain  = root->AddChild<DxUi::TextField>(L"Synthetic plain");
    masked->SetMasked(true);
    host.SetRoot(std::move(root));
    result.ready = ReadImeContext(window.get()) == custom.get();
    host.SetFocusControl(masked, false);
    result.suppressed = ReadImeContext(window.get()) == nullptr;
    masked->SetMasked(false);
    host.SyncTextInput(masked);
    result.unmaskRestored = ReadImeContext(window.get()) == custom.get();
    masked->SetMasked(true);
    host.SyncTextInput(masked);
    host.SetFocusControl(plain, false);
    result.blurRestored = ReadImeContext(window.get()) == custom.get();
    host.SetFocusControl(masked, false);
    static_cast<void>(ImmAssociateContext(window.get(), replacement.get()));
    host.SetFocusControl(plain, false);
    result.replacementPreserved = ReadImeContext(window.get()) == replacement.get();
    host.SetFocusControl(masked, false);
    const bool replacementSuppressed = ReadImeContext(window.get()) == nullptr;
    host.Detach();
    result.detachRestored   = replacementSuppressed && ReadImeContext(window.get()) == replacement.get();
    result.siblingPreserved = ReadImeContext(sibling.get()) == siblingContext.get();
    result.focusPreserved   = GetForegroundWindow() == foreground && GetFocus() == focus;
    return result;
}
} // namespace MaskedInputPolicyTests
