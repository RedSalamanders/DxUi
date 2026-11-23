#pragma once
// Plan item W2.5, Grid data paths, on the embedded host (Specs/UI/UI_ControlsAndLayout.md, "Grid data caches"). Included once,
// from Embedded.Tests.Embedded.cpp, after Embedded.Tests.PerformanceFixtures.h, whose runner, counting allocation instrument,
// GraphicsFixture, Check and Hr it uses.
//
// - TestFx3GridDataPathGate (Embedded suite, FX3's default variant, 20,000 rows in 1,000 groups, then once the receipt-only
//   grouped-full variant, 200,000 rows in 10,000 groups): every FX3 frame asks the
//   model for no column description and collects no groups, except the one fill each cache makes for the grouped model; no step
//   copies the selection; after the select-all DxUi allocates nothing for any Down key, click or Ctrl+click (each measured
//   alone, outside its frame, less what the scene's models allocate formatting their own text), and for the Down that replaces
//   the select-all only the selection model's two one-id buffers (Release; Debug and ASan count checked-iterator proxies, so
//   they report the counts without asserting them); after every
//   step the cell layouts held never exceed the cells the step's paint drew; and every frame equals a fresh view's, whose
//   registered retained caches (the Grid's data caches among them) were dropped before it.
// - --test-grid-data-paths [variant]: that gate alone, also for the receipt-only grouped-full variant (200,000 rows in 10,000
//   groups), with its run time.
// - --fx3-frame-hashes <variant> <output>: one FNV-1a hash of the prepared surface per FX3 step, in phase order: the pixel
//   receipt that compares a build before the change with one after it (Measurements/PerformanceAndMotion_2026-11-13/W2.5).
#include "Embedded.Tests.PerformanceFixtures.h"

#include <cstdio>
#include <fstream>
#include <span>

