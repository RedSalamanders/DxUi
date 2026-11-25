#pragma once
// UI Automation build cost (W8.3 of the performance and motion plan), on the FX12 and FX3 definitions with one client (the
// client gate's override, as FX6 has it). The gate:
// - FX12's 10,000-control form: a build walks the tree for no label (the count pass indexes the mnemonic labels), tests no
//   control type with dynamic_cast (each control's accessibility kind) and copies no text control's document (a record
//   borrows the control's shared text), work counters accessibility.labelLookupWalks, dynamicCasts and textCopies at 0. The
//   same build on the record reads before W8.3 (DebugSetAccessibilityLegacyRecordReadsForTest) does all three: the gate fails
//   on the old path. Its value and wheel phases then publish incrementally: a label's text re-reads one record, and a wheel
//   notch over the ScrollPanel moves its scroll scope and re-reads no record.
// - FX3's 100 wheel notches over its Grid: no whole build, and each publication re-reads at most one record, the Grid's, the
//   one scroll scope the wheel moves.
// - A layout that moves every control of FX12's form shape (one ScrollPanel of 2,000 and of 10,000 controls): the successor
//   finds each control it re-reads down its record's path, so it reads at most each such control's depth of children
//   (accessibility.pathChildReads), never more than the controls a whole build reads. The reads before W8.3 searched each
//   parent's children upward for the index: about 10^7 child reads per publication at 10,000 controls, more than a whole build.
// Round 2 of the review (each test fails on the code before its fix unless it says it is coverage):
// - Coverage of the review's probes: nested scroll scopes with a held old generation; a ScrollPanel as the root (right to
//   left, a zero-height viewport, consumer classes derived from library classes); the label index across successors; a scroll
//   while a geometry-only publication is held.
// - A whole build reads below a hidden control only for a name that looks a label up (accessibility.hiddenLabelReads), and
//   finds the name the label walk found.
// - A scrollbar gesture (a page step, a thumb drag, its press and release) of a ScrollPanel, a Grid and a Tree publishes as
//   a scroll, with no whole build.
// - A PopupLayer's content inside a ScrollPanel resolves through its scope's translation, not the viewport's clip.
// - The bytes a generation reports equal the bytes the heap frees when it goes (x64 Release; Debug containers add proxies).
// - Reading a part a record does not have allocates nothing.
// Every incremental publication is verified against a whole build of the same tree.
// Included by Embedded.Tests.Embedded.cpp after the performance fixtures; the Embedded suite runs it, and alone with
// --test-accessibility-build-cost.
#include "Controls/DxUi.Internal.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace AccessibilityBuildCost
{
// One client on a fixture's live embedded view: accessibility attached, its root provider taken, and the build gate's override
// answering that a client listens.
class Client final
{
public:
    Client(DxUi::EmbeddedHost& view, uint64_t runtimeId, UINT width, UINT height)
        : _view(view),
          _placement{{0.0, 0.0, static_cast<double>(width), static_cast<double>(height)}, true}
    {
        DxUi::DebugSetAccessibilitySystemListenersForTest(true);
        _site = std::make_shared<PerformanceFixtures::Fx6::Site>();
        Hr(_view.AttachAccessibility(_site, runtimeId, _placement), "W8.3 attach the fixture view's accessibility");
        Hr(_view.GetAccessibilityProvider(_provider.put()), "W8.3 the fixture view's root provider");
        _site->root = _provider.get();
    }
    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&)                 = delete;
    Client& operator=(Client&&)      = delete;
    ~Client()
    {
        _site->root = nullptr;
        _provider.reset();
        _view.DisconnectAccessibility();
        DxUi::DebugSetAccessibilitySystemListenersForTest(_outer);
    }
    // Publishes what the frames since the last call changed, as an application's update does; a geometry-only publication the
    // 100 ms coalescing deferred (the fixture clock stands still) is published by the settle update that follows. Returns the
    // publications made.
    uint64_t Publish()
    {
        const uint64_t before = DxUi::DebugGetAccessibilityPublicationCountForTest();
        Check(SUCCEEDED(_view.UpdateAccessibility(_placement)), "W8.3 an update publishes");
        if (DxUi::DebugGetAccessibilityPublicationCountForTest() == before)
            Check(SUCCEEDED(_view.UpdateAccessibility(_placement)), "W8.3 the settle update publishes");
        return DxUi::DebugGetAccessibilityPublicationCountForTest() - before;
    }
    [[nodiscard]] IRawElementProviderFragmentRoot* Provider() const noexcept
    {
        return _provider.get();
    }

private:
    DxUi::EmbeddedHost& _view;
    DxUi::EmbeddedAccessibilityPlacement _placement;
    std::optional<bool> _outer = DxUi::DebugGetAccessibilitySystemListenersForTest();
    std::shared_ptr<PerformanceFixtures::Fx6::Site> _site;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> _provider;
};

inline void Print(const char* what, const DxUi::Detail::WorkCounters& work)
{
    const auto& counters = work.accessibility;
    std::cout << "W8.3 " << what << ": whole builds " << counters.snapshotBuilds << ", records " << counters.snapshotNodes << ", incremental "
              << counters.incrementalPublications << ", records re-read " << counters.recordsCloned << ", label walks " << counters.labelLookupWalks
              << ", dynamic_cast " << counters.dynamicCasts << ", text copies " << counters.textCopies << ", scroll scope updates "
              << counters.scrollScopeUpdates << ", rectangle rebuilds " << counters.pointHitRebuilds << ", undeclared invalidations "
              << counters.undeclaredInvalidations << ", mutations " << counters.mutations << ", marks " << counters.recordsMarked << ", deferred "
              << counters.geometryPublicationsDeferred << std::endl;
}

