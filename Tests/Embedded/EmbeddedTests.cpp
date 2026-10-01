// Compile the public accessibility header first: consumers must not supply its COM prerequisites.
#include <DxUi/EmbeddedAccessibility.h>

#include "../../Samples/EmbeddedControls/EmbeddedScene.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../../src/Support/PostedPayload.h"
#include "../Support/FailureReports.h"
#include <DxUi/ControlCatalog.h>
#include <DxUi/Diagnostics.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <new>

// Counts library C++ heap calls within the deliberately isolated composition loop, not driver allocations. The bytes they ask
// for are counted beside them, for the opt-in measurement of GridSelectionBenchmark.h. That measurement also asks what this
// thread's heap holds at a moment: while countLiveBytes is set, liveBytes is the sizes of the blocks this thread has allocated
// less those it has freed (the plain forms only, which every standard container uses), read as a difference between two points.
static thread_local bool countAllocations  = false;
static thread_local size_t allocations     = 0;
static thread_local size_t allocationBytes = 0;
static thread_local bool countLiveBytes    = false;
static thread_local ptrdiff_t liveBytes    = 0;
void* operator new(size_t bytes)
{
    if (countAllocations)
    {
        ++allocations;
        allocationBytes += bytes;
    }
    if (auto* p = malloc(bytes ? bytes : 1))
    {
        if (countLiveBytes)
            liveBytes += static_cast<ptrdiff_t>(_msize(p));
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](size_t bytes)
{
    return ::operator new(bytes);
}
static void FreeBlock(void* p) noexcept
{
    if (p && countLiveBytes)
        liveBytes -= static_cast<ptrdiff_t>(_msize(p));
    free(p);
}
void operator delete(void* p) noexcept
{
    FreeBlock(p);
}
void operator delete[](void* p) noexcept
{
    FreeBlock(p);
}
void operator delete(void* p, size_t) noexcept
{
    FreeBlock(p);
}
void operator delete[](void* p, size_t) noexcept
{
    FreeBlock(p);
}
void* operator new(size_t bytes, std::align_val_t alignment)
{
    if (countAllocations)
    {
        ++allocations;
        allocationBytes += bytes;
    }
    if (auto* p = _aligned_malloc(bytes ? bytes : 1, static_cast<size_t>(alignment)))
        return p;
    throw std::bad_alloc();
}
void* operator new[](size_t b, std::align_val_t a)
{
    return ::operator new(b, a);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete(void* p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void* p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
static size_t checks = 0;
static void Check(bool value, const char* text)
{
    ++checks;
    if (! value)
    {
        std::cerr << "FAIL: " << text << '\n';
        std::exit(1);
    }
}
static void Hr(HRESULT hr, const char* text)
{
    if (FAILED(hr))
        std::cerr << "HRESULT: " << std::hex << hr << std::dec << '\n';
    Check(SUCCEEDED(hr), text);
}
#include "ComplexUiBenchmark.h"
#include "EmbeddedAccessibilityTests.h"
#include "EmbeddedTextInputTests.h"
#include "EmbeddedUiaTests.h"
#include "GridSelectionBenchmark.h"
#include "LocalizedLayoutTests.h"

// Hidden and zero-extent views hold no surface; the next visible sized preparation reallocates exactly one and
// reproduces the previous pixels. Device replacement while hidden keeps working without a surface.
__declspec(noinline) static void TestSurfaceLifetime(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    size_t requests = 0;
    Hr(scene.Initialize(gpu.device.get(), {&requests, [](void* p) noexcept { ++*static_cast<size_t*>(p); }}), "surface lifetime scene");
    Hr(scene.view.Prepare(480, 240), "prepare surface lifetime scene");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite before hide");
    std::vector<uint8_t> shown;
    Hr(gpu.Read(shown), "read pixels before hide");
    const auto visible = scene.view.GetStatistics();
    Check(visible.surfaceBytes == 480ull * 240 * 4 && visible.surfaceAllocations == 1, "visible view holds exactly one surface");
    const size_t requestsBeforeHide = requests;
    scene.view.SetVisible(false);
    const auto hidden = scene.view.GetStatistics();
    Check(hidden.surfaceBytes == 0 && hidden.surfaceAllocations == visible.surfaceAllocations, "hiding releases the surface without allocating");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation() && requests == requestsBeforeHide,
          "hidden view requests no preparation or animation");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "hidden view composites nothing");
    Check(scene.view.Prepare(480, 240) == S_FALSE && scene.view.GetStatistics().surfaceBytes == 0, "hidden preparation allocates no surface");
    scene.view.MarkDirty();
    Check(! scene.view.NeedsPreparation() && requests == requestsBeforeHide, "hidden invalidation stays deferred");
    scene.view.SetVisible(true);
    Check(scene.view.NeedsPreparation() && requests == requestsBeforeHide + 1, "showing requests exactly one preparation");
    Hr(scene.view.Prepare(480, 240), "prepare after show");
    const auto restored = scene.view.GetStatistics();
    Check(restored.surfaceBytes == visible.surfaceBytes && restored.surfaceAllocations == visible.surfaceAllocations + 1,
          "showing reallocates exactly one surface");
    Check(restored.replacementPeakBytes == visible.replacementPeakBytes, "a released surface does not raise the replacement peak");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite after show");
    std::vector<uint8_t> reshown;
    Hr(gpu.Read(reshown), "read pixels after show");
    Check(reshown == shown, "reallocated surface reproduces the pre-hide pixels");
    Check(scene.view.Prepare(0, 0) == S_FALSE, "zero extent suspends");
    Check(scene.view.GetStatistics().surfaceBytes == 0 && scene.view.GetStatistics().surfaceAllocations == restored.surfaceAllocations,
          "zero extent releases the surface");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation() && scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE,
          "zero extent requests no work and composites nothing");
    Hr(scene.view.Prepare(480, 240), "prepare after zero extent");
    Check(scene.view.GetStatistics().surfaceBytes == visible.surfaceBytes && scene.view.GetStatistics().surfaceAllocations == restored.surfaceAllocations + 1,
          "sized preparation after zero extent reallocates exactly one surface");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite after zero extent");
    Hr(gpu.Read(reshown), "read pixels after zero extent");
    Check(reshown == shown, "surface restored after zero extent reproduces the pixels");
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "replacement pool for hidden view");
    scene.view.SetVisible(false);
    Hr(scene.view.ReplaceDevice(pool), "replace device while hidden");
    Check(scene.view.GetStatistics().surfaceBytes == 0, "replaced hidden view holds no surface");
    scene.view.SetVisible(true);
    Hr(scene.view.Prepare(480, 240), "prepare after hidden replacement");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite after hidden replacement");
    Hr(gpu.Read(reshown), "read pixels after hidden replacement");
    Check(reshown == shown, "hidden device replacement reproduces the pixels");
}

