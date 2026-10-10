#include "DxUi.Internal.h"
#include "TextStoreTarget.h"
#include "TsfFocus.h"

#include "../Support/Diagnostics.h"
#include "../Support/WindowMessages.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>

#include <imm.h>
#include <msctf.h>
#include <textstor.h>

#pragma comment(lib, "imm32.lib")

namespace DxUi
{
namespace
{
struct NativeTextInputThreadManagerState final
{
    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    TfClientId clientId = 0u;
    bool comInitialized = false;
};

[[nodiscard]] bool NativeTextInputControlBelongsToTree(const Control* root, const Control* target) noexcept
{
    if (! root || ! target)
    {
        return false;
    }
    if (root == target)
    {
        return true;
    }
    const auto* panel = dynamic_cast<const Panel*>(root);
    if (! panel)
    {
        return false;
    }
    for (const auto& child : panel->GetChildren())
    {
        if (child && NativeTextInputControlBelongsToTree(child.get(), target))
        {
            return true;
        }
    }
    return false;
}

using Internal::ClearThreadManagerFocusIfOwnedBy;
using Internal::RestoreThreadManagerFocusAssociation;

[[nodiscard]] NativeTextInputThreadManagerState& GetNativeTextInputThreadManagerState() noexcept
{
    thread_local NativeTextInputThreadManagerState state;
    return state;
}

void ShutdownNativeTextInputThreadManagerState(NativeTextInputThreadManagerState& state) noexcept
{
    if (state.threadMgr && state.clientId != 0u)
    {
        static_cast<void>(state.threadMgr->SetFocus(nullptr));
        static_cast<void>(state.threadMgr->Deactivate());
    }

    state.threadMgr.reset();
    state.clientId = 0u;
    if (state.comInitialized)
    {
        CoUninitialize();
        state.comInitialized = false;
    }
}

[[nodiscard]] bool EnsureNativeTextInputThreadManager(wil::com_ptr_nothrow<ITfThreadMgr>& outThreadMgr, TfClientId& outClientId) noexcept
{
    outThreadMgr.reset();
    outClientId = 0u;

    NativeTextInputThreadManagerState& state = GetNativeTextInputThreadManagerState();
    if (state.threadMgr && state.clientId != 0u)
    {
        outThreadMgr = state.threadMgr;
        outClientId  = state.clientId;
        return true;
    }
    if (state.threadMgr)
    {
        ShutdownNativeTextInputThreadManagerState(state);
    }

    HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(state.threadMgr.put()));
    if (hr == CO_E_NOTINITIALIZED)
    {
        const HRESULT coInitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(coInitHr))
        {
            return false;
        }

        state.comInitialized = true;
        hr                   = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(state.threadMgr.put()));
    }

    if (FAILED(hr) || ! state.threadMgr)
    {
        ShutdownNativeTextInputThreadManagerState(state);
        return false;
    }

    TfClientId clientId = 0u;
    hr                  = state.threadMgr->Activate(&clientId);
    if (FAILED(hr) || clientId == 0u)
    {
        state.clientId = clientId;
        ShutdownNativeTextInputThreadManagerState(state);
        return false;
    }

    state.clientId = clientId;
    outThreadMgr   = state.threadMgr;
    outClientId    = state.clientId;
    return true;
}

[[nodiscard]] NativeTextInputState ToNativeTextInputState(const Control& control, const TextInputState& controlState)
{
    NativeTextInputState state;
    state.text                 = controlState.text;
    state.selectionAnchorIndex = controlState.selectionAnchorIndex;
    state.caretIndex           = controlState.caretIndex;
    state.firstVisibleLine     = controlState.firstVisibleLine;
    state.readOnly             = controlState.readOnly;
    state.masked               = controlState.masked;
    state.multiline            = controlState.multiline;
    state.flowDirection        = control.GetFlowDirection();
    state.readingDirection     = ResolveReadingDirection(state.flowDirection);
    if (const auto* textField = dynamic_cast<const TextField*>(&control))
    {
        state.passwordRevealMode    = textField->GetPasswordRevealMode();
        state.passwordRevealState   = textField->GetPasswordRevealState();
        state.maskLengthPolicy      = textField->GetPasswordMaskLengthPolicy();
        state.secretVisibleDotCount = textField->GetSecretVisibleDotCount();
    }
    return state;
}

[[nodiscard]] RECT NormalizeCaretRectPx(const RECT& rect) noexcept
{
    RECT normalized = rect;
    if (normalized.right <= normalized.left)
    {
        normalized.right = normalized.left + 1;
    }
    if (normalized.bottom <= normalized.top)
    {
        normalized.bottom = normalized.top + std::max<LONG>(12, GetSystemMetrics(SM_CYCURSOR));
    }
    return normalized;
}

[[nodiscard]] bool HasTextSelection(const TextInputState& state) noexcept
{
    if (! state.selectionAnchorIndex.has_value())
    {
        return false;
    }

    const size_t selectionAnchorIndex = std::min(state.selectionAnchorIndex.value(), state.text.size());
    const size_t caretIndex           = std::min(state.caretIndex, state.text.size());
    return selectionAnchorIndex != caretIndex;
}

[[nodiscard]] std::wstring NormalizeWin32EditTextFromControlText(std::wstring_view text, bool multiline)
{
    if (! multiline || text.empty())
    {
        return std::wstring(text);
    }

    std::wstring normalized;
    normalized.reserve(text.size() + static_cast<size_t>(std::count(text.begin(), text.end(), L'\n')));
    for (wchar_t ch : text)
    {
        if (ch == L'\r')
        {
            continue;
        }

        if (ch == L'\n')
        {
            normalized.push_back(L'\r');
        }

        normalized.push_back(ch);
    }

    return normalized;
}

[[nodiscard]] std::wstring NormalizeControlTextFromWin32EditText(std::wstring_view text, bool multiline)
{
    if (! multiline || text.empty())
    {
        return std::wstring(text);
    }

    std::wstring normalized;
    normalized.reserve(text.size());
    for (size_t index = 0u; index < text.size(); ++index)
    {
        const wchar_t ch = text[index];
        if (ch == L'\r')
        {
            if (index + 1u < text.size() && text[index + 1u] == L'\n')
            {
                normalized.push_back(L'\n');
                ++index;
            }
            else
            {
                normalized.push_back(L'\n');
            }
            continue;
        }

        normalized.push_back(ch);
    }

    return normalized;
}

[[nodiscard]] std::pair<size_t, size_t> GetTextInputSelectionRange(const TextInputState& state) noexcept
{
    const size_t anchorIndex = state.selectionAnchorIndex.value_or(state.caretIndex);
    return {(std::min)(anchorIndex, state.caretIndex), (std::max)(anchorIndex, state.caretIndex)};
}

void SetTextInputSelectionRange(TextInputState& state, size_t start, size_t end) noexcept
{
    start = (std::min)(start, state.text.size());
    end   = (std::min)(end, state.text.size());
    if (start == end)
    {
        state.selectionAnchorIndex.reset();
        state.caretIndex = end;
        return;
    }

    state.selectionAnchorIndex = start;
    state.caretIndex           = end;
}

[[nodiscard]] size_t MapControlIndexToWin32EditIndex(std::wstring_view controlText, size_t controlIndex, bool multiline) noexcept
{
    const size_t clampedIndex = std::min(controlIndex, controlText.size());
    if (! multiline)
    {
        return clampedIndex;
    }

    size_t editIndex = 0u;
    for (size_t index = 0u; index < clampedIndex; ++index)
    {
        editIndex += (controlText[index] == L'\n') ? 2u : 1u;
    }
    return editIndex;
}

[[nodiscard]] size_t MapWin32EditIndexToControlIndex(std::wstring_view win32EditText, size_t editIndex, bool multiline) noexcept
{
    const size_t clampedIndex = std::min(editIndex, win32EditText.size());
    if (! multiline)
    {
        return clampedIndex;
    }

    size_t controlIndex = 0u;
    size_t index        = 0u;
    while (index < win32EditText.size() && index < clampedIndex)
    {
        if (win32EditText[index] == L'\r' && index + 1u < win32EditText.size() && win32EditText[index + 1u] == L'\n')
        {
            if (index + 2u <= clampedIndex)
            {
                index += 2u;
                ++controlIndex;
                continue;
            }
            break;
        }

        ++index;
        ++controlIndex;
    }

    return controlIndex;
}

[[nodiscard]] bool ReplaceTextInputSelection(TextInputState& state, std::wstring_view replacement)
{
    if (state.readOnly)
    {
        return false;
    }

    auto [selectionStart, selectionEnd] = GetTextInputSelectionRange(state);
    selectionStart                      = std::min(selectionStart, state.text.size());
    selectionEnd                        = std::min(selectionEnd, state.text.size());
    if (selectionEnd < selectionStart)
    {
        std::swap(selectionStart, selectionEnd);
    }

    state.text.replace(selectionStart, selectionEnd - selectionStart, replacement);
    state.caretIndex = selectionStart + replacement.size();
    state.selectionAnchorIndex.reset();
    return true;
}

[[nodiscard]] bool NativeTextInputSelectionChanged(const NativeTextInputState& previous, const NativeTextInputState& current) noexcept
{
    return previous.caretIndex != current.caretIndex || previous.selectionAnchorIndex != current.selectionAnchorIndex;
}

[[nodiscard]] bool NativeTextInputActiveTextPositionChanged(const NativeTextInputState& previous, const NativeTextInputState& current) noexcept
{
    return previous.caretIndex != current.caretIndex || previous.compositionCursorIndex != current.compositionCursorIndex;
}

[[nodiscard]] bool NativeTextInputCompositionChanged(const NativeTextInputState& previous, const NativeTextInputState& current) noexcept
{
    return previous.compositionStartIndex != current.compositionStartIndex || previous.compositionEndIndex != current.compositionEndIndex ||
           previous.compositionCursorIndex != current.compositionCursorIndex || previous.compositionClauseBoundaries != current.compositionClauseBoundaries;
}

[[nodiscard]] bool NativeTextInputConversionTargetChanged(const NativeTextInputState& previous, const NativeTextInputState& current) noexcept
{
    return previous.conversionTargetStartIndex != current.conversionTargetStartIndex || previous.conversionTargetEndIndex != current.conversionTargetEndIndex;
}

[[nodiscard]] std::pair<size_t, size_t> ResolveImeBaseCompositionRange(const TextInputState& state) noexcept
{
    const size_t textLength = state.text.size();
    const size_t caretIndex = std::min(state.caretIndex, textLength);
    if (! state.selectionAnchorIndex.has_value())
    {
        return {caretIndex, caretIndex};
    }

    const size_t selectionAnchorIndex = std::min(state.selectionAnchorIndex.value(), textLength);
    return {std::min(selectionAnchorIndex, caretIndex), std::max(selectionAnchorIndex, caretIndex)};
}

