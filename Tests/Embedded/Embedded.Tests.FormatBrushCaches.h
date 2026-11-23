#pragma once
// Plan item W2.8, format and brush caches, on the embedded host (Specs/Core/Core_PerformanceAndResources.md, "Format and
// brush caches"). Included once, from Embedded.Tests.Embedded.cpp, after Embedded.Tests.PerformanceGridDataPaths.h, whose
// HashBytes it uses, and Embedded.Tests.PerformanceFixtures.h, whose runner, GraphicsFixture, Check and Hr it uses.
//
// - TestFormatBrushCacheGates: FX12 (default and typography-120, 120 distinct size-and-weight keys requested on every
//   paint) makes no forced full visual re-preparation, creates and evicts no text format and trims nothing in any measured
//   step; in FX11 the Tree expand and collapse phases, the transitions that animate a color (a row's fade), create no brush
//   after their first frame at 30, 60, 144 and 240 Hz and draw that color through the host's transition brushes; and every
//   FX11 and FX12 frame equals a fresh view's, whose registered retained caches (both maps among them) were dropped first.
// - TestFormatWorkingSetAboveBound: 300 configured formats in use on every frame stay resident (the bound is soft for
//   entries in use, counted as over bound), create nothing after the first frame and never thrash; once 200 of them stop
//   being drawn they are evicted on the third frame, not earlier; showing them again creates them again.
// - TestBrushEvictionByLastUse: keyed brushes beyond the bound are released only when the last two frames did not use them.
// - TestHiddenAndDetachedCaches: a hidden view trims, evicts and creates nothing and keeps both maps as before it hid (a
//   frame is a preparation, and a hidden view prepares none); the first frames after showing create nothing for what it
//   drew before; a detached host keeps no format and no brush.
// - TestDpiChangeKeepsFormats: FX12 typography-120 prepared at 144 DPI keeps its 120 live formats (none created or evicted)
//   and draws what a fresh view draws at that DPI.
// - --fixture-frame-hashes FX<N>[@variant] <output>: one FNV-1a hash of the prepared surface per step, in phase order: the
//   pixel receipt comparing a build before W2.8 with one after it (Measurements/PerformanceAndMotion_2026-11-13/W2.8).
// - --test-format-brush-caches: the tests above alone.
#include "Embedded.Tests.PerformanceFixtures.h"
#include "Embedded.Tests.PerformanceGridDataPaths.h"

#include <cstdio>
#include <fstream>
#include <span>

namespace FormatBrushCacheTests
{
using namespace PerformanceFixtures;

// Plays `definition` at `variant`, one frame per step, and calls visit(phase, step, instance) after every frame. A fresh play
// drops every registered retained cache of the view before each frame (the incremental-versus-fresh oracle's fresh side).
template <typename Visit> void PlayFixture(GraphicsFixture& gpu, const Definition& definition, const Variant& variant, bool fresh, Visit&& visit)
{
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "W2.8 scene build");
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            instance.Step(phase, step);
            if (fresh)
                instance.View().DebugDropRetainedStateForTest();
            Hr(instance.Frame(gpu), "W2.8 frame");
            visit(phase, step, instance);
        }
}

[[nodiscard]] inline uint64_t SurfaceHash(EmbeddedInstance& instance)
{
    DxUi::WindowHostBitmapCapture surface;
    Hr(instance.View().DebugReadSurfaceForTest(surface), "W2.8 surface read");
    return GridDataPathTests::HashBytes(surface.bgraPixels);
}

[[nodiscard]] inline std::vector<uint64_t> FrameHashes(GraphicsFixture& gpu, const Definition& definition, const Variant& variant, bool fresh)
{
    std::vector<uint64_t> hashes;
    PlayFixture(gpu, definition, variant, fresh, [&](size_t, size_t, EmbeddedInstance& instance) { hashes.push_back(SurfaceHash(instance)); });
    return hashes;
}

[[nodiscard]] inline uint64_t Counter(const StepRecord& step, std::string_view name)
{
    const std::optional<uint64_t> value = CounterValue(step, name);
    Check(value.has_value(), "a W2.8 gate names a counter that exists");
    return value.value_or(0u);
}