// FX12's 10,000-control form with one client: the build counters, on the current and on the legacy record reads, then the
// value and wheel phases incrementally.
__declspec(noinline) inline void TestFx12FormBuildCost()
{
    using namespace PerformanceFixtures;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "W8.3 FX12 WARP device");
    const Variant* const variant = Fx12::kDefinition.FindVariant("form-10k");
    Check(variant != nullptr, "FX12 declares its 10,000-control form");
    EmbeddedInstance instance;
    instance.Instantiate(Fx12::kDefinition);
    Hr(instance.Create(gpu.device.get(), *variant), "W8.3 FX12 form-10k scene");
    for (size_t step = 0u; step < Fx12::kPhases[Fx12::kWarm].steps; ++step)
    {
        instance.Step(Fx12::kWarm, step);
        Hr(instance.Frame(gpu), "W8.3 FX12 warm frame");
    }
    DxUi::DebugResetAccessibilityIncrementalVerificationForTest();
    Client client(instance.View(), 0x5738330000000001ull, variant->width, variant->height);

    static_cast<void>(client.Publish()); // Settles the attachment.

    // A whole build of the window: every publication whole (the reference an incremental publication is held to), after the
    // value phase's first label text.
    DxUi::Detail::WorkCounters build{};
    DxUi::DebugSetAccessibilityWholePublicationForTest(true);
    {
        instance.Step(Fx12::kValue, 0u);
        Hr(instance.Frame(gpu), "W8.3 FX12 whole-build frame");
        const DxUi::Detail::WorkCounterScope work;
        Check(client.Publish() == 1u, "W8.3 FX12: the whole build publishes");
        build = work.Delta();
    }
    Print("FX12 form-10k whole build", build);
    Check(build.accessibility.snapshotBuilds == 1u && build.accessibility.snapshotNodes == Fx12::kFormRows * Fx12::kFormRowControls,
          "W8.3 FX12: one whole build of the form's 10,000 records");
    Check(build.accessibility.labelLookupWalks == 0u, "W8.3 FX12 gate: 0 label-lookup walks per build");
    Check(build.accessibility.dynamicCasts == 0u, "W8.3 FX12 gate: 0 dynamic_cast per build");
    Check(build.accessibility.textCopies == 0u, "W8.3 FX12 gate: 0 full-text copies per build");

    // The same build on the record reads before W8.3: the gate fails on the old path.
    DxUi::Detail::WorkCounters legacy{};
    DxUi::DebugSetAccessibilityLegacyRecordReadsForTest(true);
    {
        instance.Step(Fx12::kValue, 1u);
        Hr(instance.Frame(gpu), "W8.3 FX12 legacy frame");
        const DxUi::Detail::WorkCounterScope work;
        Check(client.Publish() == 1u, "W8.3 FX12: the legacy build publishes");
        legacy = work.Delta();
    }
    DxUi::DebugSetAccessibilityWholePublicationForTest(false);
    DxUi::DebugSetAccessibilityLegacyRecordReadsForTest(false);
    Print("FX12 form-10k whole build, legacy record reads", legacy);
    Check(legacy.accessibility.snapshotBuilds == 1u, "W8.3 FX12: the legacy publication is a whole build");
    // 2,000 text fields, 2,000 check boxes and 2,000 toggles each looked their label up; each field's value and text were copied.
    Check(legacy.accessibility.labelLookupWalks >= 3u * Fx12::kFormRows, "W8.3 FX12 old path: a label walk per unnamed field, check box and toggle");
    Check(legacy.accessibility.dynamicCasts > Fx12::kFormRows * Fx12::kFormRowControls, "W8.3 FX12 old path: dynamic_cast chains per record");
    Check(legacy.accessibility.textCopies >= 2u * Fx12::kFormRows, "W8.3 FX12 old path: each field's value and text copied");

    // A label's text: one incremental publication re-reading that record, with no walk, cast or copy.
    for (size_t step = 0u; step < Fx12::kPhases[Fx12::kValue].steps; ++step)
    {
        const DxUi::Detail::WorkCounterScope work; // the change, its frame and its publication
        instance.Step(Fx12::kValue, step);
        Hr(instance.Frame(gpu), "W8.3 FX12 value frame");
        Check(client.Publish() == 1u, "W8.3 FX12: a label's text publishes");
        const DxUi::Detail::WorkCounters delta = work.Delta();
        if (step == 0u)
            Print("FX12 form-10k label text", delta);
        Check(delta.accessibility.snapshotBuilds == 0u && delta.accessibility.recordsCloned <= 1u, "W8.3 FX12: a label's text re-reads its record alone");
        Check(delta.accessibility.labelLookupWalks == 0u && delta.accessibility.dynamicCasts == 0u && delta.accessibility.textCopies == 0u,
              "W8.3 FX12: an incremental publication walks, casts and copies nothing");
    }

    // A wheel notch over the form: its ScrollPanel's scroll scope moves, every record and rectangle is shared. The first notch
    // also moves the pointer off the button the warm phase hovered, whose hover repaint declares nothing: that publication is
    // whole by the incremental-publication rules (a hover of a control between publications), so the notches after it are
    // the ones measured.
    uint64_t scrolled = 0u;
    for (size_t step = 0u; step < Fx12::kPhases[Fx12::kWheel].steps; ++step)
    {
        const DxUi::Detail::WorkCounterScope work; // the notch, its frame and its publication
        instance.Step(Fx12::kWheel, step);
        Hr(instance.Frame(gpu), "W8.3 FX12 wheel frame");
        const uint64_t publications            = client.Publish();
        const DxUi::Detail::WorkCounters delta = work.Delta();
        if (step <= 1u)
            Print(step == 0u ? "FX12 form-10k first wheel notch (leaves the hovered button)" : "FX12 form-10k wheel notch", delta);
        if (step == 0u)
        {
            Check(publications == 1u, "W8.3 FX12: the first wheel notch publishes");
            continue;
        }
        scrolled += delta.accessibility.scrollScopeUpdates;
        Check(publications == 1u && delta.accessibility.snapshotBuilds == 0u, "W8.3 FX12: a wheel notch publishes with no whole build");
        Check(delta.accessibility.recordsCloned == 0u && delta.accessibility.pointHitRebuilds == 0u,
              "W8.3 FX12: a wheel notch re-reads no record and rebuilds no rectangle (records touched <= scroll scopes)");
    }
    Check(scrolled == Fx12::kPhases[Fx12::kWheel].steps - 1u, "W8.3 FX12: every later notch moved the form's scroll scope");
    const uint64_t mismatches = DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest();
    std::cout << "W8.3 FX12 incremental publications verified against a whole build: " << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest()
              << ", differences " << mismatches << std::endl;
    if (mismatches != 0u)
        std::wcerr << L"    [W8.3] first difference: " << DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest() << L'\n';
    Check(mismatches == 0u &&
              DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() >= Fx12::kPhases[Fx12::kValue].steps + Fx12::kPhases[Fx12::kWheel].steps - 1u,
          "W8.3 FX12: every incremental publication equals a whole build");
}