// A hidden view paints nothing, so it returns what painting keeps for its controls as it returns its surface: a grid's
// text layouts, their key strings and its tables. Showing it prepares again and the grid lays out what it shows, once.
__declspec(noinline) static void TestHiddenViewReleasesGridLayouts(GraphicsFixture& gpu)
{
    for (const bool multiline : {false, true})
    {
        ComplexUiScene scene;
        scene.model.multilineGrid = multiline;
        Hr(scene.Initialize(gpu.device.get()), "hidden grid scene");
        Hr(scene.view.Prepare(1280, 720), "prepare grid scene");
        const auto shown = scene.grid->DebugGetTextLayoutStatistics();
        Check(shown.retainedLayouts > 0 && shown.capacity > 0 && shown.tableBytes > 0, "a prepared view's grid holds layouts");
        scene.view.SetVisible(false);
        const auto hidden = scene.grid->DebugGetTextLayoutStatistics();
        Check(hidden.retainedLayouts == 0 && hidden.displayLayouts == 0 && hidden.capacity == 0 && hidden.displayCapacity == 0 && hidden.textUnits == 0 &&
                  hidden.tableBytes == 0 && ! hidden.ellipsis,
              "hiding the view returns its grid's layouts, key strings and tables");
        Check(scene.view.Prepare(1280, 720) == S_FALSE && scene.grid->DebugGetTextLayoutStatistics().lookups == shown.lookups,
              "a hidden view prepares nothing, so its grid asks for no layout");
        scene.view.SetVisible(true);
        Hr(scene.view.Prepare(1280, 720), "prepare after show");
        const auto restored = scene.grid->DebugGetTextLayoutStatistics();
        Check(restored.retainedLayouts == shown.retainedLayouts && restored.layoutCreations - hidden.layoutCreations == shown.layoutCreations,
              "showing the view lays out only what its grid shows, as its first preparation did");
    }
}

// Synthetic French cells for the pixel checks below. `Twin` drops what the first row's two cells omit (their third paragraph), so
// it differs from `Full` only by the omission marker; `Empty` blanks every cell, the reference whose pixels a check subtracts.
struct FrenchGridModel final : DxUi::IGridModel
{
    enum class Kind
    {
        Full,
        Twin,
        Empty
    };
    Kind kind;
    explicit FrenchGridModel(Kind modelKind) : kind(modelKind)
    {
    }
    size_t GetRowCount() const noexcept override
    {
        return 4;
    }
    size_t GetColumnCount() const noexcept override
    {
        return 2;
    }
    DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        return {std::to_wstring(column), L"Colonne " + std::to_wstring(column), 220};
    }
    void GetCellData(size_t row, size_t column, DxUi::GridCellData& cell) const override
    {
        cell.multiline = true;
        if (kind == Kind::Empty)
            return;
        if (row == 0)
        {
            cell.text = column == 0 ? L"Première ligne française détaillée\nDeuxième ligne avec é et \U0001F4F7"
                                    : L"Synchronisation terminée\nAucune erreur à signaler";
            if (kind == Kind::Full)
                cell.text += column == 0 ? L"\nTroisième ligne masquée par l’ellipse" : L"\nRapport disponible dans le journal";
            return;
        }
        cell.text = column == 0
                        ? L"Une très longue phrase française qui passe à la ligne plusieurs fois : vérifier la configuration du serveur principal avant "
                          L"la mise en production prévue pour la semaine prochaine."
                        : L"État de la revue : en attente de validation par la responsable du service, priorité élevée, merci de répondre avant vendredi "
                          L"pour que la publication reste possible.";
        cell.text += L" (ligne " + std::to_wstring(row) + L")";
    }
    std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
    {
        return id < 4 ? std::optional<size_t>(static_cast<size_t>(id)) : std::nullopt;
    }
};

struct FrenchGridScene
{
    FrenchGridModel model; // Destroy the view before the model it borrows.
    DxUi::EmbeddedHost view;
    DxUi::Grid* grid = nullptr;
    explicit FrenchGridScene(FrenchGridModel::Kind kind = FrenchGridModel::Kind::Full) : model(kind)
    {
    }
    HRESULT Initialize(ID3D11Device* device, uint32_t lineClamp = 2)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        RETURN_IF_FAILED(DxUi::GraphicsDevice::Create(device, graphics));
        RETURN_IF_FAILED(view.Attach(std::move(graphics)));
        auto theme          = DxUi::MakeDefaultThemePalette(false);
        theme.reducedMotion = true;
        view.Controls().SetTheme(theme);
        auto root = std::make_unique<DxUi::Panel>();
        grid      = root->AddChild<DxUi::Grid>();
        grid->SetBounds(D2D1::RectF(8, 8, 472, 232));
        grid->SetHeaderHeightDip(30);
        grid->SetRowHeightDip(64);
        grid->SetLineClamp(lineClamp);
        grid->SetModel(&model);
        view.Controls().SetRoot(std::move(root));
        return S_OK;
    }
};

// Prepares the scene at `dpi` (the 480 x 240 DIP view, in physical pixels), composites it on the fixture's device and reads it back.
static void RenderFrenchGrid(GraphicsFixture& gpu, FrenchGridScene& scene, float dpi, std::vector<uint8_t>& pixels)
{
    const UINT width  = static_cast<UINT>(480 * dpi / 96);
    const UINT height = static_cast<UINT>(240 * dpi / 96);
    if (gpu.width != width || gpu.height != height)
        Hr(gpu.Resize(width, height), "resize the target for the French grid");
    Hr(scene.view.Prepare(width, height, dpi), "prepare the French grid");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "compose the French grid");
    Hr(gpu.Read(pixels), "read the French grid");
}