void ReplaceNativeTextInputRange(TextInputState& state, size_t rangeStart, size_t rangeEnd, std::wstring_view replacement)
{
    rangeStart = std::min(rangeStart, state.text.size());
    rangeEnd   = std::min(rangeEnd, state.text.size());
    if (rangeEnd < rangeStart)
    {
        std::swap(rangeStart, rangeEnd);
    }

    state.text.replace(rangeStart, rangeEnd - rangeStart, replacement);
    state.caretIndex = rangeStart + replacement.size();
    state.selectionAnchorIndex.reset();
}

[[nodiscard]] std::optional<std::wstring> ReadImeString(HIMC inputContext, DWORD index)
{
    const LONG byteCount = ImmGetCompositionStringW(inputContext, index, nullptr, 0u);
    if (byteCount < 0 || (byteCount % static_cast<LONG>(sizeof(wchar_t))) != 0)
    {
        return std::nullopt;
    }

    std::wstring value(static_cast<size_t>(byteCount) / sizeof(wchar_t), L'\0');
    if (byteCount == 0)
    {
        return value;
    }

    const LONG readCount = ImmGetCompositionStringW(inputContext, index, value.data(), static_cast<DWORD>(byteCount));
    if (readCount < 0 || (readCount % static_cast<LONG>(sizeof(wchar_t))) != 0)
    {
        return std::nullopt;
    }

    value.resize(static_cast<size_t>(readCount) / sizeof(wchar_t));
    return value;
}

[[nodiscard]] std::vector<uint8_t> ReadImeBytes(HIMC inputContext, DWORD index)
{
    const LONG byteCount = ImmGetCompositionStringW(inputContext, index, nullptr, 0u);
    if (byteCount <= 0)
    {
        return {};
    }

    std::vector<uint8_t> value(static_cast<size_t>(byteCount));
    const LONG readCount = ImmGetCompositionStringW(inputContext, index, value.data(), static_cast<DWORD>(value.size()));
    if (readCount <= 0)
    {
        return {};
    }

    value.resize(static_cast<size_t>(readCount));
    return value;
}

[[nodiscard]] std::vector<uint32_t> ReadImeDwords(HIMC inputContext, DWORD index)
{
    const std::vector<uint8_t> bytes = ReadImeBytes(inputContext, index);
    if (bytes.empty() || (bytes.size() % sizeof(uint32_t)) != 0u)
    {
        return {};
    }

    std::vector<uint32_t> value(bytes.size() / sizeof(uint32_t));
    std::memcpy(value.data(), bytes.data(), bytes.size());
    return value;
}

[[nodiscard]] bool IsImeTargetAttribute(uint8_t attribute) noexcept
{
    return attribute == ATTR_TARGET_CONVERTED || attribute == ATTR_TARGET_NOTCONVERTED;
}

[[nodiscard]] std::vector<size_t> ResolveImeClauseBoundaries(size_t compositionStartIndex, size_t compositionLength, const NativeTextInputImePayload& payload)
{
    std::vector<size_t> boundaries;
    boundaries.reserve(payload.compositionClauses.size());
    for (const uint32_t clauseOffset : payload.compositionClauses)
    {
        const size_t clampedOffset = (std::min)(static_cast<size_t>(clauseOffset), compositionLength);
        const size_t boundary      = compositionStartIndex + clampedOffset;
        if (boundaries.empty() || boundaries.back() != boundary)
        {
            boundaries.push_back(boundary);
        }
    }
    return boundaries;
}
} // namespace

ControlHost::NativeSystemCaret::~NativeSystemCaret() noexcept
{
    Reset();
}

bool ControlHost::NativeSystemCaret::Create(HWND ownerHwnd, int widthPx, int heightPx) noexcept
{
    Reset();
    if (! ownerHwnd || widthPx <= 0 || heightPx <= 0)
    {
        return false;
    }

    if (CreateCaret(ownerHwnd, nullptr, widthPx, heightPx) == FALSE)
    {
        return false;
    }

    _ownerHwnd = ownerHwnd;
    _created   = true;
    _visible   = false;
    _widthPx   = widthPx;
    _heightPx  = heightPx;
    return true;
}

bool ControlHost::NativeSystemCaret::Show() noexcept
{
    if (! _created || ! _ownerHwnd)
    {
        return false;
    }
    if (_visible)
    {
        return true;
    }

    if (ShowCaret(_ownerHwnd) == FALSE)
    {
        return false;
    }

    _visible = true;
    return true;
}

void ControlHost::NativeSystemCaret::Hide() noexcept
{
    if (_created && _visible && _ownerHwnd)
    {
        static_cast<void>(HideCaret(_ownerHwnd));
    }
    _visible = false;
}

void ControlHost::NativeSystemCaret::Reset() noexcept
{
    Hide();
    if (_created)
    {
        static_cast<void>(DestroyCaret());
    }
    _ownerHwnd = nullptr;
    _created   = false;
    _visible   = false;
    _widthPx   = 0;
    _heightPx  = 0;
}

void ControlHost::ActivateTextInput(Control* control, bool transferNativeFocus) noexcept
{
    ActivateNativeTextInputSession(control, transferNativeFocus);
}

void ControlHost::DeactivateTextInput() noexcept
{
    DeactivateNativeTextInputSession();
}

void ControlHost::SetTextInputBackend(TextInputBackend backend) noexcept
{
    if (_textInputBackend == backend)
    {
        return;
    }

    DeactivateTextInput();

    _textInputBackend = TextInputBackend::Native;

    if (! _focusedControl || ! _focusedControl->SupportsTextInput())
    {
        return;
    }

    ActivateTextInput(_focusedControl);
}

TextInputBackend ControlHost::GetTextInputBackend() const noexcept
{
    return _textInputBackend;
}

bool ControlHost::HasActiveNativeTextInputSession() const noexcept
{
    return _nativeTextInputControl != nullptr;
}

void ControlHost::ActivateNativeTextInputSession(Control* control, bool transferNativeFocus) noexcept
{
    if (_detachInProgress.load(std::memory_order_acquire))
        return;
    const auto startedAt = std::chrono::steady_clock::now();
    if (! control)
    {
        DeactivateNativeTextInputSession();
        return;
    }

    if (++_nativeTextInputSessionRevision == 0u)
        ++_nativeTextInputSessionRevision;
    const uint64_t requestedSessionRevision    = _nativeTextInputSessionRevision;
    const std::weak_ptr<int> controlLifetime   = control->GetLifetimeToken();
    uint64_t requestedFocusId                  = _nativeTextInputFocusId;
    const uint64_t requestedTransitionRevision = _nativeTextInputTsfTransitionRevision;
    const auto requestIsCurrent                = [this, control, &controlLifetime, &requestedFocusId, requestedSessionRevision]() noexcept
    {
        return ! controlLifetime.expired() && control == _focusedControl && control == _nativeTextInputControl && requestedFocusId == _nativeTextInputFocusId &&
               requestedSessionRevision == _nativeTextInputSessionRevision && NativeTextInputControlBelongsToTree(_root.get(), control);
    };
    if (! control->SupportsTextInput() || (_hwnd && (! IsWindowVisible(_hwnd) || _widthPx == 0u || _heightPx == 0u)))
    {
        DeactivateNativeTextInputSession();
        return;
    }

    TextInputState controlState;
    try
    {
        if (! control->ExportTextInputState(controlState))
        {
            DeactivateNativeTextInputSession();
            return;
        }
    }
    catch (const std::bad_alloc&)
    {
        DeactivateNativeTextInputSession();
        return;
    }
    if (controlLifetime.expired() || requestedSessionRevision != _nativeTextInputSessionRevision ||
        requestedTransitionRevision != _nativeTextInputTsfTransitionRevision || control != _focusedControl ||
        ! NativeTextInputControlBelongsToTree(_root.get(), control) || (control == _nativeTextInputControl && ! requestIsCurrent()))
    {
        return;
    }

    const bool controlChanged = _nativeTextInputControl != control;
    if (controlChanged)
    {
        if (++_nativeTextInputFocusId == 0)
            ++_nativeTextInputFocusId;
        ClearNativeTextInputCompositionState();
    }
    else if (_nativeTextInputImeComposing && (controlState.masked || controlState.readOnly))
    {
        CancelNativeTextInputImeComposition();
        if (controlLifetime.expired() || requestedSessionRevision != _nativeTextInputSessionRevision ||
            requestedTransitionRevision != _nativeTextInputTsfTransitionRevision || control != _nativeTextInputControl ||
            ! NativeTextInputControlBelongsToTree(_root.get(), control))
            return;
        try
        {
            if (! control->ExportTextInputState(controlState))
                return;
        }
        catch (const std::bad_alloc&)
        {
            return;
        }
    }
    _nativeTextInputControl         = control;
    _nativeTextInputControlLifetime = control->GetLifetimeToken();
    try
    {
        _nativeTextInputStateCache      = ToNativeTextInputState(*control, controlState);
        _nativeTextInputStateCacheValid = true;
        ApplyNativeTextInputCompositionStateToCache();
    }
    catch (const std::bad_alloc&)
    {
        SecureClearNativeTextInputStateCache();
        return;
    }
    if (controlChanged)
    {
        ++_nativeTextInputEventCounters.activationCount;
    }
    requestedFocusId = _nativeTextInputFocusId;

    if (transferNativeFocus && _hwnd && GetFocus() != _hwnd)
    {
        SetFocus(_hwnd);
    }
    // SetFocus synchronously dispatches focus messages. The previous HWND's WM_KILLFOCUS handler or this
    // host's accessibility publication can replace the control tree before it returns.
    if (controlLifetime.expired() || requestedSessionRevision != _nativeTextInputSessionRevision ||
        requestedTransitionRevision != _nativeTextInputTsfTransitionRevision || control != _focusedControl || control != _nativeTextInputControl ||
        ! NativeTextInputControlBelongsToTree(_root.get(), control))
    {
        if (_nativeTextInputControl == control && _nativeTextInputSessionRevision == requestedSessionRevision && _nativeTextInputFocusId == requestedFocusId &&
            _nativeTextInputTsfTransitionRevision == requestedTransitionRevision)
        {
            // A synchronous WM_SETFOCUS may have activated a newer session on the same
            // retained control. Only this request may clean up its own failed transfer.
            DeactivateNativeTextInputSession();
        }
        return;
    }
    if (controlState.masked)
    {
        // Password text is never offered to TSF or the host's legacy IMM composition path.
        uint64_t expectedTransition = _nativeTextInputTsfTransitionRevision + 1u;
        if (expectedTransition == 0u)
            ++expectedTransition;
        DeactivateNativeTextInputTsf();
        if (_nativeTextInputSessionRevision != requestedSessionRevision || _nativeTextInputTsfTransitionRevision != expectedTransition ||
            controlLifetime.expired() || control != _nativeTextInputControl || ! NativeTextInputControlBelongsToTree(_root.get(), control))
            return;
        ClearNativeTextInputCompositionState();
        UpdateNativeTextInputCaret();
        return;
    }
    // Logical focus supports model/UIA updates without taking the person's keyboard focus.
    // A visible inactive host can retain its cache, but may not activate an OS text service.
    if (_hwnd && GetFocus() != _hwnd)
    {
        uint64_t expectedTransition = _nativeTextInputTsfTransitionRevision + 1u;
        if (expectedTransition == 0u)
            ++expectedTransition;
        DeactivateNativeTextInputTsf();
        if (_nativeTextInputSessionRevision != requestedSessionRevision || _nativeTextInputTsfTransitionRevision != expectedTransition ||
            controlLifetime.expired() || control != _nativeTextInputControl || ! NativeTextInputControlBelongsToTree(_root.get(), control))
            return;
        UpdateNativeTextInputCaret();
        return;
    }
    if (controlChanged || ! _nativeTextInputTsfActive)
    {
        static_cast<void>(ActivateNativeTextInputTsf(control));
    }
    if (! requestIsCurrent())
    {
        return;
    }
    UpdateNativeTextInputCaret();

    Debug::Perf::Emit(L"dxui.textinput.activate_us",
                      L"native-session",
                      Debug::Perf::ElapsedUs(startedAt),
                      _nativeTextInputStateCache.multiline ? 1u : 0u,
                      _nativeTextInputStateCache.masked ? 1u : 0u,
                      S_OK);
}

