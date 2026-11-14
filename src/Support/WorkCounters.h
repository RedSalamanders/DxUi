#pragma once

// Private work counters (Specs/Core/Core_PerformanceAndResources.md, "Work counters"). One thread-local record of plain
// integer increments, present in every configuration and dormant until a test reads it: counting costs an increment, no
// allocation, lock or I/O. Nothing here is public API. Tests read the record through a WorkCounterScope delta.
//
// Adding a counter: add one X(group, name) line to its subsystem's list below, in alphabetical order (not at the end of
// the list), and increment it with ++Detail::Work().group.name where the work happens. Every counter is a uint64_t that
// only grows; snapshots, deltas, receipts and the per-name visitor pick it up from the list without further edits. A new
// subsystem adds its own list, struct and one line in WorkCounters and DXUI_WORK_COUNTER_FIELDS.

#include <cstddef>
#include <cstdint>
#include <span>
#include <unknwn.h>

// Frames and hosts: what one preparation, render or composition did.
#define DXUI_WORK_COUNTERS_FRAME(X)                                                                                                                            \
    X(frame, composites)           /* EmbeddedHost::Composite calls that drew the prepared surface. */                                                         \
    X(frame, compositionDevices)   /* DirectComposition devices a thread's shared native device stack created (DCompositionCreateDevice2). */                  \
    X(frame, d2dDevices)           /* Direct2D devices a thread's shared native device stack created (ID2D1Factory1::CreateDevice). */                         \
    X(frame, drawCalls)            /* Direct2D draw calls seen by a test executable's counting wrappers; 0 wherever none is installed. */                      \
    X(frame, paintExecutions)      /* Control::Paint calls dispatched by a host, Panel or PageHost: one per painted control. */                                \
    X(frame, pixelsPainted)        /* Device pixels inside the area a preparation or render cleared and repainted. */                                          \
    X(frame, preparations)         /* EmbeddedHost::Prepare calls that painted the surface. */                                                                 \
    X(frame, presents)             /* Native swap-chain Present or Present1 calls. */                                                                          \
    X(frame, renders)              /* Native WindowHost render passes (BeginDraw to EndDraw). */                                                               \
    X(frame, shadowPasses)         /* Shadow effect passes drawn (DrawShadowEffectPass): popups, cards and other elevated surfaces. */                         \
    X(frame, swapChains)           /* Swap chains native hosts created, for composition or for a window. */                                                    \
    X(frame, treeVisitsHitTest)    /* Controls a hit test examined: the root and each child a container offered to it. */                                      \
    X(frame, treeVisitsPaint)      /* Controls a paint walk examined, visible or not. */                                                                       \
    X(frame, treeVisitsStatistics) /* Controls an EmbeddedHost::GetStatistics retained-resource walk examined. */                                              \
    X(frame, treeVisitsTick)       /* Controls an animation tick walk examined, visible or not. */

// Wakes by source: each turn DxUi takes on the owner thread because something woke it.
#define DXUI_WORK_COUNTERS_WAKES(X)                                                                                                                            \
    X(wakes, animationTimer)  /* AnimationDispatcher WM_TIMER ticks. */                                                                                        \
    X(wakes, embeddedAdvance) /* EmbeddedHost::AdvanceAnimation calls the application made while animation was requested. */                                   \
    X(wakes, menuHoverTimer)  /* Menu submenu-hover WM_TIMER messages. */                                                                                      \
    X(wakes, paintMessage)    /* WindowHost WM_PAINT messages. */

// Text and typography beyond the resource kinds below.
#define DXUI_WORK_COUNTERS_TEXT(X)                                                                                                                             \
    X(text, formatCacheTrims) /* ControlHost::TrimCaches clears of the text-format cache above its key bound; each forces full-tree visual re-preparation. */  \
    X(text, setTextRenderingParams) /* ID2D1RenderTarget::SetTextRenderingParams calls a host made to bind or clear rendering parameters. */