// FX3's 100 wheel notches over its Grid with one client: no whole build, at most one record (the Grid's) per publication.
__declspec(noinline) inline void TestFx3WheelBuildCost()
{
    using namespace PerformanceFixtures;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "W8.3 FX3 WARP device");
    const Variant& variant = Fx3::kVariants[0];
    EmbeddedInstance instance;
    instance.Instantiate(Fx3::kDefinition);
    Hr(instance.Create(gpu.device.get(), variant), "W8.3 FX3 scene");
    for (size_t step = 0u; step < Fx3::kPhases[Fx3::kWarm].steps; ++step)
    {
        instance.Step(Fx3::kWarm, step);
        Hr(instance.Frame(gpu), "W8.3 FX3 warm frame");
    }
    DxUi::DebugResetAccessibilityIncrementalVerificationForTest();
    Client client(instance.View(), 0x5738330000000002ull, variant.width, variant.height);
    static_cast<void>(client.Publish()); // Settles the attachment.
    constexpr size_t kScrollScopes = 1u; // the Grid: the one control the wheel scrolls
    uint64_t builds = 0u, publications = 0u, records = 0u, maxRecords = 0u;
    for (size_t step = 0u; step < Fx3::kPhases[Fx3::kWheel].steps; ++step)
    {
        const DxUi::Detail::WorkCounterScope work; // the notch, its frame and its publication
        instance.Step(Fx3::kWheel, step);
        Hr(instance.Frame(gpu), "W8.3 FX3 wheel frame");
        publications += client.Publish();
        const auto delta = work.Delta().accessibility;
        builds += delta.snapshotBuilds;
        records += delta.recordsCloned;
        maxRecords = (std::max)(maxRecords, delta.recordsCloned);
    }
    std::cout << "W8.3 FX3 100 wheel notches, one client: publications " << publications << ", whole builds " << builds << ", records re-read " << records
              << " (at most " << maxRecords << " per publication), verified " << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest()
              << ", differences " << DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest() << std::endl;
    if (DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest() != 0u)
        std::wcerr << L"    [W8.3] first difference: " << DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest() << L'\n';
    Check(publications == Fx3::kPhases[Fx3::kWheel].steps, "W8.3 FX3: every wheel notch publishes");
    Check(builds == 0u, "W8.3 FX3 gate: 100 wheel notches, 0 structural builds");
    Check(maxRecords <= kScrollScopes, "W8.3 FX3 gate: records touched per publication <= scroll scopes");
    Check(DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() >= publications &&
              DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest() == 0u,
          "W8.3 FX3: every incremental publication equals a whole build");
}

// A layout moving every control of FX12's form shape, `rows` rows of a Label, a TextField, a Checkbox, a Toggle and a Button in
// one ScrollPanel (the W8.2 layout-cost scenario): an incremental publication whose child reads are bounded by the depth of
// what it resolves, on the current reads, and above a whole build's on the legacy ones.
__declspec(noinline) inline void TestLayoutSuccessorPathReads(size_t rows)
{
    using namespace DxUi;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "W8.3 layout WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "W8.3 layout graphics device");
    EmbeddedHost view;
    Hr(view.Attach(std::move(graphics), StoppedTestClock()), "W8.3 attach the layout view");
    constexpr UINT kWidth = 1920u, kHeight = 1080u;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(8.0f, 8.0f, 1912.0f, 1072.0f));
    std::vector<Control*> moved;
    for (size_t row = 0u; row < rows; ++row)
    {
        moved.push_back(scroll->AddChild<Label>(std::format(L"Field {:04}", row)));
        moved.push_back(scroll->AddChild<TextField>(std::format(L"Value of field {}", row)));
        moved.push_back(scroll->AddChild<Checkbox>(L"Enabled"));
        moved.push_back(scroll->AddChild<Toggle>(L"Sync"));
        moved.push_back(scroll->AddChild<Button>(L"Apply"));
    }
    static constexpr std::array kLefts{16.0f, 184.0f, 432.0f, 560.0f, 688.0f};
    static constexpr std::array kRights{176.0f, 424.0f, 552.0f, 680.0f, 788.0f};
    const auto layout = [&](float shift)
    {
        for (size_t index = 0u; index < moved.size(); ++index)
        {
            const float top = 12.0f + static_cast<float>(index / 5u) * 32.0f;
            moved[index]->SetBounds(D2D1::RectF(kLefts[index % 5u] + shift, top, kRights[index % 5u] + shift, top + 28.0f));
        }
    };
    layout(0.0f);
    scroll->SetContentHeight(static_cast<float>(rows) * 32.0f + 8.0f);
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(kWidth, kHeight), "W8.3 prepare the layout view");
    DxUi::DebugResetAccessibilityIncrementalVerificationForTest();
    {
        Client client(view, 0x5738330000000003ull, kWidth, kHeight);
        static_cast<void>(client.Publish());       // Settles the attachment.
        const size_t controls = moved.size() + 2u; // and the root and the ScrollPanel, which a whole build reads too
        for (const bool legacyReads : {false, true})
        {
            DxUi::DebugSetAccessibilityLegacyRecordReadsForTest(legacyReads);
            const Detail::WorkCounterScope work;
            layout(legacyReads ? 0.0f : 4.0f);
            Hr(view.Prepare(kWidth, kHeight), "W8.3 prepare a layout frame");
            Check(client.Publish() == 1u, "W8.3 layout: the frame publishes");
            const auto delta = work.Delta().accessibility;
            DxUi::DebugSetAccessibilityLegacyRecordReadsForTest(false);
            std::cout << "W8.3 layout moving every control, " << moved.size() << " controls" << (legacyReads ? ", legacy record reads" : "")
                      << ": whole builds " << delta.snapshotBuilds << ", incremental " << delta.incrementalPublications << ", records re-read "
                      << delta.recordsCloned << ", path child reads " << delta.pathChildReads << " (a whole build reads " << controls << " controls)"
                      << std::endl;
            Check(delta.snapshotBuilds == 0u && delta.incrementalPublications == 1u && delta.recordsCloned >= rows,
                  "W8.3 layout: an incremental publication re-reading each text field");
            // Each re-read record and each mark resolved down its path: depth 2 (the root's ScrollPanel, then its child).
            constexpr uint64_t kDepth = 2u;
            const uint64_t bound      = kDepth * (delta.recordsCloned + Detail::AccessibilityRecordMarks::kCapacity);
            if (! legacyReads)
            {
                Check(delta.pathChildReads <= bound, "W8.3 layout: the successor reads the depth of each control it resolves, not its siblings");
                Check(delta.pathChildReads <= controls, "W8.3 layout: an incremental publication reads no more of the tree than a whole build");
            }
            else
                Check(delta.pathChildReads > controls, "W8.3 layout old path: the upward search reads more of the tree than a whole build");
        }
    }
    Check(DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest() == 0u && DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() >= 2u,
          "W8.3 layout: every incremental publication equals a whole build");
    view.Detach();
}

// ---- Round 2 -----------------------------------------------------------------------------------------------------------

class TinyGridModel final : public DxUi::IGridModel
{
public:
    explicit TinyGridModel(size_t rows = 60u, size_t columns = 1u) noexcept : _rows(rows), _columns(columns)
    {
    }
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rows;
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columns;
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        DxUi::GridColumnDesc desc;
        desc.id       = L"c" + std::to_wstring(column);
        desc.title    = L"Column " + std::to_wstring(column);
        desc.widthDip = 160.0f;
        return desc;
    }
    void GetCellData(size_t rowIndex, size_t column, DxUi::GridCellData& cell) const override
    {
        cell.kind = DxUi::GridCellKind::Text;
        cell.text = L"Row " + std::to_wstring(rowIndex) + L" cell " + std::to_wstring(column);
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId < _rows ? std::optional<size_t>(static_cast<size_t>(rowId)) : std::nullopt;
    }