// A multiline grid in an embedded view on the supplied WARP device, with long French cells clamped to two lines in 64-DIP rows. The
// read-back pixels show the omission marker after the second line (the full cell and its twin without the omitted paragraph differ
// only after the twin's last character) and no third line; hiding and showing the view, replacing the device (hidden or not) and
// every dpi (144, 192, and back) reproduce the pixels of a fresh view made the same way, the grid keeping its dpi-independent layouts.
__declspec(noinline) static void TestEmbeddedMultilineGridFrenchCells(GraphicsFixture& gpu)
{
    const UINT savedWidth  = gpu.width;
    const UINT savedHeight = gpu.height;
    FrenchGridScene full(FrenchGridModel::Kind::Full);
    FrenchGridScene twin(FrenchGridModel::Kind::Twin);
    FrenchGridScene empty(FrenchGridModel::Kind::Empty);
    Hr(full.Initialize(gpu.device.get()), "French grid");
    Hr(twin.Initialize(gpu.device.get()), "French twin grid");
    Hr(empty.Initialize(gpu.device.get()), "empty French grid");
    std::vector<uint8_t> fullPixels, twinPixels, emptyPixels;
    RenderFrenchGrid(gpu, full, 96, fullPixels);
    RenderFrenchGrid(gpu, twin, 96, twinPixels);
    RenderFrenchGrid(gpu, empty, 96, emptyPixels);
    const auto pixelAt = [](const std::vector<uint8_t>& pixels, UINT width, UINT x, UINT y) -> uint32_t
    {
        uint32_t value = 0;
        memcpy(&value, pixels.data() + (size_t(y) * width + x) * 4, 4);
        return value;
    };
    Check(full.grid->DebugGetTextLayoutStatistics().displayCapacity > 0, "the embedded grid trims its long cells");
    for (size_t column = 0; column < 2; ++column)
    {
        const DxUi::GridCellLayoutMetrics metrics = full.grid->GetCellLayoutMetrics(full.view.Controls(), 0, column);
        const UINT left                           = static_cast<UINT>(metrics.textRect.left);
        const UINT right                          = static_cast<UINT>(metrics.textRect.right);
        const UINT top                            = static_cast<UINT>(metrics.textRect.top);
        const UINT bottom                         = static_cast<UINT>(metrics.textRect.bottom);
        // The twin's ink, row by row: where each line of the twin ends. The omission marker belongs after the end of the second
        // line, and a third line (ink where the twin has none) would be a row the twin does not paint.
        std::vector<int> rowRight(bottom - top, -1);
        UINT twinLeft = right;
        for (UINT y = top; y < bottom; ++y)
            for (UINT x = left; x < right; ++x)
                if (pixelAt(twinPixels, 480, x, y) != pixelAt(emptyPixels, 480, x, y))
                {
                    twinLeft          = (std::min)(twinLeft, x);
                    rowRight[y - top] = (std::max)(rowRight[y - top], static_cast<int>(x));
                }
        Check(twinLeft < right && std::ranges::count_if(rowRight, [](int edge) { return edge >= 0; }) > 10, "the twin cell paints its two lines");
        size_t inside = 0;
        size_t marker = 0;
        for (UINT y = top; y < bottom; ++y)
            for (UINT x = left; x < right; ++x)
                if (pixelAt(fullPixels, 480, x, y) != pixelAt(twinPixels, 480, x, y))
                {
                    const int edge = rowRight[y - top];
                    if (edge < 0 || static_cast<int>(x) + 3 < edge)
                        ++inside;
                    else if (static_cast<int>(x) > edge)
                        ++marker;
                }
        std::cout << "Embedded multiline grid cell 0," << column << ": twin ink from " << twinLeft << ", changed inside or on rows the twin leaves blank "
                  << inside << ", marker pixels " << marker << '\n';
        Check(inside == 0 && marker > 0, "the cell differs from its twin by an omission marker after the second line, with no third line");
    }
    // A long wrapped cell shows two lines: fewer ink rows than the same grid at clamp three, and more than a line.
    const auto inkRows = [&](const std::vector<uint8_t>& pixels, size_t row)
    {
        const DxUi::GridCellLayoutMetrics metrics = full.grid->GetCellLayoutMetrics(full.view.Controls(), row, 0);
        size_t rows                               = 0;
        for (UINT y = static_cast<UINT>(metrics.textRect.top); y < static_cast<UINT>(metrics.textRect.bottom); ++y)
        {
            for (UINT x = static_cast<UINT>(metrics.textRect.left); x < static_cast<UINT>(metrics.textRect.right); ++x)
                if (pixelAt(pixels, 480, x, y) != pixelAt(emptyPixels, 480, x, y))
                {
                    ++rows;
                    break;
                }
        }
        return rows;
    };
    FrenchGridScene threeLines(FrenchGridModel::Kind::Full);
    Hr(threeLines.Initialize(gpu.device.get(), 3), "French grid at clamp three");
    std::vector<uint8_t> threeLinePixels;
    RenderFrenchGrid(gpu, threeLines, 96, threeLinePixels);
    const size_t twoLineRows   = inkRows(fullPixels, 1);
    const size_t threeLineRows = inkRows(threeLinePixels, 1);
    std::cout << "Embedded multiline grid row 1: ink rows " << twoLineRows << " at clamp 2, " << threeLineRows << " at clamp 3\n";
    Check(twoLineRows > 10 && twoLineRows * 4 < threeLineRows * 3, "a wrapped cell paints two lines at clamp two, not the third");

    // Hide and show: the layouts go and come back, and the pixels are the same.
    const auto shown = full.grid->DebugGetTextLayoutStatistics();
    full.view.SetVisible(false);
    const auto hidden = full.grid->DebugGetTextLayoutStatistics();
    Check(hidden.retainedLayouts == 0 && hidden.displayLayouts == 0, "hiding the view returns the grid's layouts");
    Check(full.view.Prepare(480, 240) == S_FALSE && full.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE,
          "a hidden view prepares and composites nothing");
    full.view.SetVisible(true);
    std::vector<uint8_t> reshown;
    RenderFrenchGrid(gpu, full, 96, reshown);
    Check(reshown == fullPixels, "showing the view again paints the pixels it painted before");
    Check(full.grid->DebugGetTextLayoutStatistics().layoutCreations - hidden.layoutCreations == shown.layoutCreations,
          "showing lays out only what the grid shows, as its first paint did");

    // A replacement device, the view shown and then hidden: another WARP device and pool, the same pixels.
    GraphicsFixture replacement;
    Hr(replacement.Create(), "replacement WARP device for the French grid");
    std::shared_ptr<DxUi::GraphicsDevice> next;
    Hr(DxUi::GraphicsDevice::Create(replacement.device.get(), next), "replacement pool for the French grid");
    Hr(full.view.ReplaceDevice(next), "replace the device of the French grid");
    Hr(full.view.Prepare(480, 240), "prepare on the replacement device");
    replacement.Bind();
    Hr(full.view.Composite(replacement.context.get(), replacement.Viewport()), "compose on the replacement device");
    std::vector<uint8_t> replaced;
    Hr(replacement.Read(replaced), "read the replacement device");
    Check(replaced == fullPixels, "a replacement device paints the pixels of the first one");
    full.view.SetVisible(false);
    std::shared_ptr<DxUi::GraphicsDevice> back;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), back), "pool on the first device again");
    Hr(full.view.ReplaceDevice(back), "replace the device of the hidden French grid");
    full.view.SetVisible(true);
    RenderFrenchGrid(gpu, full, 96, reshown);
    Check(reshown == fullPixels, "a device replaced while hidden paints the same pixels when shown");

    // Dpi: one view changes dpi; a fresh view is made at each. Layouts are in device-independent units, so the changed view keeps them.
    FrenchGridScene changing(FrenchGridModel::Kind::Full);
    Hr(changing.Initialize(gpu.device.get()), "French grid whose dpi changes");
    std::vector<uint8_t> changed, fresh;
    RenderFrenchGrid(gpu, changing, 96, changed);
    for (const float dpi : {144.0f, 192.0f, 96.0f, 192.0f})
    {
        const auto before = changing.grid->DebugGetTextLayoutStatistics();
        RenderFrenchGrid(gpu, changing, dpi, changed);
        const auto after = changing.grid->DebugGetTextLayoutStatistics();
        FrenchGridScene made(FrenchGridModel::Kind::Full);
        Hr(made.Initialize(gpu.device.get()), "fresh French grid at the new dpi");
        RenderFrenchGrid(gpu, made, dpi, fresh);
        std::cout << "Embedded multiline grid at " << dpi << " dpi: layouts created by the change " << after.layoutCreations - before.layoutCreations
                  << ", retained " << after.retainedLayouts << '\n';
        Check(changed.size() == fresh.size() && changed == fresh, "a view whose dpi changed paints what a fresh view paints at that dpi");
        Check(after.retainedLayouts > 0, "the grid holds layouts at the new dpi");
    }
    if (gpu.width != savedWidth || gpu.height != savedHeight)
        Hr(gpu.Resize(savedWidth, savedHeight), "restore the fixture target");
}
// A hover or focus Invalidate must not swallow the following Down. The host message loop
// prepares after input, so a paint-dirty view is the normal state at the start of a tap.
__declspec(noinline) static void TestPointerGesturesOnPaintDirtyView(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "paint-dirty pointer scene");
    Hr(scene.view.Prepare(480, 240), "prepare paint-dirty pointer scene");
    Check(scene.view.Controls().GetInputModality() == DxUi::InputModality::Pointer, "embedded starts in pointer modality");
    Check(scene.view.DispatchKey(VK_TAB, true), "tab focuses a control");
    Check(scene.view.Controls().GetInputModality() == DxUi::InputModality::Keyboard, "key input marks keyboard focus visible");
    static_cast<void>(scene.view.DispatchPointer({DxUi::PointerAction::Move, 45, 84}));
    Check(scene.view.NeedsPreparation(), "hover marks the cached surface dirty");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "paint-dirty down still begins a gesture");
    Check(scene.view.Controls().GetInputModality() == DxUi::InputModality::Pointer, "pointer down clears keyboard-only focus visuals");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 45, 84}), "paint-dirty up still commits");
    Check(! scene.enabled, "first contact on a paint-dirty view activates the toggle");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "second dirty down still begins a gesture");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 45, 84}), "second dirty up still commits");
    Check(scene.enabled, "a second contact on the still-dirty view activates again");
}

