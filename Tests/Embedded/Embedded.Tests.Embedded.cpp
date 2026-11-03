// Compile the public accessibility header first: consumers must not supply its COM prerequisites.
#include <DxUi/EmbeddedAccessibility.h>
#include <DxUi/FileDrop.h>

#include "Controls/DxUi.Internal.h"
#include "Controls/DxUi.TextElements.h"
#include "EmbeddedScene.h"
#include "GraphicsFixture.h"
#include "Support.Tests.FailureHost.h"
#include "Support.Tests.FailureReports.h"
#include "Support.Tests.MarkdownSourceCorrelation.h"
#include "Support.Tests.RichTextStateReplacement.h"
#include "Support.Tests.SemanticSourceBlock.h"
#include "Support/PostedPayload.h"
#include <DxUi/AccessibilityTextUnits.h>
#include <DxUi/ControlCatalog.h>
#include <DxUi/Diagnostics.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <new>
#include <stdexcept>
#include <type_traits>

#define DXUI_ENABLE_FATAL_INJECTION_WITNESS 1
#include "Embedded.Tests.BenchmarkInstrument.h"
// Distinct stores keep callback origins identifiable under Release ICF/LTCG.
static std::atomic<unsigned> embeddedCallbackOriginMarker{};
static size_t checks = 0;
static void Check(bool value, const char* text)
{
    ++checks;
    if (! value)
    {
        DxUiTestFailureReports::WriteFailure(text);
        std::exit(1);
    }
}
static void Hr(HRESULT hr, const char* text)
{
    if (FAILED(hr))
        std::cerr << "HRESULT: " << std::hex << hr << std::dec << '\n';
    Check(SUCCEEDED(hr), text);
}
static HRESULT PrepareRichEmbeddedContent(DxUi::EmbeddedHost& view, UINT width, UINT height, float dpi = 96.0f)
{
    for (size_t turn = 0u; turn < 1024u; ++turn)
    {
        const HRESULT hr = view.Prepare(width, height, dpi);
        if (FAILED(hr))
            return hr;
        if (! view.NeedsPreparation())
            return hr;
    }
    return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
}
#include "Embedded.Tests.CleanPrepareTimerDiagnostic.h"
#include "Embedded.Tests.ComplexUiBenchmark.h"
#include "Embedded.Tests.ComplexUiBoundaries.h"
#include "Embedded.Tests.ComplexUiProfiling.h"
#include "Embedded.Tests.EmbeddedAccessibility.h"
#include "Embedded.Tests.EmbeddedTextInput.h"
#include "Embedded.Tests.EmbeddedUia.h"
#include "Embedded.Tests.GridSelectionBenchmark.h"
#include "Embedded.Tests.InteractionLayout.h"
#include "Embedded.Tests.LocalizedLayout.h"
#include "Embedded.Tests.RichDocumentStorageResources.h"
#include "Embedded.Tests.RichGeometryResources.h"
#include "Embedded.Tests.RichImageCapture.h"
#include "Embedded.Tests.RichImageFailures.h"
#include "Embedded.Tests.RichProjectionResources.h"
#include "Embedded.Tests.RichSemanticDocumentResources.h"
#include "Embedded.Tests.RichSemanticGeometryResources.h"
#include "Embedded.Tests.RichSemanticResources.h"
#include "Embedded.Tests.RichTextResources.h"
#include "Support.Tests.RichSemantic.h"
#include "Support.Tests.RichSemanticEditor.h"
#include "Support.Tests.RichSemanticGeometry.h"
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
#include "Embedded.Tests.FormLayoutBenchmark.h"
#endif
#include "Embedded.Tests.PreparedTreeAccessibilityBenchmark.h"
#include "Embedded.Tests.PreparedTreeAccessibilitySemantics.h"
#include "Embedded.Tests.PreparedTreeEvents.h"
#include "Embedded.Tests.PreparedTreeRootLifetime.h"

// Hidden and zero-extent views hold no surface; the next visible sized preparation reallocates exactly one and
// reproduces the previous pixels. Device replacement while hidden keeps working without a surface.
__declspec(noinline) static void TestSurfaceLifetime(GraphicsFixture& gpu)
{
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-drag-data-allocation"), "drag-data nested allocation is fatal in its owned child");
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

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
#pragma warning(pop)

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

// The application says which device a contact came from (PointerEvent::device): a touch contact that drags the slider shows
// its touch halo until it lifts, and an event that names no device is the mouse, whose drag shows none.
__declspec(noinline) static void TestEmbeddedTouchDragShowsTheSliderTouchHalo(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "touch halo scene");
    Hr(scene.view.Prepare(480, 240), "prepare touch halo scene");
    const D2D1_RECT_F thumb = scene.slider->DebugGetThumbRect();
    const float x           = (thumb.left + thumb.right) * 0.5f;
    const float y           = (thumb.top + thumb.bottom) * 0.5f;
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, x, y, 0u, 0.0f, DxUi::PointerDevice::Touch}), "a touch contact grabs the slider");
    Check(scene.view.Controls().GetPointerDevice() == DxUi::PointerDevice::Touch, "the host keeps the device of the event it handles");
    Check(scene.slider->DebugGetTouchHaloProgress() == 1.0f, "the slider shows its touch halo while the contact drags (reduced motion snaps it)");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, x, y, 0u, 0.0f, DxUi::PointerDevice::Touch}), "the contact lifts");
    Check(scene.slider->DebugGetTouchHaloProgress() == 0.0f, "and the halo is gone");
    Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, x, y}), "a press that names no device grabs the slider");
    Check(scene.view.Controls().GetPointerDevice() == DxUi::PointerDevice::Mouse, "an event that names no device is the mouse");
    Check(scene.slider->DebugGetTouchHaloProgress() == 0.0f, "whose drag shows no touch halo");
    static_cast<void>(scene.view.DispatchPointer({DxUi::PointerAction::Up, x, y}));
}

__declspec(noinline) static void TestTooltipInspectionDismissalConsumesEmbeddedGesture(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "tooltip gesture device pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool), "tooltip gesture embedded host");
    auto root    = std::make_unique<DxUi::Panel>();
    auto* button = root->AddChild<DxUi::Button>(L"Underlying action");
    button->SetBounds(D2D1::RectF(200.0f, 100.0f, 310.0f, 150.0f));
    size_t activations = 0u;
    button->SetOnClick([&] { ++activations; });
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320u, 180u), "prepare tooltip gesture host");
    Check(view.Controls().InspectTooltip(L"Persistent inspection", D2D1::Point2F(12.0f, 12.0f)), "embedded inspection opens");

    constexpr float x = 250.0f;
    constexpr float y = 125.0f;
    Check(view.DispatchPointer({DxUi::PointerAction::Down, x, y}) && ! view.Controls().IsTooltipInspectionActive(),
          "outside down dismisses and consumes the embedded gesture");
    Check(view.DispatchPointer({DxUi::PointerAction::Move, x, y}), "dismissal consumes embedded movement");
    Check(view.DispatchPointer({DxUi::PointerAction::Up, x, y}), "dismissal consumes embedded release");
    Check(activations == 0u, "the dismissing gesture cannot activate the underlying button");

    Check(view.DispatchPointer({DxUi::PointerAction::Down, x, y}), "a later independent embedded press reaches the button");
    Check(view.DispatchPointer({DxUi::PointerAction::Up, x, y}), "a later independent embedded release reaches the button");
    Check(activations == 1u, "normal interaction resumes after the consumed gesture completes");

    Check(view.Controls().InspectTooltip(L"Second inspection", D2D1::Point2F(12.0f, 12.0f)), "inspection reopens for cancellation");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, x, y}), "second outside down is consumed");
    Check(view.DispatchPointer({DxUi::PointerAction::Cancel}), "cancel consumes and clears the active dismissal gesture");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, x, y}), "a new press after cancellation reaches the button");
    Check(view.DispatchPointer({DxUi::PointerAction::Up, x, y}), "new release after cancellation reaches the button");
    Check(activations == 2u, "cancellation clears the dismissal latch without activating the canceled press");
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

// Evicting configured formats must invalidate visual-only retained layouts before this frame prepares its controls.
__declspec(noinline) static void TestEmbeddedIconLayoutRebuildsAfterFormatCacheEviction(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "format-eviction pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool), "format-eviction view");
    auto root    = std::make_unique<DxUi::Panel>();
    auto* button = root->AddChild<DxUi::Button>(L"\uE8DD");
    button->SetVariant(DxUi::ButtonVariant::IconOnly);
    button->SetBounds(D2D1::RectF(8, 8, 88, 88));
    Hr(button->SetContentFontSizeDip(18.0f), "set embedded icon text size");
    view.Controls().SetRoot(std::move(root));

    const auto capture = [&]()
    {
        Hr(PrepareRichEmbeddedContent(view, 320, 120), "prepare format-eviction view");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "composite format-eviction view");
        std::vector<uint8_t> pixels;
        Hr(gpu.Read(pixels), "read format-eviction pixels");
        return pixels;
    };
    const auto firstPixels = capture();
    Check(button->DebugGetPreparedIconTextLayoutBuildCount() == 1u && button->DebugGetPreparedIconTextLayoutPaintCount() > 0u,
          "the embedded icon prepares and paints one retained layout before eviction");

    auto& controls = view.Controls();
    for (int role = 0;
         role <= static_cast<int>(DxUi::FontRole::Small) && view.GetStatistics().cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit;
         ++role)
    {
        for (const auto alignment :
             {DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_TEXT_ALIGNMENT_JUSTIFIED})
        {
            for (const auto paragraph : {DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_PARAGRAPH_ALIGNMENT_CENTER})
            {
                for (const bool wrap : {false, true})
                {
                    Check(controls.GetTextFormat(DxUi::FontRole(role), alignment, paragraph, wrap) != nullptr,
                          "populate distinct formats before embedded eviction");
                }
            }
        }
    }
    Check(view.GetStatistics().cachedTextFormats > DxUi::ControlHost::kConfiguredTextFormatCacheLimit,
          "the embedded fixture overflows the bounded configured-format cache");

    const uint64_t buildsBeforeEviction  = button->DebugGetPreparedIconTextLayoutBuildCount();
    const uint64_t lookupsBeforeEviction = button->DebugGetPreparedIconTextLayoutFactoryLookupCount();
    const uint64_t paintsBeforeEviction  = button->DebugGetPreparedIconTextLayoutPaintCount();
    view.MarkDirty();
    const auto evictedPixels = capture();
    Check(view.GetStatistics().cachedTextFormats <= DxUi::ControlHost::kConfiguredTextFormatCacheLimit,
          "the next embedded update trims the configured-format cache before painting");
    Check(button->DebugGetPreparedIconTextLayoutBuildCount() == buildsBeforeEviction + 1u &&
              button->DebugGetPreparedIconTextLayoutFactoryLookupCount() == lookupsBeforeEviction + 1u,
          "format eviction prepares exactly one replacement layout through the factory");
    Check(button->DebugGetPreparedIconTextLayoutPaintCount() > paintsBeforeEviction, "the post-eviction embedded frame paints the prepared layout");
    Check(firstPixels == evictedPixels, "format eviction preserves every embedded BGRA pixel");

    const uint64_t warmBuilds  = button->DebugGetPreparedIconTextLayoutBuildCount();
    const uint64_t warmLookups = button->DebugGetPreparedIconTextLayoutFactoryLookupCount();
    view.MarkDirty();
    const auto warmPixels = capture();
    Check(button->DebugGetPreparedIconTextLayoutBuildCount() == warmBuilds && button->DebugGetPreparedIconTextLayoutFactoryLookupCount() == warmLookups,
          "a subsequent warm embedded preparation adds no layout or factory lookup");
    Check(evictedPixels == warmPixels, "the warm embedded frame remains pixel-identical after eviction");
    button->SetVariant(DxUi::ButtonVariant::Hyperlink);
    button->SetText(L"Embedded hyperlink");
    button->SetBounds(D2D1::RectF(8.f, 8.f, 160.f, 40.f));
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare embedded hyperlink");
    static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Move, 20, 20}));
    static_cast<void>(capture());
    Check(button->IsHovered() && button->DebugGetPreparedHyperlinkWidthDip() > 0.f && button->DebugGetHyperlinkMeasureCount() > 0u,
          "first embedded hovered hyperlink frame has prepared underline geometry");
    const auto measureLayouts    = DxUi::testSingleLineMeasureLayoutCreations;
    const auto hyperlinkMeasures = button->DebugGetHyperlinkMeasureCount();
    gpu.Bind();
    const auto beforeHoverPaints = allocations;
    countAllocations             = true;
    for (unsigned frame = 0u; frame < 8u; ++frame)
    {
        view.MarkDirty();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "repeat hovered hyperlink paint");
    }
    countAllocations = false;
    Check(allocations == beforeHoverPaints && DxUi::testSingleLineMeasureLayoutCreations == measureLayouts &&
              button->DebugGetHyperlinkMeasureCount() == hyperlinkMeasures,
          "repeated hovered embedded paints allocate no C++ memory or measuring layouts");
    Hr(button->SetContentFontSizeDip(24.f), "change embedded hyperlink font size");
    static_cast<void>(capture());
    Check(button->DebugGetHyperlinkMeasureCount() == hyperlinkMeasures + 1u, "font-size change prepares a new hyperlink underline");
}

