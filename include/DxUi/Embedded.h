#pragma once
#include "DxUi.h"
#include "EmbeddedAccessibility.h"

namespace DxUi
{
class EmbeddedHost;
// One caller-created pool per D3D11 device generation, shared by embedded views. All methods are UI-thread affine.
// The pool retains COM references; the application owns device creation, immediate-context scheduling and presentation.
class GraphicsDevice final
{
public:
    static HRESULT Create(ID3D11Device* device, std::shared_ptr<GraphicsDevice>& result) noexcept;
    ~GraphicsDevice();
    GraphicsDevice(const GraphicsDevice&)            = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;
    [[nodiscard]] ID3D11Device* GetDevice() const noexcept;

private:
    GraphicsDevice();
    struct State;
    std::unique_ptr<State> _state;
    friend class EmbeddedHost;
};

struct EmbeddedCallbacks
{
    void* context = nullptr;
    // Borrowed until detach; invoked synchronously on the UI thread and must not re-enter the host.
    void (*requestPreparation)(void* context) noexcept = nullptr;
};
enum class PointerAction : uint8_t
{
    Move,
    Down,
    Up,
    Wheel,
    Leave,
    Cancel
};
struct PointerEvent
{
    PointerAction action = PointerAction::Move;
    float xPixels        = 0;
    float yPixels        = 0;
    UINT modifiers       = 0;
    float wheelDelta     = 0;
    // The contact's device; a touch drag shows touch feedback (PointerDeviceFromMessageExtraInfo for mouse messages).
    PointerDevice device = PointerDevice::Mouse;
};
// Owned snapshot for an application-side text service; never pass this C++ record across a plugin ABI.
// The revision belongs to this EmbeddedHost instance and is invalidated by any view change.
struct EmbeddedTextInputSnapshot
{
    uint64_t revision = 0;
    // Stable during edits to one focus session; changes even if focus leaves and returns between reads.
    uint64_t focusId = 0;
    NativeTextInputState state;
    std::optional<D2D1_RECT_F> caretBoundsDip;
    std::optional<D2D1_RECT_F> viewportBoundsDip;
};
// One-use correlation token for a translated WM_CHAR produced by a Return/Space keydown. Keep the token with that
// specific key event and pass it only to its corresponding character message. A key-up does not consume the token.
// If translation produces no character, keep or discard the token explicitly; standalone text input uses the
// uncorrelated DispatchCharacter overload and is never suppressed by a pending token. Nested key dispatches must use
// distinct receipt objects; overlapping reuse of one receipt invalidates that association when the outer dispatch ends.
class EmbeddedKeyDispatchReceipt final
{
public:
    EmbeddedKeyDispatchReceipt() noexcept                                    = default;
    EmbeddedKeyDispatchReceipt(const EmbeddedKeyDispatchReceipt&)            = delete;
    EmbeddedKeyDispatchReceipt& operator=(const EmbeddedKeyDispatchReceipt&) = delete;
    EmbeddedKeyDispatchReceipt(EmbeddedKeyDispatchReceipt&& other) noexcept;
    EmbeddedKeyDispatchReceipt& operator=(EmbeddedKeyDispatchReceipt&& other) noexcept;

private:
    void Reset() noexcept;
    friend class EmbeddedHost;
    const EmbeddedHost* _embeddedHost = nullptr;
    const ControlHost* _controlHost   = nullptr;
    Control* _owner                   = nullptr;
    Control* _root                    = nullptr;
    std::weak_ptr<int> _ownerLifetime;
    std::weak_ptr<int> _rootLifetime;
    uint64_t _attachmentRevision = 0u;
    uint64_t _focusOwnerEpoch    = 0u;
    uint64_t _sequence           = 0u;
    UINT _virtualKey             = 0u;
    bool _ownerSupportsTextInput = false;
    bool _allowCharacter         = false;
    bool _valid                  = false;
};
enum class EmbeddedTextInputAction : uint8_t
{
    Preview,
    Commit,
    Cancel
};
struct EmbeddedStatistics
{
    uint64_t preparations = 0;
    uint64_t composites   = 0;
    // Bytes of the cached surface. A hidden or zero-extent view holds no surface and reports 0.
    uint64_t surfaceBytes         = 0;
    uint64_t replacementPeakBytes = 0;
    uint64_t surfaceAllocations   = 0;
    // Current sizes of the bounded per-view caches (ControlHost::kSolidBrushCacheLimit and
    // kConfiguredTextFormatCacheLimit); a cache beyond its bound is cleared at the next preparation start.
    uint64_t cachedBrushes     = 0;
    uint64_t cachedTextFormats = 0;
};

// One retained view and cached surface. Applications with two density variants create two views with one shared pool
// and bind both views to the same application model. No rendering HWND, swap chain, render worker, timer or device is created here.
// Optional accessibility notifications use one bounded event-driven delivery work item; preparation remains owner-driven.
class EmbeddedHost final
{
public:
    EmbeddedHost();
    ~EmbeddedHost();
    EmbeddedHost(const EmbeddedHost&)            = delete;
    EmbeddedHost& operator=(const EmbeddedHost&) = delete;
    HRESULT Attach(std::shared_ptr<GraphicsDevice> graphics, EmbeddedCallbacks callbacks = {}) noexcept;
    void Detach() noexcept;
    // Device replacement preserves the root/model; capture is canceled and all device resources are rebuilt.
    HRESULT ReplaceDevice(std::shared_ptr<GraphicsDevice> graphics) noexcept;
    [[nodiscard]] ControlHost& Controls() noexcept
    {
        return _host;
    }
    [[nodiscard]] const ControlHost& Controls() const noexcept
    {
        return _host;
    }
    void SetVisible(bool visible) noexcept;
    void MarkDirty() noexcept;
    [[nodiscard]] bool NeedsPreparation() const noexcept;
    [[nodiscard]] bool NeedsAnimation() const noexcept;
    // Application milliseconds, from any epoch. The view's animation clock (ControlHost::GetAnimationTickMs) advances by the
    // difference between consecutive ticks; the first tick after a pause in ticking (nothing requested, hidden, zero-sized,
    // a new attachment) only sets the baseline, and a tick that goes back adds nothing. Call only while NeedsAnimation().
    bool AdvanceAnimation(uint64_t nowTickMs) noexcept;
    // Physical pixel size and DPI. Text shaping and raster work happen only here, never in Composite.
    // S_FALSE means hidden, zero-sized, already prepared, or cooperative content preparation remains pending;
    // in the last case NeedsPreparation() stays true and the caller should prepare on a later owner turn, not spin.
    // A failed HRESULT disables interaction until preparation succeeds.
    // E_ABORT means a callback superseded the frame (root or tree changed mid-pass). The view already requested
    // another preparation; prepare again when asked instead of presenting the retired frame.
    HRESULT Prepare(UINT widthPixels, UINT heightPixels, float dpi = 96.0f) noexcept;
    // After the caller finishes arranging live control bounds, acknowledge interaction changes without painting.
    // Only the last successfully painted extent/DPI is accepted. Initial/failed preparation, resize, hide and
    // device replacement still require Prepare. S_FALSE means unchanged or unavailable; dispatch stays disabled
    // when unavailable. Does not publish accessibility/text geometry or change surface/preparation statistics.
    HRESULT PrepareInteraction(UINT widthPixels, UINT heightPixels, float dpi = 96.0f) noexcept;
    // Host binds its render target. This binds all other required pipeline state and issues one premultiplied-alpha draw.
    // Context must be the immediate context of the supplied device. No allocation, rasterization or readback occurs.
    HRESULT Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) noexcept;
    // Prepare, or arrange and PrepareInteraction, before a gesture after interaction geometry changes. Captured continuation remains live
    // until geometry/tree/availability changes, which cancels capture and requires preparation.
    bool DispatchPointer(const PointerEvent& event) noexcept;
    bool DispatchKey(UINT virtualKey, bool down, UINT modifiers = 0) noexcept;
    bool DispatchCharacter(wchar_t character, UINT modifiers = 0) noexcept;
    // Correlate a Return/Space keydown with its exact translated character. Keep a separate receipt for each keydown
    // that may translate, including repeats; key-up uses the original overload. Only matching Return/Space characters
    // consume a receipt. Foreign or mismatched attempts return false without consuming it; stale matching characters
    // return true to consume without retargeting. The old overloads retain their independent-dispatch behavior.
    bool DispatchKey(UINT virtualKey, bool down, UINT modifiers, EmbeddedKeyDispatchReceipt& translatedCharacterReceipt) noexcept;
    bool DispatchCharacter(wchar_t character, UINT modifiers, EmbeddedKeyDispatchReceipt& translatedCharacterReceipt) noexcept;
    // Non-consuming permission query for raw message pumps: after DispatchKey(keydown, ..., receipt), translate the
    // OS message only when this returns true. The emitted WM_CHAR then uses the bare DispatchCharacter overload.
    [[nodiscard]] bool ShouldTranslateCharacter(const EmbeddedKeyDispatchReceipt& receipt) noexcept;
    // S_FALSE means no available focused text control. Bounds are view-local DIPs, not screen pixels.
    HRESULT ReadTextInput(EmbeddedTextInputSnapshot& snapshot) noexcept;
    // Optional application-side clipboard transform. Checks the current focus
    // session/revision; prepares source only and never commits or takes focus.
    HRESULT TransformClipboardPaste(uint64_t revision, uint64_t focusId, const TextClipboardContent& content, std::wstring& replacement) noexcept;
    // Read a fresh snapshot after every accepted change. A stale revision returns ERROR_REVISION_MISMATCH.
    // Preview changes displayed text/composition without notifying the model. Commit notifies once relative
    // to the pre-composition text. Cancel restores that text without notification and ignores the supplied state.
    // Text is bounded to 65,536 UTF-16 units and clauses to 256. Ranges must fit; control policy is never imported.
    HRESULT ApplyTextInput(uint64_t revision, const NativeTextInputState& state, EmbeddedTextInputAction action) noexcept;
    // Revision-checked geometry in view-local DIPs. S_FALSE means hidden/unprepared layout; outputs clear.
    HRESULT HitTestTextInput(uint64_t revision, D2D1_POINT_2F point, size_t& index) noexcept;
    HRESULT GetTextInputRangeBounds(uint64_t revision, size_t start, size_t end, D2D1_RECT_F& bounds, bool& clipped) noexcept;
    // Cancel composition and release logical text focus on OS focus loss; no HWND/OS service is owned here.
    void CancelTextInput() noexcept;
    // Lazy UIA attachment on the caller's COM STA, after preparation. runtimeId is nonzero and process-unique
    // for this attachment. Uses the existing control patterns; no HWND, worker, timer or graphics work is created.
    HRESULT AttachAccessibility(std::shared_ptr<EmbeddedAccessibilitySite> site, uint64_t runtimeId, const EmbeddedAccessibilityPlacement& placement) noexcept;
    HRESULT UpdateAccessibility(const EmbeddedAccessibilityPlacement& placement) noexcept;
    HRESULT GetAccessibilityProvider(IRawElementProviderFragmentRoot** result) noexcept;
    // Required before unloading a provider's module. Detach, hide and device replacement call this automatically.
    // The application keeps that module mapped while any external provider references can survive.
    void DisconnectAccessibility() noexcept;
    [[nodiscard]] EmbeddedStatistics GetStatistics() const noexcept;

private:
    struct State;
    std::unique_ptr<State> _state;
    ControlHost _host;
    bool _detaching       = false;
    bool _replacingDevice = false;
    // Attachment identity protects State lifetime; operation identity also rejects stale work after hide/device changes.
    uint64_t _attachmentRevision = 0u;
    uint64_t _operationRevision  = 0u;
    // The attachment epoch rejects reentrant detach/reattach while text revisions continue independently.
    // Ordinary invalidation adds no new hot-path work; detach advances the epoch before a new tree is attached.
    uint64_t _textInputRevision   = 1;
    uint64_t _keyDispatchSequence = 0u;
    static void InvalidateThunk(void* context) noexcept;
    bool DispatchKeyImpl(UINT virtualKey, bool down, UINT modifiers, EmbeddedKeyDispatchReceipt* receipt) noexcept;
    void RequestPreparation() noexcept;
    void CancelPointer(bool clearPendingDoubleClick = true) noexcept;
    // A drag may continue across a bounds revision while its control is still in the tree and it and every ancestor
    // are enabled and visible. Hiding or disabling it or an ancestor, removing it, or resizing the view still cancels.
    [[nodiscard]] bool CapturedDragContinues() const noexcept;
    // Drops the cached surface and its D2D target while hidden or zero-sized; the next sized visible
    // preparation allocates exactly one replacement.
    void ReleaseSurface() noexcept;
    [[nodiscard]] bool InputIsCoherent(bool allowDirty) noexcept;
    [[nodiscard]] bool TextInputIsAvailable() noexcept;
    // Keys and characters reach the focused control, which PruneStaleInteractionState keeps current.
    // They hit-test nothing, so geometry the next preparation will move cannot make them stale: a hidden, suspended or
    // never-prepared view, or one whose input a failed callback suspended until the next preparation, refuses them, a
    // changed interaction revision does not. Pending rich preparation retains TextInputIsAvailable's source-only
    // permission and revision checks. Snapshot export and text-service actions keep their stricter coherence boundary.
    [[nodiscard]] bool KeyboardInputIsAvailable() noexcept;
};
} // namespace DxUi
