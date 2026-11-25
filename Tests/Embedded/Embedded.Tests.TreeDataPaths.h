#pragma once
// W2.6 Tree data paths on the embedded host (plan Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md): the gate on FX9
// (each navigation key reads the model's row of the focused item and no other), the gate on FX11 (an expand or collapse copies
// no row record and keeps whole records for the removed rows only), the id index's resident bytes, and the frame hashes of
// the collapse and expand transitions that Measurements/PerformanceAndMotion_2026-11-13/W2.6 compares before and after the
// change (--tree-motion-frames). Included once from Embedded.Tests.Embedded.cpp after Embedded.Tests.PerformanceFixtures.h;
// uses Check and Hr from there.
#include "Embedded.Tests.PerformanceFixtures.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace TreeDataPathTests
{
using PerformanceFixtures::EmbeddedInstance;

[[nodiscard]] inline uint64_t HashRegion(const std::vector<uint8_t>& pixels, UINT width, const D2D1_RECT_F& rect, float scale) noexcept
{
    const auto left   = static_cast<size_t>(rect.left * scale);
    const auto right  = static_cast<size_t>(rect.right * scale);
    const auto top    = static_cast<size_t>(rect.top * scale);
    const auto bottom = static_cast<size_t>(rect.bottom * scale);
    uint64_t hash     = 14695981039346656037ull;
    for (size_t y = top; y < bottom; ++y)
        for (size_t x = left * 4u; x < right * 4u; ++x)
        {
            hash ^= pixels[(y * width * 4u) + x];
            hash *= 1099511628211ull;
        }
    return hash;
}

[[nodiscard]] inline DxUi::Tree* FindTree(DxUi::Control* control) noexcept
{
    if (! control)
        return nullptr;
    if (auto* tree = dynamic_cast<DxUi::Tree*>(control))
        return tree;
    for (size_t index = 0u; index < control->GetLogicalChildCount(); ++index)
        if (DxUi::Tree* found = FindTree(control->GetLogicalChild(index)))
            return found;
    return nullptr;
}

// A fixture instance on a fresh WARP target of its variant's size.
struct LiveFixture
{
    GraphicsFixture gpu;
    EmbeddedInstance instance;

    LiveFixture(const PerformanceFixtures::Definition& definition, const PerformanceFixtures::Variant& variant)
    {
        gpu.width  = variant.width;
        gpu.height = variant.height;
        Hr(gpu.Create(), "tree data path WARP device");
        instance.Instantiate(definition);
        Hr(instance.Create(gpu.device.get(), variant), "tree data path fixture build");
    }
    LiveFixture(const LiveFixture&)            = delete;
    LiveFixture& operator=(const LiveFixture&) = delete;
    LiveFixture(LiveFixture&&)                 = delete;
    LiveFixture& operator=(LiveFixture&&)      = delete;
    ~LiveFixture()                             = default;

    // One step and its frame, as a receipt records it.
    DxUi::Detail::WorkCounters Step(size_t phase, size_t step)
    {
        const DxUi::Detail::WorkCounterScope scope;
        instance.Step(phase, step);
        Hr(instance.Frame(gpu), "tree data path frame");
        return scope.Delta();
    }
};

[[nodiscard]] inline DxUi::RetainedCacheCount IdIndexCache(DxUi::EmbeddedHost& view)
{
    std::vector<DxUi::RetainedCacheCount> counts;
    DxUi::RetainedCacheAccess::Count(view, counts);
    for (const DxUi::RetainedCacheCount& count : counts)
        if (count.name == "Tree._visibleIdIndexCache")
            return count;
    Check(false, "the Tree id index is a registered retained cache");
    return {};
}

// FX9: after its warm-up, the tree-keys phase focuses the Tree and presses 24 navigation keys. Each key reads one row of
// the model, the one that takes the focus, and builds no id index (the model answers FindVisibleItemById itself).
inline void TestFx9TreeKeysReadOnlyTheFocusedRow()
{
    namespace Fx9 = PerformanceFixtures::Fx9;
    LiveFixture live(Fx9::kDefinition, Fx9::kVariants[0]);
    for (size_t step = 0u; step < Fx9::kDefinition.phases[0].steps; ++step)
        static_cast<void>(live.Step(0u, step));
    DxUi::Tree* const tree = FindTree(live.instance.View().Controls().GetRoot());
    Check(tree != nullptr, "FX9 has its Tree");
    static_cast<void>(live.Step(Fx9::kTreeKeysPhase, 0u));
    Check(tree->GetFocusedItemId().has_value(), "FX9 tree-keys focuses a row");
    for (size_t step = 1u; step < Fx9::kDefinition.phases[Fx9::kTreeKeysPhase].steps; ++step)
    {
        const std::optional<uint64_t> before = tree->GetFocusedItemId();
        const auto work                      = live.Step(Fx9::kTreeKeysPhase, step);
        Check(tree->GetFocusedItemId() != before, "every FX9 navigation key moves the focus");
        Check(work.lists.treeItemReads == 1u, "an FX9 navigation key reads the model's row of the newly focused item and no other");
        Check(work.lists.treeIdIndexBuilds == 0u, "a model with its own FindVisibleItemById gets no id index");
    }
    Check(IdIndexCache(live.instance.View()).residentBytes == 0u, "FX9's Tree holds no id index bytes");
}

// The id index's memory: 12 bytes per distinct visible id, nothing for a model with its own lookup (FX9's), and the live
// C++ bytes it adds are those bytes. Prints the measurement for the W2.6 receipt.
inline void TestTreeIdIndexBytesPerVisibleRow()
{
    class FlatModel final : public DxUi::ITreeModel
    {
    public:
        [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
        {
            return 10'000u;
        }
        void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
        {
            item      = {};
            item.id   = 1u + visibleIndex;
            item.text = L"Row " + std::to_wstring(visibleIndex);
        }
    } model;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "id index WARP device");
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "id index graphics");
    Hr(view.Attach(graphics, StoppedTestClock()), "id index host");
    auto root        = std::make_unique<DxUi::Panel>();
    DxUi::Tree* tree = root->AddChild<DxUi::Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 400.0f));
    tree->SetModel(&model);
    view.Controls().SetRoot(std::move(root));
    view.Controls().SetFocusControl(tree, false);
    Hr(view.Prepare(320u, 420u), "id index preparation");
    tree->SetFocusedItemId(5'000u); // learns that the model keeps the default lookup
    liveBytes      = 0;
    countLiveBytes = true;
    static_cast<void>(view.DispatchKey(VK_DOWN, true));
    countLiveBytes                         = false;
    const ptrdiff_t added                  = liveBytes;
    const DxUi::RetainedCacheCount indexed = IdIndexCache(view);
    std::cout << "    [W2.6] id index over 10,000 visible rows: " << indexed.entries << " entries, " << indexed.residentBytes << " resident bytes, " << added
              << " live C++ bytes added by the key that built it\n";
    Check(indexed.entries == 10'000u && indexed.residentBytes == 120'000u, "the id index holds 12 bytes per distinct visible id");
    // Debug iterator checking may add a container proxy per vector; nothing else stays behind.
    Check(added >= 120'000 && added <= 120'000 + 256, "building the index leaves its two arrays behind and nothing else");
    view.Controls().SetRoot({});
}