__declspec(noinline) static void TestPointerDoesNotRetargetAfterHoverOrHitGeometryMutation(GraphicsFixture& gpu)
{
    struct State final
    {
        std::function<void()> mutateGeometry;
        size_t mutations = 0u;
        size_t actions   = 0u;
        bool checked     = false;
        bool rightAction = false;
        void Mutate()
        {
            auto callback = std::move(mutateGeometry);
            if (callback)
            {
                ++mutations;
                callback();
            }
        }
    };
    class Model final : public DxUi::IGridModel
    {
    public:
        explicit Model(State& state) noexcept : _state(&state)
        {
        }
        size_t GetRowCount() const noexcept override
        {
            return 1u;
        }
        size_t GetColumnCount() const noexcept override
        {
            return 2u;
        }
        DxUi::GridColumnDesc GetColumn(size_t column) const override
        {
            return {.id       = column == 0u ? L"check" : L"text",
                    .title    = column == 0u ? L"Check" : L"Text",
                    .widthDip = 160.0f,
                    .kind     = column == 0u ? DxUi::GridColumnKind::Checkbox : DxUi::GridColumnKind::Text};
        }
        std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
        {
            return id == 0u ? std::optional<size_t>(0u) : std::nullopt;
        }
        void GetCellData(size_t, size_t column, DxUi::GridCellData& cell) const override
        {
            cell.kind    = column == 0u ? DxUi::GridCellKind::Checkbox : DxUi::GridCellKind::Text;
            cell.text    = column == 0u ? L"" : L"Text cell";
            cell.checked = _state->checked;
            _state->Mutate();
        }

    private:
        State* _state;
    };
    class Delegate final : public DxUi::IGridDelegate
    {
    public:
        using DxUi::IGridDelegate::OnGridCheckboxToggled;
        explicit Delegate(State& state) noexcept : _state(&state)
        {
        }
        void OnGridCheckboxToggled(size_t row, size_t column, bool checked) override
        {
            Check(row == 0u && column == 0u, "the recovered click toggles the stable checkbox cell");
            ++_state->actions;
            _state->checked = checked;
        }

    private:
        State* _state;
    };
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class MutatingControl final : public DxUi::Control
    {
    public:
        using DxUi::Control::HitTest;
        MutatingControl(State& state, bool mutateFromHit) noexcept : _state(&state), _mutateFromHit(mutateFromHit)
        {
        }
        void Paint(DxUi::ControlHost& host) const override
        {
            const auto bounds  = GetBounds();
            const float middle = (bounds.left + bounds.right) * 0.5f;
            if (auto* dc = host.GetDeviceContext())
            {
                if (auto* brush = host.GetSolidBrush(D2D1::ColorF(0.12f, 0.72f, 0.24f, 1.0f)))
                    dc->FillRectangle(IsRightToLeft() ? D2D1::RectF(middle, bounds.top, bounds.right, bounds.bottom)
                                                      : D2D1::RectF(bounds.left, bounds.top, middle, bounds.bottom),
                                      brush);
            }
        }
        DxUi::Control* HitTest(D2D1_POINT_2F point) override
        {
            auto* result = Control::HitTest(point);
            if (result && _mutateFromHit)
                _state->Mutate();
            return result;
        }
        void OnHoverChanged(DxUi::ControlHost& host, bool hovered) override
        {
            Control::OnHoverChanged(host, hovered);
            if (hovered && ! _mutateFromHit)
                _state->Mutate();
        }
        bool OnMouseDown(DxUi::ControlHost&, D2D1_POINT_2F point, bool, UINT) override
        {
            ++_state->actions;
            const auto bounds   = GetBounds();
            _state->rightAction = (point.x >= (bounds.left + bounds.right) * 0.5f) != IsRightToLeft();
            return true;
        }

    private:
        State* _state;
        bool _mutateFromHit;
    };
#pragma warning(pop)
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "graphics for pointer reentrancy tests");
    for (const unsigned mutationSource : {0u, 1u, 2u})
    {
        State state;
        Model model(state);
        Delegate delegate(state);
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "attach pointer reentrancy test view");
        auto root             = std::make_unique<DxUi::Panel>();
        DxUi::Control* target = nullptr;
        if (mutationSource == 0u)
        {
            auto* grid = root->AddChild<DxUi::Grid>();
            grid->SetHeaderHeightDip(24.0f);
            grid->SetRowHeightDip(24.0f);
            grid->SetModel(&model);
            grid->SetDelegate(&delegate);
            target = grid;
        }
        else
            target = root->AddChild<MutatingControl>(state, mutationSource == 2u);
        target->SetBounds(D2D1::RectF(0, 0, 320, 160));
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(320, 160), "prepare LTR pointer geometry");
        state.mutateGeometry = [&] { target->SetFlowDirection(DxUi::FlowDirection::RightToLeft); };
        Check(! view.DispatchPointer({DxUi::PointerAction::Down, 240, 36}), "hover or hit mutation cannot retarget the current pointer Down");
        Check(state.mutations == 1u && state.actions == 0u && ! state.checked,
              "the one-shot geometry mutation performs no action against previously painted LTR content");
        Check(view.NeedsPreparation() && view.Controls().GetCapturedControl() == nullptr,
              "geometry-mutated pointer dispatch leaves preparation pending without capturing a rejected press");
        gpu.Bind();
        Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "the outdated pointer surface cannot be composited");
        Hr(view.Prepare(320, 160), "prepare the current RTL pointer geometry");
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 240, 36}), "a retried Down reaches the newly painted RTL target");
        Check(state.actions == 1u && (mutationSource == 0u ? state.checked : ! state.rightAction),
              "only the retried press activates the logical action now displayed under the pointer");
        static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, 240, 36}));
        Check(view.Controls().GetCapturedControl() == nullptr, "the recovered action releases pointer capture");
    }
}

__declspec(noinline) static void SeparatorCatalogFaultOrigin()
{
    std::unique_ptr<DxUi::Control> output;
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)DxUi::CreateControl(DxUi::ControlKind::Separator, output);
}
static void TestSeparatorCatalogAllocationFailures()
{
    static_assert(std::is_nothrow_default_constructible_v<DxUi::Separator>);
    std::unique_ptr<DxUi::Control> output;
    const auto oldCount    = allocations;
    const auto oldCounting = countAllocations;
    allocations            = 0u;
    countAllocations       = true;
    const HRESULT success  = DxUi::CreateControl(DxUi::ControlKind::Separator, output);
    countAllocations       = oldCounting;
    const size_t observed  = allocations;
    allocations            = oldCount;
    Check(success == S_OK && dynamic_cast<DxUi::Separator*>(output.get()) && observed >= 2u,
          "catalog success observes both object and lifetime-token allocation");
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-separator-catalog-allocation"), "separator factory allocation terminates in its owned child");
}
// Keep unrelated functional-test locals out of the benchmark entry stack, even under LTCG.
[[noreturn]] __declspec(noinline) static void EmbeddedPaintFailureOrigin(const std::runtime_error& error)
{
    embeddedCallbackOriginMarker.store(1u, std::memory_order_relaxed);
    throw error;
}
__declspec(noinline) static void TestPreparationCallbacksCanRetireRoot(GraphicsFixture& gpu, bool fatalChild = false)
{
    struct State final
    {
        std::function<void()> paint;
        std::function<void()> overlay;
        std::function<void()> dpi;
        size_t paintCalls   = 0u;
        size_t overlayCalls = 0u;
    };
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class CallbackRoot final : public DxUi::Panel
    {
    public:
        explicit CallbackRoot(State& state) noexcept : _state(&state)
        {
        }
        void Paint(DxUi::ControlHost&) const override
        {
            State* const state = _state;
            ++state->paintCalls;
            auto callback = std::move(state->paint);
            if (callback)
                callback();
        }
        void PaintOverlay(DxUi::ControlHost&) const override
        {
            State* const state = _state;
            ++state->overlayCalls;
            auto callback = std::move(state->overlay);
            if (callback)
                callback();
        }
        void OnHostDpiChanged(DxUi::ControlHost&) noexcept override
        {
            auto callback = std::move(_state->dpi);
            if (callback)
                callback();
        }

    private:
        State* _state;
    };
#pragma warning(pop)

    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "graphics for preparation retirement tests");
    for (const bool duringOverlay : {false, true})
    {
        for (const bool replaceRoot : {false, true})
        {
            State state;
            DxUi::EmbeddedHost view;
            size_t preparationRequests = 0u;
            DxUi::EmbeddedCallbacks callbacks{};
            callbacks.context            = &preparationRequests;
            callbacks.requestPreparation = [](void* context) noexcept { ++*static_cast<size_t*>(context); };
            Hr(view.Attach(graphics, callbacks), "attach preparation retirement host");
            view.Controls().SetRoot(std::make_unique<CallbackRoot>(state));
            auto& callback              = duringOverlay ? state.overlay : state.paint;
            callback                    = [&] { view.Controls().SetRoot(replaceRoot ? std::make_unique<DxUi::Panel>() : nullptr); };
            const size_t requestsBefore = preparationRequests;
            Check(view.Prepare(320, 160) == E_ABORT, "preparation aborts the frame whose paint callback retired the root");
            Check(preparationRequests == requestsBefore + 1u, "the aborted frame coalesces one request for its replacement after callbacks unwind");
            Check(state.paintCalls == 1u && state.overlayCalls == (duringOverlay ? 1u : 0u), "no stale overlay follows root retirement during painting");
            Check(view.NeedsPreparation(), "root replacement remains pending after the aborted frame");
            gpu.Bind();
            Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "the incomplete old-root surface cannot compose");
            Hr(view.Prepare(320, 160), "prepare the current root after retirement");
            Check(! view.NeedsPreparation(), "successful current-root preparation settles pending work");
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose the coherent current-root surface");
        }
    }
    for (const bool replaceRoot : {false, true})
    {
        State state;
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "attach DPI retirement host");
        view.Controls().SetRoot(std::make_unique<CallbackRoot>(state));
        state.dpi = [&] { view.Controls().SetRoot(replaceRoot ? std::make_unique<DxUi::Panel>() : nullptr); };
        Hr(view.Prepare(320, 160, 144.0f), "DPI notification may replace or clear the root before layout");
        Check(state.paintCalls == 0u && state.overlayCalls == 0u, "DPI retirement does not paint the old root");
    }
    {
        State state;
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "attach failing preparation host");
        view.Controls().SetRoot(std::make_unique<CallbackRoot>(state));
        if (fatalChild)
        {
            state.paint = []
            {
                const std::runtime_error error("injected paint failure");
                EmbeddedPaintFailureOrigin(error);
            };
            (void)view.Prepare(320, 160);
            return;
        }
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-paint-callback-throw"), "paint callback exception terminates in its owned child");
        Hr(view.Prepare(320, 160), "healthy painting succeeds");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "healthy painting composes");
        Check(state.paintCalls == 1u && state.overlayCalls == 1u, "successful preparation reaches overlay painting");
    }
}

__declspec(noinline) static void TestPageReplacementDuringPaintDefersBoundsUntilPreparation(GraphicsFixture& gpu)
{
    struct State final
    {
        DxUi::PageHost* pageHost   = nullptr;
        DxUi::Control* replacement = nullptr;
        size_t calls               = 0u;
    };
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class ReplacingPage final : public DxUi::Control
    {
    public:
        explicit ReplacingPage(State& state) noexcept : _state(&state)
        {
        }
        void Paint(DxUi::ControlHost&) const override
        {
            State* const state = _state;
            if (++state->calls == 1u)
            {
                auto next          = std::make_unique<DxUi::Panel>();
                state->replacement = next.get();
                state->pageHost->SetPage(std::move(next));
            }
        }

    private:
        State* _state;
    };
#pragma warning(pop)
    State state;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "graphics for deferred page bounds");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(graphics), "attach deferred page bounds host");
    auto pageHost  = std::make_unique<DxUi::PageHost>();
    state.pageHost = pageHost.get();
    pageHost->SetPage(std::make_unique<ReplacingPage>(state));
    view.Controls().SetRoot(std::move(pageHost));
    Check(view.Prepare(320, 160) == E_ABORT, "page replacement discards the partially painted embedded frame");
    Check(state.calls == 1u && state.pageHost->GetPage() == state.replacement, "the paint callback installs one replacement page");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "deferred page geometry cannot be composited");
    Hr(view.Prepare(320, 160), "the next explicit preparation applies page layout");
    const auto bounds = state.replacement->GetBounds();
    Check(bounds.left == 0.0f && bounds.top == 0.0f && bounds.right == 320.0f && bounds.bottom == 160.0f,
          "replacement page receives complete current bounds without resize or Advance");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose the fully laid out replacement page");
}