private:
    size_t _rows, _columns;
};

class FlatTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 80u;
    }
    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
    {
        item.id   = visibleIndex + 1u;
        item.text = L"Item " + std::to_wstring(visibleIndex);
    }
};

// Controls derived from library controls delete their copy and move members, as their bases do.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// A Panel that counts the children a walk reads from it.
class CountingPanel final : public DxUi::Panel
{
public:
    static inline size_t reads = 0u;
    [[nodiscard]] const DxUi::Control* GetLogicalChild(size_t index) const noexcept override
    {
        ++reads;
        return DxUi::Panel::GetLogicalChild(index);
    }
    [[nodiscard]] DxUi::Control* GetLogicalChild(size_t index) noexcept override
    {
        ++reads;
        return DxUi::Panel::GetLogicalChild(index);
    }
};

// Consumer classes derived from library classes.
class DerivedScrollPanel final : public DxUi::ScrollPanel
{
};
class DerivedToggle final : public DxUi::Toggle
{
public:
    DerivedToggle() : DxUi::Toggle(L"")
    {
    }
};

#pragma warning(pop)

// An embedded view of 800 x 600 at 96 DPI (a DIP is a pixel) on WARP.
struct View final
{
    GraphicsFixture gpu;
    DxUi::EmbeddedHost view;
    static constexpr UINT kWidth = 800u, kHeight = 600u;
    View()
    {
        Hr(gpu.Create(), "W8.3 round 2 WARP device");
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "W8.3 round 2 graphics device");
        Hr(view.Attach(std::move(graphics), StoppedTestClock()), "W8.3 round 2 attach");
    }
    ~View()
    {
        view.Detach();
    }
    View(const View&)            = delete;
    View& operator=(const View&) = delete;
    View(View&&)                 = delete;
    View& operator=(View&&)      = delete;
    void Prepare()
    {
        Hr(view.Prepare(kWidth, kHeight), "W8.3 round 2 prepare");
    }
    void Pointer(DxUi::PointerAction action, D2D1_POINT_2F point, float wheelDelta = 0.0f)
    {
        DxUi::PointerEvent event{};
        event.action     = action;
        event.xPixels    = point.x;
        event.yPixels    = point.y;
        event.wheelDelta = wheelDelta;
        static_cast<void>(view.DispatchPointer(event));
    }
};

// Incremental publications verified since it was made, and the differences from a whole build among them.
struct Verified final
{
    uint64_t verified = DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest();
    uint64_t mismatch = DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest();
    void Require(const char* what, uint64_t atLeast) const
    {
        const uint64_t count      = DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() - verified;
        const uint64_t mismatches = DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest() - mismatch;
        std::cout << "W8.3 " << what << ": verified incremental publications " << count << ", differences " << mismatches << std::endl;
        if (mismatches != 0u)
            std::wcerr << L"    [W8.3] first difference: " << DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest() << L'\n';
        Check(mismatches == 0u && count >= atLeast, what);
    }
};

[[nodiscard]] inline D2D1_POINT_2F Center(const D2D1_RECT_F& rect) noexcept
{
    return D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
}

// P1 (coverage): nested ScrollPanels, the inner one scrolled into and out of the outer's view, a Grid and a Toggle named by a
// Label inside; offsets set by the program and by the wheel; a scroll and a label's text in one frame. A generation a client
// holds keeps resolving its rectangles against its own scope table.
__declspec(noinline) inline void TestNestedScopesWithHeldGeneration()
{
    View v;
    TinyGridModel model;
    auto root   = std::make_unique<DxUi::Panel>();
    auto* outer = root->AddChild<DxUi::ScrollPanel>();
    outer->SetBounds(D2D1::RectF(10.0f, 10.0f, 410.0f, 310.0f));
    outer->AddChild<DxUi::Button>(L"Top")->SetBounds(D2D1::RectF(10.0f, 10.0f, 110.0f, 40.0f));
    auto* inner = outer->AddChild<DxUi::ScrollPanel>();
    inner->SetBounds(D2D1::RectF(10.0f, 500.0f, 390.0f, 800.0f));
    auto* label = inner->AddChild<DxUi::Label>(L"Sync");
    label->SetBounds(D2D1::RectF(10.0f, 10.0f, 100.0f, 30.0f));
    auto* toggle = inner->AddChild<DxUi::Toggle>(L"");
    toggle->SetBounds(D2D1::RectF(110.0f, 10.0f, 200.0f, 30.0f));
    label->SetMnemonicTarget(toggle);
    auto* grid = inner->AddChild<DxUi::Grid>();
    grid->SetModel(&model);
    grid->SetBounds(D2D1::RectF(10.0f, 40.0f, 370.0f, 240.0f));
    inner->SetContentHeight(1200.0f);
    outer->SetContentHeight(2000.0f);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000101ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    const auto held = DxUi::DebugHoldAccessibilitySnapshotForTest(v.view.Controls());
    std::vector<std::wstring> heldBefore, heldAfter;
    DxUi::DebugDescribeAccessibilitySnapshotForTest(held, heldBefore);
    uint64_t builds = 0u;
    const auto step = [&](auto&& change)
    {
        const DxUi::Detail::WorkCounterScope work;
        change();
        v.Prepare();
        Check(client.Publish() == 1u, "W8.3 P1: each step publishes");
        builds += work.Delta().accessibility.snapshotBuilds;
    };
    step([&] { outer->SetScrollOffset(400.0f); });
    step([&] { inner->SetScrollOffset(100.0f); });
    step([&] { outer->SetScrollOffset(0.0f); });
    step([&]
    {
        outer->SetScrollOffset(450.0f);
        label->SetText(L"Synchronise");
    });
    step([&] { v.Pointer(DxUi::PointerAction::Wheel, D2D1::Point2F(150.0f, 150.0f), -120.0f); });
    step([&] { v.Pointer(DxUi::PointerAction::Wheel, D2D1::Point2F(395.0f, 20.0f), -120.0f); });
    DxUi::DebugDescribeAccessibilitySnapshotForTest(held, heldAfter);
    Check(heldBefore == heldAfter, "W8.3 P1: a generation a client holds resolves its rectangles against its own scope table");
    std::cout << "W8.3 P1 nested scopes: whole builds " << builds << std::endl;
    Check(builds == 0u, "W8.3 P1: scrolls of nested scopes publish with no whole build");
    verified.Require("P1 nested scopes, every incremental publication equals a whole build", 6u);
}