void ControlHost::SecureClearNativeTextInputStateCache() noexcept
{
    SecureWipe::SecureClear(_nativeTextInputStateCache.text);
    _nativeTextInputStateCache      = NativeTextInputState{};
    _nativeTextInputStateCacheValid = false;
}

void ControlHost::DeactivateNativeTextInputSession() noexcept
{
    Control* const sessionControl = _nativeTextInputControl;
    const uint64_t sessionFocusId = _nativeTextInputFocusId;
    if (++_nativeTextInputSessionRevision == 0u)
        ++_nativeTextInputSessionRevision;
    const uint64_t sessionRevision        = _nativeTextInputSessionRevision;
    const uint64_t transitionBeforeCancel = _nativeTextInputTsfTransitionRevision;
    const auto sessionIsCurrent           = [this, sessionControl, sessionFocusId, sessionRevision]() noexcept
    {
        // Retirement clears observers/cache even after their lifetime expires. Only a
        // reentrant replacement changes this identity; no control is dereferenced here.
        return _nativeTextInputControl == sessionControl && _nativeTextInputFocusId == sessionFocusId && _nativeTextInputSessionRevision == sessionRevision;
    };
    CancelNativeTextInputImeComposition();
    if (! sessionIsCurrent() || _nativeTextInputTsfTransitionRevision != transitionBeforeCancel)
    {
        return;
    }
    DeactivateNativeTextInputTsf();
    uint64_t expectedTransition = transitionBeforeCancel + 1u;
    if (expectedTransition == 0u)
        ++expectedTransition;
    if (! sessionIsCurrent() || _nativeTextInputTsfTransitionRevision != expectedTransition)
    {
        return;
    }

    if (! _nativeTextInputControl)
    {
        _nativeTextInputControlLifetime.reset();
        SecureClearNativeTextInputStateCache();
        ClearNativeTextInputCompositionState();
        _pendingNativeTextInputPaintMetric.reset();
        return;
    }

    _nativeTextInputControl = nullptr;
    _nativeTextInputControlLifetime.reset();
    SecureClearNativeTextInputStateCache();
    ClearNativeTextInputCompositionState();
    _pendingNativeTextInputPaintMetric.reset();
    ++_nativeTextInputEventCounters.deactivationCount;
    DestroyNativeTextInputCaret();
}

bool ControlHost::ActivateNativeTextInputTsf(Control* control) noexcept
{
    if (! control)
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        return false;
    }
    const std::weak_ptr<int> controlLifetime = control->GetLifetimeToken();
    const uint64_t requestedFocusId          = _nativeTextInputFocusId;
    uint64_t activationRevision              = _nativeTextInputTsfTransitionRevision + 1u;
    if (activationRevision == 0u)
        ++activationRevision;
    DeactivateNativeTextInputTsf();
    if (_nativeTextInputTsfTransitionRevision != activationRevision)
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        return false;
    }
    const auto requestIsCurrent = [this, control, &controlLifetime, requestedFocusId, activationRevision]() noexcept
    {
        if (_detachInProgress.load(std::memory_order_acquire) || controlLifetime.expired() || control != _focusedControl ||
            control != _nativeTextInputControl || requestedFocusId != _nativeTextInputFocusId || activationRevision != _nativeTextInputTsfTransitionRevision ||
            ! NativeTextInputControlBelongsToTree(_root.get(), control) || ! IsControlEffectivelyInteractive(_root.get(), control) ||
            _textInputBackend != TextInputBackend::Native ||
            (_hwnd && (! IsWindow(_hwnd) || ! IsWindowVisible(_hwnd) || _widthPx == 0u || _heightPx == 0u || GetFocus() != _hwnd)))
        {
            return false;
        }
        const auto* currentTextField = dynamic_cast<const TextField*>(control);
        return ! currentTextField || ! currentTextField->IsMasked();
    };
    ++_nativeTextInputEventCounters.tsfActivationAttemptCount;

    if (! requestIsCurrent())
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        return false;
    }
    const auto* textField = dynamic_cast<const TextField*>(control);
    if (! control || ! control->SupportsTextInput() || ! _hwnd || (textField && textField->IsMasked()))
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        return false;
    }

    wil::com_ptr_nothrow<ITextStoreACP> textStore;
    textStore.attach(CreateNativeTextInputTextStore(*this, *control, true));
    if (! textStore)
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        return false;
    }
    const auto releaseUncommittedStore = [&textStore]() noexcept
    {
        DisconnectNativeTextInputTextStore(textStore.get());
        DetachNativeTextInputTextStore(textStore.get());
    };
    if (! requestIsCurrent())
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        releaseUncommittedStore();
        return false;
    }

    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    TfClientId clientId = 0u;
    if (! EnsureNativeTextInputThreadManager(threadMgr, clientId))
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        releaseUncommittedStore();
        return false;
    }
    if (! requestIsCurrent())
    {
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        releaseUncommittedStore();
        return false;
    }

    wil::com_ptr_nothrow<ITfDocumentMgr> documentMgr;
    bool pushed     = false;
    bool associated = false;
    wil::com_ptr_nothrow<ITfDocumentMgr> previousFocusDocumentMgr;
    auto failActivation =
        [this, &threadMgr, &documentMgr, &pushed, &associated, &previousFocusDocumentMgr, &releaseUncommittedStore, activationRevision]() noexcept
    {
        // Unwind only this staged document. A callback may already have installed a newer host session.
        ClearThreadManagerFocusIfOwnedBy(threadMgr.get(), documentMgr.get());
        if (associated && activationRevision == _nativeTextInputTsfTransitionRevision && ! _nativeTextInputTsfActive && _hwnd)
        {
            RestoreThreadManagerFocusAssociation(
                threadMgr.get(), _hwnd, documentMgr.get(), previousFocusDocumentMgr.get(), [this, activationRevision]() noexcept {
                return activationRevision == _nativeTextInputTsfTransitionRevision && ! _nativeTextInputTsfActive;
            });
        }
        if (pushed && documentMgr)
        {
            static_cast<void>(documentMgr->Pop(TF_POPF_ALL));
        }
        releaseUncommittedStore();
        ++_nativeTextInputEventCounters.tsfActivationFailureCount;
        return false;
    };

    HRESULT hr = threadMgr->CreateDocumentMgr(documentMgr.put());
    if (FAILED(hr) || ! documentMgr)
    {
        return failActivation();
    }
    if (! requestIsCurrent())
    {
        return failActivation();
    }

    wil::com_ptr_nothrow<ITfContext> context;
    TfEditCookie editCookie = 0u;
    hr                      = documentMgr->CreateContext(clientId, 0u, textStore.get(), context.put(), &editCookie);
    if (FAILED(hr) || ! context)
    {
        return failActivation();
    }
    if (! requestIsCurrent())
    {
        return failActivation();
    }

    hr     = documentMgr->Push(context.get());
    pushed = SUCCEEDED(hr);
    if (FAILED(hr) || ! requestIsCurrent())
    {
        return failActivation();
    }

    hr         = threadMgr->AssociateFocus(_hwnd, documentMgr.get(), previousFocusDocumentMgr.put());
    associated = SUCCEEDED(hr);
    if (FAILED(hr) || ! requestIsCurrent())
    {
        return failActivation();
    }

    hr = threadMgr->SetFocus(documentMgr.get());
    if (FAILED(hr) || ! requestIsCurrent())
    {
        return failActivation();
    }

    wil::com_ptr_nothrow<IUnknown> threadMgrUnknown;
    wil::com_ptr_nothrow<IUnknown> documentMgrUnknown;
    wil::com_ptr_nothrow<IUnknown> previousFocusUnknown;
    wil::com_ptr_nothrow<IUnknown> contextUnknown;
    wil::com_ptr_nothrow<IUnknown> textStoreUnknown;
    if (FAILED(threadMgr.query_to(threadMgrUnknown.put())) || FAILED(documentMgr.query_to(documentMgrUnknown.put())) ||
        FAILED(context.query_to(contextUnknown.put())) || FAILED(textStore.query_to(textStoreUnknown.put())) ||
        (previousFocusDocumentMgr && FAILED(previousFocusDocumentMgr.query_to(previousFocusUnknown.put()))) || ! requestIsCurrent())
    {
        return failActivation();
    }

    _nativeTextInputTsfThreadMgr                = std::move(threadMgrUnknown);
    _nativeTextInputTsfDocumentMgr              = std::move(documentMgrUnknown);
    _nativeTextInputTsfPreviousFocusDocumentMgr = std::move(previousFocusUnknown);
    _nativeTextInputTsfContext                  = std::move(contextUnknown);
    _nativeTextInputTsfTextStore                = std::move(textStoreUnknown);
    _nativeTextInputTsfClientId                 = clientId;
    _nativeTextInputTsfActive                   = true;
    _nativeTextInputTsfControl                  = control;
    _nativeTextInputTsfControlLifetime          = controlLifetime;
    ++_nativeTextInputEventCounters.tsfActivationSuccessCount;
    // Push/SetFocus may have synchronously changed text, selection, or geometry while the
    // staged store was not yet published to SyncNativeTextInputSession. Reconcile after commit.
    ScheduleTextStoreReconciliation(textStore.get());
    return true;
}