// Popups and menus.
#define DXUI_WORK_COUNTERS_POPUPS(X)                                                                                                                           \
    X(popups, backdropBlurPasses)    /* Gaussian blur passes drawn over a popup's captured backdrop (Menu, ComboBox, transient surfaces). */                   \
    X(popups, backdropCapturePixels) /* Screen pixels copied by GDI backdrop captures. */                                                                      \
    X(popups, backdropCaptures)      /* Synchronous GDI BitBlt captures of the screen behind a popup. */                                                       \
    X(popups, comboBoxPopupRebuilds) /* ComboBox::RebuildPopupItems calls: one filter of the items by the editable text. */                                    \
    X(popups, menuPopupPaints)       /* MenuPopup content paints: one per render pass of a menu popup window. */                                               \
    X(popups, menuRowsPainted)       /* Menu rows those paints drew (each paint draws every row it finds visible). */

// Lists: Grid and Tree.
#define DXUI_WORK_COUNTERS_LISTS(X)

// Rich documents.
#define DXUI_WORK_COUNTERS_RICH(X)                                                                                                                             \
    X(rich, chunksLaid)          /* Rich text layout chunks laid out by a document-layout preparation (not reused). */                                         \
    X(rich, diagramPreparations) /* Native diagram paint objects begun by a semantic geometry preparation (not carried). */                                    \
    X(rich, displayUnitsWritten) /* UTF-16 display units a semantic document build published in its source/display map. */                                     \
    X(rich, documentBytesCopied) /* Bytes of a rich document's source or display text copied into a string: slices, flattens, text-input copies. */            \
    X(rich, documentFlattens)    /* Copies of a whole non-empty rich document's source or display text (a subset of documentBytesCopied). */                   \
    X(rich, elementsBuilt)       /* Semantic elements a semantic document build created. */                                                                    \
    X(rich, hoveredTitleCalls)   /* RichTextEditor::HoveredTitle resolutions: the hover title under a point, for the host's tooltip. */                        \
    X(rich, hoverScanElements)   /* Semantic elements a pointer-hover lookup tested one by one; the prepared hover index adds none. */                         \
    X(rich, imageUploadBytes)    /* Pixel bytes rich-image preparation uploaded into Direct2D bitmaps (creation with pixels or CopyFromMemory). */             \
    X(rich, mathPreparations)    /* Prepared math formulas built (beside the testRichMathPreparations seam). */                                                \
    X(rich, preparationTurns)    /* RichTextEditor::PrepareContent calls: owner turns of cooperative rich-content preparation. */                              \
    X(rich, svgRasterizations)   /* SVG strips rasterized into image bitmaps (beside the testRichSvgRasterizations seam). */                                   \
    X(rich, wholeLayoutsLaid)    /* Whole-document text layouts a document-layout preparation built (not reused). */

// UI Automation.
#define DXUI_WORK_COUNTERS_ACCESSIBILITY(X)                                                                                                                    \
    X(accessibility, snapshotBuilds)   /* Whole-window UI Automation snapshot builds that reached the tree walk. */                                            \
    X(accessibility, snapshotBytes)    /* Bytes reserved by those snapshots' per-control record arrays (excluding owned strings). */                           \
    X(accessibility, snapshotNodes)    /* Control records those snapshots hold. */                                                                             \
    X(accessibility, wordUnitsVisited) /* UTF-16 units text-range word movement stepped over, the enclosing-word scan from offset 0 included. */

// Memory. Reserved for counted memory work; byte metrics live in receipts, not here.
#define DXUI_WORK_COUNTERS_MEMORY(X)