// P2 (coverage): a ScrollPanel that is the root, right to left, with a zero-height ScrollPanel inside, and consumer classes
// derived from ScrollPanel and Toggle, which carry their library class's kind.
__declspec(noinline) inline void TestRootScrollPanelRtlZeroAndDerived()
{
    {
        const DerivedScrollPanel panel;
        const DerivedToggle derivedToggle;
        Check(DxUi::ControlModelQueryAccess::AccessibilityKind(panel) == DxUi::Detail::AccessibilityControlKind::ScrollPanel,
              "W8.3 P2: a consumer ScrollPanel carries the ScrollPanel kind");
        Check(DxUi::ControlModelQueryAccess::AccessibilityKind(derivedToggle) == DxUi::Detail::AccessibilityControlKind::Toggle,
              "W8.3 P2: a consumer Toggle carries the Toggle kind");
    }
    View v;
    auto root = std::make_unique<DxUi::ScrollPanel>();
    root->SetFlowDirection(DxUi::FlowDirection::RightToLeft);
    auto* rootPanel = root.get();
    auto* toggle    = root->AddChild<DxUi::Toggle>(L"");
    toggle->SetBounds(D2D1::RectF(20.0f, 20.0f, 200.0f, 50.0f));
    auto* label = root->AddChild<DxUi::Label>(L"Derived toggle");
    label->SetBounds(D2D1::RectF(220.0f, 20.0f, 400.0f, 50.0f));
    label->SetMnemonicTarget(toggle);
    auto* zero = root->AddChild<DxUi::ScrollPanel>();
    zero->SetBounds(D2D1::RectF(20.0f, 100.0f, 400.0f, 100.0f));
    zero->AddChild<DxUi::Button>(L"In a zero viewport")->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 30.0f));
    zero->SetContentHeight(300.0f);
    root->AddChild<DxUi::Button>(L"Far below")->SetBounds(D2D1::RectF(20.0f, 1500.0f, 200.0f, 1530.0f));
    root->SetContentHeight(3000.0f);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000102ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    uint64_t builds = 0u;
    for (const float offset : {700.0f, 1200.0f, 0.0f})
    {
        const DxUi::Detail::WorkCounterScope work;
        rootPanel->SetScrollOffset(offset);
        zero->SetScrollOffset(offset / 10.0f);
        v.Prepare();
        static_cast<void>(client.Publish());
        builds += work.Delta().accessibility.snapshotBuilds;
    }
    Check(builds == 0u, "W8.3 P2: a root ScrollPanel's scrolls publish with no whole build");
    verified.Require("P2 root ScrollPanel, right to left, zero viewport, every incremental publication equals a whole build", 3u);
}

// P3 (coverage): the label index across successors: two labels of one target, the first empty and then named, a hidden label,
// a label's text changed while its target scrolls.
__declspec(noinline) inline void TestLabelIndexAcrossSuccessors()
{
    View v;
    auto root    = std::make_unique<DxUi::Panel>();
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 600.0f, 400.0f));
    auto* slider = scroll->AddChild<DxUi::Slider>();
    slider->SetBounds(D2D1::RectF(10.0f, 10.0f, 300.0f, 40.0f));
    auto* first = scroll->AddChild<DxUi::Label>(L"");
    first->SetBounds(D2D1::RectF(10.0f, 50.0f, 100.0f, 70.0f));
    first->SetMnemonicTarget(slider);
    auto* second = scroll->AddChild<DxUi::Label>(L"Second");
    second->SetBounds(D2D1::RectF(10.0f, 80.0f, 100.0f, 100.0f));
    second->SetMnemonicTarget(slider);
    auto* hidden = scroll->AddChild<DxUi::Label>(L"Hidden");
    hidden->SetVisible(false);
    auto* field = scroll->AddChild<DxUi::TextField>(L"Value");
    field->SetBounds(D2D1::RectF(10.0f, 120.0f, 300.0f, 150.0f));
    hidden->SetMnemonicTarget(field);
    scroll->SetContentHeight(1000.0f);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000103ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    const auto step = [&](auto&& change)
    {
        change();
        v.Prepare();
        static_cast<void>(client.Publish());
    };
    step([&] { first->SetText(L"First"); });
    step([&] { first->SetText(L""); });
    step([&] { hidden->SetText(L"Hidden renamed"); });
    step([&]
    {
        second->SetText(L"Second renamed");
        scroll->SetScrollOffset(200.0f);
    });
    step([&] { field->SetText(L"Edited value"); });
    verified.Require("P3 label index across successors, every incremental publication equals a whole build", 1u);
}

// P4 (coverage): a geometry-only publication held by the 100 ms coalescing (the clock stands still), then a scroll before the
// next update: the scroll publishes at once, the held geometry with it.
__declspec(noinline) inline void TestScrollWhileGeometryHeld()
{
    View v;
    auto root    = std::make_unique<DxUi::Panel>();
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 600.0f, 400.0f));
    auto* field = scroll->AddChild<DxUi::TextField>(L"Moves");
    field->SetBounds(D2D1::RectF(10.0f, 10.0f, 300.0f, 40.0f));
    scroll->AddChild<DxUi::Button>(L"Below")->SetBounds(D2D1::RectF(10.0f, 600.0f, 300.0f, 640.0f));
    scroll->SetContentHeight(1000.0f);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000104ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    const DxUi::EmbeddedAccessibilityPlacement placement{{0.0, 0.0, double(View::kWidth), double(View::kHeight)}, true};
    const auto update = [&]
    {
        const uint64_t before = DxUi::DebugGetAccessibilityPublicationCountForTest();
        Check(SUCCEEDED(v.view.UpdateAccessibility(placement)), "W8.3 P4: an update succeeds");
        return DxUi::DebugGetAccessibilityPublicationCountForTest() - before;
    };
    field->SetBounds(D2D1::RectF(12.0f, 10.0f, 302.0f, 40.0f));
    v.Prepare();
    Check(update() == 1u, "W8.3 P4: the first move publishes");
    field->SetBounds(D2D1::RectF(14.0f, 10.0f, 304.0f, 40.0f));
    v.Prepare();
    Check(update() == 0u, "W8.3 P4: a second move within 100 ms is held");
    scroll->SetScrollOffset(300.0f);
    v.Prepare();
    Check(update() == 1u, "W8.3 P4: a scroll publishes at once, the held geometry with it");
    verified.Require("P4 a scroll while a geometry publication is held, every incremental publication equals a whole build", 2u);
}

