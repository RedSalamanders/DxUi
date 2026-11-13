#pragma once
// The incremental-versus-fresh oracle on the embedded host (W0.5): WARP, the view's own surface texture read exactly
// (DebugReadSurfaceForTest, no composition or sampling in between), the control bounds and the UI Automation tree.
// Embedded preparation clears and repaints the whole surface today, and the surface persists between preparations, so a
// later partial preparation (W4.7) is compared here against a dropped surface, with no extra reader needed.
#include "Controls/DxUi.RetainedCaches.h"
#include "Support.Tests.IncrementalParity.h"
#include "Support/RetainedCaches.Tests.Hooks.h"

namespace EmbeddedParity
{
using namespace IncrementalParity;

constexpr D2D1_SIZE_F kSceneDip{660.0f, 480.0f};
constexpr uint64_t kClockStartMs = 1'000'000u;

struct ParitySite final : DxUi::EmbeddedAccessibilitySite
{
    IRawElementProviderFragmentRoot* root = nullptr; // Borrowed; cleared before the view detaches.
    void ActionCompleted() noexcept override
    {
    }
    HRESULT Navigate(NavigateDirection, IRawElementProviderFragment** result) noexcept override
    {
        *result = nullptr;
        return S_OK;
    }
    HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        *result = root;
        if (root)
            root->AddRef();
        return root ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT RequestFocus() noexcept override
    {
        return S_OK;
    }
};

struct EmbeddedParityScene final
{
    static constexpr uint32_t kKnownDifferences = kNoKnownDifference;
    static constexpr bool kCrossStepCapture     = true;
    DxUi::EmbeddedHost view;
    std::shared_ptr<ParitySite> site = std::make_shared<ParitySite>();
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    std::array<DxUi::Control*, kParitySlots> slots{};
    std::vector<std::shared_ptr<void>> models;
    bool dragging  = false;
    uint64_t clock = kClockStartMs;
    UINT dpi       = 96u;

    explicit EmbeddedParityScene(ID3D11Device* device)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(device, graphics), "parity graphics pool");
        Hr(view.Attach(std::move(graphics)), "parity view attachment");
    }
    ~EmbeddedParityScene()
    {
        site->root = nullptr;
        provider.reset();
        view.Detach();
    }
    EmbeddedParityScene(const EmbeddedParityScene&)            = delete;
    EmbeddedParityScene& operator=(const EmbeddedParityScene&) = delete;

    [[nodiscard]] DxUi::ControlHost& Host() noexcept
    {
        return view.Controls();
    }
    void SetRoot(std::unique_ptr<DxUi::Control> root)
    {
        view.Controls().SetRoot(std::move(root));
    }
    [[nodiscard]] UINT WidthPx() const noexcept
    {
        return static_cast<UINT>(std::lround(kSceneDip.width * static_cast<float>(dpi) / 96.0f));
    }
    [[nodiscard]] UINT HeightPx() const noexcept
    {
        return static_cast<UINT>(std::lround(kSceneDip.height * static_cast<float>(dpi) / 96.0f));
    }
    void Pointer(ParityPointer kind, D2D1_POINT_2F dip, float wheelDelta = 0.0f)
    {
        const float scale = static_cast<float>(dpi) / 96.0f;
        DxUi::PointerEvent event{};
        event.xPixels    = std::round(dip.x * scale);
        event.yPixels    = std::round(dip.y * scale);
        event.wheelDelta = wheelDelta;
        switch (kind)
        {
            case ParityPointer::Move: event.action = DxUi::PointerAction::Move; break;
            case ParityPointer::Down: event.action = DxUi::PointerAction::Down; break;
            case ParityPointer::Up: event.action = DxUi::PointerAction::Up; break;
            case ParityPointer::Wheel: event.action = DxUi::PointerAction::Wheel; break;
            case ParityPointer::Leave: event.action = DxUi::PointerAction::Leave; break;
        }
        static_cast<void>(view.DispatchPointer(event));
    }
    void Key(UINT virtualKey)
    {
        static_cast<void>(view.DispatchKey(virtualKey, true));
        static_cast<void>(view.DispatchKey(virtualKey, false));
    }
    void Character(wchar_t character)
    {
        static_cast<void>(view.DispatchCharacter(character));
    }
    void SetDpi(UINT value)
    {
        dpi = value;
    }
    void Advance(uint64_t milliseconds)
    {
        clock += milliseconds;
        if (view.NeedsAnimation())
            static_cast<void>(view.AdvanceAnimation(clock));
    }
    [[nodiscard]] ParityFrame Capture(bool fresh)
    {
        if (fresh)
        {
            view.DebugDropRetainedStateForTest();
            std::vector<DxUi::RetainedCacheCount> counts;
            DxUi::RetainedCacheAccess::Count(view, counts);
            RequireRetainedCachesEmpty(counts, "embedded drop");
        }
        HRESULT prepared = S_FALSE;
        for (int turn = 0; turn < 512; ++turn)
        {
            prepared = view.Prepare(WidthPx(), HeightPx(), static_cast<float>(dpi));
            if (FAILED(prepared) && prepared != E_ABORT)
                Fail(std::format("embedded parity preparation failed: 0x{:08X}", static_cast<uint32_t>(prepared)));
            if (prepared == S_FALSE && ! view.NeedsPreparation())
                break;
            if (prepared == S_OK && ! view.NeedsPreparation())
                break;
        }
        const DxUi::EmbeddedAccessibilityPlacement placement{{0.0, 0.0, static_cast<double>(WidthPx()), static_cast<double>(HeightPx())}, true};
        if (! provider)
        {
            Hr(view.AttachAccessibility(site, 0x5041524954590001ull, placement), "parity accessibility attachment");
            Hr(view.GetAccessibilityProvider(provider.put()), "parity accessibility provider");
            site->root = provider.get();
        }
        else
            Check(SUCCEEDED(view.UpdateAccessibility(placement)), "parity accessibility publication");
        DxUi::WindowHostBitmapCapture surface;
        Hr(view.DebugReadSurfaceForTest(surface), "parity surface read");
        ParityFrame frame;
        frame.widthPx  = surface.widthPx;
        frame.heightPx = surface.heightPx;
        frame.dpi      = static_cast<float>(dpi);
        frame.bgra     = std::move(surface.bgraPixels);
        frame.controls = CollectControls(Host());
        frame.uia      = DescribeUia(provider.get());
        return frame;
    }
};