// FX12's format gate on one run (either host's runner records the same counters).
inline void CheckFx12FormatRun(const RunRecord& run, std::string_view host)
{
    uint64_t lookupsChecked = 0u;
    for (size_t phase = 0u; phase < run.definition->phases.size(); ++phase)
    {
        if (run.definition->phases[phase].kind != PhaseKind::Measured)
            continue;
        const auto& steps = run.phases[phase].steps;
        for (size_t step = 0u; step < steps.size(); ++step)
        {
            const uint64_t full    = Counter(steps[step], "frame.fullVisualPreparations");
            const uint64_t trims   = Counter(steps[step], "text.formatCacheTrims");
            const uint64_t evicted = Counter(steps[step], "text.formatEvictions");
            const uint64_t created = Counter(steps[step], "resources.textFormat.created");
            if (full != 0u || trims != 0u || evicted != 0u || created != 0u)
                std::cerr << "FX12@" << run.variant->name << " (" << host << ") " << run.definition->phases[phase].name << " step " << step
                          << ": full preparations " << full << ", trims " << trims << ", formats evicted " << evicted << ", formats created " << created
                          << '\n';
            Check(full == 0u, "W2.8: an FX12 frame forces no full visual re-preparation");
            Check(trims == 0u && evicted == 0u, "W2.8: an FX12 frame trims and evicts no text format");
            Check(created == 0u, "W2.8: an FX12 frame creates no text format after the warm-up");
            ++lookupsChecked;
        }
    }
    Check(lookupsChecked > 0u, "the FX12 gate checked measured steps");
}

// FX11's brush gate on one run: the Tree expand and collapse windows at every rate.
inline void CheckFx11BrushRun(const RunRecord& run, std::string_view host)
{
    for (size_t rate = 0u; rate < Fx11::kRates.size(); ++rate)
        for (const Fx11::Transition transition : {Fx11::Transition::TreeExpand, Fx11::Transition::TreeCollapse})
        {
            const size_t phase = Fx11::PhaseIndex(rate, transition);
            const auto& steps  = run.phases[phase].steps;
            uint64_t reuses    = 0u;
            for (size_t step = 0u; step < steps.size(); ++step)
            {
                reuses += Counter(steps[step], "frame.brushColorReuses");
                const uint64_t created = Counter(steps[step], "resources.brush.created");
                if (step > 0u && created != 0u)
                    std::cerr << "FX11 (" << host << ") " << Fx11::kPhases[phase].name << " step " << step << ": " << created << " brushes created\n";
                Check(step == 0u || created == 0u, "W2.8: an FX11 color transition creates no brush after its first frame");
            }
            Check(reuses > 0u, "W2.8: an FX11 Tree fade draws its colors through the host's transition brushes");
        }
}

// The incremental-versus-fresh oracle over every step of a fixture variant. A step of a phase keyColorPhase accepts may
// differ: brushes are keyed by color quantized to 8 bits per channel and a key draws the exact color it was first asked
// for, so a fade whose colors share keys with an earlier phase's draws those earlier colors (as before W2.8, which keeps
// that rule), while a fresh view, whose brush map was dropped, draws the current ones. Returns the steps that differ.
template <typename Accepts> inline size_t CheckFreshParity(GraphicsFixture& gpu, const Definition& definition, const Variant& variant, Accepts&& keyColorPhase)
{
    const std::vector<uint64_t> incremental = FrameHashes(gpu, definition, variant, false);
    const std::vector<uint64_t> fresh       = FrameHashes(gpu, definition, variant, true);
    Check(fresh.size() == incremental.size(), "both plays render every step");
    size_t differing = 0u;
    for (size_t index = 0u; index < fresh.size() && index < incremental.size(); ++index)
        if (fresh[index] != incremental[index])
        {
            const std::optional<PhaseStep> at = StepAt(definition, index);
            const std::string_view phase      = definition.phases[at->phase].name;
            ++differing;
            if (! keyColorPhase(phase))
            {
                std::cerr << definition.id << '@' << variant.name << ' ' << phase << " step " << at->step << " differs from a fresh view\n";
                Check(false, "W2.8: every frame equals a fresh view's (each registered cache, both maps among them, dropped)");
            }
        }
    return differing;
}