// A model may synchronously change while the Grid asks for a cell during paint. The old row image and hit geometry must
// not become a coherent embedded frame after replacing the model, notifying a changed row count or replacing the delegate.
__declspec(noinline) static void TestGridModelMutationDuringCellPaintAbortsPreparation(GraphicsFixture& gpu)
{
    struct QueryState final
    {
        std::function<void()> onNextCellQuery;
        size_t cellQueryCount = 0u;
        size_t callbackCount  = 0u;
    };
    struct Model final : DxUi::IGridModel
    {
        Model(QueryState& queryState, size_t rowTotal) noexcept : state(&queryState), rows(rowTotal)
        {
        }

        size_t GetRowCount() const noexcept override
        {
            return rows;
        }
        size_t GetColumnCount() const noexcept override
        {
            return 1u;
        }
        DxUi::GridColumnDesc GetColumn(size_t) const override
        {
            return {L"value", L"Value", 240.0f};
        }
        void GetCellData(size_t row, size_t, DxUi::GridCellData& cell) const override
        {
            cell.text = L"Row " + std::to_wstring(row);
            ++state->cellQueryCount;
            auto callback = std::move(state->onNextCellQuery);
            if (callback)
            {
                ++state->callbackCount;
                callback();
            }
        }
        std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
        {
            return id < rows ? std::optional<size_t>(static_cast<size_t>(id)) : std::nullopt;
        }

        QueryState* state = nullptr;
        size_t rows       = 0u;
    };

    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "graphics for reentrant Grid model query tests");

    for (const unsigned mutation : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u})
    {
        QueryState queryState;
        Model originalModel(queryState, 2u);
        Model replacementModel(queryState, 5u);
        DxUi::IGridDelegate replacementDelegate;
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "attach Grid model query host");
        auto root  = std::make_unique<DxUi::Panel>();
        auto* grid = root->AddChild<DxUi::Grid>();
        grid->SetBounds(D2D1::RectF(0, 0, 320, 160));
        grid->SetHeaderHeightDip(24.0f);
        grid->SetRowHeightDip(24.0f);
        if (mutation == 9u)
            grid->SetFlowDirection(DxUi::FlowDirection::RightToLeft);
        if (mutation == 10u)
            grid->SetDensity(DxUi::Density::Compact);
        grid->SetModel(&originalModel);
        view.Controls().SetRoot(std::move(root));

        Hr(view.Prepare(320, 160), "prepare the initial Grid model");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose the initial Grid model");
        Check(! view.NeedsPreparation(), "the initial Grid model has coherent prepared geometry");
        auto colorOnlyTheme             = view.Controls().GetTheme();
        colorOnlyTheme.windowBackground = D2D1::ColorF(0.12f, 0.18f, 0.24f, 1.0f);
        view.Controls().SetTheme(colorOnlyTheme);
        Check(view.NeedsPreparation() && view.DispatchPointer({DxUi::PointerAction::Move, 160, 50}),
              "a color-only theme update outside paint permits pointer dispatch on still-current geometry");
        Hr(view.Prepare(320, 160), "prepare the color-only theme update");

        queryState.cellQueryCount  = 0u;
        queryState.callbackCount   = 0u;
        queryState.onNextCellQuery = [&]
        {
            switch (mutation)
            {
                case 0u:
                    originalModel.rows = 5u;
                    grid->NotifyDataChanged();
                    break;
                case 1u: grid->SetModel(&replacementModel); break;
                case 2u: grid->SetDelegate(&replacementDelegate); break;
                case 3u: grid->SetFlowDirection(DxUi::FlowDirection::RightToLeft); break;
                case 4u: grid->SetDensity(DxUi::Density::Compact); break;
                case 5u: grid->SetRowHeightDip(36.0f); break;
                case 6u: grid->SetEffectiveRowHeightDip(40.0f); break;
                case 7u: grid->SetHeaderHeightDip(40.0f); break;
                case 8u:
                case 11u:
                {
                    auto theme = view.Controls().GetTheme();
                    if (mutation == 8u)
                        theme.density = DxUi::Density::Compact;
                    else
                        theme.windowBackground = D2D1::ColorF(0.24f, 0.18f, 0.12f, 1.0f);
                    view.Controls().SetTheme(theme);
                    break;
                }
                case 9u: grid->ClearFlowDirection(); break;
                case 10u: grid->ClearDensity(); break;
            }
        };
        view.MarkDirty();

        Check(view.Prepare(320, 160) == E_ABORT, "cell-query model mutation aborts the partially painted Grid frame");
        Check(queryState.callbackCount == 1u && queryState.cellQueryCount > 0u, "the external one-shot callback runs from GetCellData exactly once");
        Check(view.NeedsPreparation(), "the aborted model-change frame remains pending preparation");
        gpu.Bind();
        Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "the partial model-change frame is not composited");
        Check(! view.DispatchPointer({DxUi::PointerAction::Down, 16, 40}), "pointer dispatch refuses geometry from the aborted model-change frame");

        Hr(view.Prepare(320, 160), "prepare the current Grid model and row list");
        Check(! view.NeedsPreparation(), "successful preparation settles the current model geometry");
        DxUi::IGridModel* const currentModel =
            mutation == 1u ? static_cast<DxUi::IGridModel*>(&replacementModel) : static_cast<DxUi::IGridModel*>(&originalModel);
        Check(grid->GetModel() == currentModel, "recovery preparation keeps the current model installed");
        const size_t expectedRows = mutation >= 2u ? 2u : 5u;
        Check(grid->GetVisibleRowCount() == expectedRows && grid->GetVisibleRowAt(expectedRows - 1u) == std::optional<size_t>(expectedRows - 1u),
              "recovery preparation exposes the current visible row list");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose the fully prepared current Grid model");
        Check(queryState.callbackCount == 1u, "recovery does not repeat the consumed one-shot callback");
        grid->SetFlowDirection(grid->IsRightToLeft() ? DxUi::FlowDirection::LeftToRight : DxUi::FlowDirection::RightToLeft);
        Check(view.NeedsPreparation() && ! view.DispatchPointer({DxUi::PointerAction::Down, 160, 50}),
              "an external flow change revokes prepared pointer geometry before another paint begins");
        Hr(view.Prepare(320, 160), "prepare externally changed flow geometry");
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 160, 50}), "fresh preparation restores pointer dispatch against current row geometry");
        static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, 160, 50}));
        Check(view.Controls().GetCapturedControl() == nullptr, "release the recovered row-selection gesture regardless of its handled return value");
    }
}

__declspec(noinline) static void TestTreeReorderWakeRetiresOwner(GraphicsFixture& gpu)
{
    using namespace DxUi;
    struct Model final : ITreeModel
    {
        size_t GetVisibleItemCount() const noexcept override
        {
            return 3u;
        }
        void GetVisibleItem(size_t index, TreeItemData& item) const override
        {
            item = {.id = index + 1u, .text = L"Row " + std::to_wstring(index + 1u)};
        }
        std::optional<size_t> FindVisibleItemById(uint64_t id) const noexcept override
        {
            return id >= 1u && id <= 3u ? std::optional<size_t>(static_cast<size_t>(id - 1u)) : std::nullopt;
        }
    } model;
    struct Delegate final : ITreeDelegate
    {
        unsigned reorders{}, menus{};
        void OnTreeReorder(const TreeDrop&) override
        {
            ++reorders;
        }
        void OnTreeContextMenu(uint64_t, POINT) override
        {
            ++menus;
        }
    } delegate;
    struct Wake
    {
        EmbeddedHost* view{};
        bool armed{};
        unsigned retirements{};
    } wake;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create Tree reorder retirement graphics");
    EmbeddedHost view;
    wake.view = &view;
    EmbeddedCallbacks callbacks{};
    callbacks.context            = &wake;
    callbacks.requestPreparation = [](void* context) noexcept
    {
        auto& state = *static_cast<Wake*>(context);
        if (std::exchange(state.armed, false))
        {
            ++state.retirements;
            state.view->Controls().SetRoot(nullptr);
        }
    };
    Hr(view.Attach(graphics, callbacks), "attach Tree reorder retirement host");
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0, 0, 220, 160));
    tree->SetReorderEnabled(true);
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 320, 160), "prepare Tree reorder retirement fixture");
    const auto first = tree->GetVisibleItemHitRect(0u), third = tree->GetVisibleItemHitRect(2u);
    Check(first && third, "Tree reorder retirement rows have actual hit rectangles");
    const auto from   = D2D1::Point2F((first->left + first->right) * .5f, (first->top + first->bottom) * .5f);
    const auto target = D2D1::Point2F((third->left + third->right) * .5f, third->top + 2.f);
    Check(tree->OnMouseDown(view.Controls(), from, false, 0u) && tree->OnMouseMove(view.Controls(), target, 0u),
          "Tree reorder retirement fixture arms a real drop");
    Hr(PrepareRichEmbeddedContent(view, 320, 160), "consume the reorder move wake before release");
    wake.armed = true;
    static_cast<void>(tree->OnMouseUp(view.Controls(), target, false, 0u));
    Check(wake.retirements == 1u && ! view.Controls().GetRoot() && delegate.reorders == 0u,
          "release scheduling can retire the Tree before reorder dispatch without invoking its stale delegate");
    for (const bool rightClick : {true, false})
    {
        EmbeddedHost nextView;
        Wake nextWake{.view = &nextView};
        callbacks.context = &nextWake;
        Hr(nextView.Attach(graphics, callbacks), "attach Tree cancellation retirement host");
        auto nextRoot  = std::make_unique<Panel>();
        auto* nextTree = nextRoot->AddChild<Tree>();
        nextTree->SetBounds(D2D1::RectF(0, 0, 220, 160));
        nextTree->SetReorderEnabled(true);
        nextTree->SetModel(&model);
        nextTree->SetDelegate(&delegate);
        nextView.Controls().SetRoot(std::move(nextRoot));
        Hr(PrepareRichEmbeddedContent(nextView, 320, 160), "prepare Tree cancellation retirement fixture");
        const auto nextFirst = nextTree->GetVisibleItemHitRect(0u);
        Check(nextFirst.has_value(), "Tree cancellation retirement row is hittable");
        const auto press = D2D1::Point2F((nextFirst->left + nextFirst->right) * .5f, (nextFirst->top + nextFirst->bottom) * .5f);
        Check(nextTree->OnMouseDown(nextView.Controls(), press, false, 0u), "arm Tree reorder before cancellation");
        Hr(PrepareRichEmbeddedContent(nextView, 320, 160), "consume the initial Tree press wake");
        nextWake.armed = true;
        if (rightClick)
            static_cast<void>(nextTree->OnMouseDown(nextView.Controls(), press, true, 0u));
        else
            nextTree->OnCaptureLost(nextView.Controls());
        Check(nextWake.retirements == 1u && ! nextView.Controls().GetRoot() && delegate.menus == 0u && delegate.reorders == 0u,
              "cancelling Tree reorder stops after a synchronous wake retires its owner");
    }
}

__declspec(noinline) static void TestTreeDelegateMutationDuringItemPaintAbortsPreparation(GraphicsFixture& gpu)
{
    struct QueryState final
    {
        std::function<void()> onNextItemQuery;
        size_t callbackCount = 0u;
    };
    struct Model final : DxUi::ITreeModel
    {
        explicit Model(QueryState& queryState) noexcept : state(&queryState)
        {
        }
        size_t GetVisibleItemCount() const noexcept override
        {
            return 2u;
        }
        void GetVisibleItem(size_t index, DxUi::TreeItemData& item) const override
        {
            item          = {.id          = index + 1u,
                             .text        = L"Item " + std::to_wstring(index),
                             .depth       = static_cast<uint32_t>(index),
                             .hasChildren = index == 0u,
                             .expanded    = true};
            auto callback = std::move(state->onNextItemQuery);
            if (callback)
            {
                ++state->callbackCount;
                callback();
            }
        }
        std::optional<size_t> FindVisibleItemById(uint64_t id) const noexcept override
        {
            return id >= 1u && id <= 2u ? std::optional<size_t>(static_cast<size_t>(id - 1u)) : std::nullopt;
        }
        QueryState* state;
    };
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "graphics for reentrant Tree delegate query test");
    for (const unsigned mutation : {0u, 1u, 2u})
    {
        QueryState state;
        Model model(state);
        DxUi::ITreeDelegate replacementDelegate;
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "attach Tree delegate query host");
        auto root  = std::make_unique<DxUi::Panel>();
        auto* tree = root->AddChild<DxUi::Tree>();
        tree->SetBounds(D2D1::RectF(0, 0, 320, 160));
        tree->SetModel(&model);
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(320, 160), "prepare the initial Tree delegate binding");
        state.onNextItemQuery = [&]
        {
            switch (mutation)
            {
                case 0u: tree->SetDelegate(&replacementDelegate); break;
                case 1u: tree->SetRowHeightDip(36.0f); break;
                case 2u: tree->SetIndentDip(32.0f); break;
            }
        };
        view.MarkDirty();
        Check(view.Prepare(320, 160) == E_ABORT, "item-query delegate replacement aborts the partially painted Tree frame");
        Check(state.callbackCount == 1u && view.NeedsPreparation(), "one-shot delegate replacement leaves Tree preparation pending");
        gpu.Bind();
        Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "partial Tree delegate-change frame is not composited");
        Check(! view.DispatchPointer({DxUi::PointerAction::Down, 16, 40}), "pointer dispatch refuses the aborted Tree frame geometry");
        Hr(view.Prepare(320, 160), "prepare Tree with the current delegate binding");
        Check(state.callbackCount == 1u && ! view.NeedsPreparation(), "recovery settles the new Tree binding without repeating the callback");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose the fully prepared current Tree binding");
    }
}