void ControlHost::DeactivateNativeTextInputTsf() noexcept
{
    if (++_nativeTextInputTsfTransitionRevision == 0u)
        ++_nativeTextInputTsfTransitionRevision;
    const uint64_t transitionRevision = _nativeTextInputTsfTransitionRevision;
    InvalidateNativeTextStoreDeferredWork();
    const bool wasActive     = _nativeTextInputTsfActive;
    const bool controlIsLive = _nativeTextInputTsfControl && ! _nativeTextInputTsfControlLifetime.expired() &&
                               NativeTextInputControlBelongsToTree(_root.get(), _nativeTextInputTsfControl);

    // Retire this session before any TSF call: SetFocus and Pop synchronously invoke
    // installed sinks, which may activate a replacement session on this host.
    wil::com_ptr_nothrow<IUnknown> textStoreToDisconnect = std::move(_nativeTextInputTsfTextStore);
    wil::com_ptr_nothrow<IUnknown> threadMgrUnknown      = std::move(_nativeTextInputTsfThreadMgr);
    wil::com_ptr_nothrow<IUnknown> documentMgrUnknown    = std::move(_nativeTextInputTsfDocumentMgr);
    wil::com_ptr_nothrow<IUnknown> previousFocusUnknown  = std::move(_nativeTextInputTsfPreviousFocusDocumentMgr);
    wil::com_ptr_nothrow<IUnknown> contextUnknown        = std::move(_nativeTextInputTsfContext);
    _nativeTextInputTsfClientId                          = 0u;
    _nativeTextInputTsfActive                            = false;
    _nativeTextInputTsfControl                           = nullptr;
    _nativeTextInputTsfControlLifetime.reset();

    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    wil::com_ptr_nothrow<ITfDocumentMgr> documentMgr;
    wil::com_ptr_nothrow<ITfDocumentMgr> previousFocusDocumentMgr;
    if (threadMgrUnknown)
        static_cast<void>(threadMgrUnknown.query_to(threadMgr.put()));
    if (documentMgrUnknown)
        static_cast<void>(documentMgrUnknown.query_to(documentMgr.put()));
    if (previousFocusUnknown)
        static_cast<void>(previousFocusUnknown.query_to(previousFocusDocumentMgr.put()));

    if (! controlIsLive)
    {
        // A retained TSF context may synchronously call its store during Pop. Sever all
        // host/control access before Pop when the retained control is no longer alive.
        DetachNativeTextInputTextStore(textStoreToDisconnect.get());
    }
    if (threadMgr)
    {
        ClearThreadManagerFocusIfOwnedBy(threadMgr.get(), documentMgr.get());
        if (_hwnd && transitionRevision == _nativeTextInputTsfTransitionRevision)
        {
            RestoreThreadManagerFocusAssociation(
                threadMgr.get(), _hwnd, documentMgr.get(), previousFocusDocumentMgr.get(), [this, transitionRevision]() noexcept {
                return transitionRevision == _nativeTextInputTsfTransitionRevision;
            });
        }
    }

    if (documentMgr)
    {
        static_cast<void>(documentMgr->Pop(TF_POPF_ALL));
    }

    // Pop may activate a replacement session; this cleanup owns only the detached old store.
    if (controlIsLive)
    {
        DisconnectNativeTextInputTextStore(textStoreToDisconnect.get());
        DetachNativeTextInputTextStore(textStoreToDisconnect.get());
    }
    contextUnknown.reset();
    if (wasActive)
    {
        ++_nativeTextInputEventCounters.tsfDeactivationCount;
    }
}

void ShutdownNativeTextInputForCurrentThread() noexcept
{
    NativeTextInputThreadManagerState& state = GetNativeTextInputThreadManagerState();
    ShutdownNativeTextInputThreadManagerState(state);
}

void ControlHost::SyncNativeTextInputSession(Control* control) noexcept
{
    if (! control || control != _nativeTextInputControl)
    {
        return;
    }

    const std::weak_ptr<int> controlLifetime = _nativeTextInputControlLifetime;
    const uint64_t sessionRevision           = _nativeTextInputSessionRevision;
    const auto isCurrent                     = [this, control, &controlLifetime, sessionRevision]() noexcept
    {
        return ! controlLifetime.expired() && sessionRevision == _nativeTextInputSessionRevision && control == _nativeTextInputControl &&
               NativeTextInputControlBelongsToTree(_root.get(), control);
    };

    TextInputState controlState;
    try
    {
        if (! control->ExportTextInputState(controlState))
            return;
    }
    catch (const std::bad_alloc&)
    {
        SecureClearNativeTextInputStateCache();
        return;
    }

    if (! isCurrent())
        return;
    std::optional<NativeTextInputState> previousState;
    try
    {
        if (_nativeTextInputStateCacheValid)
            previousState = _nativeTextInputStateCache;
    }
    catch (const std::bad_alloc&)
    {
        SecureClearNativeTextInputStateCache();
        return;
    }

    if (_nativeTextInputImeComposing &&
        (controlState.masked || controlState.readOnly || (_nativeTextInputImePreviewText && controlState.text != _nativeTextInputImePreviewText.value())))
    {
        CancelNativeTextInputImeComposition();
        if (! isCurrent())
            return;
        try
        {
            if (! control->ExportTextInputState(controlState))
                return;
        }
        catch (const std::bad_alloc&)
        {
            return;
        }
        if (! isCurrent())
            return;
    }
    else if (controlState.masked)
    {
        DeactivateNativeTextInputTsf();
        if (! isCurrent())
            return;
    }

    try
    {
        NativeTextInputState nextState  = ToNativeTextInputState(*control, controlState);
        _nativeTextInputStateCache      = std::move(nextState);
        _nativeTextInputStateCacheValid = true;
        ApplyNativeTextInputCompositionStateToCache();
    }
    catch (const std::bad_alloc&)
    {
        SecureClearNativeTextInputStateCache();
        return;
    }
    ++_nativeTextInputEventCounters.synchronizationCount;
    if (previousState.has_value())
    {
        RaiseNativeTextInputAccessibilityEvents(previousState.value());
    }
    if (! isCurrent())
        return;
    UpdateNativeTextInputCaret();
    if (isCurrent() && _nativeTextInputTsfTextStore)
    {
        wil::com_ptr_nothrow<ITextStoreACP> textStore;
        if (SUCCEEDED(_nativeTextInputTsfTextStore.query_to(textStore.put())) && textStore)
            NotifyTextStoreChanged(textStore.get());
    }
}

bool ControlHost::ScheduleNativeTextStoreDeferredWork(Control* control, UINT_PTR sessionCookie) noexcept
{
    if (sessionCookie != _nativeTextStoreDispatchCookie || ! control || control != _nativeTextInputControl || _nativeTextInputControlLifetime.expired() ||
        ! NativeTextInputControlBelongsToTree(_root.get(), control) || ! _hwnd || ! IsWindow(_hwnd) || ! _nativeTextInputTsfActive ||
        ! _nativeTextInputTsfTextStore)
        return false;
    if (_nativeTextStoreNotificationPosted)
        return true;
    const WndMsg::RegisteredMessage deferredWork = WndMsg::NativeTextStoreDeferredWork();
    if (! deferredWork)
        return false;
    _nativeTextStoreNotificationPosted = PostMessageW(_hwnd, deferredWork.value, static_cast<WPARAM>(_nativeTextStoreDispatchCookie), 0) != FALSE;
    return _nativeTextStoreNotificationPosted;
}

void ControlHost::InvalidateNativeTextStoreDeferredWork() noexcept
{
    ++_nativeTextStoreDispatchCookie;
    if (_nativeTextStoreDispatchCookie == 0u)
        ++_nativeTextStoreDispatchCookie;
    _nativeTextStoreNotificationPosted = false;
}

void ControlHost::RaiseNativeTextInputAccessibilityEvent(TextInputAutomationEventKind kind) noexcept
{
    if (! RaiseWindowHostTextInputAutomationEvent(_hwnd, _nativeTextInputControl, kind))
    {
        return;
    }

    switch (kind)
    {
        case TextInputAutomationEventKind::TextChanged: ++_nativeTextInputEventCounters.uiaTextChangedCount; break;
        case TextInputAutomationEventKind::TextSelectionChanged: ++_nativeTextInputEventCounters.uiaTextSelectionChangedCount; break;
        case TextInputAutomationEventKind::ActiveTextPositionChanged: ++_nativeTextInputEventCounters.uiaActiveTextPositionChangedCount; break;
        case TextInputAutomationEventKind::TextEditCompositionChanged: ++_nativeTextInputEventCounters.uiaTextEditTextChangedCount; break;
        case TextInputAutomationEventKind::TextEditConversionTargetChanged: ++_nativeTextInputEventCounters.uiaTextEditConversionTargetChangedCount; break;
        default: break;
    }
}

void ControlHost::RaiseNativeTextInputAccessibilityEvents(const NativeTextInputState& previousState) noexcept
{
    Control* const control            = _nativeTextInputControl;
    const std::weak_ptr<int> lifetime = _nativeTextInputControlLifetime;
    const auto isCurrent              = [this, control, &lifetime]() noexcept
    { return control && ! lifetime.expired() && control == _nativeTextInputControl && NativeTextInputControlBelongsToTree(_root.get(), control); };
    if (! _nativeTextInputStateCacheValid || ! isCurrent())
    {
        return;
    }

    const NativeTextInputState& currentState = _nativeTextInputStateCache;
    const bool textChanged                   = previousState.text != currentState.text;
    const bool selectionChanged              = NativeTextInputSelectionChanged(previousState, currentState);
    const bool activeTextPositionChanged     = NativeTextInputActiveTextPositionChanged(previousState, currentState);
    const bool compositionChanged            = NativeTextInputCompositionChanged(previousState, currentState);
    const bool conversionTargetChanged       = NativeTextInputConversionTargetChanged(previousState, currentState);
    if (! textChanged && ! selectionChanged && ! activeTextPositionChanged && ! compositionChanged && ! conversionTargetChanged)
    {
        return;
    }

    RefreshWindowHostAccessibilitySnapshot(_hwnd, this);
    if (! isCurrent())
    {
        return;
    }
    if (textChanged)
    {
        RaiseNativeTextInputAccessibilityEvent(TextInputAutomationEventKind::TextChanged);
        if (! isCurrent())
            return;
    }
    if (selectionChanged)
    {
        RaiseNativeTextInputAccessibilityEvent(TextInputAutomationEventKind::TextSelectionChanged);
        if (! isCurrent())
            return;
    }
    if (activeTextPositionChanged)
    {
        RaiseNativeTextInputAccessibilityEvent(TextInputAutomationEventKind::ActiveTextPositionChanged);
        if (! isCurrent())
            return;
    }
    if (compositionChanged)
    {
        RaiseNativeTextInputAccessibilityEvent(TextInputAutomationEventKind::TextEditCompositionChanged);
        if (! isCurrent())
            return;
    }
    if (conversionTargetChanged)
    {
        RaiseNativeTextInputAccessibilityEvent(TextInputAutomationEventKind::TextEditConversionTargetChanged);
    }
}