// P5 and finding 1: a hidden page of 2,000 controls. With every control named, a whole build reads nothing below the hidden
// control (the count pass defers it, and no name looks a label up); with an unnamed field whose label is on the hidden page, the
// build reads the page once, for that name, and finds the name the label walk before W8.3 found. A successor whose name lookup
// meets an index without the hidden labels publishes whole, and finds it too.
__declspec(noinline) inline void TestHiddenSubtreeReadOnlyForALabel()
{
    View v;
    auto root   = std::make_unique<DxUi::Panel>();
    auto* shown = root->AddChild<DxUi::Button>(L"Shown");
    shown->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 30.0f));
    auto* field = root->AddChild<DxUi::TextField>(L"Value");
    field->SetBounds(D2D1::RectF(0.0f, 40.0f, 200.0f, 70.0f));
    field->SetAccessibleName(L"Named field");
    auto* hidden = root->AddChild<CountingPanel>();
    hidden->SetVisible(false);
    for (size_t index = 0u; index < 2'000u; ++index)
        hidden->AddChild<DxUi::Button>(std::format(L"Hidden {}", index));
    hidden->AddChild<DxUi::Label>(L"Field label on the hidden page")->SetMnemonicTarget(field);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000105ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    size_t turn           = 0u;
    const auto wholeBuild = [&](const char* what)
    {
        DxUi::DebugSetAccessibilityWholePublicationForTest(true);
        CountingPanel::reads = 0u;
        shown->SetText(std::format(L"Shown {}", ++turn));
        v.Prepare();
        const DxUi::Detail::WorkCounterScope work;
        Check(client.Publish() == 1u, "W8.3 hidden page: the build publishes");
        const auto delta = work.Delta().accessibility;
        DxUi::DebugSetAccessibilityWholePublicationForTest(false);
        std::cout << "W8.3 hidden page of 2,000 controls, " << what << ": whole builds " << delta.snapshotBuilds << ", records " << delta.snapshotNodes
                  << ", children read from the hidden page " << CountingPanel::reads << ", hidden label reads " << delta.hiddenLabelReads << ", label walks "
                  << delta.labelLookupWalks << std::endl;
        return std::pair{delta, CountingPanel::reads};
    };
    // The published record of the field, as a client reads it (each whole build here renames the Shown button).
    const auto describe = [&]
    {
        std::vector<std::wstring> lines;
        DxUi::DebugDescribeAccessibilitySnapshotForTest(DxUi::DebugHoldAccessibilitySnapshotForTest(v.view.Controls()), lines);
        const auto line = std::ranges::find_if(lines, [](const std::wstring& text) { return text.find(L"Value") != std::wstring::npos; });
        return line == lines.end() ? std::wstring{} : *line;
    };
    const auto [named, namedReads] = wholeBuild("every record named");
    Check(named.snapshotBuilds == 1u && namedReads == 0u && named.hiddenLabelReads == 0u,
          "W8.3 hidden page: a whole build whose names need no label reads nothing below a hidden control");
    field->SetAccessibleName(L"");
    const auto [unnamed, unnamedReads] = wholeBuild("an unnamed field labelled on the page");
    Check(unnamedReads >= 2'000u && unnamed.hiddenLabelReads >= 2'000u && unnamed.hiddenLabelReads <= 2'002u,
          "W8.3 hidden page: a name that looks a label up reads the hidden page once");
    const std::wstring lines = describe();
    std::wcout << L"W8.3 hidden page: the field's record: " << lines << std::endl;
    Check(lines.find(L"Field label on the hidden page") != std::wstring::npos, "W8.3 hidden page: the field is named by its label on the hidden page");
    // The same names on the record reads before W8.3, which walk the tree for them.
    DxUi::DebugSetAccessibilityLegacyRecordReadsForTest(true);
    const auto [legacy, legacyReads] = wholeBuild("legacy record reads");
    DxUi::DebugSetAccessibilityLegacyRecordReadsForTest(false);
    Check(legacy.labelLookupWalks >= 1u && legacyReads >= 2'000u && describe() == lines, "W8.3 hidden page: the label walk finds the same names");
    // A successor: the field named and published (an index without the hidden labels), then unnamed again.
    field->SetAccessibleName(L"Named again");
    static_cast<void>(wholeBuild("named again"));
    field->SetAccessibleName(L"");
    v.Prepare();
    const DxUi::Detail::WorkCounterScope work;
    Check(client.Publish() == 1u, "W8.3 hidden page: the name change publishes");
    const auto delta = work.Delta().accessibility;
    std::cout << "W8.3 hidden page, a successor's name looking a label up: whole builds " << delta.snapshotBuilds << ", hidden label reads "
              << delta.hiddenLabelReads << std::endl;
    Check(delta.snapshotBuilds == 1u && delta.hiddenLabelReads >= 2'000u && describe() == lines,
          "W8.3 hidden page: a successor whose name looks a label up in an index without the hidden labels publishes whole");
    verified.Require("hidden page, every incremental publication equals a whole build", 0u);
}

// Finding 2: scrollbar gestures. A ScrollPanel's page step and thumb drag, a Grid's vertical page step, vertical thumb drag
// and horizontal page step, and a Tree's page step and thumb drag, each with its press and release, publish as a scroll of
// that control: no whole build, and each publication equals a whole build. The pointer is placed and the control focused
// first, outside what is counted (a hover or a focus change is no scroll).
__declspec(noinline) inline void TestScrollbarGesturesPublishAsScrolls()
{
    using enum DxUi::PointerAction;
    View v;
    TinyGridModel gridModel(200u, 8u);
    FlatTreeModel treeModel;
    auto root    = std::make_unique<DxUi::Panel>();
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(10.0f, 10.0f, 250.0f, 290.0f));
    for (size_t index = 0u; index < 40u; ++index)
        scroll->AddChild<DxUi::Button>(std::format(L"Row {}", index))
            ->SetBounds(D2D1::RectF(8.0f, 8.0f + 40.0f * float(index), 180.0f, 40.0f + 40.0f * float(index)));
    scroll->SetContentHeight(1700.0f);
    auto* grid = root->AddChild<DxUi::Grid>();
    grid->SetModel(&gridModel);
    grid->SetBounds(D2D1::RectF(270.0f, 10.0f, 570.0f, 290.0f));
    auto* tree = root->AddChild<DxUi::Tree>();
    tree->SetModel(&treeModel);
    tree->SetBounds(D2D1::RectF(590.0f, 10.0f, 790.0f, 290.0f));
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000106ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    const DxUi::ThemePalette theme = v.view.Controls().GetTheme();
    uint64_t gestures = 0u, publications = 0u;
    using Events       = std::vector<std::pair<DxUi::PointerAction, D2D1_POINT_2F>>;
    const auto gesture = [&](const char* what, DxUi::Control& control, const Events& events, const auto& offset)
    {
        // Placed and focused first, outside the count.
        v.Pointer(Move, events.front().second);
        v.view.Controls().SetFocusControl(&control, false);
        v.Prepare();
        static_cast<void>(client.Publish());
        const float before = offset();
        uint64_t builds    = 0u;
        std::string steps;
        for (const auto& [action, point] : events)
        {
            const DxUi::Detail::WorkCounterScope work;
            v.Pointer(action, point);
            v.Prepare();
            publications += client.Publish();
            const auto delta = work.Delta().accessibility;
            builds += delta.snapshotBuilds;
            steps += std::format(" [{}: whole {}, undeclared {}]",
                                 action == Down ? "down"
                                 : action == Up ? "up"
                                                : "move",
                                 delta.snapshotBuilds,
                                 delta.undeclaredInvalidations);
        }
        ++gestures;
        std::cout << "W8.3 scrollbar gesture, " << what << ": offset " << before << " -> " << offset() << ", whole builds " << builds << steps << std::endl;
        Check(offset() != before, "W8.3 scrollbar gesture: the gesture scrolls");
        Check(builds == 0u, "W8.3 scrollbar gesture: a scrollbar gesture publishes as a scroll, with no whole build");
    };
    const auto below = [](const D2D1_RECT_F& thumb) { return D2D1::Point2F(Center(thumb).x, thumb.bottom + 30.0f); };
    const auto drag  = [](const D2D1_RECT_F& thumb, float dx, float dy)
    {
        return Events{{Down, Center(thumb)},
                      {Move, D2D1::Point2F(Center(thumb).x + dx, Center(thumb).y + dy)},
                      {Up, D2D1::Point2F(Center(thumb).x + dx, Center(thumb).y + dy)}};
    };
    D2D1_RECT_F thumb{};
    Check(scroll->DebugGetScrollbarThumbHitRect(thumb), "W8.3 the ScrollPanel shows its scrollbar");
    const auto scrollOffset = [&] { return scroll->GetScrollOffset(); };
    gesture("ScrollPanel page step", *scroll, Events{{Down, below(thumb)}, {Up, below(thumb)}}, scrollOffset);
    Check(scroll->DebugGetScrollbarThumbHitRect(thumb), "W8.3 the ScrollPanel still shows its scrollbar");
    gesture("ScrollPanel thumb drag", *scroll, drag(thumb, 0.0f, 40.0f), scrollOffset);
    const auto gridState      = [&] { return grid->DebugGetScrollbarVisualState(theme); };
    const auto gridVertical   = [&] { return gridState().verticalThumbRect.top; };
    const auto gridHorizontal = [&] { return gridState().horizontalThumbRect.left; };
    Check(gridState().hasVerticalScrollbar && gridState().hasHorizontalScrollbar, "W8.3 the Grid shows both scrollbars");
    gesture("Grid vertical page step", *grid, Events{{Down, below(gridState().verticalThumbRect)}, {Up, below(gridState().verticalThumbRect)}}, gridVertical);
    gesture("Grid vertical thumb drag", *grid, drag(gridState().verticalThumbRect, 0.0f, 30.0f), gridVertical);
    const D2D1_RECT_F horizontal = gridState().horizontalThumbRect;
    const D2D1_POINT_2F right    = D2D1::Point2F(horizontal.right + 30.0f, Center(horizontal).y);
    gesture("Grid horizontal page step", *grid, Events{{Down, right}, {Up, right}}, gridHorizontal);
    const auto treeState    = [&] { return tree->DebugGetScrollbarVisualState(theme); };
    const auto treeVertical = [&] { return treeState().verticalThumbRect.top; };
    Check(treeState().hasVerticalScrollbar, "W8.3 the Tree shows its scrollbar");
    gesture("Tree page step", *tree, Events{{Down, below(treeState().verticalThumbRect)}, {Up, below(treeState().verticalThumbRect)}}, treeVertical);
    gesture("Tree thumb drag", *tree, drag(treeState().verticalThumbRect, 0.0f, 30.0f), treeVertical);
    Check(gestures == 7u && publications >= 7u, "W8.3 scrollbar gestures: every gesture published");
    verified.Require("scrollbar gestures, every incremental publication equals a whole build", 7u);
}