__declspec(noinline) inline void TestFormatBrushCacheGates(GraphicsFixture& gpu)
{
    const UINT width  = gpu.width;
    const UINT height = gpu.height;
    for (const char* name : {"default", "typography-120"})
    {
        const Variant* variant = Fx12::kDefinition.FindVariant(name);
        Check(variant != nullptr, "FX12 declares the W2.8 variants");
        CheckFx12FormatRun(RunEmbedded(Fx12::kDefinition, *variant), "embedded");
        Check(CheckFreshParity(gpu, Fx12::kDefinition, *variant, [](std::string_view) { return false; }) == 0u, "every FX12 frame equals a fresh view's");
    }
    CheckFx11BrushRun(RunEmbedded(Fx11::kDefinition, Fx11::kVariants[0]), "embedded");
    const size_t fades = CheckFreshParity(gpu, Fx11::kDefinition, Fx11::kVariants[0], [](std::string_view phase) {
        return phase.starts_with("tree-expand") || phase.starts_with("tree-collapse");
    });
    std::cout << "FX11 (embedded): " << fades
              << " Tree fade frames draw a color key's first color, unlike a fresh view; every other frame equals a fresh view's\n";
    Hr(gpu.Resize(width, height), "restore the shared target's size after the W2.8 gate");
    std::cout << "PASS W2.8 format and brush caches on FX12 (default, typography-120) and FX11, embedded\n";
}

// A control that asks the host for one configured format of its own on every paint, as a Tree row with a typography
// override does, and draws a glyph with it.
// Probes are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class FormatProbe final : public DxUi::Control
{
public:
    FormatProbe(float sizeDip, DWRITE_FONT_WEIGHT weight) noexcept : sizeDip_(sizeDip), weight_(weight)
    {
    }
    void Paint(DxUi::ControlHost& host) const override
    {
        auto* const format = host.GetTextFormat(DxUi::FontRole::Body, sizeDip_, weight_, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, false);
        auto* const dc     = host.GetDeviceContext();
        auto* const brush  = host.GetSolidBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f));
        if (format && dc && brush)
            dc->DrawTextW(L"Ag", 2u, format, GetBounds(), brush);
    }

private:
    float sizeDip_             = 12.0f;
    DWRITE_FONT_WEIGHT weight_ = DWRITE_FONT_WEIGHT_NORMAL;
};
#pragma warning(pop)

// 300 probes with 300 distinct (size, weight) keys: 60 sizes from 6 to 35.5 DIP and five weights.
inline std::vector<FormatProbe*> AddProbes(DxUi::Panel& root, size_t count)
{
    std::vector<FormatProbe*> probes;
    for (size_t key = 0u; key < count; ++key)
    {
        auto* probe = root.AddChild<FormatProbe>(6.0f + static_cast<float>(key % 60u) * 0.5f, static_cast<DWRITE_FONT_WEIGHT>(300u + (key / 60u % 5u) * 100u));
        const float x = static_cast<float>(key % 20u) * 48.0f;
        const float y = static_cast<float>(key / 20u) * 40.0f;
        probe->SetBounds(D2D1::RectF(x, y, x + 46.0f, y + 38.0f));
        probes.push_back(probe);
    }
    return probes;
}

struct FrameWork
{
    DxUi::Detail::WorkCounters work{};
    uint64_t hash = 0u;
};

inline FrameWork CountedFrame(GraphicsFixture& gpu, DxUi::EmbeddedHost& view, UINT width, UINT height, float dpi = 96.0f)
{
    const DxUi::Detail::WorkCounterScope scope;
    Hr(view.Prepare(width, height, dpi), "W2.8 probe frame");
    FrameWork frame{scope.Delta(), 0u};
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "W2.8 probe composite");
    DxUi::WindowHostBitmapCapture surface;
    Hr(view.DebugReadSurfaceForTest(surface), "W2.8 probe surface");
    frame.hash = GridDataPathTests::HashBytes(surface.bgraPixels);
    return frame;
}