void ControlHost::ApplyNativeTextInputCompositionStateToCache() noexcept
{
    if (! _nativeTextInputStateCacheValid)
    {
        return;
    }

    if (! _nativeTextInputImeComposing && ! _nativeTextInputTsfComposing)
    {
        _nativeTextInputStateCache.compositionStartIndex.reset();
        _nativeTextInputStateCache.compositionEndIndex.reset();
        _nativeTextInputStateCache.conversionTargetStartIndex.reset();
        _nativeTextInputStateCache.conversionTargetEndIndex.reset();
        _nativeTextInputStateCache.compositionCursorIndex.reset();
        _nativeTextInputStateCache.compositionClauseBoundaries.clear();
        return;
    }

    _nativeTextInputStateCache.compositionStartIndex      = _nativeTextInputCompositionStartIndex;
    _nativeTextInputStateCache.compositionEndIndex        = _nativeTextInputCompositionEndIndex;
    _nativeTextInputStateCache.conversionTargetStartIndex = _nativeTextInputConversionTargetStartIndex;
    _nativeTextInputStateCache.conversionTargetEndIndex   = _nativeTextInputConversionTargetEndIndex;
    _nativeTextInputStateCache.compositionCursorIndex     = _nativeTextInputCompositionCursorIndex;
    try
    {
        _nativeTextInputStateCache.compositionClauseBoundaries = _nativeTextInputCompositionClauseBoundaries;
    }
    catch (const std::bad_alloc&)
    {
        _nativeTextInputStateCache.compositionStartIndex.reset();
        _nativeTextInputStateCache.compositionEndIndex.reset();
        _nativeTextInputStateCache.conversionTargetStartIndex.reset();
        _nativeTextInputStateCache.conversionTargetEndIndex.reset();
        _nativeTextInputStateCache.compositionCursorIndex.reset();
        _nativeTextInputStateCache.compositionClauseBoundaries.clear();
    }
}

bool ControlHost::SetNativeTextInputCompositionRange(Control* control,
                                                     std::optional<size_t> start,
                                                     std::optional<size_t> end,
                                                     std::optional<size_t> cursor) noexcept
{
    if (! control || control != _nativeTextInputControl || _nativeTextInputControlLifetime.expired() ||
        ! NativeTextInputControlBelongsToTree(_root.get(), control) || ! _nativeTextInputStateCacheValid)
    {
        return false;
    }

    NativeTextInputState previousState;
    try
    {
        previousState = _nativeTextInputStateCache;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }

    _nativeTextInputTsfComposing           = start.has_value() && end.has_value();
    _nativeTextInputCompositionStartIndex  = start;
    _nativeTextInputCompositionEndIndex    = end;
    _nativeTextInputCompositionCursorIndex = cursor;
    _nativeTextInputConversionTargetStartIndex.reset();
    _nativeTextInputConversionTargetEndIndex.reset();
    _nativeTextInputCompositionClauseBoundaries.clear();
    ApplyNativeTextInputCompositionStateToCache();
    RaiseNativeTextInputAccessibilityEvents(previousState);

    if (_nativeTextInputControlLifetime.expired() || control != _nativeTextInputControl || ! NativeTextInputControlBelongsToTree(_root.get(), control))
    {
        return false;
    }
    UpdateNativeTextInputCaret();
    return ! _nativeTextInputControlLifetime.expired() && control == _nativeTextInputControl && NativeTextInputControlBelongsToTree(_root.get(), control);
}

void ControlHost::CancelNativeTextInputImeComposition() noexcept
{
    // END and queued payloads still belong to this retired IMM composition. A fresh START admits another one.
    _nativeTextInputImeCancelledUntilStart = true;
    Control* const control                 = _nativeTextInputControl;
    const auto lifetime                    = _nativeTextInputControlLifetime;
    if (! _nativeTextInputImeBaseState || ! _nativeTextInputImePreviewText || ! control || lifetime.expired() ||
        ! NativeTextInputControlBelongsToTree(_root.get(), control))
    {
        ClearNativeTextInputCompositionState();
        return;
    }
    const uint64_t sessionRevision = _nativeTextInputSessionRevision;
    TextInputState base            = std::move(_nativeTextInputImeBaseState.value());
    std::wstring preview           = std::move(_nativeTextInputImePreviewText.value());
    ClearNativeTextInputCompositionState();
    const uint64_t cancellationImeRevision = _nativeTextInputImeRevision;
    const auto cancellationIsCurrent       = [this, control, &lifetime, sessionRevision, cancellationImeRevision]() noexcept
    {
        return ! lifetime.expired() && control == _nativeTextInputControl && sessionRevision == _nativeTextInputSessionRevision &&
               cancellationImeRevision == _nativeTextInputImeRevision && NativeTextInputControlBelongsToTree(_root.get(), control);
    };
    const auto clearCacheIfCancellationStillOwnsPreview = [this, &cancellationIsCurrent, &preview]() noexcept
    {
        if (cancellationIsCurrent() && _nativeTextInputStateCacheValid && _nativeTextInputStateCache.text == preview)
            SecureClearNativeTextInputStateCache();
    };
    try
    {
        TextInputState live;
        if (! control->ExportTextInputState(live) || ! cancellationIsCurrent() || live.text != preview || ! _nativeTextInputStateCacheValid ||
            _nativeTextInputStateCache.text != preview)
            return;
        // Restore only the preview we own. Application text and current field policy win cancellation.
        base.masked   = live.masked;
        base.readOnly = live.readOnly;
        if (control->ImportTextInputState(*this, base, false) && cancellationIsCurrent())
            SyncNativeTextInputSession(control);
    }
    catch (const std::exception&)
    {
        // Allocation failure or a throwing control override leaves cancellation complete. Invalidate only the
        // cache still owned by this operation; a reentrant successor must survive the window-message boundary.
        clearCacheIfCancellationStillOwnsPreview();
    }
}

void ControlHost::ClearNativeTextInputCompositionState() noexcept
{
    if (++_nativeTextInputImeRevision == 0u)
        ++_nativeTextInputImeRevision;
    _nativeTextInputImeComposing = false;
    _nativeTextInputTsfComposing = false;
    _nativeTextInputCompositionStartIndex.reset();
    _nativeTextInputCompositionEndIndex.reset();
    _nativeTextInputConversionTargetStartIndex.reset();
    _nativeTextInputConversionTargetEndIndex.reset();
    _nativeTextInputCompositionCursorIndex.reset();
    _nativeTextInputCompositionClauseBoundaries.clear();
    _nativeTextInputImeBaseState.reset();
    if (_nativeTextInputImePreviewText)
        SecureWipe::SecureClear(_nativeTextInputImePreviewText.value());
    _nativeTextInputImePreviewText.reset();
    ApplyNativeTextInputCompositionStateToCache();
}