// Keep unrelated functional-test locals out of the benchmark entry stack, even under LTCG.
__declspec(noinline) static int RunFunctionalTests()
{
    if (__argc == 4 && std::wstring_view(__wargv[1]) == L"--capture-rich-images")
    {
        const std::wstring_view theme(__wargv[3]);
        Check(theme == L"Dark" || theme == L"Light", "image capture requires Dark or Light");
        CaptureRichImageSample(__wargv[2], theme == L"Dark");
        return 0;
    }
    if (__argc == 2 &&
        (std::wstring_view(__wargv[1]) == L"--test-rich-projection-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-snapshot-slice-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-layout-clear" || std::wstring_view(__wargv[1]) == L"--test-rich-flowchart-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-image-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-image-decode-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-image-resource-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-file-reference-paste" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-geometry-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-flat-projection" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-markdown-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-semantic-geometry-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-semantic-document-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-semantic-uia-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-semantic-query-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-command-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-command-host-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-footnote-failures" ||
         std::wstring_view(__wargv[1]) == L"--test-rich-capacity-failures" || std::wstring_view(__wargv[1]) == L"--test-rich-input-failures"))
    {
        // Checked refusal probes use the same host and fatal path as the full suite.
        if (std::wstring_view(__wargv[1]) == L"--test-rich-input-failures")
        {
            TestRichAppearanceConstructionFailureAtomicity();
            TestRichInputAllocationFailureAtomicity();
            GraphicsFixture gpu;
            Hr(gpu.Create(), "task allocation failure WARP device");
            TestRichTaskPlanAllocationFailureKeepsPointerSelection(gpu);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-layout-clear")
            TestRichPreparedLayoutClearAllocationBudget(true);
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-geometry-failures")
        {
            TestRichPreparedLayoutValueFailureAtomicity(true);
            TestRichDocumentGeometryFailureAtomicity(true);
            TestRichDocumentGeometryFailureAtomicity(true, true);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-flat-projection")
            TestRichFlatProjectionFailureAtomicity(true);
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-semantic-geometry-failures")
            TestRichSemanticGeometryFailureBudgets(true);
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-semantic-document-failures")
            TestRichSemanticDocumentFailureAndStorageBudgets(true);
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-semantic-uia-failures")
        {
            GraphicsFixture gpu;
            Hr(gpu.Create(), "semantic UIA failure WARP device");
            TestRichSemanticAccessibilityFailureAtomicity(gpu);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-command-failures")
            TestRichMarkdownCommandFailureAtomicity();
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-command-host-failures")
        {
            GraphicsFixture gpu;
            Hr(gpu.Create(), "command host failure WARP device");
            TestRichMarkdownCommandHostFailureAtomicity(gpu);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-semantic-query-failures")
        {
            GraphicsFixture gpu;
            Hr(gpu.Create(), "query failure isolated borrowed WARP device");
            TestRichSemanticEditorQueryFailureBudgets(gpu, true);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-snapshot-slice-failures")
            TestRichSnapshotSliceAllocationFailureAtomicity(true);
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-capacity-failures")
        {
            GraphicsFixture gpu;
            Hr(gpu.Create(), "capacity isolated borrowed WARP device");
            TestEmbeddedParserCapacitySourceFallback(gpu);
            TestParserCapacityMemoryFailureIsRetryable(gpu);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-footnote-failures")
        {
            TestFootnoteBlockAllocationFailures(true);
            TestRichSemanticGeometryFailureBudgets(true, true);
            TestRichSemanticGeometryFailureBudgets(true, false, true);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-image-failures")
            TestRichImagePathAllocationFailures();
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-file-drop-failures")
            TestRichFileDropTransportAllocationFailures();
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-file-reference-paste")
        {
            GraphicsFixture gpu;
            Hr(gpu.Create(), "file reference isolated WARP device");
            TestEmbeddedImagePathPaste(gpu);
            TestEmbeddedFileReferencePaste(gpu);
            TestRichImagePathAllocationFailures();
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-image-decode-failures")
            TestRichImageDecoderAllocationFailures();
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-image-resource-failures")
        {
            TestRichImageResourceAllocationFailures();
            TestRichImageGeometryAllocationFailures();
            TestRichImageRasterFailureAndBudgets();
            TestRichImageFrameHostFailure();
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-flowchart-failures")
        {
            TestNativeFlowchartAllocationFailureAtomicity(true);
            TestNativeFlowchartLayoutAllocationFailureAtomicity(true);
            TestPreparedFlowchartAllocationFailures(true);
            TestSemanticDiagramAllocationFailures(true);
            TestScopedSemanticSourceAllocationFailures(true);
        }
        else if (std::wstring_view(__wargv[1]) == L"--test-rich-markdown-failures")
        {
            TestRichMarkdownParserFailureAndStorageBudgets(true);
            TestRichFrontMatterAllocationFailureAtomicity(true);
        }
        else
            TestRichProjectionPreparationAllocationFailureAtomicity(true);
        return 0;
    }
    // Isolated resource cases retain their individual failing baseline logs;
    // the ordinary suite below still executes both behavior budgets.
    if (__argc == 3 && std::wstring_view(__wargv[1]) == L"--test-rich-document-storage")
    {
        const std::wstring_view test(__wargv[2]);
        Check(test == L"Retained" || test == L"SameContent" || test == L"Recorded" || test == L"RecordedFailures", "known rich source storage case");
        if (test == L"Retained")
            TestRichDocumentFragmentedHistoryStorageBudget();
        else if (test == L"SameContent")
            TestRichDocumentSameContentAllocationBudget();
        else if (test == L"Recorded")
        {
            RichTextStateReplacementTests::Core(Check);
            TestRichRecordedWholeTextStorageBudget();
        }
        else
            TestRichDocumentAllocationFailureAtomicity();
        return 0;
    }
    if (__argc >= 2 && std::wstring_view(__wargv[1]) == L"--benchmark-complex-ui-boundaries-v3")
    {
        ComplexUiBoundaries::WarpRasterMode mode{};
        ComplexUiBoundaries::Scenario scenario{};
        bool dirty = false;
        if (__argc != 7 || (std::wstring_view(__wargv[3]) != L"Off" && std::wstring_view(__wargv[3]) != L"On") ||
            ! ComplexUiBoundaries::TryParseRasterMode(__wargv[4], mode) || ! ComplexUiBoundaries::TryParseScenario(__wargv[5], scenario) ||
            ! ComplexUiBoundaries::TryParsePhase(__wargv[6], dirty))
        {
            std::fputs("Boundary v3 diagnostic requires output, Off|On, Default|CallingThread, an exact scenario and Clean|Dirty.\n", stderr);
            return 1;
        }
        ComplexUiBoundaries::Run(__wargv[2], std::wstring_view(__wargv[3]) == L"On", mode, scenario, dirty);
        return 0;
    }
    if (__argc >= 2 && std::wstring_view(__wargv[1]) == L"--benchmark-complex-ui-boundaries")
    {
        if (__argc != 4 && __argc != 5)
        {
            std::fputs("Boundary diagnostic requires output, Off|On and optional Default|CallingThread.\n", stderr);
            return 1;
        }
        const std::wstring_view timers(__wargv[3]);
        if (timers != L"Off" && timers != L"On")
        {
            std::fputs("Unknown boundary diagnostic timer mode.\n", stderr);
            return 1;
        }
        std::optional<ComplexUiBoundaries::WarpRasterMode> mode;
        if (__argc == 5)
        {
            auto parsed = ComplexUiBoundaries::WarpRasterMode::Default;
            if (! ComplexUiBoundaries::TryParseRasterMode(__wargv[4], parsed))
            {
                std::fputs("Unknown boundary diagnostic WARP raster mode.\n", stderr);
                return 1;
            }
            mode = parsed;
        }
        ComplexUiBoundaries::Run(__wargv[2], timers == L"On", mode);
        return 0;
    }
    // Opt-in diagnostics stay outside the canonical benchmark dispatcher/closure.
    if (__argc == 5 && std::wstring_view(__wargv[1]) == L"--benchmark-complex-ui-profile")
    {
        const std::wstring_view scenario(__wargv[3]);
        const std::wstring_view heaps(__wargv[4]);
        Check(scenario == L"Default" || scenario == L"MultilineGrid", "profiling scenario");
        Check(heaps == L"Off" || heaps == L"On", "profiling heap sampling mode");
        ComplexUiProfiling::Run(__wargv[2], scenario == L"MultilineGrid", heaps == L"On");
        return 0;
    }
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
    if (__argc >= 3 && __argc <= 5 && std::wstring_view(__wargv[1]) == L"--benchmark-form-layout")
    {
        FormLayoutBenchmark::Run(__wargv[2],
                                 __argc >= 4 ? FormLayoutBenchmark::ParseSeed(__wargv[3]) : FormLayoutBenchmark::kDefaultSeed,
                                 __argc == 5 ? FormLayoutBenchmark::ParseBlocks(__wargv[4]) : FormLayoutBenchmark::kBlockCount);
        return 0;
    }
#endif
    if (__argc == 2 && std::wstring_view(__wargv[1]) == L"--test-prepared-tree-accessibility")
    {
#if DXUI_TEST_HAS_PREPARED_TREE_ROWS
        GraphicsFixture gpu;
        Hr(gpu.Create(), "prepared Tree semantics WARP device");
        TestEmbeddedRadioGroupAccessibility(gpu);
        TestPreparedTreeAccessibilitySemantics(gpu);
#if DXUI_ENABLE_DIAGNOSTICS
        TestPreparedTreeUnfocusedExpansionAction(gpu);
#endif
        TestPreparedTreeRootDoesNotRetainSupersededSource(gpu);
        return 0;
#else
        Check(false, "prepared Tree semantics requires the optional source API");
#endif
    }
    if (__argc == 2 && std::wstring_view(__wargv[1]) == L"--benchmark-tree-accessibility")
    {
        GraphicsFixture gpu;
        Hr(gpu.Create(), "large Tree benchmark WARP device");
        RunPreparedTreeAccessibilityBenchmark(gpu, false);
        RunPreparedTreeAccessibilityBenchmark(gpu, true);
        return 0;
    }
    if (__argc >= 2 && std::wstring_view(__wargv[1]) == L"--test-clean-prepare-timer-oracle")
    {
        if (__argc != 2)
        {
            std::fputs("Clean Prepare timer oracle accepts no additional arguments.\n", stderr);
            return 2;
        }
        CleanPrepareTimerDiagnostic::RunOracle();
        std::cout << "PASS clean Prepare timer integration oracle\n";
        return 0;
    }
    if (__argc >= 2 && std::wstring_view(__wargv[1]) == L"--benchmark-clean-prepare-floor")
    {
        if (__argc != 4)
        {
            std::fputs("Clean Prepare timer diagnostic requires a fresh output file and an unsigned seed.\n", stderr);
            return 2;
        }
        uint64_t seed = 0;
        Check(CleanPrepareTimerDiagnostic::TryParseSeed(__wargv[3], seed), "clean Prepare timer diagnostic seed is an unsigned integer");
        CleanPrepareTimerDiagnostic::Run(__wargv[2], seed);
        return 0;
    }
    // The opt-in grid selection measurement is dispatched here, not in Embedded.Tests.BenchmarkMain.h, whose hash identifies the complex-UI fixture.
    if ((__argc == 3 || __argc == 4) && std::wstring_view(__wargv[1]) == L"--benchmark-grid-selection")
    {
        GridSelectionBenchmark::Run(__wargv[2], __argc == 4 ? std::wstring_view(__wargv[3]) : std::wstring_view());
        return 0;
    }
#if DXUI_ENABLE_DIAGNOSTICS
    if (__argc == 2 && std::wstring_view(__wargv[1]) == L"--test-property-registration-retirement")
    {
        GraphicsFixture gpu;
        Hr(gpu.Create(), "property registration retirement WARP device");
        TestEmbeddedPropertyRegistrationRejectsRetiredControl(gpu);
        return 0;
    }
#endif
    if (__argc == 2 && std::wstring_view(__wargv[1]) == L"--test-prepared-tree-events")
    {
        GraphicsFixture gpu;
        Hr(gpu.Create(), "embedded real client WARP device");
        TestPreparedTreeSourceReplacementNotifiesReentrantEmbeddedClient(gpu);
        TestPreparedTreeSourceBurstRetiresIntermediateSourcesDuringEmbeddedCallback(gpu);
        TestPreparedTreePropertyEventPermitsOwnerReentry(gpu);
        TestPreparedTreePropertyBurstCoalescesWithoutRetainingSources(gpu);
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
        TestPreparedTreeLabeledByEventPermitsOwnerReentry(gpu);
        TestPreparedTreeLabeledByBurstCoalescesWithoutRetainingSources(gpu);
#endif
        return 0;
    }
    if (__argc == 2 && std::wstring_view(__wargv[1]) == L"--test-markdown-closed-roots")
    {
        MarkdownSourceCorrelationTests::ClosedRootDifferential(Check);
        MarkdownSourceCorrelationTests::ClosedRootCancellation(Check);
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
    CleanPrepareTimerDiagnostic::RunOracle();
    GraphicsFixture gpu;
    Hr(gpu.Create(), "supplied WARP device");
    if (__argc == 2 && std::wstring_view(__wargv[1]) == L"--test-rich-incremental-preparation")
    {
        MarkdownSourceCorrelationTests::LexicalEdits(Check);
        MarkdownSourceCorrelationTests::ClosedRootDifferential(Check);
        MarkdownSourceCorrelationTests::ClosedRootCancellation(Check);
        RichSemanticTests::Run(RichSemanticGeometryTests::MarkdownSubtlePresentation);
        RichSemanticTests::Run(RichSemanticGeometryTests::PaddedCodePresentation);
        RichSemanticTests::Run(RichSemanticEditorTests::SourceWindowUnicodeParity);
        RichSemanticTests::Run(RichSemanticEditorTests::RichSourceWindowParity);
        TestPaintIndexTransactionalBuild();
        TestMathKernPreparationAllocationFailures();
        TestEmbeddedSemanticReusePixels(gpu);
        TestEmbeddedCodeLineNumberPixels(gpu);
        TestEmbeddedSourceGuidesPixels(gpu);
        TestSourceGuideAllocationAtomicity();
        TestHtmlClipboardFragment();
        TestEmbeddedImageResources(gpu);
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "incremental borrowed graphics");
        Hr(view.Attach(graphics), "incremental embedded view");
        RichSemanticTests::Run([&]
        {
            RichSemanticEditorTests::TransientPreviewRetainsChunks(
                view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded transient preview"); });
            RichSemanticEditorTests::PendingPointerPreservesFocus(
                view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded pending pointer"); });
            RichSemanticEditorTests::ReusedEditorGeometryMatchesFresh(
                view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded geometry parity"); });
            RichSemanticEditorTests::OverCapacityPaddedCellTyping(
                view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded over-capacity table"); });
#if DXUI_ENABLE_DIAGNOSTICS
            RichSemanticEditorTests::SelectionCancellationPublication(
                view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded selection cancellation"); });
            RichSemanticEditorTests::TitleTooltipContract(view.Controls(),
                                                          [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded title tooltip preparation"); },
                                                          [&](D2D1_POINT_2F point, UINT modifiers)
            {
                const auto scale = static_cast<float>(view.Controls().GetDpi()) / 96.f;
                static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Move, point.x * scale, point.y * scale, modifiers}));
            },
                                                          [&](bool down) { static_cast<void>(view.DispatchKey(VK_CONTROL, down, down ? MK_CONTROL : 0u)); });
#endif
            RichSemanticEditorTests::SemanticGeometryReuse(view.Controls(),
                                                           [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "incremental preparation"); });
            RichSemanticEditorTests::ImagePreparationWorkMetric(
                view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded image work metric"); });
            RichSemanticEditorTests::NativeObjectSourceContract(view.Controls(),
                                                                [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "shared embedded object contract"); });
            RichSemanticEditorTests::PendingSourceWindowTransport(view.Controls());
            RichSemanticEditorTests::PendingRichSourceWindowTransport(view.Controls());
            RichSemanticEditorTests::UrlOnSelectionPaste(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "URL paste"); });
            RichSemanticEditorTests::SemanticWords(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "semantic word viewport"); });
            RichSemanticEditorTests::SemanticDirection(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "paragraph direction"); });
            RichSemanticEditorTests::InlineExtensions(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "inline extension styles"); });
            RichSemanticEditorTests::WikiTargets(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "wiki targets"); });
            RichSemanticEditorTests::InlineScriptFallback(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "inline script fallback"); });
            RichSemanticEditorTests::SourceHorizontalAndGuides(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "source guides"); });
            RichSemanticEditorTests::RichTableHorizontalOverflow(view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "table overflow"); });
        });
        std::vector<DxUi::RichText::CodeToken> tokens(0u);
        Check(DxUi::RichText::TokenizeBuiltinCode(L"json", L"{\"a\": true, \"b\": 3}", tokens) && tokens.size() >= 4u,
              "builtin JSON tokenizer emits bounded literal and keyword ranges");
        Check(DxUi::RichText::TokenizeBuiltinCode(L"cpp", L"int x; // comment", tokens) && tokens.size() == 2u,
              "builtin C-like tokenizer recognizes keywords and comments");
        Check(DxUi::RichText::TokenizeBuiltinCode(L"pwsh", L"function f { # comment\n}", tokens) && tokens.size() == 2u,
              "builtin PowerShell tokenizer recognizes keywords and comments");
        Check(DxUi::RichText::TokenizeBuiltinCode(L"diff", L"+added\n-removed\n", tokens) && tokens.size() == 2u,
              "builtin diff tokenizer recognizes changed lines");
        Check(! DxUi::RichText::TokenizeBuiltinCode(L"unknown", L"text", tokens) && tokens.empty(), "unknown code language is ordinary ink");
        auto owned   = std::make_unique<DxUi::RichTextEditor>();
        auto* editor = owned.get();
        view.Controls().SetRoot(std::move(owned));
        editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        editor->SetText(L"```json\n{\"a\": true}\n```\n");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "ordinary code preparation");
        Hr(gpu.Resize(480u, 240u), "resize tokenizer raster fixture");
        std::vector<std::uint8_t> ordinaryPixels, coloredPixels;
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "ordinary code composite");
        Hr(gpu.Read(ordinaryPixels), "ordinary code raster");
        unsigned calls = 0u;
        editor->SetCodeTokenizer([&](std::wstring_view language, std::wstring_view code, std::vector<DxUi::RichText::CodeToken>& out)
        {
            ++calls;
            Check(language == L"json" && (code.find(L"true") != code.npos || code.find(L"false") != code.npos),
                  "code callback borrows current language and display body");
            return DxUi::RichText::TokenizeBuiltinCode(language, code, out);
        });
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "tokenized code preparation");
        Check(calls == 1u && editor->GetText() == L"```json\n{\"a\": true}\n```\n", "tokenization is cached and does not edit source");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "colored code composite");
        Hr(gpu.Read(coloredPixels), "colored code raster");
        Check(ordinaryPixels != coloredPixels, "prepared token ranges change rendered glyph ink");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "idle tokenized preparation");
        Check(calls == 1u, "idle preparation does not reinvoke tokenizer");
        const auto boolean = editor->GetText().find(L"true");
        editor->SetSelection({boolean, boolean + 4u});
        Check(editor->ReplaceSelection(L"false"), "edit tokenized source");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare edited tokenized code");
        Check(calls == 2u && editor->GetText().find(L"false") != std::wstring::npos, "source edits retokenize the current code publication");
        editor->SetCodeTokenizer([&](std::wstring_view, std::wstring_view, std::vector<DxUi::RichText::CodeToken>& out)
        {
            out.push_back({{999u, 1000u}, DxUi::RichText::CodeTokenRole::Keyword});
            return true;
        });
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "invalid tokenizer result uses ordinary ink");
        bool edited = false;
        editor->SetCodeTokenizer([&](std::wstring_view, std::wstring_view, std::vector<DxUi::RichText::CodeToken>&)
        {
            if (! edited)
            {
                edited = true;
                editor->SetText(L"plain replacement");
            }
            return true;
        });
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "reentrant tokenizer edit retries current source");
        Check(edited && editor->GetText() == L"plain replacement", "reentrant tokenizer cannot publish stale code ranges");
        editor->SetText(L"```cpp\nint x;\n```\n");
        editor->SetCodeTokenizer([&](std::wstring_view, std::wstring_view, std::vector<DxUi::RichText::CodeToken>&)
        {
            view.Controls().SetRoot({});
            return true;
        });
        static_cast<void>(PrepareRichEmbeddedContent(view, 480u, 240u));
        Check(view.Controls().GetRoot() == nullptr, "tokenizer may retire its owner without use after free");
        return 0;
    }
    TestMultilineCaretViewport(gpu);
    TestEmbeddedTextInput(gpu);
    TestEmbeddedTabHeaderPreparationAvoidsPaintWork(gpu);
    TestEmbeddedInputCallbackAllocationBoundary(gpu);
    TestEmbeddedTranslatedCharacterOwnership(gpu);
    TestEmbeddedAccessibility(gpu);
    TestPreparedTreeAccessibilitySemantics(gpu);