// FX11: every expand and collapse at every rate copies no row record; a collapse keeps the six removed rows' records and an
// expand none, both under the viewport's rows.
inline void TestFx11TreeTransitionsCopyNoRecords()
{
    namespace Fx11 = PerformanceFixtures::Fx11;
    LiveFixture live(Fx11::kDefinition, Fx11::kVariants[0]);
    DxUi::Tree* const tree = FindTree(live.instance.View().Controls().GetRoot());
    Check(tree != nullptr, "FX11 has its Tree");
    const D2D1_RECT_F bounds   = tree->GetBounds();
    const size_t viewportRows  = static_cast<size_t>((bounds.bottom - bounds.top) / 28.0f);
    size_t transitionsMeasured = 0u;
    for (size_t phase = 0u; phase < Fx11::kDefinition.phases.size(); ++phase)
    {
        const bool expand   = phase > 0u && phase < Fx11::kProgressSlide && (phase - 1u) % Fx11::kTransitions == size_t(Fx11::Transition::TreeExpand);
        const bool collapse = phase > 0u && phase < Fx11::kProgressSlide && (phase - 1u) % Fx11::kTransitions == size_t(Fx11::Transition::TreeCollapse);
        for (size_t step = 0u; step < Fx11::kDefinition.phases[phase].steps; ++step)
        {
            const auto work = live.Step(phase, step);
            if ((expand || collapse) && step == 0u)
            {
                ++transitionsMeasured;
                Check(work.lists.treeExpansionRecordCopies == 0u, "an FX11 expand or collapse copies no row record");
                Check(work.lists.treeExpansionRecords == (collapse ? Fx11::MotionTreeModel::kChildren : 0u),
                      "an FX11 collapse keeps only its removed rows' records and an expand none");
                Check(work.lists.treeExpansionRecords <= viewportRows, "FX11 records stay within the viewport's rows");
            }
        }
    }
    Check(transitionsMeasured == 2u * Fx11::kRates.size(), "FX11 measured every Tree transition");
}