[[nodiscard]] inline uint64_t Created(const DxUi::Detail::WorkCounters& work, std::string_view kind)
{
    StepRecord step{work, 0u, 0u};
    return Counter(step, std::string("resources.") + std::string(kind) + ".created");
}

__declspec(noinline) inline void TestFormatWorkingSetAboveBound(GraphicsFixture& gpu)
{
    constexpr UINT width = 960u, height = 600u;
    Hr(gpu.Resize(width, height), "W2.8 probe target");
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "W2.8 probe pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool, StoppedTestClock()), "W2.8 probe view");
    auto root                              = std::make_unique<DxUi::Panel>();
    const std::vector<FormatProbe*> probes = AddProbes(*root, 300u);
    view.Controls().SetRoot(std::move(root));
    auto& host = view.Controls();

    const FrameWork first = CountedFrame(gpu, view, width, height);
    Check(Created(first.work, "textFormat") >= 300u, "the first frame creates the 300 probe formats");
    const size_t resident = host.DebugGetConfiguredTextFormatCount();
    Check(resident >= 300u && resident > DxUi::ControlHost::kConfiguredTextFormatCacheLimit, "300 live keys exceed the format bound");
    for (int frame = 0; frame < 4; ++frame)
    {
        view.MarkDirty();
        const FrameWork next = CountedFrame(gpu, view, width, height);
        Check(Created(next.work, "textFormat") == 0u && next.work.text.formatEvictions == 0u && next.work.text.formatCacheTrims == 0u,
              "W2.8: a working set above the bound that is all in use creates and evicts nothing (no thrash)");
        Check(next.work.text.formatCacheOverBound == 1u, "W2.8: a trim that keeps entries above the bound because they are in use is counted");
        Check(next.work.frame.fullVisualPreparations == 0u, "W2.8: no frame of the working set forces a full visual re-preparation");
        Check(next.hash == first.hash, "the probes draw the same pixels on every frame");
        Check(host.DebugGetConfiguredTextFormatCount() == resident, "every format in use stays resident");
    }

    // 200 probes stop being drawn: their formats survive the two frames after, and are evicted by the third trim.
    for (size_t index = 100u; index < probes.size(); ++index)
        probes[index]->SetVisible(false);
    std::array<uint64_t, 3> evicted{};
    for (uint64_t& count : evicted)
    {
        view.MarkDirty();
        count = CountedFrame(gpu, view, width, height).work.text.formatEvictions;
    }
    Check(evicted[0] == 0u && evicted[1] == 0u, "W2.8: a format the current or previous frame used is never evicted");
    Check(evicted[2] == 200u, "W2.8: formats unused for two frames are evicted once the map is above its bound");
    Check(host.DebugGetConfiguredTextFormatCount() == resident - 200u, "the evicted formats leave the map");
    for (size_t index = 100u; index < probes.size(); ++index)
        probes[index]->SetVisible(true);
    view.MarkDirty();
    const FrameWork shown = CountedFrame(gpu, view, width, height);
    Check(Created(shown.work, "textFormat") == 200u, "showing the probes again creates their 200 formats once");
    Check(shown.hash == first.hash, "the recreated formats draw what the evicted ones drew");
    Hr(gpu.Resize(480u, 240u), "restore the shared target after the W2.8 probes");
}

