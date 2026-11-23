#pragma once
// ComboBox popup layout (plan item W2.2) on the embedded host. Included once from Embedded.Tests.Embedded.cpp after the
// performance fixtures; uses Check and Hr from there.
//
// - TestFx2PopupLayoutGate: the item's gate on FX2. Per step: the single-line measuring layouts
//   (testSingleLineMeasureLayoutCreations), the popup's width measurements and placements. A first open measures each
//   item once; hovering, typing open or closed and repainting the open popup measure nothing; a second open measures
//   nothing.
// - TestComboBoxPopupMatchesLegacyMeasurement: the retained widths and placement give exactly the bounds and rows of the
//   measurement before W2.2, kept callable through testComboBoxMeasurePopupItemsEveryLayout, on seeded item sets.
// - TestComboBoxPopupWidthsFollowTypography: a device replacement (a new DirectWrite factory) measures the items again.
// - TestComboBoxPopupParity: the "combo-popup" incremental-versus-fresh script (Support.Tests.ComboBoxPopupLayout.h).
#include "Embedded.Tests.IncrementalParity.h"
#include "Support.Tests.ComboBoxPopupLayout.h"

#include <cstring>

namespace ComboBoxPopupLayoutTests
{
struct StepMeasure
{
    uint64_t measureLayouts    = 0u; // DxUi::testSingleLineMeasureLayoutCreations
    uint64_t widthMeasurements = 0u; // popups.comboBoxItemWidthMeasurements
    uint64_t placements        = 0u; // popups.comboBoxPopupLayouts
    uint64_t paints            = 0u; // frame.paintExecutions
};

// One action and its frame on a live fixture, measured.
template <typename Action> [[nodiscard]] StepMeasure MeasureStep(PerformanceFixtures::EmbeddedInstance& instance, GraphicsFixture& gpu, const Action& action)
{
    const uint64_t layoutsBefore = DxUi::testSingleLineMeasureLayoutCreations;
    const DxUi::Detail::WorkCounterScope scope;
    action();
    Hr(instance.Frame(gpu), "FX2 popup layout gate frame");
    const DxUi::Detail::WorkCounters work = scope.Delta();
    return {DxUi::testSingleLineMeasureLayoutCreations - layoutsBefore,
            work.popups.comboBoxItemWidthMeasurements,
            work.popups.comboBoxPopupLayouts,
            work.frame.paintExecutions};
}

__declspec(noinline) inline void TestFx2PopupLayoutGate()
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx2::kDefinition;
    const Variant& variant       = Fx2::kVariants[0];
    GraphicsFixture gpu;
    gpu.width  = variant.width;
    gpu.height = variant.height;
    Hr(gpu.Create(true), "FX2 popup layout gate device");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "FX2 popup layout gate build");

    std::vector<std::vector<StepMeasure>> phases(definition.phases.size());
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
            phases[phase].push_back(MeasureStep(instance, gpu, [&] { instance.Step(phase, step); }));

    const auto total = [&](size_t phase, uint64_t StepMeasure::* field)
    {
        uint64_t sum = 0u, maximum = 0u;
        for (const StepMeasure& step : phases[phase])
        {
            sum += step.*field;
            maximum = (std::max)(maximum, step.*field);
        }
        return std::pair{sum, maximum};
    };
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
    {
        const auto layouts    = total(phase, &StepMeasure::measureLayouts);
        const auto widths     = total(phase, &StepMeasure::widthMeasurements);
        const auto placements = total(phase, &StepMeasure::placements);
        std::cout << "  FX2 " << definition.phases[phase].name << ": single-line measuring layouts " << layouts.first << " (max " << layouts.second
                  << " per step), item widths measured " << widths.first << ", popup placements " << placements.first << " (max " << placements.second
                  << " per step)\n";
    }

    // The first open measures each of the 500 items once, and nothing else measures a single line in that step.
    Check(phases[Fx2::kOpen][0].widthMeasurements == Fx2::kItems && phases[Fx2::kOpen][0].measureLayouts == Fx2::kItems,
          "FX2 open measures each of the 500 items once");
    for (const StepMeasure& step : phases[Fx2::kHover])
        Check(step.measureLayouts == 0u && step.widthMeasurements == 0u && step.placements == 0u && step.paints > 0u,
              "an FX2 open hover frame paints, measures no item and places the popup no more");
    for (const StepMeasure& step : phases[Fx2::kType])
        Check(step.measureLayouts == 0u && step.widthMeasurements == 0u, "a character typed into the closed FX2 ComboBox measures nothing");
    Check(phases[Fx2::kFitOpen][0].widthMeasurements == Fx2::kFitItems && phases[Fx2::kFitOpen][0].measureLayouts == Fx2::kFitItems,
          "FX2 fit-open measures each of the 100 items once");
    for (const StepMeasure& step : phases[Fx2::kFitHover])
        Check(step.measureLayouts == 0u && step.widthMeasurements == 0u && step.placements == 0u && step.paints > 0u,
              "each paint of the 100-item all-fit popup after the first measure creates no measuring layout");

    // A second open of unchanged items measures nothing; typing into the open popup changes only the filtered set.
    Context& context = instance.GetContext();
    const auto key   = [&](UINT virtualKey)
    {
        static_cast<void>(context.port.Key(virtualKey, true));
        static_cast<void>(context.port.Key(virtualKey, false));
    };
    const StepMeasure reopen = MeasureStep(instance, gpu, [&] { key(VK_F4); });
    Check(reopen.measureLayouts == 0u && reopen.widthMeasurements == 0u && reopen.paints > 0u, "a second FX2 open of unchanged items measures nothing");
    for (const wchar_t typed : std::wstring_view(L"Item 4"))
    {
        const StepMeasure step = MeasureStep(instance, gpu, [&] { static_cast<void>(context.port.Character(typed)); });
        Check(step.measureLayouts == 0u && step.widthMeasurements == 0u, "a character typed into the open FX2 popup measures nothing");
    }
    std::cout << "PASS FX2 popup layout gate: first opens measure 500 and 100 items once; hover, typing and repaints measure 0\n";
}