// ---- Collapse and expand frames (W2.6 receipt) ---------------------------------------------------------------------
// A tree of three folders: A with 40 children, B with 5, C with 60, all open, over a model that keeps ITreeModel's default
// lookup. Its delegate publishes every toggle before it returns.
class MotionCaseModel final : public DxUi::ITreeModel, public DxUi::ITreeDelegate
{
public:
    static constexpr std::array<size_t, 3> kChildren{40u, 5u, 60u};

    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        size_t count = 0u;
        for (size_t folder = 0u; folder < kChildren.size(); ++folder)
            count += 1u + (_expanded[folder] ? kChildren[folder] : 0u);
        return count;
    }
    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
    {
        item = {};
        for (size_t folder = 0u; folder < kChildren.size(); ++folder)
        {
            if (visibleIndex == 0u)
            {
                item.id          = FolderId(folder);
                item.text        = L"Folder " + std::to_wstring(folder + 1u);
                item.iconText    = L"F";
                item.hasChildren = true;
                item.expanded    = _expanded[folder];
                return;
            }
            --visibleIndex;
            if (! _expanded[folder])
                continue;
            if (visibleIndex < kChildren[folder])
            {
                item.id       = FolderId(folder) + 1u + visibleIndex;
                item.parentId = FolderId(folder);
                item.text     = L"Entry " + std::to_wstring(folder + 1u) + L"." + std::to_wstring(visibleIndex + 1u);
                item.depth    = 1u;
                if (visibleIndex % 5u == 0u)
                    item.badgeText = std::to_wstring(visibleIndex);
                return;
            }
            visibleIndex -= kChildren[folder];
        }
    }
    void OnTreeToggleExpanded(uint64_t itemId, bool expanded) override
    {
        for (size_t folder = 0u; folder < kChildren.size(); ++folder)
            if (FolderId(folder) == itemId)
                _expanded[folder] = expanded;
        if (_tree)
            _tree->NotifyDataChanged();
    }
    void Bind(DxUi::Tree* tree) noexcept
    {
        _tree = tree;
    }
    [[nodiscard]] static constexpr uint64_t FolderId(size_t folder) noexcept
    {
        return 1000u * (folder + 1u);
    }

private:
    std::array<bool, 3> _expanded{true, true, true};
    DxUi::Tree* _tree = nullptr;
};

// One scripted case: the action at each 60 Hz step of a 400 ms window, frame hashes of the Tree's area.
enum class MotionCase : uint8_t
{
    CollapseScrolled, // collapse A from the top; a wheel notch at step 3 scrolls rows the collapse still draws
    CollapseResized,  // collapse A; the tree grows at step 4, showing removed rows that were below the viewport
    CollapseReopened, // collapse A; expand it again at step 3, before the collapse finishes
    ExpandScrolled,   // collapse A without motion, then expand it while scrolled one notch
    CollapseMidList,  // scroll so A's children straddle the viewport, then collapse A
};
inline constexpr std::array kMotionCases{std::pair{MotionCase::CollapseScrolled, "collapse-scrolled"},
                                         std::pair{MotionCase::CollapseResized, "collapse-resized"},
                                         std::pair{MotionCase::CollapseReopened, "collapse-reopened"},
                                         std::pair{MotionCase::ExpandScrolled, "expand-scrolled"},
                                         std::pair{MotionCase::CollapseMidList, "collapse-mid-list"}};