__declspec(noinline) inline void TestBrushEvictionByLastUse(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "W2.8 brush pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(pool, StoppedTestClock()), "W2.8 brush view");
    auto root = std::make_unique<DxUi::Panel>();
    root->AddChild<DxUi::Label>(L"Bounded brushes")->SetBounds(D2D1::RectF(8, 8, 300, 40));
    root->AddChild<DxUi::Toggle>(L"Toggle")->SetBounds(D2D1::RectF(8, 48, 300, 88));
    view.Controls().SetRoot(std::move(root));
    auto& host = view.Controls();
    Hr(view.Prepare(320, 160), "prepare the brush view");
    const size_t painted = host.DebugGetBrushCacheSize();
    Check(painted > 0u && painted <= DxUi::ControlHost::kSolidBrushCacheLimit, "a painted view holds its working set of brushes");
    // Colors asked for between frames count as used by the latest frame: they survive the next two trims.
    for (size_t index = 0u; index <= DxUi::ControlHost::kSolidBrushCacheLimit; ++index)
        Check(host.GetSolidBrush(D2D1::ColorF(static_cast<float>(index % 256u) / 255.0f, static_cast<float>(index / 256u) / 255.0f, 0.5f)) != nullptr,
              "distinct solid brush");
    const size_t over = host.DebugGetBrushCacheSize();
    Check(over > DxUi::ControlHost::kSolidBrushCacheLimit, "distinct colors can exceed the brush bound between frames");
    std::array<uint64_t, 3> evicted{};
    std::array<size_t, 3> sizes{};
    for (size_t frame = 0u; frame < evicted.size(); ++frame)
    {
        view.MarkDirty();
        const DxUi::Detail::WorkCounterScope scope;
        Hr(view.Prepare(320, 160), "prepare after the extra colors");
        evicted[frame] = scope.Delta().frame.brushEvictions;
        sizes[frame]   = host.DebugGetBrushCacheSize();
    }
    Check(evicted[0] == 0u && evicted[1] == 0u && sizes[1] == over, "W2.8: a brush the current or previous frame used is never evicted");
    Check(evicted[2] == over - painted && sizes[2] == painted, "W2.8: the third trim releases exactly the brushes the last two frames did not use");
}

__declspec(noinline) inline void TestHiddenAndDetachedCaches(GraphicsFixture& gpu)
{
    const Variant& variant = *Fx12::kDefinition.FindVariant("typography-120");
    const UINT width       = gpu.width;
    const UINT height      = gpu.height;
    EmbeddedInstance instance;
    instance.Instantiate(Fx12::kDefinition);
    Hr(instance.Create(gpu.device.get(), variant), "W2.8 hidden scene");
    for (int frame = 0; frame < 3; ++frame)
    {
        instance.View().MarkDirty();
        Hr(instance.Frame(gpu), "W2.8 hidden scene frame");
    }
    auto& host               = instance.View().Controls();
    const size_t formats     = host.DebugGetConfiguredTextFormatCount();
    const size_t brushes     = host.DebugGetBrushCacheSize();
    const uint64_t shownHash = SurfaceHash(instance);
    Check(formats >= 120u && brushes > 0u, "the typography scene holds its formats and brushes");
    instance.View().SetVisible(false);
    for (int frame = 0; frame < 4; ++frame)
    {
        const DxUi::Detail::WorkCounterScope scope;
        instance.View().MarkDirty();
        Check(instance.Prepare() == S_FALSE, "a hidden view prepares nothing");
        const DxUi::Detail::WorkCounters work = scope.Delta();
        Check(work.text.formatEvictions == 0u && work.frame.brushEvictions == 0u && work.text.formatCacheTrims == 0u,
              "W2.8: a hidden view trims and evicts nothing: it has no frames");
    }
    Check(host.DebugGetConfiguredTextFormatCount() == formats && host.DebugGetBrushCacheSize() == brushes,
          "a hidden view keeps both maps as they were (as before W2.8)");
    instance.View().SetVisible(true);
    {
        const DxUi::Detail::WorkCounterScope scope;
        Hr(instance.Frame(gpu), "W2.8 frame after showing");
        const DxUi::Detail::WorkCounters work = scope.Delta();
        Check(Created(work, "textFormat") == 0u && work.text.formatEvictions == 0u, "W2.8: the first frame after showing creates no format it drew before");
        Check(SurfaceHash(instance) == shownHash, "the shown view draws what it drew before hiding");
    }
    instance.View().Detach();
    Check(host.DebugGetConfiguredTextFormatCount() == 0u && host.DebugGetBrushCacheSize() == 0u, "W2.8: a detached host keeps no format and no brush");
    Hr(gpu.Resize(width, height), "restore the shared target after the hidden scene");
}