// Every counter of every group except the resource kinds.
#define DXUI_WORK_COUNTER_FIELDS(X)                                                                                                                            \
    DXUI_WORK_COUNTERS_FRAME(X)                                                                                                                                \
    DXUI_WORK_COUNTERS_WAKES(X)                                                                                                                                \
    DXUI_WORK_COUNTERS_TEXT(X)                                                                                                                                 \
    DXUI_WORK_COUNTERS_POPUPS(X)                                                                                                                               \
    DXUI_WORK_COUNTERS_LISTS(X)                                                                                                                                \
    DXUI_WORK_COUNTERS_RICH(X)                                                                                                                                 \
    DXUI_WORK_COUNTERS_ACCESSIBILITY(X)                                                                                                                        \
    DXUI_WORK_COUNTERS_MEMORY(X)

// Native resources created through the counted helpers of Support/CountedCreation.h, one tally each. Every kind counts
// creations; destructions are observed only for objects created while a test's ResourceLifetimeTracker is installed. An
// implicit text layout (DrawText lays out its whole string on every call) is created and destroyed inside the call.
#define DXUI_WORK_RESOURCE_KINDS(X)                                                                                                                            \
    X(bitmap)                                                                                                                                                  \
    X(brush)                                                                                                                                                   \
    X(commandList)                                                                                                                                             \
    X(d3d11Device)                                                                                                                                             \
    X(deviceContext)                                                                                                                                           \
    X(effect)                                                                                                                                                  \
    X(geometry)                                                                                                                                                \
    X(gradientStops)                                                                                                                                           \
    X(implicitTextLayout)                                                                                                                                      \
    X(layer)                                                                                                                                                   \
    X(renderingParams)                                                                                                                                         \
    X(strokeStyle)                                                                                                                                             \
    X(textFormat)                                                                                                                                              \
    X(textLayout)

namespace DxUi::Detail
{
#define DXUI_WORK_DECLARE_FIELD(group, name) uint64_t name = 0u;
struct FrameWork final
{
    DXUI_WORK_COUNTERS_FRAME(DXUI_WORK_DECLARE_FIELD)
};
struct WakeWork final
{
    DXUI_WORK_COUNTERS_WAKES(DXUI_WORK_DECLARE_FIELD)
};
struct TextWork final
{
    DXUI_WORK_COUNTERS_TEXT(DXUI_WORK_DECLARE_FIELD)
};
struct PopupWork final
{
    DXUI_WORK_COUNTERS_POPUPS(DXUI_WORK_DECLARE_FIELD)
};
struct ListWork final
{
    DXUI_WORK_COUNTERS_LISTS(DXUI_WORK_DECLARE_FIELD)
};
struct RichWork final
{
    DXUI_WORK_COUNTERS_RICH(DXUI_WORK_DECLARE_FIELD)
};
struct AccessibilityWork final
{
    DXUI_WORK_COUNTERS_ACCESSIBILITY(DXUI_WORK_DECLARE_FIELD)
};
struct MemoryWork final
{
    DXUI_WORK_COUNTERS_MEMORY(DXUI_WORK_DECLARE_FIELD)
};
#undef DXUI_WORK_DECLARE_FIELD

struct ResourceTally final
{
    uint64_t created   = 0u;
    uint64_t destroyed = 0u;
};

enum class ResourceKind : uint8_t
{
#define DXUI_WORK_DECLARE_KIND(kind) kind,
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_DECLARE_KIND)
#undef DXUI_WORK_DECLARE_KIND
    Count,
};

struct ResourceWork final
{
#define DXUI_WORK_DECLARE_TALLY(kind) ResourceTally kind;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_DECLARE_TALLY)
#undef DXUI_WORK_DECLARE_TALLY
};

struct WorkCounters final
{
    FrameWork frame;
    WakeWork wakes;
    ResourceWork resources;
    TextWork text;
    PopupWork popups;
    ListWork lists;
    RichWork rich;
    AccessibilityWork accessibility;
    MemoryWork memory;
};

// The calling thread's record. Constant-initialized, so reading it never runs a thread-local constructor.
inline thread_local constinit WorkCounters g_workCounters{};

[[nodiscard]] inline WorkCounters& Work() noexcept
{
    return g_workCounters;
}