#if DXUI_ENABLE_DIAGNOSTICS
    TestPreparedTreeUnfocusedExpansionAction(gpu);
#endif
    TestPreparedTreeRootDoesNotRetainSupersededSource(gpu);
    TestInteractionLayoutWithoutPaint(gpu);
    TestEmbeddedUiaEventHarness(gpu);
    TestPreparedTreeSourceReplacementNotifiesReentrantEmbeddedClient(gpu);
    TestPreparedTreeSourceBurstRetiresIntermediateSourcesDuringEmbeddedCallback(gpu);
    TestPreparedTreePropertyEventPermitsOwnerReentry(gpu);
    TestPreparedTreePropertyBurstCoalescesWithoutRetainingSources(gpu);
#if defined(DXUI_HAS_INTRINSIC_LAYOUT)
    TestPreparedTreeLabeledByEventPermitsOwnerReentry(gpu);
    TestPreparedTreeLabeledByBurstCoalescesWithoutRetainingSources(gpu);
#endif
    TestGridModelMutationDuringCellPaintAbortsPreparation(gpu);
    TestTreeDelegateMutationDuringItemPaintAbortsPreparation(gpu);
    TestTreeReorderWakeRetiresOwner(gpu);
    TestPointerDoesNotRetargetAfterHoverOrHitGeometryMutation(gpu);
    TestPageReplacementDuringPaintDefersBoundsUntilPreparation(gpu);
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
    TestUnicodeGraphemeForwardWalkAllocationBudget();
    TestUnicodeBulkMovementAndDeletionAllocationBudget();
    TestRichTextUtf8RejectionAllocationBudget();
    TestSeparatorCatalogAllocationFailures();
    TestRichAppearanceConstructionFailureAtomicity();
    TestRichInputAllocationFailureAtomicity();
    TestRichDocumentFragmentedHistoryStorageBudget();
    RichTextStateReplacementTests::Core(Check);
    TestRichRecordedWholeTextStorageBudget();
    TestEmbeddedLocalizedRecordedServiceCommit(gpu);
    TestEmbeddedRetainedObjectSourceEditing(gpu);
    TestEmbeddedImagePathPaste(gpu);
    TestPreparedCommandStateAllocationFailures(gpu);
    TestPreparedCommandStateComplexAllocationFailures(gpu);
    TestTableCommandPreparationAllocationFailures(gpu);
    TestEmbeddedFileReferencePaste(gpu);
    TestEmbeddedImageResources(gpu);
    RichImageTests::Core(Check);
    RichImageResourceTests::Admission(Check);
    TestRichImagePathAllocationFailures();
    TestRichFileDropTransportAllocationFailures();
    TestRichFileDropTransactionAllocationFailures(gpu);
    TestRichImageDecoderAllocationFailures();
    TestRichEditorSteadyLinkHoverAllocatesNothing(gpu);
    TestRichImageResourceAllocationFailures();
    TestRichImageGeometryAllocationFailures();
    TestRichImageRasterFailureAndBudgets();
    TestRichImageFrameHostFailure();
    TestRichDocumentSameContentAllocationBudget();
    TestRichDocumentAllocationFailureAtomicity();
    TestRichLabelWakeupAndRetirement(gpu);
    RichTextExportTests::Core(Check);
    TestRichExportAllocationFailures();
    TestRichPreparedPageRaster(gpu);
    TestRichPageAuthoredColors(gpu);
    TestRichTooltipReentrantTransitions(gpu);
    TestRichSourceDragGesture(gpu);
    TestRichMiddleAutoScroll(gpu);
    TestRichSourceMoveTransaction();
    TestRichTripleClickParagraph(gpu);
    TestRichPreparedWordFailureAtomicity(gpu);
    TestHeadingAdmissionNoAllocation();
    TestRichDefaultMenuModelAllocationFailureAtomicity();
    TestReadOnlyParsedSnapshotAllocationFailures();
    TestRichSourceChangeAllocationBudget();
    TestRichSourceChangeTransientStorageBudget();
    TestRichSourceChangeRolloverFailureAtomicity();
    TestRichProjectionMappingAndUnchangedAllocationBudget();
    TestRichSnapshotSliceAllocationFailureAtomicity();
    ComplexUiBoundaries::TestDevicePolicies();
    TestRichFlatProjectionFailureAtomicity();
    TestRichProjectionPreparationAllocationFailureAtomicity();
    TestRichProjectionRetainedStorageBudget();
    TestRichProjectionMaximumSourceStorageBudget();
    TestRichTaskPlanAllocationFailureKeepsPointerSelection(gpu);
    TestRetainedRichTextDefaultFontChunkReuse(gpu);
    TestRichTextCooperativeSchedulingAndTextInput(gpu);
    TestEmbeddedContinuationCallbackCannotReenterOrUseDetachedState(gpu);
    TestEmbeddedLosslessMarkdownPreparation(gpu);
    TestEmbeddedFrontMatterFencesAndSourceEdits(gpu);
    TestEmbeddedFlowchartRenderingAndFallbacks(gpu);
    SemanticDiagramTests::Core(Check);
    MarkdownSourceCorrelationTests::Reads(Check);
    MarkdownSourceCorrelationTests::ParserIdentities(Check);
    MarkdownSourceCorrelationTests::LexicalEdits(Check);
    MarkdownSourceCorrelationTests::ClosedRootDifferential(Check);
    MarkdownSourceCorrelationTests::ClosedRootCancellation(Check);
    SemanticSourceBlockTests::Core(Check);
    SemanticSourceBlockTests::Geometry(Check);
    SemanticSourceBlockTests::SourceRegions(Check);
    SemanticDiagramTests::Geometry(Check);
    SemanticDiagramTests::PositionedRaster(Check);
    TestEmbeddedDiagramViewCapacityRecovery(gpu);
    TestEmbeddedDiagramSelectionRaster(gpu);
    TestEmbeddedFootnoteBlockSourceEdits(gpu);
    TestEmbeddedFootnoteResolutionAndSourceEdits(gpu);
    TestEmbeddedParserCapacitySourceFallback(gpu);
    TestParserCapacityMemoryFailureIsRetryable(gpu);
    RichSemanticTests::Run(RichSemanticTests::CoordinateOracles);
    RichSemanticTests::Run(RichSemanticTests::UnicodeAndAdmission);
    RichSemanticTests::Run(RichSemanticTests::TransactionOracles);
    RichSemanticTests::Run(RichSemanticTests::TransactionRejections);
    RichSemanticTests::Run(RichCommandTests::ListsAndHeadings);
    RichSemanticTests::Run(RichCommandTests::InlineTasksLinks);
    RichSemanticTests::Run(RichCommandTests::Tables);
    RichSemanticTests::Run(RichCommandTests::MoveLinesAndItems);
    RichSemanticTests::Run(RichCommandTests::GuardsAndDeletion);
    RichSemanticTests::Run(TestRichMarkdownCommandFailureAtomicity);
    RichSemanticTests::Run([&] { TestRichMarkdownCommandHostFailureAtomicity(gpu); });
    RichSemanticTests::Run([&] { TestRichSemanticAccessibilityFailureAtomicity(gpu); });
    RichSemanticTests::Run(TestRichSemanticFailureAndStorageBudgets);
    RichSemanticTests::Run(RichSemanticDocumentTests::DirectedValues);
    RichSemanticTests::Run(RichSemanticDocumentTests::LocalizedAccessibilityLabels);
    RichSemanticTests::Run(RichSemanticDocumentTests::StructuralStyleValues);
    RichSemanticTests::Run(RichSemanticDocumentTests::SourceFormatOwnership);
    RichSemanticTests::Run(RichSemanticDocumentTests::AuthoredImagePresentation);
    RichSemanticTests::Run(RichSemanticDocumentTests::NativeHtmlAndLinkPresentation);
    RichSemanticTests::Run(RichSemanticDocumentTests::HeadingOutlineAndFragments);
    RichSemanticTests::Run(RichSemanticDocumentTests::OptionalDefinitionsAndAbbreviations);
    RichSemanticTests::Run(RichSemanticDocumentTests::FootnoteHtmlActionAdmission);
    RichSemanticTests::Run(RichSemanticDocumentTests::SchedulingAndAdmission);
    RichSemanticTests::Run(RichSemanticGeometryTests::ParagraphsTablesAndSourceRanges);
    RichSemanticTests::Run(RichSemanticGeometryTests::MarkdownSubtlePresentation);
    RichSemanticTests::Run(RichSemanticGeometryTests::PaddedCodePresentation);
    RichSemanticTests::Run(RichSemanticEditorTests::SourceWindowUnicodeParity);
    RichSemanticTests::Run(RichSemanticEditorTests::RichSourceWindowParity);
    RichSemanticTests::Run(RichSemanticGeometryTests::AtomicNavigationAndCancellation);
    RichSemanticTests::Run(RichSemanticGeometryTests::SourceCaretSites);
    RichSemanticTests::Run(RichSemanticGeometryTests::HangingMarkersLinksAndHtml);
    RichSemanticTests::Run(RichSemanticGeometryTests::LinkRasterOracle);
    RichSemanticTests::Run(RichSemanticGeometryTests::DiagramPaletteTargetReplacement);
    RichSemanticTests::Run(RichSemanticGeometryTests::DiagramLabelWidthReflow);
    RichSemanticTests::Run(RichSemanticGeometryTests::HtmlBlockAlignment);
    RichSemanticTests::Run([]
    {
        bool allocated{};
        std::size_t calls{};
        const auto wasCounting   = countAllocations;
        const auto originalCalls = allocations;
        const auto restore       = wil::scope_exit([&]() noexcept
        {
            countAllocations = wasCounting;
            allocations      = originalCalls;
        });
        RichSemanticGeometryTests::HtmlScripts([&](bool before) noexcept
        {
            if (before)
            {
                calls            = allocations;
                countAllocations = true;
            }
            else
            {
                allocated        = allocated || allocations != calls;
                countAllocations = wasCounting;
            }
        });
        Check(! allocated, "script native and RTL linked drawing performs zero tracked C++ allocations");
    });