namespace GridDataPathTests
{
using namespace PerformanceFixtures;

[[nodiscard]] inline uint64_t HashBytes(std::span<const uint8_t> bytes) noexcept
{
    uint64_t hash = 0xCBF29CE484222325ull;
    for (const uint8_t byte : bytes)
    {
        hash ^= byte;
        hash *= 0x100000001B3ull;
    }
    return hash;
}

// Plays FX3 at `variant`, one frame per step, and calls visit(phase, step, instance) after every frame. A fresh play drops every
// registered retained cache of the view before each frame (the incremental-versus-fresh oracle's fresh side).
template <typename Visit> void PlayFx3(GraphicsFixture& gpu, const Variant& variant, bool fresh, Visit&& visit)
{
    EmbeddedInstance instance;
    instance.Instantiate(Fx3::kDefinition);
    Hr(instance.Create(gpu.device.get(), variant), "FX3 scene build");
    for (size_t phase = 0u; phase < Fx3::kPhases.size(); ++phase)
        for (size_t step = 0u; step < Fx3::kPhases[phase].steps; ++step)
        {
            instance.Step(phase, step);
            if (fresh)
                instance.View().DebugDropRetainedStateForTest();
            Hr(instance.Frame(gpu), "FX3 frame");
            visit(phase, step, instance);
        }
}

[[nodiscard]] inline std::vector<uint64_t> Fx3FrameHashes(GraphicsFixture& gpu, const Variant& variant, bool fresh)
{
    std::vector<uint64_t> hashes;
    PlayFx3(gpu,
            variant,
            fresh,
            [&](size_t, size_t, EmbeddedInstance& instance)
    {
        DxUi::WindowHostBitmapCapture surface;
        Hr(instance.View().DebugReadSurfaceForTest(surface), "FX3 surface read");
        hashes.push_back(HashBytes(surface.bgraPixels));
    });
    return hashes;
}

[[nodiscard]] inline DxUi::Grid& Fx3Grid(EmbeddedInstance& instance)
{
    DxUi::Control* const root = instance.View().Controls().GetRoot();
    auto* const grid          = root && root->GetLogicalChildCount() == 1u ? dynamic_cast<DxUi::Grid*>(root->GetLogicalChild(0u)) : nullptr;
    Check(grid != nullptr, "the FX3 scene is a panel holding its Grid");
    return *grid;
}

// The C++ allocations of one action alone, outside any frame, less those the scene's own models made in it (they report them,
// Embedded.Tests.PerformanceGridModels.h): what DxUi allocated.
template <typename Action> [[nodiscard]] size_t CountDxUiAllocations(Action&& action)
{
    g_gridModelAllocationProbe = []() noexcept { return allocations; };
    g_gridModelAllocations     = 0u;
    allocations                = 0u;
    countAllocations           = true;
    action();
    countAllocations           = false;
    g_gridModelAllocationProbe = nullptr;
    return allocations - g_gridModelAllocations;
}

struct InputAllocations
{
    size_t collapse  = 0u; // the first Down after the select-all, which replaces the whole selection
    size_t arrows    = 0u; // the most any of the ten Down keys after it made
    size_t clicks    = 0u; // the most any plain click or Ctrl+click on a visible row made
    size_t keyPairs  = 0u; // arrows measured
    size_t clickings = 0u; // clicks measured
};

// After FX3's phases: the select-all again, then Down keys and clicks one by one, each measured alone (its frame is not).
inline InputAllocations MeasureInputAfterSelectAll(EmbeddedInstance& instance, GraphicsFixture& gpu)
{
    DxUi::Grid& grid  = Fx3Grid(instance);
    Context& context  = instance.GetContext();
    const float scale = context.variant.dpi / 96.0f;
    Check(grid.OnSelectAll(context.controls), "the select-all is handled");
    Hr(instance.Frame(gpu), "FX3 frame after the select-all");
    Check(grid.GetSelectionModel().GetCount() == grid.GetModel()->GetRowCount(), "the select-all selects every row of the expanded groups");
    const auto key = [&]
    {
        static_cast<void>(context.port.Key(VK_DOWN, true));
        static_cast<void>(context.port.Key(VK_DOWN, false));
    };
    InputAllocations measured;
    measured.collapse = CountDxUiAllocations(key);
    Hr(instance.Frame(gpu), "FX3 frame after the collapse");
    // From the first rows (Home, not measured), so that the clicks below land on rows on screen above the focused one.
    static_cast<void>(context.port.Key(VK_HOME, true));
    static_cast<void>(context.port.Key(VK_HOME, false));
    Hr(instance.Frame(gpu), "FX3 frame after Home");
    static_cast<void>(context.port.Key(VK_DOWN, true));
    static_cast<void>(context.port.Key(VK_DOWN, false));
    Hr(instance.Frame(gpu), "FX3 frame after the first Down");
    for (size_t index = 0u; index < 10u; ++index)
    {
        measured.arrows = (std::max)(measured.arrows, CountDxUiAllocations(key));
        ++measured.keyPairs;
        Hr(instance.Frame(gpu), "FX3 frame after an arrow");
    }
    // Clicks on rows on screen: a hover move first (its tooltip work is not a click's), then the press and release alone. A plain
    // click selects the row above the focused one; a Ctrl+click toggles the focused row off and on again, within the room the
    // selection already has (adding a row beyond it would grow the selection, which is not this gate's).
    struct Click
    {
        UINT modifiers;
        bool above;
    };
    for (const Click click : {Click{0u, true}, Click{MK_CONTROL, false}, Click{MK_CONTROL, false}, Click{0u, true}, Click{MK_CONTROL, false}})
    {
        const UINT modifiers                = click.modifiers;
        const std::optional<size_t> focused = grid.GetFocusedRowId() ? grid.GetModel()->FindRowByStableId(*grid.GetFocusedRowId()) : std::nullopt;
        Check(focused.has_value(), "a row is focused");
        const std::optional<D2D1_RECT_F> rect = grid.GetVisibleRowRect(click.above ? *focused - 1u : *focused);
        Check(rect.has_value(), "the clicked row is on screen");
        // Each click in another column, so that no two consecutive presses make a double-click.
        const float x = (rect->left + 36.0f + Fx3::kColumnWidth * static_cast<float>(measured.clickings % 3u)) * scale;
        const float y = (rect->top + rect->bottom) * 0.5f * scale;
        static_cast<void>(context.port.Pointer({DxUi::PointerAction::Move, x, y, modifiers}));
        Hr(instance.Frame(gpu), "FX3 frame after the hover");
        const size_t clickAllocations = CountDxUiAllocations([&]
        {
            static_cast<void>(context.port.Pointer({DxUi::PointerAction::Down, x, y, modifiers}));
            static_cast<void>(context.port.Pointer({DxUi::PointerAction::Up, x, y, modifiers}));
        });
        std::cout << "  click " << measured.clickings << (modifiers != 0u ? " (Ctrl)" : "") << ": " << clickAllocations << " C++ allocations\n";
        measured.clicks = (std::max)(measured.clicks, clickAllocations);
        ++measured.clickings;
        Hr(instance.Frame(gpu), "FX3 frame after a click");
    }
    return measured;
}

// The W2.5 gate on one FX3 variant (plan W2.5, budgets G3 and G13): see the file comment.
inline void CheckFx3Gate(GraphicsFixture& gpu, const Variant& variant)
{
    const auto started = std::chrono::steady_clock::now();
    InputAllocations input;
    RunOptions options;
    options.afterPhases = [&](EmbeddedInstance& instance, GraphicsFixture& device) { input = MeasureInputAfterSelectAll(instance, device); };
    const RunRecord run = RunEmbedded(Fx3::kDefinition, variant, options);
    Check(run.trackerOverflow == 0u, "the tracker observed every resource FX3 created");
    for (size_t phase = 0u; phase < Fx3::kPhases.size(); ++phase)
    {
        if (Fx3::kPhases[phase].kind != PhaseKind::Measured)
            continue;
        const auto& steps = run.phases[phase].steps;
        for (size_t step = 0u; step < steps.size(); ++step)
        {
            const DxUi::Detail::ListWork& lists = steps[step].work.lists;
            // A new model (grouped-wheel step 0) fills the caches once; every other frame reads them.
            const bool modelChange = phase == Fx3::kGroupedWheel && step == 0u;
            if ((lists.gridGetColumnCalls != 0u && ! modelChange) || lists.gridColumnDescRebuilds != (modelChange ? 1u : 0u) ||
                lists.gridCollectOrderedGroups != (modelChange ? 1u : 0u) || lists.gridGroupLayoutRebuilds != (modelChange ? 1u : 0u) ||
                lists.gridSelectionCopies != 0u)
                std::cerr << "FX3 " << Fx3::kPhases[phase].name << " step " << step << ": GetColumn " << lists.gridGetColumnCalls << ", column fills "
                          << lists.gridColumnDescRebuilds << ", group collections " << lists.gridCollectOrderedGroups << ", group fills "
                          << lists.gridGroupLayoutRebuilds << ", selection copies " << lists.gridSelectionCopies << '\n';
            Check((lists.gridGetColumnCalls == 0u || modelChange) && lists.gridColumnDescRebuilds == (modelChange ? 1u : 0u),
                  "an FX3 frame asks the model for no column description: one fill per model");
            Check(lists.gridCollectOrderedGroups == (modelChange ? 1u : 0u) && lists.gridGroupLayoutRebuilds == (modelChange ? 1u : 0u),
                  "an FX3 frame collects no groups: one collection per model");
            Check(lists.gridSelectionCopies == 0u, "no FX3 step copies the selection");
        }
    }
    Check(input.keyPairs == 10u && input.clickings == 5u, "every input after the select-all was measured");
    std::cout << "FX3@" << variant.name << " input after the select-all: collapse " << input.collapse << ", most per Down " << input.arrows
              << ", most per click " << input.clicks << " C++ allocations\n";
#if _ITERATOR_DEBUG_LEVEL == 0 && ! defined(__SANITIZE_ADDRESS__)
    // Release only: Debug and ASan builds allocate a checked-iterator proxy for every standard container they construct.
    Check(input.arrows == 0u, "a Down key after the select-all allocates nothing");
    Check(input.clicks == 0u, "a click or Ctrl+click after the select-all allocates nothing");
    // The first Down replaces the whole selection: the selection model gives back the room of the select-all and takes room for
    // one id in each of its two copies (Specs/Core/Core_PerformanceAndResources.md, the selection budget); nothing else.
    Check(input.collapse == 2u, "the Down that replaces the select-all allocates only the selection's two one-id buffers");
#endif

    // Held cell layouts never exceed the cells the step's paint drew, and every frame equals a fresh view's.
    std::vector<uint64_t> incremental;
    PlayFx3(gpu,
            variant,
            false,
            [&](size_t phase, size_t step, EmbeddedInstance& instance)
    {
        const DxUi::EmbeddedStatistics statistics = instance.View().GetStatistics();
        const DxUi::GridVisibleWorkMetrics cells  = Fx3Grid(instance).GetVisibleWorkMetrics();
        if (statistics.retainedTextLayouts > cells.visibleCellCount)
            std::cerr << "FX3 " << Fx3::kPhases[phase].name << " step " << step << ": " << statistics.retainedTextLayouts << " layouts held for "
                      << cells.visibleCellCount << " cells\n";
        Check(statistics.retainedTextLayouts <= cells.visibleCellCount, "the cell layouts held never exceed the cells the last paint drew");
        DxUi::WindowHostBitmapCapture surface;
        Hr(instance.View().DebugReadSurfaceForTest(surface), "FX3 surface read");
        incremental.push_back(HashBytes(surface.bgraPixels));
    });
    const std::vector<uint64_t> fresh = Fx3FrameHashes(gpu, variant, true);
    Check(fresh.size() == incremental.size(), "both plays render every step");
    for (size_t index = 0u; index < fresh.size(); ++index)
        if (fresh[index] != incremental[index])
        {
            const std::optional<PhaseStep> at = StepAt(Fx3::kDefinition, index);
            std::cerr << "FX3 " << Fx3::kPhases[at->phase].name << " step " << at->step << " differs from a fresh view\n";
            Check(false, "every FX3 frame equals a fresh view's (each registered cache dropped)");
        }
    std::cout << "PASS W2.5 Grid data paths on FX3@" << variant.name << ": no column or group query per steady frame, at most " << input.arrows
              << " allocations per arrow (" << input.keyPairs << ") and " << input.clicks << " per click (" << input.clickings << ") after the select-all, "
              << input.collapse << " for the collapse, layouts within the drawn cells and " << incremental.size() << " frames equal to a fresh view ("
              << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count() << " ms)\n";
}

// The Embedded suite's gate: FX3's default variant (1,000 groups), then once the receipt-only grouped-full variant (10,000
// groups, 200,000 rows), which the determinism gate leaves out; with the caches it takes seconds (5 s Release, WARP).
__declspec(noinline) inline void TestFx3GridDataPathGate(GraphicsFixture& gpu)
{
    // The plays resize the suite's shared target to FX3's 800x600; the tests after this one composite into it and read it at
    // the size they found, so it is given back as it was.
    const UINT width  = gpu.width;
    const UINT height = gpu.height;
    CheckFx3Gate(gpu, Fx3::kVariants[0]);
    CheckFx3Gate(gpu, *Fx3::kDefinition.FindVariant("grouped-full"));
    Hr(gpu.Resize(width, height), "restore the shared target's size after the W2.5 gate");
}

[[nodiscard]] inline const Variant* FindFx3Variant(const wchar_t* text)
{
    std::string name;
    for (const wchar_t c : std::wstring_view(text))
        name.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return Fx3::kDefinition.FindVariant(name);
}

// DxUi.EmbeddedTests.exe --test-grid-data-paths [variant]
inline int RunGateCommand(int argc, wchar_t** argv)
{
    const Variant* variant = argc >= 3 ? FindFx3Variant(argv[2]) : &Fx3::kVariants[0];
    if (! variant || argc > 3)
    {
        std::fputs("--test-grid-data-paths takes an optional FX3 variant.\n", stderr);
        return 2;
    }
    GraphicsFixture gpu;
    gpu.width  = variant->width;
    gpu.height = variant->height;
    Hr(gpu.Create(), "W2.5 gate device");
    CheckFx3Gate(gpu, *variant);
    return 0;
}

// DxUi.EmbeddedTests.exe --fx3-frame-hashes <variant> <output>
inline int RunFrameHashCommand(int argc, wchar_t** argv)
{
    if (argc != 4)
    {
        std::fputs("--fx3-frame-hashes requires a variant and an output path.\n", stderr);
        return 2;
    }
    const Variant* variant = FindFx3Variant(argv[2]);
    if (! variant)
    {
        std::fputs("--fx3-frame-hashes names no FX3 variant.\n", stderr);
        return 2;
    }
    GraphicsFixture gpu;
    gpu.width  = variant->width;
    gpu.height = variant->height;
    Hr(gpu.Create(), "FX3 frame hash device");
    const std::vector<uint64_t> hashes = Fx3FrameHashes(gpu, *variant, false);
    std::ofstream output{std::filesystem::path(argv[3]), std::ios::binary};
    Check(bool(output), "FX3 frame hash file");
    size_t index = 0u;
    for (size_t phase = 0u; phase < Fx3::kPhases.size(); ++phase)
        for (size_t step = 0u; step < Fx3::kPhases[phase].steps; ++step)
            output << Fx3::kPhases[phase].name << ' ' << step << ' ' << std::hex << hashes[index++] << std::dec << '\n';
    output.close();
    Check(bool(output), "FX3 frame hashes written");
    std::cout << "FX3@" << variant->name << ": " << hashes.size() << " frame hashes written to " << std::filesystem::path(argv[3]).string() << '\n';
    return 0;
}
} // namespace GridDataPathTests