inline std::vector<uint64_t> RunMotionCase(MotionCase motionCase)
{
    constexpr UINT kWidth = 480u, kHeight = 360u;
    GraphicsFixture gpu;
    gpu.width  = kWidth;
    gpu.height = kHeight;
    Hr(gpu.Create(), "tree motion WARP device");
    MotionCaseModel model;
    std::vector<uint64_t> hashes;
    {
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "tree motion graphics");
        Hr(view.Attach(graphics, StoppedTestClock()), "tree motion host");
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = false;
        view.Controls().SetTheme(theme);
        auto root              = std::make_unique<DxUi::Panel>();
        DxUi::Tree* tree       = root->AddChild<DxUi::Tree>();
        const D2D1_RECT_F area = D2D1::RectF(8.0f, 8.0f, 300.0f, 352.0f);
        tree->SetBounds(D2D1::RectF(area.left, area.top, area.right, 200.0f));
        tree->SetModel(&model);
        tree->SetDelegate(&model);
        model.Bind(tree);
        view.Controls().SetRoot(std::move(root));
        uint64_t now       = PerformanceFixtures::kClockOriginMs;
        const auto advance = [&](uint64_t milliseconds)
        {
            now += milliseconds;
            if (AnimationRequested(view.GetAnimationRequest()))
                static_cast<void>(view.AdvanceAnimation(now));
        };
        std::vector<uint8_t> pixels;
        const auto frame = [&](bool record)
        {
            Hr(view.Prepare(kWidth, kHeight), "tree motion preparation");
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "tree motion composition");
            if (record)
            {
                Hr(gpu.Read(pixels), "tree motion readback");
                hashes.push_back(HashRegion(pixels, kWidth, area, 1.0f));
            }
        };
        const auto wheel = [&]
        {
            const D2D1_RECT_F bounds = tree->GetBounds();
            DxUi::PointerEvent event{DxUi::PointerAction::Wheel, (bounds.left + bounds.right) * 0.5f, (bounds.top + bounds.bottom) * 0.5f};
            event.wheelDelta = -120.0f;
            static_cast<void>(view.DispatchPointer(event));
        };
        for (size_t warm = 0u; warm < 4u; ++warm)
        {
            advance(16u);
            frame(false);
        }
        if (motionCase == MotionCase::ExpandScrolled)
        {
            theme.reducedMotion = true;
            view.Controls().SetTheme(theme);
            static_cast<void>(tree->RequestExpandedState(0u, false));
            frame(false);
            theme.reducedMotion = false;
            view.Controls().SetTheme(theme);
            advance(16u);
            frame(false);
        }
        if (motionCase == MotionCase::CollapseMidList || motionCase == MotionCase::ExpandScrolled)
        {
            wheel();
            advance(16u);
            frame(false);
        }
        const size_t steps = PerformanceFixtures::Fx11::WindowSteps(60u);
        for (size_t step = 0u; step < steps; ++step)
        {
            if (step > 0u)
                advance(PerformanceFixtures::Fx11::FrameTimeMs(60u, step) - PerformanceFixtures::Fx11::FrameTimeMs(60u, step - 1u));
            if (step == 0u)
                static_cast<void>(tree->RequestExpandedState(0u, motionCase == MotionCase::ExpandScrolled));
            if (motionCase == MotionCase::CollapseScrolled && step == 3u)
                wheel();
            if (motionCase == MotionCase::CollapseResized && step == 4u)
                tree->SetBounds(area);
            if (motionCase == MotionCase::CollapseReopened && step == 3u)
                static_cast<void>(tree->RequestExpandedState(0u, true));
            frame(true);
        }
        view.Controls().SetRoot({});
    }
    return hashes;
}