// Popup geometry compared exactly (bit for bit): the placement, the hit bounds and the first rows.
struct PopupGeometry
{
    bool open = false;
    D2D1_RECT_F bounds{};
    D2D1_RECT_F hit{};
    std::array<D2D1_RECT_F, 4u> rows{};
    std::array<D2D1_RECT_F, 4u> textRows{};
    [[nodiscard]] bool operator==(const PopupGeometry& other) const noexcept
    {
        return open == other.open && std::memcmp(&bounds, &other.bounds, sizeof(bounds)) == 0 && std::memcmp(&hit, &other.hit, sizeof(hit)) == 0 &&
               std::memcmp(rows.data(), other.rows.data(), sizeof(rows)) == 0 && std::memcmp(textRows.data(), other.textRows.data(), sizeof(textRows)) == 0;
    }
};

[[nodiscard]] inline PopupGeometry ReadGeometry(DxUi::ComboBox& combo, const DxUi::ControlHost& host, bool legacy)
{
    DxUi::testComboBoxMeasurePopupItemsEveryLayout = legacy;
    PopupGeometry geometry;
    geometry.open = combo.IsPopupOpen();
    constexpr std::array<size_t, 4u> rows{0u, 1u, 7u, 99u};
    for (size_t index = 0u; index < rows.size(); ++index)
    {
        geometry.rows[index]     = combo.DebugGetPopupItemRect(rows[index], &host);
        geometry.textRows[index] = combo.DebugGetPopupItemTextRect(rows[index], &host);
    }
    geometry.bounds                                = combo.DebugGetPopupBounds();
    geometry.hit                                   = combo.GetHitBounds();
    DxUi::testComboBoxMeasurePopupItemsEveryLayout = false;
    return geometry;
}