// Finding 4: a PopupLayer inside a ScrollPanel whose content reaches past the panel's viewport. The overlay pass draws it with
// the panel's offset and through no viewport, so its items past the viewport's edge keep their whole rectangles, are found at
// a point there and are not offscreen, on the whole build and on the successor a scroll publishes.
__declspec(noinline) inline void TestOverlayContentInsideAScrollPanel()
{
    View v;
    auto root    = std::make_unique<DxUi::Panel>();
    auto* scroll = root->AddChild<DxUi::ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(10.0f, 10.0f, 410.0f, 210.0f));
    scroll->AddChild<DxUi::Button>(L"In the content")->SetBounds(D2D1::RectF(10.0f, 10.0f, 150.0f, 40.0f));
    auto* layer = scroll->AddChild<DxUi::PopupLayer>();
    layer->SetBounds(D2D1::RectF(0.0f, 0.0f, 400.0f, 1000.0f));
    layer->AddChild<DxUi::Button>(L"Popup inside")->SetBounds(D2D1::RectF(200.0f, 120.0f, 380.0f, 150.0f));
    layer->AddChild<DxUi::Button>(L"Popup past the edge")->SetBounds(D2D1::RectF(200.0f, 260.0f, 380.0f, 290.0f));
    scroll->SetContentHeight(1000.0f);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    const Verified verified;
    Client client(v.view, 0x5738330000000107ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    const auto read = [&](const wchar_t* what, float offset)
    {
        // The item's rectangle in the view: its bounds (a ScrollPanel's content is in the host's coordinates) moved by the offset,
        // not clipped by the viewport, which ends at 210.
        const D2D1_RECT_F expected = D2D1::RectF(200.0f, 260.0f - offset, 380.0f, 290.0f - offset);
        wil::com_ptr_nothrow<IRawElementProviderFragment> hit;
        Hr(client.Provider()->ElementProviderFromPoint(Center(expected).x, Center(expected).y, hit.put()), "W8.3 overlay: element from point");
        Check(hit != nullptr, "W8.3 overlay: an item past the viewport's edge is found at a point there");
        UiaRect bounds{};
        Hr(hit->get_BoundingRectangle(&bounds), "W8.3 overlay: bounding rectangle");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(hit->QueryInterface(IID_PPV_ARGS(simple.put())), "W8.3 overlay: simple provider");
        wil::unique_variant name, offscreen;
        Hr(simple->GetPropertyValue(UIA_NamePropertyId, name.addressof()), "W8.3 overlay: name");
        Hr(simple->GetPropertyValue(UIA_IsOffscreenPropertyId, offscreen.addressof()), "W8.3 overlay: offscreen");
        std::wcout << L"W8.3 overlay content past the viewport, " << what << L": found " << (name.vt == VT_BSTR ? name.bstrVal : L"(none)") << L" at "
                   << bounds.left << L"," << bounds.top << L" " << bounds.width << L"x" << bounds.height << L", offscreen "
                   << (offscreen.vt == VT_BOOL && offscreen.boolVal ? 1 : 0) << std::endl;
        Check(name.vt == VT_BSTR && std::wstring_view(name.bstrVal) == L"Popup past the edge", "W8.3 overlay: the point finds the item past the edge");
        Check(std::abs(bounds.left - expected.left) < 0.5 && std::abs(bounds.top - expected.top) < 0.5 && std::abs(bounds.width - 180.0) < 0.5 &&
                  std::abs(bounds.height - 30.0) < 0.5,
              "W8.3 overlay: the item past the viewport's edge keeps its whole rectangle");
        Check(offscreen.vt == VT_BOOL && ! offscreen.boolVal, "W8.3 overlay: the item past the viewport's edge is not offscreen");
    };
    read(L"whole build", 0.0f);
    {
        const DxUi::Detail::WorkCounterScope work;
        scroll->SetScrollOffset(40.0f);
        v.Prepare();
        Check(client.Publish() == 1u, "W8.3 overlay: the scroll publishes");
        Check(work.Delta().accessibility.snapshotBuilds == 0u, "W8.3 overlay: the scroll publishes incrementally");
    }
    read(L"after a scroll", 40.0f);
    verified.Require("overlay content, every incremental publication equals a whole build", 1u);
}