// Host ticks dirty a view only through control invalidation: an idle root or an unchanged caret phase leaves a
// clean prepared view clean; a blink-phase flip prepares exactly once.
__declspec(noinline) static void TestTickDirtying(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "tick pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool), "tick view");
    auto root = std::make_unique<DxUi::Panel>();
    root->AddChild<DxUi::Label>(L"Static content")->SetBounds(D2D1::RectF(8, 8, 300, 40));
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 160), "prepare static root");
    const auto idle = view.GetStatistics();
    Check(view.NeedsAnimation() && ! view.NeedsPreparation(), "a new root schedules one discovery tick on a clean prepared view");
    Check(! view.AdvanceAnimation(GetTickCount64()), "static root stops ticking after discovery");
    Check(! view.NeedsAnimation() && ! view.NeedsPreparation(), "the discovery tick does not dirty a static view");
    Check(view.Prepare(320, 160) == S_FALSE && view.GetStatistics().preparations == idle.preparations, "the discovery tick causes no preparation");
    view.Controls().RequestAnimation();
    Hr(view.Prepare(320, 160), "prepare after an explicit animation request");
    const auto requested = view.GetStatistics();
    Check(view.NeedsAnimation() && ! view.NeedsPreparation(), "an explicit animation request leaves a clean prepared view");
    Check(! view.AdvanceAnimation(GetTickCount64() + 1) && ! view.NeedsPreparation(), "a requested idle tick does not dirty the view");
    Check(view.Prepare(320, 160) == S_FALSE && view.GetStatistics().preparations == requested.preparations, "a requested idle tick causes no preparation");

    auto field     = std::make_unique<DxUi::TextField>();
    auto* fieldPtr = field.get();
    fieldPtr->SetText(L"Caret");
    fieldPtr->SetBounds(D2D1::RectF(8, 8, 300, 48));
    view.Controls().SetRoot(std::move(field));
    Hr(view.Prepare(320, 160), "prepare text field root");
    view.Controls().SetFocusControl(fieldPtr);
    const uint64_t focusTick = GetTickCount64();
    Check(fieldPtr->HasFocus() && view.NeedsAnimation(), "focused text field requests caret ticks");
    Hr(view.Prepare(320, 160), "prepare focused text field");
    const auto focused = view.GetStatistics();
    Check(! view.NeedsPreparation(), "focused text field is clean after preparation");
    Check(view.AdvanceAnimation(focusTick + 10), "caret keeps ticking");
    Check(! view.NeedsPreparation(), "a tick inside the blink phase does not dirty");
    Check(view.AdvanceAnimation(focusTick + 20), "caret keeps ticking within the phase");
    Check(! view.NeedsPreparation() && view.Prepare(320, 160) == S_FALSE && view.GetStatistics().preparations == focused.preparations,
          "ticks within the blink period cause no preparation");
    constexpr uint64_t kCaretBlinkPeriodMs = 530u; // kCaretBlinkPeriodMs in DxUi.TextInput.cpp
    Check(view.AdvanceAnimation(focusTick + 20 + kCaretBlinkPeriodMs), "caret keeps ticking across the blink");
    Check(view.NeedsPreparation(), "a blink-phase flip dirties the view");
    Check(view.Prepare(320, 160) == S_OK && view.GetStatistics().preparations == focused.preparations + 1, "a blink-phase flip prepares exactly once");
}

