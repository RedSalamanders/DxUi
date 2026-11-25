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
// Every incremental publication is verified against a whole build of the same tree.
// Included by Embedded.Tests.Embedded.cpp after the performance fixtures; the Embedded suite runs it, and alone with
// --test-accessibility-build-cost.
#include "Controls/DxUi.Internal.h"
#include "Support/WorkCounters.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>

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

inline void Run()
{
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
    DxUi::DebugSetAccessibilityIncrementalVerificationForTest(wasVerifying);
}
} // namespace AccessibilityBuildCost