NativeTextInputImePayload ControlHost::ReadNativeTextInputImePayload(LPARAM compositionFlags) noexcept
{
    if (_debugNativeTextInputImePayload.has_value())
    {
        NativeTextInputImePayload payload = std::move(_debugNativeTextInputImePayload.value());
        _debugNativeTextInputImePayload.reset();
        return payload;
    }

    NativeTextInputImePayload payload;
    if (! _hwnd)
    {
        return payload;
    }

    HIMC inputContext = ImmGetContext(_hwnd);
    if (! inputContext)
    {
        return payload;
    }
    const auto releaseContext = wil::scope_exit([&] { ImmReleaseContext(_hwnd, inputContext); });

    try
    {
        if ((compositionFlags & GCS_RESULTSTR) != 0)
        {
            if (std::optional<std::wstring> resultString = ReadImeString(inputContext, GCS_RESULTSTR); resultString.has_value())
            {
                payload.hasResultString = true;
                payload.resultString    = std::move(resultString.value());
            }
        }
        if ((compositionFlags & GCS_COMPSTR) != 0)
        {
            if (std::optional<std::wstring> compositionString = ReadImeString(inputContext, GCS_COMPSTR); compositionString.has_value())
            {
                payload.hasCompositionString = true;
                payload.compositionString    = std::move(compositionString.value());
            }
        }
        if ((compositionFlags & GCS_COMPATTR) != 0)
        {
            payload.compositionAttributes = ReadImeBytes(inputContext, GCS_COMPATTR);
        }
        if ((compositionFlags & GCS_COMPCLAUSE) != 0)
        {
            payload.compositionClauses = ReadImeDwords(inputContext, GCS_COMPCLAUSE);
        }
        if ((compositionFlags & GCS_CURSORPOS) != 0)
        {
            const LONG cursorPosition = ImmGetCompositionStringW(inputContext, GCS_CURSORPOS, nullptr, 0u);
            if (cursorPosition >= 0)
            {
                payload.hasCursorPosition = true;
                payload.cursorPosition    = static_cast<size_t>(cursorPosition);
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        return NativeTextInputImePayload{};
    }
    return payload;
}

void ControlHost::UpdateNativeTextInputImeWindows() noexcept
{
    const auto startedAt = std::chrono::steady_clock::now();
    if (! _hwnd || _textInputBackend != TextInputBackend::Native || ! _nativeTextInputControl || ! _nativeTextInputStateCacheValid)
    {
        return;
    }

    HIMC inputContext = ImmGetContext(_hwnd);
    if (! inputContext)
    {
        return;
    }
    const auto releaseContext = wil::scope_exit([&] { ImmReleaseContext(_hwnd, inputContext); });

    D2D1_RECT_F caretRectDip{};
    RECT caretClientRectPx{};
    RECT caretScreenRectPx{};
    if (! TryGetNativeTextInputCaretRects(caretRectDip, caretClientRectPx, caretScreenRectPx))
    {
        return;
    }

    COMPOSITIONFORM compositionForm{};
    compositionForm.dwStyle      = CFS_FORCE_POSITION;
    compositionForm.ptCurrentPos = POINT{caretClientRectPx.left, caretClientRectPx.top};
    static_cast<void>(ImmSetCompositionWindow(inputContext, &compositionForm));

    CANDIDATEFORM candidateForm{};
    candidateForm.dwIndex      = 0u;
    candidateForm.dwStyle      = CFS_EXCLUDE;
    candidateForm.ptCurrentPos = POINT{caretClientRectPx.left, caretClientRectPx.bottom};
    candidateForm.rcArea       = caretClientRectPx;
    static_cast<void>(ImmSetCandidateWindow(inputContext, &candidateForm));

    Debug::Perf::Emit(L"dxui.textinput.candidate_rect_update_us",
                      L"native-ime-window",
                      Debug::Perf::ElapsedUs(startedAt),
                      static_cast<uint64_t>(std::max<LONG>(0, caretClientRectPx.right - caretClientRectPx.left)),
                      static_cast<uint64_t>(std::max<LONG>(0, caretClientRectPx.bottom - caretClientRectPx.top)),
                      S_OK);
}

bool ControlHost::HandleNativeTextInputEditMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& outResult) noexcept
try
{
    outResult = 0;
    // The host HWND is a presentation surface, not an edit control. Let the window procedure keep
    // ownership of its caption text even while a DxUi editor has logical focus.
    if (msg == WM_GETTEXTLENGTH || msg == WM_GETTEXT || msg == WM_SETTEXT)
    {
        return false;
    }
    if (_textInputBackend != TextInputBackend::Native || ! _focusedControl || ! _focusedControl->SupportsTextInput())
    {
        return false;
    }

    Control* const editTarget     = _focusedControl;
    const auto editTargetLifetime = editTarget->GetLifetimeToken();
    const auto inputStartedAt     = std::chrono::steady_clock::now();

    const auto syncImportedState = [this, editTarget, &editTargetLifetime](const TextInputState& state, bool notifyChange, bool clearComposition) -> bool
    {
        if (clearComposition)
        {
            ClearNativeTextInputCompositionState();
        }

        if (! editTarget->ImportTextInputState(*this, state, notifyChange))
        {
            return false;
        }

        if (! editTargetLifetime.expired() && editTarget == _focusedControl && NativeTextInputControlBelongsToTree(_root.get(), editTarget))
        {
            SyncNativeTextInputSession(editTarget);
        }
        return true;
    };

    bool controlHandled = false;
    switch (msg)
    {
        case EM_GETSEL:
        {
            TextInputState state;
            if (! editTarget->ExportTextInputState(state))
            {
                return false;
            }

            const auto [selectionStart, selectionEnd] = GetTextInputSelectionRange(state);
            const DWORD startIndex = static_cast<DWORD>((std::min)(MapControlIndexToWin32EditIndex(state.text, selectionStart, state.multiline),
                                                                   static_cast<size_t>(std::numeric_limits<DWORD>::max())));
            const DWORD endIndex   = static_cast<DWORD>(
                (std::min)(MapControlIndexToWin32EditIndex(state.text, selectionEnd, state.multiline), static_cast<size_t>(std::numeric_limits<DWORD>::max())));
            if (wp != 0)
            {
                *reinterpret_cast<DWORD*>(wp) = startIndex;
            }
            if (lp != 0)
            {
                *reinterpret_cast<DWORD*>(lp) = endIndex;
            }

            outResult = MAKELRESULT(static_cast<WORD>(std::min<DWORD>(startIndex, std::numeric_limits<WORD>::max())),
                                    static_cast<WORD>(std::min<DWORD>(endIndex, std::numeric_limits<WORD>::max())));
            return true;
        }
        case EM_SETSEL:
        {
            TextInputState state;
            if (! editTarget->ExportTextInputState(state))
            {
                return false;
            }

            const std::wstring win32EditText = NormalizeWin32EditTextFromControlText(state.text, state.multiline);
            const auto mapSelectionIndex     = [&win32EditText, &state](uint64_t unsignedValue, LPARAM signedValue) noexcept
            {
                constexpr uint64_t kWin32EditEndSentinel = static_cast<uint64_t>(std::numeric_limits<UINT>::max());
                if (signedValue < 0 || unsignedValue == kWin32EditEndSentinel || unsignedValue == static_cast<uint64_t>((std::numeric_limits<WPARAM>::max)()))
                {
                    return state.text.size();
                }

                return MapWin32EditIndexToControlIndex(win32EditText, static_cast<size_t>(unsignedValue), state.multiline);
            };

            const size_t end    = mapSelectionIndex(static_cast<uint64_t>(lp), lp);
            const bool deselect = static_cast<LPARAM>(wp) < 0 || wp == static_cast<WPARAM>((std::numeric_limits<UINT>::max)());
            // EM_SETSEL preserves anchor/active-end orientation, even for reversed ranges. A -1 start deselects.
            SetTextInputSelectionRange(state, deselect ? end : mapSelectionIndex(static_cast<uint64_t>(wp), static_cast<LPARAM>(wp)), end);
            outResult = syncImportedState(state, false, false) ? TRUE : FALSE;
            return true;
        }
        case EM_REPLACESEL:
        {
            TextInputState state;
            if (! editTarget->ExportTextInputState(state))
            {
                return false;
            }
            // Password and read-only fields are not eligible text-edit targets. Reject before
            // inspecting the caller's replacement pointer or invoking model callbacks.
            if (state.masked || state.readOnly)
            {
                outResult = FALSE;
                return true;
            }

            const wchar_t* const replacementText     = (lp != 0) ? reinterpret_cast<const wchar_t*>(lp) : L"";
            const std::wstring normalizedReplacement = NormalizeControlTextFromWin32EditText(replacementText, state.multiline);
            if (! ReplaceTextInputSelection(state, normalizedReplacement))
            {
                outResult = FALSE;
                return true;
            }

            outResult = syncImportedState(state, true, true) ? TRUE : FALSE;
            return true;
        }
        case WM_COPY:
            SetInputModality(InputModality::Keyboard);
            controlHandled = editTarget->OnCopy(*this);
            break;
        case WM_CUT:
            SetInputModality(InputModality::Keyboard);
            controlHandled = editTarget->OnKeyDown(*this, 'X', MK_CONTROL);
            break;
        case WM_PASTE:
            SetInputModality(InputModality::Keyboard);
            controlHandled = editTarget->OnKeyDown(*this, 'V', MK_CONTROL);
            break;
        case WM_CLEAR:
        {
            SetInputModality(InputModality::Keyboard);
            TextInputState state;
            if (! editTarget->ExportTextInputState(state) || state.readOnly || ! HasTextSelection(state))
            {
                return false;
            }
            controlHandled = editTarget->OnKeyDown(*this, VK_DELETE, 0);
            break;
        }
        case WM_UNDO:
            SetInputModality(InputModality::Keyboard);
            controlHandled = editTarget->OnKeyDown(*this, 'Z', MK_CONTROL);
            break;
        default: return false;
    }

    if (controlHandled && ! editTargetLifetime.expired() && editTarget == _focusedControl && NativeTextInputControlBelongsToTree(_root.get(), editTarget))
    {
        SyncNativeTextInputSession(editTarget);
        RecordNativeTextInputKeyToStateMetric(inputStartedAt, L"native-edit-message", static_cast<uint64_t>(msg), 0u, msg != WM_COPY);
        outResult = (msg == WM_UNDO) ? TRUE : 0;
    }
    return controlHandled;
}
catch (const std::bad_alloc&)
{
    // Allocation failure cannot unwind through the window-message boundary.
    outResult = FALSE;
    return true;
}

bool ControlHost::HandleNativeTextInputImeMessage(UINT msg, WPARAM wp, LPARAM lp) noexcept
{
    static_cast<void>(wp);
    if (_textInputBackend != TextInputBackend::Native || ! _focusedControl || ! _focusedControl->SupportsTextInput())
    {
        return false;
    }
    if (_hwnd && (! IsWindowVisible(_hwnd) || _widthPx == 0u || _heightPx == 0u || GetFocus() != _hwnd))
    {
        DeactivateNativeTextInputSession();
        return true;
    }

    Control* const editTarget                   = _focusedControl;
    const std::weak_ptr<int> editTargetLifetime = editTarget->GetLifetimeToken();
    if (editTarget != _nativeTextInputControl)
    {
        ActivateNativeTextInputSession(editTarget);
    }
    const uint64_t editSessionRevision = _nativeTextInputSessionRevision;
    const auto editTargetSurvived      = [this, editTarget, &editTargetLifetime, editSessionRevision]() noexcept
    {
        return ! editTargetLifetime.expired() && editSessionRevision == _nativeTextInputSessionRevision && editTarget == _focusedControl &&
               editTarget == _nativeTextInputControl && NativeTextInputControlBelongsToTree(_root.get(), editTarget);
    };
    if (! editTargetSurvived())
    {
        return true;
    }
    if (! _nativeTextInputStateCacheValid)
    {
        SyncNativeTextInputSession(editTarget);
    }
    if (! editTargetSurvived())
    {
        return true;
    }
    if (! _nativeTextInputStateCacheValid)
    {
        return false;
    }

    SetInputModality(InputModality::Keyboard);
    const auto inputStartedAt = std::chrono::steady_clock::now();
    if (_nativeTextInputStateCache.masked)
    {
        // A field may become masked while an IMM composition is in flight. Drop its preview before handling any
        // further IME payload; password text is never an input-method candidate or composition buffer.
        DeactivateNativeTextInputTsf();
        if (! editTargetSurvived())
            return true;
        CancelNativeTextInputImeComposition();
        if (! editTargetSurvived())
            return true;
        UpdateNativeTextInputCaret();
        Debug::Perf::Emit(
            L"dxui.textinput.ime_update_us", L"masked-ime-lifecycle", Debug::Perf::ElapsedUs(inputStartedAt), 0u, static_cast<uint64_t>(msg), S_OK);
        return true;
    }
    if (_nativeTextInputStateCache.readOnly)
    {
        CancelNativeTextInputImeComposition();
        Debug::Perf::Emit(
            L"dxui.textinput.ime_update_us", L"native-ime-lifecycle", Debug::Perf::ElapsedUs(inputStartedAt), 0u, static_cast<uint64_t>(msg), S_OK);
        return true;
    }

    uint64_t expectedImeRevision     = _nativeTextInputImeRevision;
    const auto imeOperationIsCurrent = [this, &editTargetSurvived, &expectedImeRevision]() noexcept
    { return editTargetSurvived() && expectedImeRevision == _nativeTextInputImeRevision; };
    const auto beginImeOperation = [this, &expectedImeRevision]() noexcept
    {
        if (++_nativeTextInputImeRevision == 0u)
            ++_nativeTextInputImeRevision;
        expectedImeRevision = _nativeTextInputImeRevision;
    };

    auto startCompositionFromCurrentRange = [this, editTarget, &imeOperationIsCurrent, &beginImeOperation]()
    {
        if (! imeOperationIsCurrent())
        {
            return false;
        }
        beginImeOperation();
        _nativeTextInputImeCancelledUntilStart   = false;
        const NativeTextInputState previousState = _nativeTextInputStateCache;
        TextInputState baseState;
        const bool exported = editTarget->ExportTextInputState(baseState);
        if (! imeOperationIsCurrent())
            return false;
        if (exported)
        {
            _nativeTextInputImeBaseState   = baseState;
            _nativeTextInputImePreviewText = baseState.text;
        }
        else
        {
            _nativeTextInputImeBaseState.reset();
        }

        const TextInputState& rangeState                        = _nativeTextInputImeBaseState.has_value()
                                                                      ? _nativeTextInputImeBaseState.value()
                                                                      : TextInputState{
                                                                            .text                 = _nativeTextInputStateCache.text,
                                                                            .selectionAnchorIndex = _nativeTextInputStateCache.selectionAnchorIndex,
                                                                            .caretIndex           = _nativeTextInputStateCache.caretIndex,
                                                                            .firstVisibleLine     = _nativeTextInputStateCache.firstVisibleLine,
                                                                            .readOnly             = _nativeTextInputStateCache.readOnly,
                                                                            .masked               = _nativeTextInputStateCache.masked,
                                                                            .multiline            = _nativeTextInputStateCache.multiline,
                                                                        };
        const auto [compositionStartIndex, compositionEndIndex] = ResolveImeBaseCompositionRange(rangeState);

        _nativeTextInputImeComposing          = true;
        _nativeTextInputCompositionStartIndex = compositionStartIndex;
        _nativeTextInputCompositionEndIndex   = compositionEndIndex;
        _nativeTextInputConversionTargetStartIndex.reset();
        _nativeTextInputConversionTargetEndIndex.reset();
        _nativeTextInputCompositionCursorIndex.reset();
        _nativeTextInputCompositionClauseBoundaries.clear();
        ApplyNativeTextInputCompositionStateToCache();
        RaiseNativeTextInputAccessibilityEvents(previousState);
        return imeOperationIsCurrent();
    };

    auto ensureCompositionBaseState = [this, editTarget, &imeOperationIsCurrent]() -> bool
    {
        if (_nativeTextInputImeBaseState.has_value())
        {
            return true;
        }

        TextInputState baseState;
        if (! editTarget->ExportTextInputState(baseState) || ! imeOperationIsCurrent())
        {
            return false;
        }
        _nativeTextInputImeBaseState = baseState;
        return true;
    };

    auto updateConversionTargetFromAttributes =
        [this](size_t compositionStartIndex, size_t compositionLength, const NativeTextInputImePayload& payload) noexcept
    {
        _nativeTextInputConversionTargetStartIndex.reset();
        _nativeTextInputConversionTargetEndIndex.reset();

        const size_t attributeCount = std::min(payload.compositionAttributes.size(), compositionLength);
        std::optional<size_t> targetStart;
        size_t targetEnd = 0u;
        for (size_t index = 0u; index < attributeCount; ++index)
        {
            if (IsImeTargetAttribute(payload.compositionAttributes[index]))
            {
                if (! targetStart.has_value())
                {
                    targetStart = index;
                }
                targetEnd = index + 1u;
            }
            else if (targetStart.has_value())
            {
                break;
            }
        }

        if (targetStart.has_value())
        {
            _nativeTextInputConversionTargetStartIndex = compositionStartIndex + targetStart.value();
            _nativeTextInputConversionTargetEndIndex   = compositionStartIndex + targetEnd;
        }
    };

    auto applyCompositionPayload = [this, editTarget, &ensureCompositionBaseState, &updateConversionTargetFromAttributes, &imeOperationIsCurrent](
                                       const NativeTextInputImePayload& payload) -> bool
    {
        if (! payload.hasCompositionString || ! ensureCompositionBaseState())
        {
            return false;
        }
        TextInputState liveState;
        if (! editTarget->ExportTextInputState(liveState) || ! imeOperationIsCurrent())
            return false;
        if (_nativeTextInputImePreviewText && liveState.text != _nativeTextInputImePreviewText.value())
        {
            CancelNativeTextInputImeComposition();
            return false;
        }

        TextInputState previewState                             = _nativeTextInputImeBaseState.value();
        const auto [compositionStartIndex, compositionEndIndex] = ResolveImeBaseCompositionRange(previewState);
        ReplaceNativeTextInputRange(previewState, compositionStartIndex, compositionEndIndex, payload.compositionString);
        if (payload.hasCursorPosition)
        {
            previewState.caretIndex = compositionStartIndex + std::min(payload.cursorPosition, payload.compositionString.size());
        }

        std::wstring nextPreview = previewState.text;

        if (! editTarget->ImportTextInputState(*this, previewState, false) || ! imeOperationIsCurrent())
        {
            return false;
        }

        _nativeTextInputImePreviewText = std::move(nextPreview);

        SyncNativeTextInputSession(editTarget);
        if (! imeOperationIsCurrent())
        {
            return false;
        }
        const NativeTextInputState previousState = _nativeTextInputStateCache;
        _nativeTextInputImeComposing             = true;
        _nativeTextInputCompositionStartIndex    = compositionStartIndex;
        _nativeTextInputCompositionEndIndex      = compositionStartIndex + payload.compositionString.size();
        updateConversionTargetFromAttributes(compositionStartIndex, payload.compositionString.size(), payload);
        _nativeTextInputCompositionCursorIndex =
            payload.hasCursorPosition ? std::optional<size_t>{compositionStartIndex + std::min(payload.cursorPosition, payload.compositionString.size())}
                                      : std::nullopt;
        _nativeTextInputCompositionClauseBoundaries = ResolveImeClauseBoundaries(compositionStartIndex, payload.compositionString.size(), payload);
        ApplyNativeTextInputCompositionStateToCache();
        RaiseNativeTextInputAccessibilityEvents(previousState);
        if (! imeOperationIsCurrent())
        {
            return false;
        }
        UpdateNativeTextInputCaret();
        const uint64_t conversionTargetLength =
            _nativeTextInputConversionTargetStartIndex.has_value() && _nativeTextInputConversionTargetEndIndex.has_value()
                ? static_cast<uint64_t>(_nativeTextInputConversionTargetEndIndex.value() - _nativeTextInputConversionTargetStartIndex.value())
                : 0u;
        Debug::Perf::Emit(L"dxui.textinput.composition_length",
                          L"native-ime-composition",
                          0u,
                          static_cast<uint64_t>(payload.compositionString.size()),
                          conversionTargetLength,
                          S_OK);
        return true;
    };

    auto applyResultPayload = [this, editTarget, &ensureCompositionBaseState, &imeOperationIsCurrent](const NativeTextInputImePayload& payload) -> bool
    {
        if (! payload.hasResultString || ! ensureCompositionBaseState())
        {
            return false;
        }

        TextInputState liveState;
        if (! editTarget->ExportTextInputState(liveState) || ! imeOperationIsCurrent())
            return false;
        if (_nativeTextInputImePreviewText && liveState.text != _nativeTextInputImePreviewText.value())
        {
            CancelNativeTextInputImeComposition();
            return false;
        }

        TextInputState committedState                           = _nativeTextInputImeBaseState.value();
        const auto [compositionStartIndex, compositionEndIndex] = ResolveImeBaseCompositionRange(committedState);
        ReplaceNativeTextInputRange(committedState, compositionStartIndex, compositionEndIndex, payload.resultString);
        std::wstring nextPreview = committedState.text;
        // Preview imports do not create history. Restore the entry state before the single
        // notifying import so undo removes the complete composition, including an equal final preview.
        const TextInputState entryState = _nativeTextInputImeBaseState.value();
        if (! editTarget->ImportTextInputState(*this, entryState, false) || ! imeOperationIsCurrent())
            return false;
        // A notification can synchronize this legitimate commit or replace it. Publish its expected text before
        // the callback, including its committed cancellation base if the callback moves focus. Retain it only
        // while this operation still owns the composition and live document.
        _nativeTextInputImeBaseState   = committedState;
        _nativeTextInputImePreviewText = std::move(nextPreview);
        if (! editTarget->ImportTextInputState(*this, committedState, true) || ! imeOperationIsCurrent())
        {
            return false;
        }

        if (! editTarget->ExportTextInputState(liveState) || ! imeOperationIsCurrent())
            return false;
        if (liveState.text != committedState.text)
        {
            CancelNativeTextInputImeComposition();
            return false;
        }

        SyncNativeTextInputSession(editTarget);
        if (! imeOperationIsCurrent())
        {
            return false;
        }
        _nativeTextInputImeBaseState = std::move(liveState);
        Debug::Perf::Emit(L"dxui.textinput.composition_length", L"native-ime-result", 0u, static_cast<uint64_t>(payload.resultString.size()), 0u, S_OK);
        return true;
    };

    try
    {
        switch (msg)
        {
            case WM_IME_STARTCOMPOSITION:
                // Retire the preceding metadata before synchronizing this explicit START. A callback that begins
                // another composition during synchronization owns the newer revision and cannot be overwritten.
                ClearNativeTextInputCompositionState();
                expectedImeRevision = _nativeTextInputImeRevision;
                SyncNativeTextInputSession(editTarget);
                if (! imeOperationIsCurrent() || ! startCompositionFromCurrentRange())
                {
                    return true;
                }
                UpdateNativeTextInputImeWindows();
                break;
            case WM_IME_COMPOSITION:
            {
                constexpr LPARAM kCompositionPayloadMask = GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS;
                const bool hasResultPayload              = (lp & GCS_RESULTSTR) != 0;
                const bool hasCompositionPayload         = (lp & kCompositionPayloadMask) != 0;
                const NativeTextInputImePayload payload  = ReadNativeTextInputImePayload(lp);
                if (_nativeTextInputImeCancelledUntilStart || ! imeOperationIsCurrent())
                    break;
                beginImeOperation();
                if ((hasResultPayload && ! payload.hasResultString) || ((lp & GCS_COMPSTR) != 0 && ! payload.hasCompositionString))
                {
                    // A flagged string that could not be obtained is an aborted IME update.
                    // Do not turn an allocation/read failure into an empty-text commit.
                    CancelNativeTextInputImeComposition();
                    UpdateNativeTextInputCaret();
                    break;
                }
                if (! hasResultPayload && ! hasCompositionPayload)
                {
                    constexpr LPARAM kAnyGcsFlags = GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE |
                                                    GCS_CURSORPOS | GCS_DELTASTART | GCS_RESULTSTR | GCS_RESULTCLAUSE | GCS_RESULTREADSTR |
                                                    GCS_RESULTREADCLAUSE;
                    // No GCS flags signals cancellation. Read-only IME updates and CS_INSERTCHAR are separate
                    // message forms; they must not cancel an existing inline preview merely for lacking COMPSTR.
                    if (_nativeTextInputImeComposing && (lp & (kAnyGcsFlags | CS_INSERTCHAR)) == 0)
                        CancelNativeTextInputImeComposition();
                    else if (! _nativeTextInputImeComposing)
                        ClearNativeTextInputCompositionState();
                }
                else if (hasResultPayload && ! hasCompositionPayload)
                {
                    static_cast<void>(applyResultPayload(payload));
                    if (imeOperationIsCurrent())
                        ClearNativeTextInputCompositionState();
                }
                else
                {
                    if (! _nativeTextInputImeComposing)
                    {
                        SyncNativeTextInputSession(editTarget);
                        if (! imeOperationIsCurrent() || ! startCompositionFromCurrentRange())
                        {
                            return true;
                        }
                    }
                    else
                    {
                        ApplyNativeTextInputCompositionStateToCache();
                    }
                    if (hasResultPayload)
                    {
                        if (! applyResultPayload(payload))
                        {
                            if (imeOperationIsCurrent())
                                ClearNativeTextInputCompositionState();
                            break;
                        }
                        if (! imeOperationIsCurrent() || ! startCompositionFromCurrentRange())
                        {
                            return true;
                        }
                    }
                    if (hasCompositionPayload)
                    {
                        static_cast<void>(applyCompositionPayload(payload));
                        if (! imeOperationIsCurrent())
                        {
                            return true;
                        }
                    }
                    UpdateNativeTextInputImeWindows();
                }
                break;
            }
            case WM_IME_ENDCOMPOSITION:
                if (_nativeTextInputImeComposing)
                {
                    CancelNativeTextInputImeComposition();
                }
                else
                {
                    ClearNativeTextInputCompositionState();
                }
                break;
            default: return false;
        }
    }
    catch (const std::bad_alloc&)
    {
        if (imeOperationIsCurrent())
        {
            CancelNativeTextInputImeComposition();
            UpdateNativeTextInputCaret();
        }
        return true;
    }

    Debug::Perf::Emit(L"dxui.textinput.ime_update_us",
                      L"native-ime-lifecycle",
                      Debug::Perf::ElapsedUs(inputStartedAt),
                      _nativeTextInputImeComposing ? 1u : 0u,
                      static_cast<uint64_t>(msg),
                      S_OK);
    return true;
}

bool ControlHost::TryReadNativeTextInputState(const Control* control, NativeTextInputState& outState) const noexcept
{
    if (! control || control != _nativeTextInputControl || ! _nativeTextInputStateCacheValid)
    {
        return false;
    }

    try
    {
        outState = _nativeTextInputStateCache;
        return true;
    }
    catch (const std::bad_alloc&)
    {
        outState = {};
        return false;
    }
}

bool ControlHost::TryGetNativeTextInputCaretRects(D2D1_RECT_F& outRectDip, RECT& outClientRectPx, RECT& outScreenRectPx) const noexcept
{
    if (! _hwnd || ! _nativeTextInputControl || ! _nativeTextInputStateCacheValid)
    {
        return false;
    }

    const std::optional<D2D1_RECT_F> caretRectDip = _nativeTextInputControl->GetTextInputCaretRect(*this, _nativeTextInputStateCache.caretIndex);
    if (! caretRectDip.has_value())
    {
        return false;
    }

    outRectDip      = caretRectDip.value();
    outClientRectPx = NormalizeCaretRectPx(RECT{static_cast<LONG>(std::lround(DipsToPixels(outRectDip.left))),
                                                static_cast<LONG>(std::lround(DipsToPixels(outRectDip.top))),
                                                static_cast<LONG>(std::lround(DipsToPixels(outRectDip.right))),
                                                static_cast<LONG>(std::lround(DipsToPixels(outRectDip.bottom)))});

    outScreenRectPx = outClientRectPx;
    MapWindowPoints(_hwnd, nullptr, reinterpret_cast<POINT*>(&outScreenRectPx), 2);
    return true;
}

void ControlHost::UpdateNativeTextInputCaret() noexcept
{
    D2D1_RECT_F caretRectDip{};
    RECT caretClientRectPx{};
    RECT caretScreenRectPx{};
    if (! TryGetNativeTextInputCaretRects(caretRectDip, caretClientRectPx, caretScreenRectPx))
    {
        DestroyNativeTextInputCaret();
        return;
    }

    _nativeTextInputCaretRectDip      = caretRectDip;
    _nativeTextInputCaretClientRectPx = caretClientRectPx;
    _nativeTextInputCaretScreenRectPx = caretScreenRectPx;
    _nativeTextInputCaretRectValid    = true;
    ++_nativeTextInputEventCounters.caretUpdateCount;

    if (! _hwnd || GetFocus() != _hwnd)
    {
        return;
    }

    const int caretWidthPx  = std::max(1, static_cast<int>(caretClientRectPx.right - caretClientRectPx.left));
    const int caretHeightPx = std::max(1, static_cast<int>(caretClientRectPx.bottom - caretClientRectPx.top));
    const bool recreateCaret =
        ! _nativeTextInputCaret.IsCreated() || caretWidthPx != _nativeTextInputCaret.WidthPx() || caretHeightPx != _nativeTextInputCaret.HeightPx();

    if (recreateCaret && ! _nativeTextInputCaret.Create(_hwnd, caretWidthPx, caretHeightPx))
    {
        return;
    }

    static_cast<void>(SetCaretPos(caretClientRectPx.left, caretClientRectPx.top));
    static_cast<void>(_nativeTextInputCaret.Show());
    if (_nativeTextInputImeComposing)
    {
        UpdateNativeTextInputImeWindows();
    }
}

void ControlHost::DestroyNativeTextInputCaret() noexcept
{
    _nativeTextInputCaret.Reset();
    _nativeTextInputCaretRectValid    = false;
    _nativeTextInputCaretRectDip      = D2D1::RectF();
    _nativeTextInputCaretClientRectPx = RECT{};
    _nativeTextInputCaretScreenRectPx = RECT{};
}

#if DXUI_ENABLE_DIAGNOSTICS
bool ControlHost::DebugHasActiveNativeTextInputSession() const noexcept
{
    return HasActiveNativeTextInputSession();
}

bool ControlHost::DebugHasActiveNativeTextInputTsfDocument() const noexcept
{
    return _nativeTextInputTsfActive && _nativeTextInputTsfThreadMgr && _nativeTextInputTsfDocumentMgr && _nativeTextInputTsfContext &&
           _nativeTextInputTsfTextStore;
}

bool ControlHost::DebugGetNativeTextInputState(NativeTextInputState& outState) const noexcept
{
    if (! _nativeTextInputStateCacheValid)
    {
        return false;
    }

    outState = _nativeTextInputStateCache;
    return true;
}

bool ControlHost::DebugGetNativeTextInputCaretRect(D2D1_RECT_F& outRectDip, RECT& outScreenRectPx) const noexcept
{
    if (! _nativeTextInputCaretRectValid)
    {
        return false;
    }

    outRectDip      = _nativeTextInputCaretRectDip;
    outScreenRectPx = _nativeTextInputCaretScreenRectPx;
    return true;
}

NativeTextInputEventCounters ControlHost::DebugGetNativeTextInputEventCounters() const noexcept
{
    return _nativeTextInputEventCounters;
}

bool ControlHost::DebugPostActiveNativeTextStoreWorkForTest() noexcept
{
    return _nativeTextInputTsfActive && _nativeTextInputControl && ScheduleNativeTextStoreDeferredWork(_nativeTextInputControl, _nativeTextStoreDispatchCookie);
}

UINT_PTR ControlHost::DebugGetNativeTextStoreDispatchCookieForTest() const noexcept
{
    return _nativeTextStoreDispatchCookie;
}

bool ControlHost::DebugHasPostedNativeTextStoreWorkForTest() const noexcept
{
    return _nativeTextStoreNotificationPosted;
}

void ControlHost::DebugGetActiveNativeTextStoreNotificationCountsForTest(uint64_t& textChanges,
                                                                         uint64_t& selectionChanges,
                                                                         uint64_t& layoutChanges) const noexcept
{
    textChanges = selectionChanges = layoutChanges = 0u;
    wil::com_ptr_nothrow<ITextStoreACP> textStore;
    if (_nativeTextInputTsfTextStore && SUCCEEDED(_nativeTextInputTsfTextStore.query_to(textStore.put())) && textStore)
        DebugGetTextStoreNotificationCountsForTest(textStore.get(), textChanges, selectionChanges, layoutChanges);
}

HRESULT ControlHost::DebugAdviseNativeTextInputThreadMgrEventSinkForTest(ITfThreadMgrEventSink* sink, DWORD* cookie) noexcept
{
    if (! sink || ! cookie)
        return E_POINTER;
    *cookie = TF_INVALID_COOKIE;
    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    TfClientId clientId = 0u;
    if (! EnsureNativeTextInputThreadManager(threadMgr, clientId))
        return E_FAIL;
    static_cast<void>(clientId);
    wil::com_ptr_nothrow<ITfSource> source;
    const HRESULT queryResult = threadMgr.query_to(source.put());
    if (FAILED(queryResult) || ! source)
        return FAILED(queryResult) ? queryResult : E_NOINTERFACE;
    return source->AdviseSink(__uuidof(ITfThreadMgrEventSink), sink, cookie);
}

HRESULT ControlHost::DebugUnadviseNativeTextInputThreadMgrEventSinkForTest(DWORD cookie) noexcept
{
    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    TfClientId clientId = 0u;
    if (! EnsureNativeTextInputThreadManager(threadMgr, clientId))
        return E_FAIL;
    static_cast<void>(clientId);
    wil::com_ptr_nothrow<ITfSource> source;
    const HRESULT queryResult = threadMgr.query_to(source.put());
    if (FAILED(queryResult) || ! source)
        return FAILED(queryResult) ? queryResult : E_NOINTERFACE;
    return source->UnadviseSink(cookie);
}

HRESULT ControlHost::DebugCreateFocusedNativeTextInputDocumentForTest(IUnknown** outDocumentMgr) noexcept
{
    if (! outDocumentMgr)
        return E_POINTER;
    *outDocumentMgr = nullptr;
    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    TfClientId clientId = 0u;
    if (! EnsureNativeTextInputThreadManager(threadMgr, clientId))
        return E_FAIL;
    static_cast<void>(clientId);
    wil::com_ptr_nothrow<ITfDocumentMgr> documentMgr;
    const HRESULT createResult = threadMgr->CreateDocumentMgr(documentMgr.put());
    if (FAILED(createResult) || ! documentMgr)
        return FAILED(createResult) ? createResult : E_FAIL;
    const HRESULT focusResult = threadMgr->SetFocus(documentMgr.get());
    if (FAILED(focusResult))
        return focusResult;
    return documentMgr.query_to(outDocumentMgr);
}

HRESULT ControlHost::DebugGetFocusedNativeTextInputDocumentForTest(IUnknown** outDocumentMgr) noexcept
{
    if (! outDocumentMgr)
        return E_POINTER;
    *outDocumentMgr = nullptr;
    wil::com_ptr_nothrow<ITfThreadMgr> threadMgr;
    TfClientId clientId = 0u;
    if (! EnsureNativeTextInputThreadManager(threadMgr, clientId))
        return E_FAIL;
    static_cast<void>(clientId);
    wil::com_ptr_nothrow<ITfDocumentMgr> documentMgr;
    const HRESULT focusResult = threadMgr->GetFocus(documentMgr.put());
    if (FAILED(focusResult) || ! documentMgr)
        return FAILED(focusResult) ? focusResult : S_FALSE;
    return documentMgr.query_to(outDocumentMgr);
}

void ControlHost::DebugDeactivateNativeTextInputTsfForTest() noexcept
{
    DeactivateNativeTextInputTsf();
}

void ControlHost::DebugSetNativeTextInputImePayloadForTest(NativeTextInputImePayload payload)
{
    _debugNativeTextInputImePayload = std::move(payload);
}
#endif
} // namespace DxUi