[[nodiscard]] constexpr ResourceTally& Tally(ResourceWork& resources, ResourceKind kind) noexcept
{
    switch (kind)
    {
#define DXUI_WORK_TALLY_CASE(name)                                                                                                                             \
    case ResourceKind::name: return resources.name;
        DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_TALLY_CASE)
#undef DXUI_WORK_TALLY_CASE
        case ResourceKind::Count:
        default: break;
    }
    return resources.textLayout;
}

[[nodiscard]] constexpr const ResourceTally& Tally(const ResourceWork& resources, ResourceKind kind) noexcept
{
    return Tally(const_cast<ResourceWork&>(resources), kind);
}

// Calls visit(group, name, value) for every counter in declaration order; a resource kind reports "<kind>.created" and
// "<kind>.destroyed" in the "resources" group. Names are string literals.
template <typename Visitor> void ForEachWorkCounter(const WorkCounters& counters, Visitor&& visit)
{
#define DXUI_WORK_VISIT_FIELD(group, name) visit(#group, #name, counters.group.name);
    DXUI_WORK_COUNTERS_FRAME(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_WAKES(DXUI_WORK_VISIT_FIELD)
#define DXUI_WORK_VISIT_KIND(kind)                                                                                                                             \
    visit("resources", #kind ".created", counters.resources.kind.created);                                                                                     \
    visit("resources", #kind ".destroyed", counters.resources.kind.destroyed);
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_VISIT_KIND)
#undef DXUI_WORK_VISIT_KIND
    DXUI_WORK_COUNTERS_TEXT(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_POPUPS(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_LISTS(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_RICH(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_ACCESSIBILITY(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_MEMORY(DXUI_WORK_VISIT_FIELD)
#undef DXUI_WORK_VISIT_FIELD
}

// The work done between two readings of the same thread's record: later minus earlier, counter by counter.
[[nodiscard]] constexpr WorkCounters Subtract(const WorkCounters& later, const WorkCounters& earlier) noexcept
{
    WorkCounters delta{};
#define DXUI_WORK_SUBTRACT_FIELD(group, name) delta.group.name = later.group.name - earlier.group.name;
    DXUI_WORK_COUNTER_FIELDS(DXUI_WORK_SUBTRACT_FIELD)
#undef DXUI_WORK_SUBTRACT_FIELD
#define DXUI_WORK_SUBTRACT_KIND(kind)                                                                                                                          \
    delta.resources.kind.created   = later.resources.kind.created - earlier.resources.kind.created;                                                            \
    delta.resources.kind.destroyed = later.resources.kind.destroyed - earlier.resources.kind.destroyed;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_SUBTRACT_KIND)
#undef DXUI_WORK_SUBTRACT_KIND
    return delta;
}

// Resources created, over every kind.
[[nodiscard]] constexpr uint64_t TotalCreated(const WorkCounters& counters) noexcept
{
    uint64_t total = 0u;
#define DXUI_WORK_ADD_CREATED(kind) total += counters.resources.kind.created;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_ADD_CREATED)
#undef DXUI_WORK_ADD_CREATED
    return total;
}

// Churn of a delta: per kind, the objects both created and destroyed in it (min(created, destroyed)), summed over kinds.
// Meaningful only while a ResourceLifetimeTracker observes destructions.
[[nodiscard]] constexpr uint64_t Churn(const WorkCounters& delta) noexcept
{
    uint64_t total = 0u;
#define DXUI_WORK_ADD_CHURN(kind)                                                                                                                              \
    total += delta.resources.kind.created < delta.resources.kind.destroyed ? delta.resources.kind.created : delta.resources.kind.destroyed;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_ADD_CHURN)
#undef DXUI_WORK_ADD_CHURN
    return total;
}

// A test's view of the work one action does on this thread: construct it, run the action, read Delta().
class WorkCounterScope final
{
public:
    WorkCounterScope() noexcept : _start(Work())
    {
    }
    [[nodiscard]] WorkCounters Delta() const noexcept
    {
        return Subtract(Work(), _start);
    }
    void Restart() noexcept
    {
        _start = Work();
    }

private:
    WorkCounters _start;
};

struct TrackedResource final
{
    IUnknown* object  = nullptr;
    ResourceKind kind = ResourceKind::Count;
};

class ResourceLifetimeTracker;
inline thread_local constinit ResourceLifetimeTracker* g_resourceLifetimeTracker = nullptr;

// Test-only observation of destructions. While installed on a thread, it holds one extra reference to every resource the
// counted helpers create on that thread, in storage the test supplies (no allocation). Sweep() drops each held reference:
// an object whose count reaches zero was released by its owners since creation and is counted destroyed; any other keeps
// its reference. Destruction therefore happens at the sweep rather than when the owner releases it; nothing else changes.
// Objects beyond the storage are counted as overflow and not tracked. Destroying the tracker drops every held reference
// without counting and restores the previous tracker.
class ResourceLifetimeTracker final
{
public:
    explicit ResourceLifetimeTracker(std::span<TrackedResource> storage) noexcept : _storage(storage), _previous(g_resourceLifetimeTracker)
    {
        g_resourceLifetimeTracker = this;
    }
    ResourceLifetimeTracker(const ResourceLifetimeTracker&)            = delete;
    ResourceLifetimeTracker& operator=(const ResourceLifetimeTracker&) = delete;
    ResourceLifetimeTracker(ResourceLifetimeTracker&&)                 = delete;
    ResourceLifetimeTracker& operator=(ResourceLifetimeTracker&&)      = delete;
    ~ResourceLifetimeTracker()
    {
        if (g_resourceLifetimeTracker == this)
            g_resourceLifetimeTracker = _previous;
        for (size_t index = 0u; index < _held; ++index)
            _storage[index].object->Release();
        _held = 0u;
    }

    void Track(ResourceKind kind, IUnknown* object) noexcept
    {
        if (! object)
            return;
        if (_held == _storage.size())
        {
            ++_overflow;
            return;
        }
        object->AddRef();
        _storage[_held++] = TrackedResource{object, kind};
    }

    // Counts and releases every tracked object its owners have released; returns how many were destroyed.
    size_t Sweep() noexcept
    {
        size_t kept      = 0u;
        size_t destroyed = 0u;
        for (size_t index = 0u; index < _held; ++index)
        {
            const TrackedResource entry = _storage[index];
            if (entry.object->Release() == 0u)
            {
                ++Tally(Work().resources, entry.kind).destroyed;
                ++destroyed;
                continue;
            }
            entry.object->AddRef();
            _storage[kept++] = entry;
        }
        _held = kept;
        return destroyed;
    }

    [[nodiscard]] size_t Held() const noexcept
    {
        return _held;
    }
    [[nodiscard]] uint64_t Overflow() const noexcept
    {
        return _overflow;
    }

private:
    std::span<TrackedResource> _storage;
    ResourceLifetimeTracker* _previous = nullptr;
    size_t _held                       = 0u;
    uint64_t _overflow                 = 0u;
};

// A host or container dispatched Paint to one control it had already found visible.
inline void NotePaintDispatch() noexcept
{
    auto& frame = Work().frame;
    ++frame.treeVisitsPaint;
    ++frame.paintExecutions;
}

// Counts one created resource and, under a tracker, holds it for destruction counting.
inline void NoteResourceCreated(ResourceKind kind, IUnknown* object) noexcept
{
    if (! object)
        return;
    ++Tally(Work().resources, kind).created;
    if (ResourceLifetimeTracker* const tracker = g_resourceLifetimeTracker)
        tracker->Track(kind, object);
}

// One implicit text layout, created and released inside a single DrawText call.
inline void NoteImplicitTextLayout() noexcept
{
    auto& tally = Work().resources.implicitTextLayout;
    ++tally.created;
    ++tally.destroyed;
}
} // namespace DxUi::Detail