// Finding 5: the bytes AccessibilitySnapshotHeapBytes reports for a generation (what RetainedAccessibilitySnapshotBytes and
// accessibility.snapshotBytes count) against the heap bytes released when that generation goes: a whole build never
// published, held alone, then released, while the test allocator counts the blocks it frees.
__declspec(noinline) inline void TestSnapshotBytesMatchTheHeap()
{
    View v;
    TinyGridModel gridModel(40u, 3u);
    FlatTreeModel treeModel;
    auto root  = std::make_unique<DxUi::Panel>();
    auto* form = root->AddChild<DxUi::ScrollPanel>();
    form->SetBounds(D2D1::RectF(0.0f, 0.0f, 790.0f, 590.0f));
    for (size_t row = 0u; row < 30u; ++row)
    {
        const float top = 8.0f + 36.0f * float(row);
        auto* label     = form->AddChild<DxUi::Label>(std::format(L"Field number {}", row));
        label->SetBounds(D2D1::RectF(8.0f, top, 160.0f, top + 28.0f));
        auto* field = form->AddChild<DxUi::TextField>(std::format(L"A value long enough to own its buffer, {}", row));
        field->SetBounds(D2D1::RectF(170.0f, top, 420.0f, top + 28.0f));
        label->SetMnemonicTarget(field);
        form->AddChild<DxUi::Checkbox>(L"Enabled")->SetBounds(D2D1::RectF(430.0f, top, 540.0f, top + 28.0f));
        form->AddChild<DxUi::Slider>()->SetBounds(D2D1::RectF(550.0f, top, 780.0f, top + 28.0f));
    }
    auto* grid = form->AddChild<DxUi::Grid>();
    grid->SetModel(&gridModel);
    grid->SetBounds(D2D1::RectF(8.0f, 1100.0f, 400.0f, 1400.0f));
    auto* tree = form->AddChild<DxUi::Tree>();
    tree->SetModel(&treeModel);
    tree->SetBounds(D2D1::RectF(420.0f, 1100.0f, 780.0f, 1400.0f));
    form->SetContentHeight(1500.0f);
    v.view.Controls().SetRoot(std::move(root));
    v.Prepare();
    Client client(v.view, 0x5738330000000108ull, View::kWidth, View::kHeight);
    static_cast<void>(client.Publish());
    countLiveBytes        = true;
    liveBytes             = 0;
    uint64_t reported     = 0u;
    auto held             = DxUi::DebugBuildUnpublishedAccessibilitySnapshotForTest(v.view.Controls(), reported);
    const ptrdiff_t built = liveBytes;
    Check(held != nullptr, "W8.3 accounting: the unpublished build succeeds");
    held.reset();
    const ptrdiff_t freed = built - liveBytes;
    countLiveBytes        = false;
    std::cout << "W8.3 generation bytes: reported " << reported << ", freed by the heap when released " << freed << std::endl;
#if _ITERATOR_DEBUG_LEVEL == 0
    Check(static_cast<ptrdiff_t>(reported) == freed, "W8.3 accounting: a generation reports exactly the bytes the heap frees when it goes");
#else
    // Each Debug container also allocates a proxy (16 B), which the accounting leaves out: the heap frees more, never less, and
    // the proxies are about a tenth of this generation (12,832 of 125,763 B measured; the accounting before round 2 left 34% out).
    Check(static_cast<ptrdiff_t>(reported) <= freed && freed - static_cast<ptrdiff_t>(reported) <= freed / 5,
          "W8.3 accounting: a generation reports what the heap frees, less the Debug containers' proxies");
#endif
}

// Finding 6: reading a part a record does not have, as a UI Automation read does, allocates nothing. The first read of the
// process matters (before the fix the empty part was made at its first read): the gate run alone reads here first.
__declspec(noinline) inline void TestEmptyPartReadAllocatesNothing()
{
    const auto record      = DxUi::DebugMakeEmptyAccessibilityRecordForTest();
    const size_t before    = allocations;
    const bool wasCounting = countAllocations;
    countAllocations       = true;
    const size_t items     = DxUi::DebugReadAccessibilityRecordPartsForTest(record);
    countAllocations       = wasCounting;
    const size_t made      = allocations - before;
    std::cout << "W8.3 reading the five parts of a record without parts: items " << items << ", allocations " << made << std::endl;
    Check(items == 0u && made == 0u, "W8.3: reading a part a record does not have allocates nothing");
}

inline void Run()
{
    TestEmptyPartReadAllocatesNothing(); // First: before anything in the process reads a record's part.
    // Every incremental publication is compared with a whole build of the same tree, in every configuration.
    // The record (W8.3): its hot core, and the parts a control with text, a range, a grid, a tree or tabs adds.
    const DxUi::AccessibilityRecordBytesForTest bytes = DxUi::DebugGetAccessibilityRecordBytesForTest();
    std::cout << "W8.3 record bytes: core " << bytes.core << ", text part " << bytes.text << ", range part " << bytes.range << ", grid part " << bytes.grid
              << ", tree part " << bytes.tree << ", tab part " << bytes.tabs << std::endl;
    Check(bytes.core <= 512u, "W8.3: a record without parts is at most 512 B (the W9.7 line)");
    const bool wasVerifying = DxUi::DebugGetAccessibilityIncrementalVerificationForTest();
    DxUi::DebugSetAccessibilityIncrementalVerificationForTest(true);
    TestFx3WheelBuildCost();
    TestFx12FormBuildCost();
    TestLayoutSuccessorPathReads(400u);   // 2,000 controls
    TestLayoutSuccessorPathReads(2'000u); // 10,000 controls
    TestNestedScopesWithHeldGeneration();
    TestRootScrollPanelRtlZeroAndDerived();
    TestLabelIndexAcrossSuccessors();
    TestScrollWhileGeometryHeld();
    TestHiddenSubtreeReadOnlyForALabel();
    TestScrollbarGesturesPublishAsScrolls();
    TestOverlayContentInsideAScrollPanel();
    TestSnapshotBytesMatchTheHeap();
    DxUi::DebugSetAccessibilityIncrementalVerificationForTest(wasVerifying);
}
} // namespace AccessibilityBuildCost