#if DXUI_ENABLE_DIAGNOSTICS
    RichSemanticTests::Run(RichSemanticGeometryTests::MathWidthReflowAndTargetReplacement);
#endif
    RichSemanticTests::Run(RichSemanticGeometryTests::TaskCheckboxAlignment);
    RichSemanticTests::Run(RichSemanticGeometryTests::LiteralFootnoteBlockCaretSites);
    RichSemanticTests::Run(RichSemanticGeometryTests::ReorderedFootnoteCaretSitesAndSourceLines);
    RichSemanticTests::Run([] { TestRichSemanticDocumentFailureAndStorageBudgets(); });
    RichSemanticTests::Run([] { TestRichSemanticGeometryFailureBudgets(); });
    RichSemanticTests::Run([] { TestRichSemanticGeometryFailureBudgets(false, true); });
    RichSemanticTests::Run([] { TestRichSemanticGeometryFailureBudgets(false, false, true); });
    RichSemanticTests::Run([] { TestRichSemanticGeometryFailureBudgets(false, false, false, false, true); });
    TestFootnoteBlockAllocationFailures();
    RichSemanticTests::Run([&] { TestRichSemanticEditorQueryFailureBudgets(gpu); });
    {
        DxUi::EmbeddedHost transactionView;
        std::shared_ptr<DxUi::GraphicsDevice> transactionGraphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), transactionGraphics), "transaction borrowed graphics");
        Hr(transactionView.Attach(transactionGraphics), "transaction embedded view");
        RichSemanticTests::Run([&]
        {
#if DXUI_ENABLE_DIAGNOSTICS
            RichSemanticEditorTests::TitleTooltipContract(
                transactionView.Controls(),
                [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded title tooltip preparation"); },
                [&](D2D1_POINT_2F point, UINT modifiers)
            {
                const auto scale = static_cast<float>(transactionView.Controls().GetDpi()) / 96.f;
                static_cast<void>(transactionView.DispatchPointer({DxUi::PointerAction::Move, point.x * scale, point.y * scale, modifiers}));
            },
                [&](bool down) { static_cast<void>(transactionView.DispatchKey(VK_CONTROL, down, down ? MK_CONTROL : 0u)); });
#endif
            RichSemanticEditorTests::SemanticGeometryReuse(transactionView.Controls(),
                                                           [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded semantic reuse"); });
            RichSemanticEditorTests::PendingSourceWindowTransport(transactionView.Controls());
            RichSemanticEditorTests::PendingRichSourceWindowTransport(transactionView.Controls());
            RichSemanticEditorTests::UrlOnSelectionPaste(transactionView.Controls(),
                                                         [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "URL paste"); });
            RichSemanticEditorTests::SourceHorizontalAndGuides(transactionView.Controls(),
                                                               [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "source guides"); });
            RichSemanticEditorTests::RichTableHorizontalOverflow(transactionView.Controls(),
                                                                 [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "table overflow"); });
            RichSemanticEditorTests::Transactions(transactionView.Controls(),
                                                  [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded transaction preparation"); });
            RichSemanticEditorTests::StructuralCommands(transactionView.Controls(),
                                                        [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded structural commands"); });
            RichSemanticEditorTests::StructuralHostRouting(transactionView.Controls(), [&] {
                Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded structural host routing");
            }, [&](UINT key, UINT modifiers) { return transactionView.DispatchKey(key, true, modifiers); }, [&](wchar_t value, UINT modifiers) {
                return transactionView.DispatchCharacter(value, modifiers);
            });
            RichSemanticEditorTests::MathAndBroaderDiagrams(transactionView.Controls(), [&] {
                Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded in-document math and broader diagrams");
            });
            for (const UINT dpi : {96u, 144u, 192u})
                RichSemanticEditorTests::MathRunFontSize(transactionView.Controls(),
                                                         [&]
                {
                    Hr(PrepareRichEmbeddedContent(transactionView, 640u * dpi / 96u, 480u * dpi / 96u, static_cast<float>(dpi)),
                       "prepare styled embedded math at the requested DPI");
                });
            RichSemanticEditorTests::InlineMathSourceEditing(
                transactionView.Controls(), [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded inline math source session"); });
            RichSemanticEditorTests::NativeObjectFailureContainment(
                transactionView.Controls(), [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded local native object fallback"); });
            RichSemanticEditorTests::Rendering(transactionView.Controls(),
                                               [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded semantic rendering preparation"); });
            RichSemanticEditorTests::RenderingAdmissionFallback(transactionView.Controls(), [&] {
                Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded semantic fallback preparation");
            }, [&](UINT dpi) {
                Hr(PrepareRichEmbeddedContent(transactionView, 480u * dpi / 96u, 240u * dpi / 96u, static_cast<float>(dpi)),
                   "embedded semantic fallback at changed DPI");
            });
            RichSemanticEditorTests::SourceCaretSites(
                transactionView.Controls(), [&] { Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded source caret site preparation"); });
            RichSemanticEditorTests::HangingMarkersLinksAndHtml(transactionView.Controls(), [&] {
                Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded hanging markers and HTML preparation");
            });
            RichSemanticEditorTests::TaskCheckboxAlignment(transactionView.Controls(), [&] {
                Hr(PrepareRichEmbeddedContent(transactionView, 480u, 240u), "embedded task checkbox alignment preparation");
            });
        });
        transactionView.Detach();
    }
    TestRichMarkdownParserFailureAndStorageBudgets();
    TestRichFrontMatterAllocationFailureAtomicity();
    TestNativeFlowchartAllocationFailureAtomicity();
    FlowchartLayoutTests::Geometry(Check);
    FlowchartLayoutTests::Corpus(Check);
    FlowchartLayoutTests::CrossingReduction(Check);
    FlowchartLayoutTests::Admission(Check);
    TestNativeFlowchartLayoutAllocationFailureAtomicity();
    TestPreparedFlowchartAllocationFailures();
    TestSemanticDiagramAllocationFailures();
    TestScopedSemanticSourceAllocationFailures();
    PreparedFlowchartTests::BudgetAndLifecycle(Check);
    PreparedFlowchartTests::RasterOracle(Check);
    PreparedFlowchartTests::EdgeLabelRasterOracle(Check);
    PreparedFlowchartTests::LineStyleRasterOracle(Check);
    PreparedFlowchartTests::CardinalityRaster(Check);
    PreparedFlowchartTests::GroupRaster(Check);
    PreparedFlowchartTests::SequenceRaster(Check);
    PreparedFlowchartTests::PieRaster(Check);
    PreparedFlowchartTests::ErRaster(Check);
    PreparedFlowchartTests::GanttRaster(Check);
    PreparedFlowchartTests::MindmapRaster(Check);
    PreparedFlowchartTests::NodeStrokeRaster(Check);
    PreparedFlowchartTests::ExpandedShapeRaster(Check);
    PreparedFlowchartTests::LegacyPolygonRaster(Check);
    PreparedFlowchartTests::StyledLabelRaster(Check);
    PreparedFlowchartTests::ThemeStyleRaster(Check);
    PreparedFlowchartTests::PaletteRaster(Check);
    PreparedFlowchartTests::ConfiguredLabelWidth(Check);
    PreparedFlowchartTests::StateNoteRaster(Check);
    TestRichTextCooperativeFailureDoesNotRetryAutomatically(gpu);
    TestRetainedRichTextSharedViewsAndResources(gpu);
    TestRetainedRichTextSharedViewsAndResources(gpu, true);
    TestRichPreparedLayoutClearAllocationBudget();
    TestRichPreparedLayoutValueFailureAtomicity();
    TestRichDocumentGeometryFailureAtomicity();
    TestRichDocumentGeometryFailureAtomicity(false, true);
    TestRichDocumentGeometryUnchangedAllocationBudget();
    TestRichDocumentGeometryMaximumStorageBudget();
    TestRichDocumentGeometryReplacementPeakBudget();
    TestPaintIndexTransactionalBuild();
    TestMathKernPreparationAllocationFailures();
    TestEmbeddedSemanticReusePixels(gpu);
    TestEmbeddedCodeLineNumberPixels(gpu);
    TestEmbeddedSourceGuidesPixels(gpu);
    TestSourceGuideAllocationAtomicity();
    TestHtmlClipboardFragment();
    TestEmbeddedRichGeometryReuseAndRetirement(gpu);
    TestRetainedRichTextSchedulingCanRetireOwner(gpu);
    TestRetainedRichTextFailedPreparation(gpu);
    TestRetainedRichTextUiaRangeAllocationBudget(gpu);
    TestPreparationCallbacksCanRetireRoot(gpu);
    TestHiddenViewReleasesGridLayouts(gpu);
    TestEmbeddedMultilineGridFrenchCells(gpu);
    TestPointerGesturesOnPaintDirtyView(gpu);
    TestEmbeddedTouchDragShowsTheSliderTouchHalo(gpu);
    TestTooltipInspectionDismissalConsumesEmbeddedGesture(gpu);
    TestTickDirtying(gpu);
    TestCacheBounds(gpu);
    TestEmbeddedIconLayoutRebuildsAfterFormatCacheEviction(gpu);
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
    const auto start                      = std::chrono::steady_clock::now();
    const auto allocationsBeforeComposite = allocations;
    countAllocations                      = true;
    for (int i = 0; i < 1000; ++i)
        Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "warm compose");
    countAllocations = false;
    const auto us    = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    Check(allocations == allocationsBeforeComposite, "1000 warm composites have zero C++ allocations");
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
    Check(DxUi::GetControlCatalog().size() == 35, "catalog contains all 35 controls");
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
    // The first tick after animation is requested again starts the view's clock; the next one passes the show delay.
    b.AdvanceAnimation(tick + 2000);
    b.AdvanceAnimation(tick + 4000);
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
        Check(! DxUi::TakeMessagePayload<Payload>(payloadWindow.get(), WM_APP + 77, stale.lParam), "drained tokens cannot resurrect payload");
    DxUi::InitPostedPayloadWindow(payloadWindow.get());
    Check(DxUi::PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)), "reinitialize window generation");
    Check(PeekMessageW(&stale, payloadWindow.get(), WM_APP + 77, WM_APP + 77, PM_REMOVE) != FALSE, "take new token");
    wil::unique_hwnd otherPayloadWindow(CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr));
    Check(bool(otherPayloadWindow), "wrong-window payload fixture");
    Check(! DxUi::TakeMessagePayload<Payload>(otherPayloadWindow.get(), WM_APP + 77, stale.lParam) && destroyed == 129,
          "a foreign window cannot consume or destroy a valid token");
    Check(! DxUi::TakeMessagePayload<int>(payloadWindow.get(), WM_APP + 77, stale.lParam) && destroyed == 129,
          "strict protocol mismatch leaves the genuine payload available");
    auto genuinePayload = DxUi::TakeMessagePayload<Payload>(payloadWindow.get(), WM_APP + 77, stale.lParam);
    Check(bool(genuinePayload), "matching window and protocol consume the payload");
    genuinePayload.reset();
    Check(destroyed == 130, "matching payload is released exactly once");
    Check(DxUi::PostMessagePayload(payloadWindow.get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)), "legacy type mismatch fixture");
    Check(PeekMessageW(&stale, payloadWindow.get(), WM_APP + 77, WM_APP + 77, PM_REMOVE) != FALSE, "take legacy type mismatch token");
    Check(! DxUi::TakeMessagePayload<int>(payloadWindow.get(), WM_APP + 77, stale.lParam) && destroyed == 130,
          "wrong payload type is rejected without consuming the registered entry");
    auto matchingAfterWrongType = DxUi::TakeMessagePayload<Payload>(payloadWindow.get(), WM_APP + 77, stale.lParam);
    Check(bool(matchingAfterWrongType), "the matching type can still take the entry after a mismatch");
    matchingAfterWrongType.reset();
    Check(destroyed == 131, "matching payload is released exactly once after a type mismatch");
    DxUi::DrainPostedPayloadsForWindow(payloadWindow.get());
    Check(! DxUi::InitPostedPayloadWindow(nullptr) && GetLastError() == ERROR_INVALID_WINDOW_HANDLE,
          "invalid payload hosts fail with a diagnosed handle error");
    size_t registeredBefore = 0u;
    {
        auto& registry = DxUi::Detail::Payloads();
        const std::scoped_lock lock(registry.mutex);
        registeredBefore = static_cast<size_t>(std::ranges::count_if(registry.windows, [](HWND window) { return window != nullptr; }));
    }
    std::vector<wil::unique_hwnd> registryWindows;
    const auto drainRegistryWindows = wil::scope_exit([&]
    {
        for (const auto& window : registryWindows)
            DxUi::DrainPostedPayloadsForWindow(window.get());
    });
    const size_t freeHostSlots      = 128u - registeredBefore;
    for (size_t index = 0u; index <= freeHostSlots; ++index)
    {
        registryWindows.emplace_back(CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr));
        Check(bool(registryWindows.back()), "host capacity fixture creates a real message-only window");
        const bool registered = DxUi::InitPostedPayloadWindow(registryWindows.back().get());
        if (index < freeHostSlots)
        {
            Check(registered, "remaining bounded host slots accept registration");
            Check(DxUi::InitPostedPayloadWindow(registryWindows.back().get()), "duplicate host registration consumes no additional slot");
        }
        else
            Check(! registered && GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "host registry exhaustion is diagnosed");
    }
    Check(freeHostSlots > 0u, "host capacity fixture has a slot to release");
    DxUi::DrainPostedPayloadsForWindow(registryWindows.front().get());
    Check(DxUi::InitPostedPayloadWindow(registryWindows.back().get()), "a drained host slot admits a previously rejected registration");
    Check(DxUi::PostMessagePayload(registryWindows.back().get(), WM_APP + 77, 0, std::make_unique<Payload>(&destroyed)),
          "registration recovery restores payload delivery");
    Check(DxUi::DrainPostedPayloadsForWindow(registryWindows.back().get()) == 1u && destroyed == 132u,
          "recovered registration drains its payload exactly once");
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
    std::cout << "PASS " << checks << " checks; 1000 warm composites " << us << " us; C++ allocations " << allocations - allocationsBeforeComposite
              << "; changed toggle pixels " << changed << '\n';
    return 0;
}

// Registered before child dispatch without modifying the fingerprinted benchmark driver.
static std::atomic<unsigned> embeddedDragDataOriginMarker{};
__declspec(noinline) static void EmbeddedDragDataAllocationOrigin()
{
    const std::wstring text(4096u, L'x');
    wil::com_ptr_nothrow<IDataObject> output;
    embeddedDragDataOriginMarker.store(17u, std::memory_order_relaxed);
    failAllocationAfter = 1u;
    static_cast<void>(DxUi::CreateRichTextDragData(text, output.put()));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichCodecOrigin()
{
    std::wstring output(0u, L' ');
    failAllocationAfter = 0u;
    static_cast<void>(DxUi::RichText::TryDecodeUtf8("a deliberately long UTF-8 source requiring allocated output", output));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichDocumentOrigin()
{
    using namespace DxUi::RichText;
    Document document(L"a deliberately long document source");
    bool changed        = false;
    failAllocationAfter = 0u;
    static_cast<void>(document.TryReplace(0u, 1u, L"replacement requiring allocation", {1u, 1u}, changed));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichProjectionOrigin()
{
    using namespace DxUi::RichText;
    Document document(L"a deliberately long projection source");
    PreparedProjection projection;
    failAllocationAfter = 0u;
    static_cast<void>(projection.TryPrepare(document.Snapshot(), document.Revision(), ViewMode::Rich));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichCommandOrigin()
{
    using namespace DxUi::RichText;
    Document document(L"a deliberately long formatted command input");
    const auto syntax = RichCommandTests::Parse(document);
    MarkdownCommandRequest request;
    request.command = MarkdownCommand::ToggleStyle;
    std::optional<MarkdownCommandPlan> output;
    failAllocationAfter = 0u;
    static_cast<void>(TryBuildMarkdownCommand(document.Snapshot(), document.Revision(), syntax, {0u, 4u}, request, output));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichParserOrigin()
{
    using namespace DxUi::RichText;
    MarkdownParser parser;
    failAllocationAfter = 0u;
    static_cast<void>(parser.TryBegin(L"a deliberately long parser source requiring allocated staging", 1u));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichSemanticOrigin()
{
    using namespace DxUi::RichText;
    const auto syntax = RichSemanticDocumentTests::Parse(L"a deliberately long semantic source with **strong text**");
    SemanticBuilder builder;
    if (builder.TryBegin(syntax) != SourceResult::Success)
        return;
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    static_cast<void>(builder.Advance(4096u));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichExportOrigin()
{
    using namespace DxUi::RichText;
    const auto document = RichSemanticDocumentTests::Build(L"a deliberately long **semantic export** source");
    std::optional<ExportContent> output;
    failAllocationAfter = 0u;
    static_cast<void>(TryExportSemanticDocument(*document, {0u, document->Map().Display().size()}, output));
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
__declspec(noinline) static void EmbeddedRichGeometryOrigin()
{
    using namespace DxUi::RichText;
    const auto factory  = RichSemanticGeometryTests::Factory();
    const auto document = RichSemanticDocumentTests::Build(L"a deliberately long semantic geometry source");
    Detail::PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip = 360.0f;
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    geometry.Begin(factory.get(), document, options);
    failAllocationAfter = (std::numeric_limits<size_t>::max)();
}
static void EmbeddedRichNativeToggleChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestRichSemanticNativeDispatchFailureAtomicity(true);
}
static void EmbeddedRichUiaCollectionChild(unsigned scenario)
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRichSemanticAccessibilityFailureAtomicity(gpu, scenario);
}
static void EmbeddedRichUiaChildrenChild()
{
    EmbeddedRichUiaCollectionChild(0u);
}
static void EmbeddedRichUiaHeadersChild()
{
    EmbeddedRichUiaCollectionChild(1u);
}
static void EmbeddedRichCommandHostChild(unsigned scenario)
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRichMarkdownCommandHostFailureAtomicity(gpu, scenario);
}
static void EmbeddedRichTextCacheChild()
{
    EmbeddedRichCommandHostChild(1u);
}
static void EmbeddedRichCommandApplyChild()
{
    EmbeddedRichCommandHostChild(2u);
}
static void EmbeddedRichPaintIndexChild()
{
    TestPaintIndexTransactionalBuild(true);
}
static void EmbeddedRichMathChild(const wchar_t* source)
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestMathPreparationAllocationFailures(source, true);
}
static void EmbeddedRichMathKernChild()
{
    EmbeddedRichMathChild(L"1_1^1");
}
static void EmbeddedRichMathColorChild()
{
    EmbeddedRichMathChild(L"\\textcolor{white}{\\fcolorbox{blue}{black}{hello}}+\\color{blue}x^2");
}
static void EmbeddedRichLayoutCopyChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestRichPreparedLayoutValueFailureAtomicity(false, true);
}
static void EmbeddedRichDocumentGeometryChild(bool whole)
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestRichDocumentGeometryFailureAtomicity(false, whole, true);
}
static void EmbeddedRichDocumentGeometryChunkChild()
{
    EmbeddedRichDocumentGeometryChild(false);
}
static void EmbeddedRichDocumentGeometryWholeChild()
{
    EmbeddedRichDocumentGeometryChild(true);
}
static void EmbeddedRichSourceGuideChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestSourceGuideAllocationAtomicity(true);
}
static void EmbeddedRichClipboardBitmapChild()
{
    TestHtmlClipboardFragment(1u);
}
static void EmbeddedRichClipboardHtmlChild()
{
    TestHtmlClipboardFragment(2u);
}
template <unsigned Scenario> static void EmbeddedSemanticGeometryChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestRichSemanticGeometryFailureBudgets(false, (Scenario & 1u) != 0u, (Scenario & 2u) != 0u, (Scenario & 4u) != 0u, (Scenario & 8u) != 0u, true);
}
template <unsigned Scenario> static void EmbeddedSemanticQueryChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRichSemanticEditorQueryFailureBudgets(gpu, false, Scenario);
}
template <unsigned Scenario> static void EmbeddedImagePathChild()
{
    TestRichImagePathAllocationFailures(Scenario);
}
static void EmbeddedDefaultMenuChild()
{
    TestRichDefaultMenuModelAllocationFailureAtomicity(true);
}
template <unsigned Scenario> static void EmbeddedImageDecoderChild()
{
    TestRichImageDecoderAllocationFailures(Scenario);
}
template <unsigned Scenario> static void EmbeddedImageResourceChild()
{
    TestRichImageResourceAllocationFailures(Scenario);
}
template <unsigned Scenario> static void EmbeddedPreparedFlowchartChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    TestPreparedFlowchartAllocationFailures(false, Scenario);
}
template <unsigned Scenario> static void EmbeddedFlowchartLayoutChild()
{
    TestNativeFlowchartLayoutAllocationFailureAtomicity(false, Scenario);
}
template <unsigned Scenario> static void EmbeddedFlowchartParserChild()
{
    TestNativeFlowchartAllocationFailureAtomicity(false, Scenario);
}
template <unsigned Scenario> static void EmbeddedSnapshotAdoptionChild()
{
    TestReadOnlyParsedSnapshotAllocationFailures(Scenario);
}
template <unsigned Scenario> static void EmbeddedInputChild()
{
    TestRichInputAllocationFailureAtomicity(Scenario);
}
template <unsigned Scenario> static void EmbeddedWordQueryChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRichPreparedWordFailureAtomicity(gpu, Scenario);
}
template <unsigned Scenario> static void EmbeddedPasteChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestEmbeddedFileReferencePaste(gpu, Scenario);
}
template <unsigned Scenario> static void EmbeddedUiaRangeChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRetainedRichTextUiaRangeAllocationBudget(gpu, Scenario);
}
template <unsigned Scenario> static void EmbeddedInputCallbackChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestEmbeddedInputCallbackAllocationBoundary(gpu, Scenario);
}
template <bool Nested> static void EmbeddedInputExceptionChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    if constexpr (Nested)
        TestEmbeddedTranslatedCharacterOwnership(gpu, true);
    else
        TestEmbeddedTextInput(gpu, true);
}
static void EmbeddedTaskPointerChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRichTaskPlanAllocationFailureKeepsPointerSelection(gpu, true);
}
static void EmbeddedCodeCopyChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestEmbeddedCodeLineNumberPixels(gpu, true);
}
static void EmbeddedRichPaintCallbackChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRetainedRichTextFailedPreparation(gpu, true);
}
static void EmbeddedTreeModelCallbackChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestEmbeddedAccessibilityContainsAThrowingModelGetter(gpu, true);
}
static void EmbeddedParserPreparationChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestParserCapacityMemoryFailureIsRetryable(gpu, true);
}
static void EmbeddedPaintCallbackChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestPreparationCallbacksCanRetireRoot(gpu, true);
}
static void EmbeddedFileDropTransactionChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    TestRichFileDropTransactionAllocationFailures(gpu, true);
}
static void EmbeddedImageGeometryChild()
{
    TestRichImageGeometryAllocationFailures(true);
}
static void EmbeddedFileDropTransportChild()
{
    TestRichFileDropTransportAllocationFailures(true);
}
#if DXUI_ENABLE_DIAGNOSTICS
template <unsigned Stage> static void EmbeddedPreparationFailureChild()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return;
    const auto uninitialize = wil::scope_exit([] { CoUninitialize(); });
    GraphicsFixture gpu;
    if (FAILED(gpu.Create()))
        return;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    if (FAILED(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics)))
        return;
    DxUi::EmbeddedHost view;
    if (FAILED(view.Attach(graphics)))
        return;
    auto editor = std::make_unique<DxUi::RichTextEditor>(L"**retained** and $x$\n");
    editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(editor));
    DxUi::RichText::Detail::testRichPreparationFailure = Stage;
    (void)PrepareRichEmbeddedContent(view, 480u, 240u);
}
#endif
static bool RegisterEmbeddedFatalCases() noexcept
{
    const auto RegisterFatalChildCase = [](DxUiFailureTests::FatalChildCase entry) noexcept
    {
        if (entry.name)
        {
            const std::wstring_view name(entry.name);
            if (name.starts_with(L"embedded-") && ! name.ends_with(L"-throw"))
                entry.faultClass = DxUiFailureTests::FatalFaultClass::InjectedAllocation;
        }
        return DxUiFailureTests::RegisterFatalChildCase(entry);
    };
    return RegisterFatalChildCase({L"embedded-drag-data-allocation", "EmbeddedDragDataAllocationOrigin", EmbeddedDragDataAllocationOrigin}) &&
           RegisterFatalChildCase({L"embedded-codec-allocation", "EmbeddedRichCodecOrigin", EmbeddedRichCodecOrigin}) &&
           RegisterFatalChildCase({L"embedded-document-allocation", "EmbeddedRichDocumentOrigin", EmbeddedRichDocumentOrigin}) &&
           RegisterFatalChildCase({L"embedded-projection-allocation", "EmbeddedRichProjectionOrigin", EmbeddedRichProjectionOrigin}) &&
           RegisterFatalChildCase({L"embedded-command-allocation", "EmbeddedRichCommandOrigin", EmbeddedRichCommandOrigin}) &&
           RegisterFatalChildCase({L"embedded-parser-allocation", "EmbeddedRichParserOrigin", EmbeddedRichParserOrigin}) &&
           RegisterFatalChildCase({L"embedded-semantic-allocation", "EmbeddedRichSemanticOrigin", EmbeddedRichSemanticOrigin}) &&
           RegisterFatalChildCase({L"embedded-export-allocation", "EmbeddedRichExportOrigin", EmbeddedRichExportOrigin}) &&
           RegisterFatalChildCase({L"embedded-geometry-allocation", "EmbeddedRichGeometryOrigin", EmbeddedRichGeometryOrigin}) &&
           RegisterFatalChildCase({L"embedded-uia-toggle-allocation", "RichNativeToggleFaultOrigin", EmbeddedRichNativeToggleChild}) &&
           RegisterFatalChildCase({L"embedded-uia-children-allocation", "RichUiaCollectionFaultOrigin", EmbeddedRichUiaChildrenChild}) &&
           RegisterFatalChildCase({L"embedded-uia-headers-allocation", "RichUiaCollectionFaultOrigin", EmbeddedRichUiaHeadersChild}) &&
           RegisterFatalChildCase({L"embedded-text-cache-allocation", "RichInputCacheFaultOrigin", EmbeddedRichTextCacheChild}) &&
           RegisterFatalChildCase({L"embedded-command-apply-allocation", "RichCommandApplyFaultOrigin", EmbeddedRichCommandApplyChild}) &&
           RegisterFatalChildCase({L"embedded-paint-index-allocation", "RichPaintIndexFaultOrigin", EmbeddedRichPaintIndexChild}) &&
           RegisterFatalChildCase({L"embedded-math-kern-allocation", "RichMathAllocationOrigin", EmbeddedRichMathKernChild}) &&
           RegisterFatalChildCase({L"embedded-math-color-allocation", "RichMathAllocationOrigin", EmbeddedRichMathColorChild}) &&
           RegisterFatalChildCase({L"embedded-layout-copy-allocation", "RichLayoutCopyFaultOrigin", EmbeddedRichLayoutCopyChild}) &&
           RegisterFatalChildCase(
               {L"embedded-document-geometry-chunk-allocation", "RichDocumentGeometryFaultOrigin", EmbeddedRichDocumentGeometryChunkChild}) &&
           RegisterFatalChildCase(
               {L"embedded-document-geometry-whole-allocation", "RichDocumentGeometryFaultOrigin", EmbeddedRichDocumentGeometryWholeChild}) &&
           RegisterFatalChildCase({L"embedded-source-guide-allocation", "RichSourceGuideFaultOrigin", EmbeddedRichSourceGuideChild}) &&
           RegisterFatalChildCase({L"embedded-clipboard-bitmap-allocation", "RichClipboardBitmapFaultOrigin", EmbeddedRichClipboardBitmapChild}) &&
           RegisterFatalChildCase({L"embedded-clipboard-html-allocation", "RichClipboardHtmlFaultOrigin", EmbeddedRichClipboardHtmlChild}) &&
           RegisterFatalChildCase(
               {L"embedded-appearance-construction-allocation", "RichAppearanceConstructionFaultOrigin", RichAppearanceConstructionFaultOrigin}) &&
           RegisterFatalChildCase({L"embedded-editor-construction-allocation", "RichEditorConstructionFaultOrigin", RichEditorConstructionFaultOrigin}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-default", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<0u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-literal", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-rendered", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-spacing", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-literal-spacing", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-rendered-spacing", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<6u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-subtle", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<8u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-geometry-subtle-spacing", "RichSemanticGeometryFaultOrigin", EmbeddedSemanticGeometryChild<12u>}) &&
           RegisterFatalChildCase({L"embedded-source-range-query", "RichSemanticQueryFaultOrigin", EmbeddedSemanticQueryChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-source-line-query", "RichSemanticQueryFaultOrigin", EmbeddedSemanticQueryChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-range-query", "RichSemanticQueryFaultOrigin", EmbeddedSemanticQueryChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-semantic-line-query", "RichSemanticQueryFaultOrigin", EmbeddedSemanticQueryChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-history-undo-allocation", "RichHistoryUndoFaultOrigin", RichHistoryUndoFaultOrigin}) &&
           RegisterFatalChildCase({L"embedded-image-reference-allocation", "RichImagePathFaultOrigin", EmbeddedImagePathChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-file-list-decode-allocation", "RichImagePathFaultOrigin", EmbeddedImagePathChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-file-reference-allocation", "RichImagePathFaultOrigin", EmbeddedImagePathChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-image-path-resolution-allocation", "RichImagePathFaultOrigin", EmbeddedImagePathChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-image-geometry-allocation", "RichImageGeometryFaultOrigin", EmbeddedImageGeometryChild}) &&
           RegisterFatalChildCase({L"embedded-input-character-allocation", "RichInputFaultOrigin", EmbeddedInputChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-input-command-allocation", "RichInputFaultOrigin", EmbeddedInputChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-input-backspace-allocation", "RichInputFaultOrigin", EmbeddedInputChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-input-surrogate-allocation", "RichInputFaultOrigin", EmbeddedInputChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-source-word-query", "RichPreparedWordFaultOrigin", EmbeddedWordQueryChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-rich-word-query", "RichPreparedWordFaultOrigin", EmbeddedWordQueryChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-file-drop-transport-allocation", "RichFileDropTransportFaultOrigin", EmbeddedFileDropTransportChild}) &&
           RegisterFatalChildCase({L"embedded-file-drop-transaction-allocation", "RichFileDropTransactionFaultOrigin", EmbeddedFileDropTransactionChild}) &&
           RegisterFatalChildCase({L"embedded-image-decode-allocation", "RichImageDecoderFaultOrigin", EmbeddedImageDecoderChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-image-create-allocation", "RichImageDecoderFaultOrigin", EmbeddedImageDecoderChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-prepared-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-capacity-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-sequence-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-pie-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-er-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-gantt-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<6u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-mindmap-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<7u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-shapes-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<8u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-label-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<9u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-style-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<10u>}) &&
           RegisterFatalChildCase({L"embedded-diagram-palette-allocation", "RichPreparedFlowchartFaultOrigin", EmbeddedPreparedFlowchartChild<11u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-normal", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-node-capacity", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-edge-capacity", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-sequence", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-pie", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-er", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<6u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-gantt", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<7u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-layout-mindmap", "RichNativeFlowchartLayoutFaultOrigin", EmbeddedFlowchartLayoutChild<8u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-01", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-02", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-03", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-04", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-05", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-06", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<6u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-07", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<7u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-08", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<8u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-09", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<9u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-10", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<10u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-11", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<11u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-12", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<12u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-13", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<13u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-14", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<14u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-15", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<15u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-16", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<16u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-17", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<17u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-18", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<18u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-19", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<19u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-20", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<20u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-21", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<21u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-22", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<22u>}) &&
           RegisterFatalChildCase({L"embedded-flowchart-parser-23", "RichNativeFlowchartParserFaultOrigin", EmbeddedFlowchartParserChild<23u>}) &&
           RegisterFatalChildCase({L"embedded-default-menu-allocation", "RichDefaultMenuFaultOrigin", EmbeddedDefaultMenuChild}) &&
           RegisterFatalChildCase({L"embedded-parser-adoption-allocation", "RichSnapshotAdoptionFaultOrigin", EmbeddedSnapshotAdoptionChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-editor-adoption-allocation", "RichSnapshotAdoptionFaultOrigin", EmbeddedSnapshotAdoptionChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-typing-pair-allocation", "RichSnapshotAdoptionFaultOrigin", EmbeddedSnapshotAdoptionChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-typing-typography-allocation", "RichSnapshotAdoptionFaultOrigin", EmbeddedSnapshotAdoptionChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-typing-fence-allocation", "RichSnapshotAdoptionFaultOrigin", EmbeddedSnapshotAdoptionChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-mixed-paste-allocation", "RichPasteTransformFaultOrigin", EmbeddedPasteChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-deep-paste-allocation", "RichPasteTransformFaultOrigin", EmbeddedPasteChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-uia-range-compare", "RichUiaRangeFaultOrigin", EmbeddedUiaRangeChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-uia-range-expand", "RichUiaRangeFaultOrigin", EmbeddedUiaRangeChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-uia-range-rectangles", "RichUiaRangeFaultOrigin", EmbeddedUiaRangeChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-uia-range-text", "RichUiaRangeFaultOrigin", EmbeddedUiaRangeChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-uia-range-move", "RichUiaRangeFaultOrigin", EmbeddedUiaRangeChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-uia-range-endpoint", "RichUiaRangeFaultOrigin", EmbeddedUiaRangeChild<6u>}) &&
           RegisterFatalChildCase({L"embedded-task-pointer-allocation", "RichTaskPointerFaultOrigin", EmbeddedTaskPointerChild}) &&
           RegisterFatalChildCase({L"embedded-code-copy-allocation", "RichCodeCopyFaultOrigin", EmbeddedCodeCopyChild}) &&
           RegisterFatalChildCase({L"embedded-image-resource-stage", "RichImageResourceFaultOrigin", EmbeddedImageResourceChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-image-resource-take", "RichImageResourceFaultOrigin", EmbeddedImageResourceChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-image-resource-publish", "RichImageResourceFaultOrigin", EmbeddedImageResourceChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-image-resource-frame", "RichImageResourceFaultOrigin", EmbeddedImageResourceChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-image-resource-remote", "RichImageResourceFaultOrigin", EmbeddedImageResourceChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-separator-catalog-allocation", "SeparatorCatalogFaultOrigin", SeparatorCatalogFaultOrigin}) &&
           RegisterFatalChildCase({L"embedded-input-key-throw", "EmbeddedInputAllocationThrowOrigin", EmbeddedInputCallbackChild<1u>}) &&
           RegisterFatalChildCase({L"embedded-input-character-throw", "EmbeddedInputAllocationThrowOrigin", EmbeddedInputCallbackChild<2u>}) &&
           RegisterFatalChildCase({L"embedded-input-receipt-throw", "EmbeddedInputAllocationThrowOrigin", EmbeddedInputCallbackChild<3u>}) &&
           RegisterFatalChildCase({L"embedded-input-key-commit-throw", "EmbeddedInputAllocationThrowOrigin", EmbeddedInputCallbackChild<4u>}) &&
           RegisterFatalChildCase({L"embedded-input-character-commit-throw", "EmbeddedInputAllocationThrowOrigin", EmbeddedInputCallbackChild<5u>}) &&
           RegisterFatalChildCase({L"embedded-input-receipt-commit-throw", "EmbeddedInputAllocationThrowOrigin", EmbeddedInputCallbackChild<6u>}) &&
           RegisterFatalChildCase({L"embedded-text-change-callback-throw", "EmbeddedTextChangeThrowOrigin", EmbeddedInputExceptionChild<false>}) &&
           RegisterFatalChildCase({L"embedded-nested-key-callback-throw", "EmbeddedNestedKeyThrowOrigin", EmbeddedInputExceptionChild<true>}) &&
           RegisterFatalChildCase({L"embedded-rich-paint-callback-throw", "EmbeddedRichPaintThrowOrigin", EmbeddedRichPaintCallbackChild}) &&
           RegisterFatalChildCase({L"embedded-tree-model-callback-throw", "EmbeddedTreeModelThrowOrigin", EmbeddedTreeModelCallbackChild}) &&
           RegisterFatalChildCase({L"embedded-paint-callback-throw", "EmbeddedPaintFailureOrigin", EmbeddedPaintCallbackChild}) &&
           RegisterFatalChildCase({L"embedded-parser-preparation-allocation", "RichParserPreparationFaultOrigin", EmbeddedParserPreparationChild})
#if DXUI_ENABLE_DIAGNOSTICS
           && RegisterFatalChildCase({L"rich-preparation-stage1", "SemanticStagingDiagnosticThrowOrigin", EmbeddedPreparationFailureChild<1u>}) &&
           RegisterFatalChildCase({L"rich-preparation-stage2", "NativeGeometryDiagnosticThrowOrigin", EmbeddedPreparationFailureChild<2u>})
#endif
        ;
}
// Routed before main runs, so that Embedded.Tests.BenchmarkMain.h, a fingerprinted benchmark input, stays as it is.
const bool g_failureReportsRouted = (DxUiTestFailureReports::RouteAwayFromDialogs(), RegisterEmbeddedFatalCases());

#include "Embedded.Tests.BenchmarkMain.h"