// FX11's own Tree transitions, every step of every tree-expand and tree-collapse phase, hashed over the Tree's bounds.
inline std::vector<std::pair<std::string, std::vector<uint64_t>>> RunFx11TreeFrames()
{
    namespace Fx11 = PerformanceFixtures::Fx11;
    LiveFixture live(Fx11::kDefinition, Fx11::kVariants[0]);
    DxUi::Tree* const tree = FindTree(live.instance.View().Controls().GetRoot());
    Check(tree != nullptr, "FX11 has its Tree");
    const D2D1_RECT_F bounds = tree->GetBounds();
    std::vector<std::pair<std::string, std::vector<uint64_t>>> phases;
    std::vector<uint8_t> pixels;
    for (size_t phase = 0u; phase < Fx11::kDefinition.phases.size(); ++phase)
    {
        const size_t transition = phase > 0u && phase < Fx11::kProgressSlide ? (phase - 1u) % Fx11::kTransitions : Fx11::kTransitions;
        const bool treePhase    = transition == size_t(Fx11::Transition::TreeExpand) || transition == size_t(Fx11::Transition::TreeCollapse);
        if (treePhase)
            phases.emplace_back(std::string(Fx11::kDefinition.phases[phase].name), std::vector<uint64_t>{});
        for (size_t step = 0u; step < Fx11::kDefinition.phases[phase].steps; ++step)
        {
            static_cast<void>(live.Step(phase, step));
            if (treePhase)
            {
                Hr(live.gpu.Read(pixels), "FX11 tree readback");
                phases.back().second.push_back(HashRegion(pixels, live.gpu.width, bounds, 1.0f));
            }
        }
    }
    return phases;
}

// --tree-motion-frames <path>: every hash above as JSON, for the before/after comparison of the W2.6 receipt.
inline int WriteMotionFrames(const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary);
    Check(bool(output), "tree motion frames file");
    // The receipt stamps its inputs' SHA-256 (benchmarkInputs) and baseCommit when it is retained under Measurements.
    output << "{\"workloadOwner\":\"DxUi\",\"fixture\":\"dxui-tree-motion-frames-v1\",\"fixtureId\":\"FX11\",\"phases\":[";
    bool first = true;
    for (const auto& [name, hashes] : RunFx11TreeFrames())
    {
        output << (first ? "" : ",") << "{\"name\":\"" << name << "\",\"hashes\":[";
        for (size_t index = 0u; index < hashes.size(); ++index)
            output << (index == 0u ? "" : ",") << '"' << std::hex << hashes[index] << std::dec << '"';
        output << "]}";
        first = false;
    }
    output << "],\"cases\":[";
    first = true;
    for (const auto& [motionCase, name] : kMotionCases)
    {
        const std::vector<uint64_t> hashes = RunMotionCase(motionCase);
        output << (first ? "" : ",") << "{\"name\":\"" << name << "\",\"hashes\":[";
        for (size_t index = 0u; index < hashes.size(); ++index)
            output << (index == 0u ? "" : ",") << '"' << std::hex << hashes[index] << std::dec << '"';
        output << "]}";
        first = false;
    }
    output << "]}\n";
    output.close();
    Check(bool(output), "tree motion frames written");
    return 0;
}

// The scripted cases run to completion under every configuration (ASan Debug included) and draw the same frames twice.
inline void TestTreeMotionCasesAreDeterministic()
{
    for (const auto& [motionCase, name] : kMotionCases)
    {
        const std::vector<uint64_t> first  = RunMotionCase(motionCase);
        const std::vector<uint64_t> second = RunMotionCase(motionCase);
        Check(first.size() == PerformanceFixtures::Fx11::WindowSteps(60u) && first == second, name);
    }
}

inline void Run()
{
    TestFx9TreeKeysReadOnlyTheFocusedRow();
    TestFx11TreeTransitionsCopyNoRecords();
    TestTreeIdIndexBytesPerVisibleRow();
    TestTreeMotionCasesAreDeterministic();
}
} // namespace TreeDataPathTests