__declspec(noinline) inline void TestComboBoxPopupMatchesLegacyMeasurement(GraphicsFixture& gpu)
{
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "popup legacy comparison graphics");
    Hr(view.Attach(std::move(graphics), StoppedTestClock()), "popup legacy comparison view");
    DxUi::ControlHost& host = view.Controls();
    host.SetTheme(IncrementalParity::ParityTheme(false));
    auto rootOwner          = std::make_unique<DxUi::Panel>();
    DxUi::Panel* const root = rootOwner.get();
    host.SetRoot(std::move(rootOwner));
    // A preparation sets the host's size and DPI. It also paints: an open legacy ComboBox is then placed once without the
    // seam, which changes nothing that is compared, because every comparison places it again with the seam set.
    const auto setSize = [&](UINT dpi)
    {
        Hr(view.Prepare(
               static_cast<UINT>(MulDiv(720, static_cast<int>(dpi), 96)), static_cast<UINT>(MulDiv(560, static_cast<int>(dpi), 96)), static_cast<float>(dpi)),
           "popup legacy comparison size");
    };
    setSize(96u);

    constexpr std::array<uint64_t, 2u> seeds{0x5EED0221u, 0x5EED0222u};
    size_t compared = 0u;
    for (uint32_t set = 0u; set < static_cast<uint32_t>(ItemSet::Count); ++set)
    {
        for (const uint64_t seed : seeds)
        {
            auto* const legacy = root->AddChild<DxUi::ComboBox>();
            auto* const cached = root->AddChild<DxUi::ComboBox>();
            Rng rng{seed ^ set};
            // Applies one change to both ComboBoxes, the legacy one with the seam set for everything it does.
            const auto apply = [&](const auto& change)
            {
                DxUi::testComboBoxMeasurePopupItemsEveryLayout = true;
                change(*legacy);
                DxUi::testComboBoxMeasurePopupItemsEveryLayout = false;
                change(*cached);
            };
            const auto compare = [&](std::string_view operation)
            {
                const PopupGeometry expected = ReadGeometry(*legacy, host, true);
                const PopupGeometry actual   = ReadGeometry(*cached, host, false);
                if (! (expected == actual))
                    std::cerr << "  legacy/retained popup geometry differs: set " << ItemSetName(static_cast<ItemSet>(set))
                              << std::format(", seed 0x{:X}", seed) << ", after " << operation
                              << std::format(": bounds [{} {} {} {}] vs [{} {} {} {}]\n",
                                             expected.bounds.left,
                                             expected.bounds.top,
                                             expected.bounds.right,
                                             expected.bounds.bottom,
                                             actual.bounds.left,
                                             actual.bounds.top,
                                             actual.bounds.right,
                                             actual.bounds.bottom);
                Check(expected == actual, "the retained popup widths and placement equal the per-layout measurement");
                ++compared;
            };
            apply([&](DxUi::ComboBox& combo)
            {
                combo.SetEditable(true);
                combo.SetAutoOpenOnTextInput(true);
                combo.SetItems(MakeItems(static_cast<ItemSet>(set), seed));
                combo.SetBounds(D2D1::RectF(40.0f, 200.0f, 220.0f, 232.0f));
            });
            apply([&](DxUi::ComboBox& combo) { static_cast<void>(combo.OnKeyDown(host, VK_F4, 0u)); });
            compare("open");
            for (size_t step = 0u; step < 14u; ++step)
            {
                switch (rng.Below(11u))
                {
                    case 0u:
                    case 1u:
                    case 2u:
                    {
                        const std::wstring_view typed = kTyped[rng.Below(static_cast<uint32_t>(kTyped.size()))];
                        apply([&](DxUi::ComboBox& combo)
                        {
                            for (const wchar_t unit : typed)
                                static_cast<void>(combo.OnChar(host, unit, 0u));
                        });
                        compare("typing");
                        break;
                    }
                    case 3u:
                        apply([&](DxUi::ComboBox& combo) { static_cast<void>(combo.OnKeyDown(host, VK_BACK, 0u)); });
                        compare("backspace");
                        break;
                    case 4u:
                    {
                        const size_t rows = rng.Below(10u);
                        apply([&](DxUi::ComboBox& combo) { combo.SetMaxVisibleItems(rows); });
                        compare("maximum rows");
                        break;
                    }
                    case 5u:
                    {
                        const float height = rng.Below(2u) ? 0.0f : 40.0f;
                        apply([&](DxUi::ComboBox& combo) { combo.SetMinimumPopupItemHeight(height); });
                        compare("row height");
                        break;
                    }
                    case 6u:
                    {
                        const float left = 8.0f + 400.0f * rng.Unit(), top = 8.0f + 480.0f * rng.Unit(), width = 60.0f + 500.0f * rng.Unit();
                        apply([&](DxUi::ComboBox& combo) { combo.SetBounds(D2D1::RectF(left, top, left + width, top + 32.0f)); });
                        compare("bounds");
                        break;
                    }
                    case 7u:
                    {
                        DxUi::ThemePalette theme = host.GetTheme();
                        theme.density            = theme.density == DxUi::Density::Compact ? DxUi::Density::Standard : DxUi::Density::Compact;
                        host.SetTheme(theme);
                        compare("density");
                        break;
                    }
                    case 8u:
                    {
                        constexpr std::array<UINT, 3u> dpis{96u, 144u, 192u};
                        setSize(dpis[rng.Below(3u)]);
                        compare("dpi");
                        break;
                    }
                    case 9u:
                    {
                        const auto next      = static_cast<ItemSet>(rng.Below(static_cast<uint32_t>(ItemSet::Count)));
                        const uint64_t items = rng.Next();
                        apply([&](DxUi::ComboBox& combo) { combo.SetItems(MakeItems(next, items)); });
                        compare("replaced items");
                        break;
                    }
                    default:
                    {
                        const bool translated = rng.Below(2u) != 0u;
                        apply([&](DxUi::ComboBox& combo)
                        {
                            combo.SetNoMatchesText(translated ? std::wstring(L"Aucun r\u00E9sultat pour cette recherche") : std::wstring());
                            if (! combo.IsPopupOpen())
                                static_cast<void>(combo.OnKeyDown(host, VK_F4, 0u));
                        });
                        compare("no-matches text or reopen");
                        break;
                    }
                }
            }
            static_cast<void>(root->TakeChild(root->GetLogicalChildCount() - 1u));
            static_cast<void>(root->TakeChild(root->GetLogicalChildCount() - 1u));
            static_cast<void>(root->CompactChildren());
            setSize(96u);
            DxUi::ThemePalette theme = host.GetTheme();
            theme.density            = DxUi::Density::Standard;
            host.SetTheme(theme);
        }
    }
    std::cout << "PASS ComboBox popup: retained widths equal the per-layout measurement in " << compared << " comparisons over "
              << static_cast<uint32_t>(ItemSet::Count) << " item sets\n";
}