// Per-view brush and configured text-format caches stay within their bounds: an over-bound cache is cleared at
// the next preparation start, never while a paint borrows its entries, and refills with the painted working set.
__declspec(noinline) static void TestCacheBounds(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "cache pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool), "cache view");
    auto root = std::make_unique<DxUi::Panel>();
    root->AddChild<DxUi::Label>(L"Bounded caches")->SetBounds(D2D1::RectF(8, 8, 300, 40));
    root->AddChild<DxUi::Toggle>(L"Toggle")->SetBounds(D2D1::RectF(8, 48, 300, 88));
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 160), "prepare cache view");
    const auto painted = view.GetStatistics();
    Check(painted.cachedBrushes > 0 && painted.cachedBrushes <= DxUi::ControlHost::kSolidBrushCacheLimit, "a painted view reports its cached brushes");
    Check(painted.cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit, "a painted view reports its cached text formats");
    auto& controls = view.Controls();
    for (size_t i = 0; i <= DxUi::ControlHost::kSolidBrushCacheLimit; ++i)
        Check(controls.GetSolidBrush(D2D1::ColorF(float(i % 256) / 255.0f, float(i / 256) / 255.0f, 0.5f)) != nullptr, "distinct solid brush");
    Check(view.GetStatistics().cachedBrushes > DxUi::ControlHost::kSolidBrushCacheLimit, "distinct colors can exceed the brush bound between paints");
    for (int role = 0; role <= int(DxUi::FontRole::Small) && view.GetStatistics().cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit;
         ++role)
        for (auto alignment : {DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_TEXT_ALIGNMENT_JUSTIFIED})
            for (auto paragraph : {DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_PARAGRAPH_ALIGNMENT_CENTER})
                for (bool wrap : {false, true})
                    Check(controls.GetTextFormat(DxUi::FontRole(role), alignment, paragraph, wrap) != nullptr, "distinct configured text format");
    Check(view.GetStatistics().cachedTextFormats > DxUi::ControlHost::kConfiguredTextFormatCacheLimit,
          "distinct configurations can exceed the text-format bound between paints");
    view.MarkDirty();
    Hr(view.Prepare(320, 160), "prepare trims the caches");
    const auto trimmed = view.GetStatistics();
    Check(trimmed.cachedBrushes > 0 && trimmed.cachedBrushes <= DxUi::ControlHost::kSolidBrushCacheLimit, "preparation trims the brush cache to its bound");
    Check(trimmed.cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit, "preparation trims the text-format cache to its bound");
    Check(trimmed.cachedBrushes == painted.cachedBrushes, "the trimmed brush cache holds exactly the painted working set");
}