constexpr std::string_view kReplay = "DxUi.EmbeddedTests.exe --test-incremental-parity";

inline void TestEmbeddedRetainedCachesDropToZero(GraphicsFixture& gpu)
{
    std::vector<DxUi::RetainedCacheCount> populated;
    for (auto build : {&BuildControlsScene<EmbeddedParityScene>, &BuildRichScene<EmbeddedParityScene>})
    {
        EmbeddedParityScene scene(gpu.device.get());
        build(scene);
        if (scene.slots[kTree])
        {
            scene.Host().SetFocusControl(scene.slots[kField], false);
            static_cast<void>(scene.Capture(false));
            scene.Pointer(ParityPointer::Move, CenterOf(*scene.slots[kButton]));
        }
        static_cast<void>(scene.Capture(false));
        std::vector<DxUi::RetainedCacheCount> counts;
        DxUi::RetainedCacheAccess::Count(scene.view, counts);
        if (populated.empty())
            populated = counts;
        else
            for (size_t index = 0u; index < counts.size(); ++index)
                populated[index].entries += counts[index].entries;
        static_cast<void>(scene.Capture(true)); // Requires every count to read 0 right after the drop.
    }
    std::string unpopulated;
    for (const auto& count : populated)
    {
        std::cout << "  embedded retained cache " << count.name << ": " << count.entries << " before the drop\n";
        const bool exempt = std::ranges::find(kRowsNotPopulatedByScenes, count.name) != kRowsNotPopulatedByScenes.end();
        if (! exempt && count.entries == 0u)
            unpopulated += std::string(count.name) + ' ';
    }
    if (! unpopulated.empty())
        std::cerr << "  unpopulated rows: " << unpopulated << '\n';
    Check(unpopulated.empty(), "every registered cache the parity scenes exercise is populated before the drop");
}

inline void TestEmbeddedIncrementalParityDetectsAStaleCache(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    constexpr std::array<uint64_t, 1u> seed{0x5EED00D1u};
    const ParityScript<EmbeddedParityScene> script{"stale-label",
                                                   seed,
                                                   1u,
                                                   &BuildControlsScene<EmbeddedParityScene>,
                                                   [](EmbeddedParityScene& scene, Rng&, size_t)
    { static_cast<DxUi::Label*>(scene.slots[kMnemonicLabel])->SetText(L"Renamed with mnemonic"); }};
    Check(RunParityScriptOnce(script, seed[0], make).empty(), "without the injection the label edit renders identically");
    DxUi::Detail::SetStaleLabelLayoutInjectionForTest(true);
    const auto restore           = wil::scope_exit([] { DxUi::Detail::SetStaleLabelLayoutInjectionForTest(false); });
    const std::string difference = RunParityScriptOnce(script, seed[0], make);
    std::cerr << "  embedded stale-cache negative control: " << difference << '\n' << std::flush;
    Check(difference.starts_with("step 1 ") && difference.find("pixels differ") != std::string::npos && difference.find("root/1 (") != std::string::npos,
          "an injected stale Label layout yields a nonzero BGRA difference at the label");
}

inline void TestEmbeddedIncrementalParityScripts(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParityScene>(gpu.device.get()); };
    RunParityScript(LayoutScript<EmbeddedParityScene>(), make, "embedded", kReplay);
    RunParityScript(InteractionScript<EmbeddedParityScene>(), make, "embedded", kReplay);
    RunParityScript(RichScript<EmbeddedParityScene>(), make, "embedded", kReplay);
}

inline void TestEmbeddedIncrementalParity(GraphicsFixture& gpu)
{
    TestEmbeddedRetainedCachesDropToZero(gpu);
    TestEmbeddedIncrementalParityDetectsAStaleCache(gpu);
    TestEmbeddedIncrementalParityScripts(gpu);
}
} // namespace EmbeddedParity
