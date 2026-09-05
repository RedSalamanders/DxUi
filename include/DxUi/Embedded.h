#pragma once
#include "DxUi.h"

namespace DxUi
{
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
};
// Owned snapshot for an application-side text service; never pass this C++ record across a plugin ABI.
// The revision belongs to this EmbeddedHost instance and is invalidated by any view change.
struct EmbeddedTextInputSnapshot
{
    uint64_t revision = 0;
    NativeTextInputState state;
    std::optional<D2D1_RECT_F> caretBoundsDip;
    std::optional<D2D1_RECT_F> viewportBoundsDip;
};
enum class EmbeddedTextInputAction : uint8_t
{
    Preview,
    Commit,
    Cancel
};
struct EmbeddedStatistics
{
    uint64_t preparations         = 0;
    uint64_t composites           = 0;
    uint64_t surfaceBytes         = 0;
    uint64_t replacementPeakBytes = 0;
    uint64_t surfaceAllocations   = 0;
};

// One retained view and cached surface. Applications with two density variants create two views with one shared pool
// and bind both views to the same application model. No HWND, swap chain, worker, timer or device is created here.
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
    bool AdvanceAnimation(uint64_t nowTickMs) noexcept;
    // Physical pixel size and DPI. Changed text/layout/raster work happens only here, never in Composite.
    // S_FALSE means hidden, zero-sized or already prepared; failure disables interaction until preparation succeeds.
    HRESULT Prepare(UINT widthPixels, UINT heightPixels, float dpi = 96.0f) noexcept;
    // Host binds its render target. This binds all other required pipeline state and issues one premultiplied-alpha draw.
    // Context must be the immediate context of the supplied device. No allocation, rasterization or readback occurs.
    HRESULT Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) noexcept;
    // Prepare dirty content before starting a new hit-tested gesture. Captured continuation remains live
    // until geometry/tree/availability changes, which cancels capture and requires preparation.
    bool DispatchPointer(const PointerEvent& event) noexcept;
    bool DispatchKey(UINT virtualKey, bool down, UINT modifiers = 0) noexcept;
    bool DispatchCharacter(wchar_t character, UINT modifiers = 0) noexcept;
    // S_FALSE means no available focused text control. Bounds are view-local DIPs, not screen pixels.
    HRESULT ReadTextInput(EmbeddedTextInputSnapshot& snapshot) noexcept;
    // Read a fresh snapshot after every accepted change. A stale revision returns ERROR_REVISION_MISMATCH.
    // Preview changes displayed text/composition without notifying the model. Commit notifies once relative
    // to the pre-composition text. Cancel restores that text without notification and ignores the supplied state.
    // Text is bounded to 65,536 UTF-16 units and clauses to 256. Ranges must fit; control policy is never imported.
    HRESULT ApplyTextInput(uint64_t revision, const NativeTextInputState& state, EmbeddedTextInputAction action) noexcept;
    // Cancel composition and release logical text focus on OS focus loss; no HWND/OS service is owned here.
    void CancelTextInput() noexcept;
    [[nodiscard]] EmbeddedStatistics GetStatistics() const noexcept;

private:
    struct State;
    std::unique_ptr<State> _state;
    ControlHost _host;
    // Attachment epoch plus the existing dirty revision forms the text token. Ordinary invalidation adds no
    // new hot-path work; the epoch advances at detach so old services cannot address a new tree.
    uint64_t _textInputRevision = 1;
    static void InvalidateThunk(void* context) noexcept;
    void CancelPointer() noexcept;
    [[nodiscard]] bool InputIsCoherent(bool allowDirty) noexcept;
};
} // namespace DxUi