// Keep unrelated functional-test locals out of the benchmark entry stack, even under LTCG.
__declspec(noinline) static int RunFunctionalTests()
{
    // The opt-in grid selection measurement is dispatched here, not in BenchmarkMain.h, whose hash identifies the complex-UI fixture.
    if ((__argc == 3 || __argc == 4) && std::wstring_view(__wargv[1]) == L"--benchmark-grid-selection")
    {
        GridSelectionBenchmark::Run(__wargv[2], __argc == 4 ? std::wstring_view(__wargv[3]) : std::wstring_view());
        return 0;
    }
    static size_t diagnosticCalls = 0;
    DxUi::Diagnostics::sink       = [](std::wstring_view, std::wstring_view message) noexcept
    {
        if (message.find(L"public-probe detail") != std::wstring_view::npos)
            ++diagnosticCalls;
    };
    Check(DxUi::IsContextMenuDiagnosticsEnabled(), "native diagnostics use public sink");
    DxUi::TraceContextMenuDiagnostics(L"public-probe", L"detail");
    Check(diagnosticCalls == 1, "native trace reaches caller sink");
    DxUi::Diagnostics::sink = nullptr;
    Check(! DxUi::IsContextMenuDiagnosticsEnabled(), "clearing sink disables native diagnostics");
    GraphicsFixture gpu;
    Hr(gpu.Create(), "supplied WARP device");
    TestEmbeddedTextInput(gpu);
    TestEmbeddedAccessibility(gpu);
    TestEmbeddedUiaEventHarness(gpu);
    TestLocalizedShortViewport(gpu,
                               [](DxUi::Button& action) { action.SetMultiline(true); },
                               [](const auto& sizes, float width, auto& bounds, float& height)
    {
        DxUi::MeasuredActionLayout layout{};
        const HRESULT result = DxUi::ArrangeMeasuredActions(sizes, width, D2D1::SizeF(8, 8), bounds, layout);
        height               = layout.heightDip;
        return result;
    });
    TestLocalizedInvalidationAndSelectedDetail(gpu);
    TestLocalizedStackedBodyClipping(gpu);
    TestLocalizedCheckboxCaption(gpu);
    TestRightToLeftCheckboxCaptionHugsIndicator(gpu);
    TestRightToLeftTabTitleStartsAtTheRight(gpu);
    TestSurfaceLifetime(gpu);
    TestHiddenViewReleasesGridLayouts(gpu);
    TestEmbeddedMultilineGridFrenchCells(gpu);
    TestPointerGesturesOnPaintDirtyView(gpu);
    TestTickDirtying(gpu);
    TestCacheBounds(gpu);
    EmbeddedScene scene;
    size_t requests = 0;
    Hr(scene.Initialize(gpu.device.get(), {&requests, [](void* p) noexcept { ++*static_cast<size_t*>(p); }}), "public scene");
    Check(scene.view.Controls().GetHwnd() == nullptr, "embedded has no HWND");
    Hr(scene.view.Prepare(480, 240), "prepare controls");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "composite controls");
    std::vector<uint8_t> before;
    Hr(gpu.Read(before), "read initial pixels");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "toggle down");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 45, 84}), "toggle up");
    Check(! scene.enabled, "toggle changes model once");
    Check(scene.view.Controls().GetCapturedControl() == nullptr, "toggle release clears logical capture");
    Check(requests == 1, "dirty notifications coalesce");
    Hr(scene.view.Prepare(480, 240), "prepare changed toggle");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "compose changed toggle");
    std::vector<uint8_t> after;
    Hr(gpu.Read(after), "read changed pixels");
    size_t changed = 0;
    for (size_t i = 0; i < before.size(); i += 4)
        if (memcmp(before.data() + i, after.data() + i, 4) != 0)
            ++changed;
    Check(changed > 100, "toggle changes visible pixels");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "toggle press before cancel");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Cancel}), "toggle cancellation");
    Check(! scene.enabled && scene.view.Controls().GetCapturedControl() == nullptr, "canceled toggle does not activate");
    Hr(scene.view.Prepare(480, 240), "prepare after canceled toggle");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "toggle press before outside release");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 470, 230}), "captured toggle release outside");
    Check(! scene.enabled, "outside release does not toggle");
    Hr(scene.view.Prepare(480, 240), "prepare after outside release");
    size_t preview = 0, commit = 0, cancel = 0;
    scene.slider->SetOnChange([&](DxUi::SliderChange c)
    {
        scene.intensity = c.value;
        if (c.phase == DxUi::SliderChangePhase::Preview)
            ++preview;
        else if (c.phase == DxUi::SliderChangePhase::Commit)
            ++commit;
        else
            ++cancel;
    });
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 120, 184}), "slider down");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Move, 340, 184}), "slider preview");
    Check(preview >= 2 && commit == 0 && scene.intensity > 70, "live slider previews");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Cancel}), "capture cancellation");
    Check(cancel == 1 && scene.intensity == 65, "cancel restores initial value");
    Hr(scene.view.Prepare(480, 240), "prepare between gestures");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 300, 184}), "second slider down");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 300, 184}), "slider release");
    Check(commit == 1 && scene.intensity > 60, "exactly one drag commit");
    Check(scene.view.DispatchKey(VK_END, true), "keyboard end");
    Check(scene.intensity == 100 && commit == 2, "keyboard commits");
    Hr(scene.view.Prepare(480, 240), "prepare after input");
    const auto prepared = scene.view.GetStatistics();
    Check(scene.view.Prepare(480, 240) == S_FALSE, "clean preparation is skipped");
    gpu.Bind();
    const auto start = std::chrono::steady_clock::now();
    countAllocations = true;
    for (int i = 0; i < 1000; ++i)
        Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "warm compose");
    countAllocations = false;
    const auto us    = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    Check(allocations == 0, "1000 warm composites have zero C++ allocations");
    Check(scene.view.GetStatistics().surfaceAllocations == prepared.surfaceAllocations && scene.view.GetStatistics().preparations == prepared.preparations,
          "warm composition creates no surface or raster work");
    // Poison the state a preceding widget can leave behind; composite must establish its own pipeline.
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "reference pipeline");
    std::vector<uint8_t> reference;
    Hr(gpu.Read(reference), "reference pixels");
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode      = D3D11_FILL_SOLID;
    raster.CullMode      = D3D11_CULL_FRONT;
    raster.ScissorEnable = TRUE;
    wil::com_ptr_nothrow<ID3D11RasterizerState> hostileRaster;
    Hr(gpu.device->CreateRasterizerState(&raster, hostileRaster.put()), "hostile raster state");
    gpu.Bind();
    gpu.context->RSSetState(hostileRaster.get());
    const D3D11_RECT empty{};
    gpu.context->RSSetScissorRects(1, &empty);
    gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    gpu.context->VSSetShader(nullptr, nullptr, 0);
    gpu.context->PSSetShader(nullptr, nullptr, 0);
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "hostile state composite");
    std::vector<uint8_t> hostile;
    Hr(gpu.Read(hostile), "hostile output");
    Check(hostile == reference, "output independent of previous raster and shader state");
    auto negative     = gpu.Viewport();
    negative.TopLeftX = -20;
    negative.TopLeftY = -10;
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), negative), "negative page-swipe viewport");
    Hr(gpu.Read(hostile), "negative output");
    Check(memcmp(hostile.data() + size_t(174 * 480 + 280) * 4, reference.data() + size_t(184 * 480 + 300) * 4, 4) == 0,
          "negative origin translates pixels without rerasterization");
    auto invalid     = gpu.Viewport();
    invalid.TopLeftX = std::numeric_limits<float>::quiet_NaN();
    Check(scene.view.Composite(gpu.context.get(), invalid) == E_INVALIDARG, "reject NaN viewport");
    const auto shownStats = scene.view.GetStatistics();
    scene.view.SetVisible(false);
    Check(! scene.view.NeedsAnimation() && ! scene.view.NeedsPreparation(), "hidden has no scheduled work");
    Check(scene.view.GetStatistics().surfaceBytes == 0, "hidden view holds no surface");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}),
          "hidden ignores render/input");
    scene.view.SetVisible(true);
    Hr(scene.view.Prepare(480, 240), "resume");
    Check(scene.view.GetStatistics().surfaceBytes == shownStats.surfaceBytes &&
              scene.view.GetStatistics().surfaceAllocations == shownStats.surfaceAllocations + 1,
          "resume reallocates exactly one surface");
    Check(scene.view.Prepare(0, 0) == S_FALSE, "zero target suspends");
    Check(scene.view.GetStatistics().surfaceBytes == 0, "zero target holds no surface");
    Check(! scene.view.NeedsPreparation() && ! scene.view.NeedsAnimation(), "zero target schedules no work");
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 45, 84}), "zero target ignores input");
    Hr(scene.view.Prepare(960, 480, 192), "DPI and resize");
    // The surface released by the zero target is replaced by exactly one allocation at the new physical extent.
    Check(scene.view.GetStatistics().surfaceBytes == 960ull * 480 * 4 && scene.view.GetStatistics().surfaceAllocations == shownStats.surfaceAllocations + 2,
          "surface budget accounted after the zero-target release");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 90, 168}) && scene.view.DispatchPointer({DxUi::PointerAction::Up, 90, 168}),
          "physical pixel input transforms at 200 percent");
    Check(scene.enabled, "DPI toggle works");
    Check(scene.view.Prepare(16384, 16384) == E_OUTOFMEMORY, "over-budget rejected");
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 90, 168}), "failed preparation disables input");
    Hr(scene.view.Prepare(480, 240), "recover from invalid target");
    const auto originalBounds  = scene.slider->GetBounds();
    const double capturedStart = scene.slider->GetValue();
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 184}), "capture before external geometry mutation");
    scene.slider->SetBounds(D2D1::RectF(24, 200, 440, 232));
    Check(! scene.view.DispatchPointer({DxUi::PointerAction::Move, 220, 210}), "unprepared geometry cannot retarget captured input");
    Check(scene.view.Controls().GetCapturedControl() == nullptr && scene.slider->GetValue() == capturedStart, "geometry invalidation cancels draft");
    scene.slider->SetBounds(originalBounds);
    Hr(scene.view.Prepare(480, 240), "prepare restored geometry");
    for (bool hide : {false, true})
    {
        const double initial        = scene.slider->GetValue();
        const size_t canceledBefore = cancel;
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 80, 184}), "press before availability change");
        if (hide)
            scene.slider->SetVisible(false);
        else
            scene.slider->SetEnabled(false);
        Hr(scene.view.Prepare(480, 240), "prepare unavailable control cancels before pruning capture");
        Check(scene.slider->GetValue() == initial && cancel == canceledBefore + 1 && scene.view.Controls().GetCapturedControl() == nullptr,
              "hidden or disabled slider restores its draft exactly once");
        scene.slider->SetVisible(true);
        scene.slider->SetEnabled(true);
        Hr(scene.view.Prepare(480, 240), "restore control availability");
    }
    // A splitter drag survives the pane layout that follows a preview. Resizing the view still cancels, above.
    {
        auto* root     = static_cast<DxUi::Panel*>(scene.view.Controls().GetRoot());
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetMinimumFirstPane(80);
        splitter->SetMinimumSecondPane(80);
        splitter->SetPosition(200);
        Hr(scene.view.Prepare(480, 240), "prepare splitter drag");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "splitter press");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Move, 260, 120}), "splitter preview");
        const float dragged = splitter->GetPosition();
        Check(dragged > 240.0f, "splitter preview moved");
        scene.slider->SetBounds(D2D1::RectF(24, 164, 440, 212));
        Hr(scene.view.Prepare(480, 240), "pane layout during splitter drag");
        Check(scene.view.Controls().GetCapturedControl() == splitter && splitter->GetPosition() == dragged, "pane layout keeps the splitter drag");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Move, 300, 120}), "splitter drag continues after layout");
        Check(splitter->GetPosition() > dragged, "continued drag moves again");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 300, 120}), "splitter release");
        scene.slider->SetBounds(D2D1::RectF(24, 160, 440, 208));
    }
    // The drag survives only while the splitter and every ancestor stay in the tree, visible and enabled. Hiding a
    // containing panel cancels it even though the splitter's own flags are unchanged; destroying the captured
    // splitter releases the capture without dereferencing the destroyed control (ASan Debug detects that).
    {
        auto* root     = static_cast<DxUi::Panel*>(scene.view.Controls().GetRoot());
        auto* pane     = root->AddChild<DxUi::Panel>();
        auto* splitter = pane->AddChild<DxUi::Splitter>();
        pane->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetMinimumFirstPane(80);
        splitter->SetMinimumSecondPane(80);
        splitter->SetPosition(200);
        Hr(scene.view.Prepare(480, 240), "prepare nested splitter drag");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "nested splitter press");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Move, 260, 120}), "nested splitter preview");
        Check(splitter->IsDragging() && splitter->GetPosition() > 240.0f, "nested splitter preview moved");
        pane->SetVisible(false);
        Hr(scene.view.Prepare(480, 240), "hidden ancestor during splitter drag");
        Check(scene.view.Controls().GetCapturedControl() == nullptr && ! splitter->IsDragging() && splitter->GetPosition() == 200.0f,
              "hiding an ancestor cancels the splitter drag and restores its start");
        pane->SetVisible(true);
        Hr(scene.view.Prepare(480, 240), "restore nested splitter");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "nested splitter second press");
        Check(scene.view.Controls().GetCapturedControl() == splitter, "nested splitter captures again");
        pane->ClearChildren();
        Hr(scene.view.Prepare(480, 240), "destroyed captured splitter");
        Check(scene.view.Controls().GetCapturedControl() == nullptr, "a destroyed captured control is released");
        pane->SetVisible(false);
        Hr(scene.view.Prepare(480, 240), "retire nested splitter pane");
    }
    std::shared_ptr<DxUi::GraphicsDevice> shared;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), shared), "shared supplied device pool");
    DxUi::EmbeddedHost a, b;
    Hr(a.Attach(shared), "first shared view");
    Hr(b.Attach(shared), "second shared view");
    for (const auto& item : DxUi::GetControlCatalog())
    {
        std::unique_ptr<DxUi::Control> control;
        Hr(DxUi::CreateControl(item.kind, control), "catalog creates control");
        a.Controls().SetRoot(std::move(control));
        a.MarkDirty();
        Hr(a.Prepare(480, 240), "catalog prepares every concrete control");
        gpu.Bind();
        Check(a.Composite(gpu.context.get(), gpu.Viewport()) == S_OK, "catalog draws every concrete control without suppression");
    }
    Check(DxUi::GetControlCatalog().size() == 30, "catalog contains all 30 controls");
    b.Controls().SetRoot(std::make_unique<DxUi::Toggle>(L"Second view"));
    Hr(b.Prepare(320, 160, 144), "independent second layout");
    Check(a.GetStatistics().surfaceBytes != b.GetStatistics().surfaceBytes, "shared pool has independent surfaces");
    // The tick contract below needs motion; the default palette follows the desktop's animation setting.
    const DxUi::ThemePalette desktopTheme = b.Controls().GetTheme();
    DxUi::ThemePalette motionTheme        = desktopTheme;
    motionTheme.reducedMotion             = false;
    b.Controls().SetTheme(motionTheme);
    auto progress     = std::make_unique<DxUi::ProgressBar>();
    auto* progressPtr = progress.get();
    progressPtr->SetIndeterminate(true);
    b.Controls().SetRoot(std::move(progress));
    Check(b.NeedsAnimation(), "attaching an indeterminate control schedules initial discovery");
    Hr(b.Prepare(320, 160, 144), "prepare indeterminate progress");
    Check(! b.NeedsPreparation(), "prepared indeterminate progress is clean until it ticks");
    const auto tick = GetTickCount64();
    Check(b.AdvanceAnimation(tick), "indeterminate progress requests another host tick");
    Check(b.NeedsPreparation(), "indeterminate progress dirties every tick");
    Check(b.Prepare(0, 0) == S_FALSE, "zero-sized animation suspends");
    Check(b.GetStatistics().surfaceBytes == 0, "zero-sized view releases its surface");
    Check(! b.NeedsAnimation() && ! b.NeedsPreparation() && ! b.AdvanceAnimation(tick + 8), "zero-sized view has no ticking or preparation");
    b.MarkDirty();
    Check(! b.NeedsPreparation(), "zero-sized invalidation remains deferred");
    Hr(b.Prepare(320, 160, 144), "resume sized animation");
    Check(b.NeedsAnimation(), "sizing restores pending animation");
    b.SetVisible(false);
    Check(! b.NeedsAnimation(), "hidden animation suspends");
    b.SetVisible(true);
    Check(b.NeedsAnimation(), "shown animation resumes");
    progressPtr->SetIndeterminate(false);
    Check(! b.AdvanceAnimation(tick + 16), "stopped animation goes idle");
    b.Controls().SetTheme(desktopTheme);
    Check(b.Controls().SetTooltipDelayed(L"Help", D2D1::Point2F(8, 8)), "schedule embedded tooltip");
    b.AdvanceAnimation(tick + 2000);
    Check(b.Controls().HasTooltip(), "host ticks advance tooltip deadlines");
    Hr(scene.view.Prepare(480, 240), "prepare before resize cancellation");
    const double resizeInitial = scene.slider->GetValue();
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 120, 184}), "drag before resize");
    Hr(scene.view.Prepare(640, 320), "resize cancels capture");
    Check(scene.slider->GetValue() == resizeInitial, "resize restores canceled draft");
    GraphicsFixture replacement;
    Hr(replacement.Create(), "new device generation");
    std::shared_ptr<DxUi::GraphicsDevice> next;
    Hr(DxUi::GraphicsDevice::Create(replacement.device.get(), next), "replacement pool");
    const double previous = scene.slider->GetValue();
    Hr(scene.view.ReplaceDevice(next), "device replacement");
    Check(scene.slider->GetValue() == previous, "model survives replacement");
    Hr(scene.view.Prepare(480, 240), "prepare replacement");
    Check(scene.view.Composite(gpu.context.get(), gpu.Viewport()) == E_INVALIDARG, "reject foreign device context");
    replacement.Bind();
    Hr(scene.view.Composite(replacement.context.get(), replacement.Viewport()), "draw replacement");
    Hr(replacement.Save(L".build/test-artifacts/embedded-controls.png"), "PNG evidence");
    struct Payload
    {
        size_t* destroyed;
        ~Payload()
        {
            ++*destroyed;
        }
    };
    wil::unique_hwnd payloadWindow(CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr));
    Check(bool(payloadWindow), "payload fixture window");
    DxUi::InitPostedPayloadWindow(payloadWindow.get());
    size_t destroyed = 0;
    for (size_t i = 0; i < 128; ++i)
        Check(DxUi::PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)), "bounded payload accepted");
    Check(! DxUi::PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)) && destroyed == 1,
          "saturation rejects and releases ownership");
    Check(DxUi::DrainPostedPayloadsForWindow(payloadWindow.get()) == 128 && destroyed == 129, "drain releases each payload once");
    MSG stale{};
    while (PeekMessageW(&stale, payloadWindow.get(), WM_APP + 77, WM_APP + 77, PM_REMOVE))
        Check(! DxUi::TakeMessagePayload<Payload>(stale.lParam), "drained tokens cannot resurrect payload");
    DxUi::InitPostedPayloadWindow(payloadWindow.get());
    Check(DxUi::PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)), "reinitialize window generation");
    Check(PeekMessageW(&stale, payloadWindow.get(), WM_APP + 77, WM_APP + 77, PM_REMOVE) != FALSE, "take new token");
    Check(! DxUi::TakeMessagePayload<int>(stale.lParam) && destroyed == 130, "wrong payload type rejected and released");
    DxUi::DrainPostedPayloadsForWindow(payloadWindow.get());
    // A callback may destroy the current tree, including the slider dispatching the event.
    scene.slider->SetOnChange([&](DxUi::SliderChange) { scene.view.Controls().SetRoot(std::make_unique<DxUi::Panel>()); });
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 184}), "root replacement during slider callback");
    {
        ComplexUiScene complex;
        Hr(complex.Initialize(gpu.device.get()), "independent complex sample");
        Hr(complex.view.Prepare(1280, 720), "prepare independent controls");
        Check(complex.view.DispatchPointer({DxUi::PointerAction::Down, 240, 52}), "complex sample slider input");
        Check(complex.progress[0]->GetValue() == complex.sliders[0]->GetValue() && complex.sliders[0]->GetValue() > 50,
              "independent sample binds slider preview to progress");
        Check(complex.view.DispatchPointer({DxUi::PointerAction::Cancel}), "complex sample slider cancellation");
        Check(complex.progress[0]->GetValue() == 0, "complex sample cancellation restores progress");
    }
    std::cout << "PASS " << checks << " checks; 1000 warm composites " << us << " us; C++ allocations " << allocations << "; changed toggle pixels " << changed
              << '\n';
    return 0;
}

// Routed before main runs, so that BenchmarkMain.h, a fingerprinted benchmark input, stays as it is.
const bool g_failureReportsRouted = (DxUiTestFailureReports::RouteAwayFromDialogs(), true);

#include "BenchmarkMain.h"