__declspec(noinline) inline void TestDpiChangeKeepsFormats(GraphicsFixture& gpu)
{
    const Variant& variant = *Fx12::kDefinition.FindVariant("typography-120");
    const UINT width       = gpu.width;
    const UINT height      = gpu.height;
    EmbeddedInstance instance;
    instance.Instantiate(Fx12::kDefinition);
    Hr(instance.Create(gpu.device.get(), variant), "W2.8 DPI scene");
    for (int frame = 0; frame < 3; ++frame)
    {
        instance.View().MarkDirty();
        Hr(instance.Frame(gpu), "W2.8 DPI scene frame");
    }
    auto& host           = instance.View().Controls();
    const size_t formats = host.DebugGetConfiguredTextFormatCount();
    Check(formats >= 120u, "the typography scene holds its 120 live formats");
    instance.Resize(variant.width, variant.height, 144.0f);
    {
        const DxUi::Detail::WorkCounterScope scope;
        Hr(instance.Frame(gpu), "W2.8 frame at 144 DPI");
        const DxUi::Detail::WorkCounters work = scope.Delta();
        Check(Created(work, "textFormat") == 0u && work.text.formatEvictions == 0u && work.text.formatCacheTrims == 0u,
              "W2.8: a DPI change keeps every live format (formats are device-independent)");
        std::cout << "FX12@typography-120 at 144 DPI (embedded): " << work.frame.fullVisualPreparations << " full visual preparations\n";
    }
    Check(host.DebugGetConfiguredTextFormatCount() == formats, "the format map is unchanged by the DPI change");
    const uint64_t incremental = SurfaceHash(instance);
    instance.View().DebugDropRetainedStateForTest();
    Hr(instance.Frame(gpu), "W2.8 fresh frame at 144 DPI");
    Check(SurfaceHash(instance) == incremental, "W2.8: the frame at 144 DPI equals a fresh view's");
    Hr(gpu.Resize(width, height), "restore the shared target after the DPI scene");
}

inline void RunAll(GraphicsFixture& gpu)
{
    TestFormatBrushCacheGates(gpu);
    TestFormatWorkingSetAboveBound(gpu);
    TestBrushEvictionByLastUse(gpu);
    TestHiddenAndDetachedCaches(gpu);
    TestDpiChangeKeepsFormats(gpu);
}

// DxUi.EmbeddedTests.exe --test-format-brush-caches
inline int RunTestCommand()
{
    GraphicsFixture gpu;
    Hr(gpu.Create(), "W2.8 device");
    RunAll(gpu);
    return 0;
}

// DxUi.EmbeddedTests.exe --fixture-frame-hashes FX<N>[@variant] <output>
inline int RunFrameHashCommand(int argc, wchar_t** argv)
{
    if (argc != 4)
    {
        std::fputs("--fixture-frame-hashes requires FX<N>[@variant] and an output path.\n", stderr);
        return 2;
    }
    const FixtureSelection selection = ParseSelection(argv[2]);
    const Definition* definition     = FindFixture(selection.id);
    const Variant* variant           = definition ? FindVariant(*definition, selection.variant) : nullptr;
    if (! definition || definition->interactiveOnly || ! definition->Built() || ! Supports(definition->hosts, Host::Embedded) || ! variant ||
        variant->hardwareOnly)
    {
        std::fputs("--fixture-frame-hashes names no WARP variant of a fixture built for the embedded host.\n", stderr);
        return 2;
    }
    GraphicsFixture gpu;
    gpu.width  = variant->width;
    gpu.height = variant->height;
    Hr(gpu.Create(), "frame hash device");
    const std::vector<uint64_t> hashes = FrameHashes(gpu, *definition, *variant, false);
    std::ofstream output{std::filesystem::path(argv[3]), std::ios::binary};
    Check(bool(output), "frame hash file");
    size_t index = 0u;
    for (size_t phase = 0u; phase < definition->phases.size(); ++phase)
        for (size_t step = 0u; step < definition->phases[phase].steps; ++step)
            output << definition->phases[phase].name << ' ' << step << ' ' << std::hex << hashes[index++] << std::dec << '\n';
    output.close();
    Check(bool(output), "frame hashes written");
    std::cout << definition->id << '@' << variant->name << " (embedded): " << hashes.size() << " frame hashes written to "
              << std::filesystem::path(argv[3]).string() << '\n';
    return 0;
}
} // namespace FormatBrushCacheTests