__declspec(noinline) inline void TestComboBoxPopupWidthsFollowTypography(GraphicsFixture& gpu)
{
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "popup typography graphics");
    Hr(view.Attach(graphics, StoppedTestClock()), "popup typography view");
    auto rootOwner = std::make_unique<DxUi::Panel>();
    auto* combo    = rootOwner->AddChild<DxUi::ComboBox>();
    combo->SetItems(MakeItems(ItemSet::Mixed, 0x5EED0231u));
    combo->SetBounds(D2D1::RectF(16.0f, 16.0f, 200.0f, 48.0f));
    view.Controls().SetRoot(std::move(rootOwner));
    Hr(view.Prepare(640u, 480u, 96.0f), "popup typography first frame");
    const size_t items = combo->GetItems().size();
    const auto open    = [&]() -> uint64_t
    {
        const DxUi::Detail::WorkCounterScope scope;
        static_cast<void>(combo->OnKeyDown(view.Controls(), VK_F4, 0u));
        Hr(view.Prepare(640u, 480u, 96.0f), "popup typography frame");
        const uint64_t measured = scope.Delta().popups.comboBoxItemWidthMeasurements;
        static_cast<void>(combo->OnKeyDown(view.Controls(), VK_ESCAPE, 0u));
        return measured;
    };
    Check(open() == items, "the first open measures every item");
    Check(open() == 0u, "a second open measures nothing");
    const uint64_t revision = view.Controls().GetTypographyRevision();
    std::shared_ptr<DxUi::GraphicsDevice> replacement;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), replacement), "popup typography replacement graphics");
    Hr(view.ReplaceDevice(replacement), "popup typography device replacement");
    Check(view.Controls().GetTypographyRevision() != revision, "a device replacement moves the typography revision");
    Check(open() == items, "a new DirectWrite factory (typography revision) measures every item again");
    Check(open() == 0u, "and the next open measures nothing");
    view.Detach();
    std::cout << "PASS ComboBox popup widths follow the host typography revision\n";
}

__declspec(noinline) inline void TestComboBoxPopupParity(GraphicsFixture& gpu)
{
    const auto make = [&] { return std::make_unique<EmbeddedParity::EmbeddedParityScene>(gpu.device.get()); };
    IncrementalParity::RunParityScript(ComboPopupScript<EmbeddedParity::EmbeddedParityScene>(), make, "embedded", EmbeddedParity::kReplay);
}

inline void Run(GraphicsFixture& gpu)
{
    TestFx2PopupLayoutGate();
    TestComboBoxPopupMatchesLegacyMeasurement(gpu);
    TestComboBoxPopupWidthsFollowTypography(gpu);
    TestComboBoxPopupParity(gpu);
}
} // namespace ComboBoxPopupLayoutTests
