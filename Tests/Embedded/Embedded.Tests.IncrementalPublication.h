#pragma once
// Incremental UI Automation publication (W8.2 of the performance and motion plan), on the FX6 definition (one client: the
// client gate's override). The gate: the 100 Slider steps build no whole window and each re-reads at most one record, sharing
// every other with the generation before; the 200 px Splitter drag builds no whole window while the separator moves (its press
// moves the keyboard focus, which re-reads two records) and builds once at the release. Every incremental publication is
// verified against a whole build of the same tree (DebugSetAccessibilityIncrementalVerificationForTest): 0 differences. The
// per-step numbers of the receipt (Measurements/PerformanceAndMotion_2026-11-13/W8.2) are printed here.
// Included by Embedded.Tests.Embedded.cpp after the performance fixtures.
#include "Controls/DxUi.Internal.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace IncrementalPublication
{
struct PublicationTotals
{
    uint64_t builds = 0u, incremental = 0u, records = 0u, chunks = 0u, bytes = 0u, pointHits = 0u, maxRecords = 0u, maxBuilds = 0u;
};

[[nodiscard]] inline PublicationTotals Sum(const PerformanceFixtures::PhaseRecord& phase, size_t first, size_t last)
{
    PublicationTotals totals;
    for (size_t step = first; step <= last && step < phase.steps.size(); ++step)
    {
        const auto& work = phase.steps[step].work.accessibility;
        totals.builds += work.snapshotBuilds;
        totals.incremental += work.incrementalPublications;
        totals.records += work.recordsCloned;
        totals.chunks += work.chunksCloned;
        totals.bytes += work.spineBytes;
        totals.pointHits += work.pointHitRebuilds;
        totals.maxRecords = (std::max)(totals.maxRecords, work.recordsCloned);
        totals.maxBuilds  = (std::max)(totals.maxBuilds, work.snapshotBuilds);
    }
    return totals;
}

inline void Print(const char* what, const PublicationTotals& totals, size_t steps)
{
    std::cout << "W8.2 FX6 " << what << ": " << steps << " publications, whole builds " << totals.builds << ", incremental " << totals.incremental
              << ", records re-read " << totals.records << " (at most " << totals.maxRecords << " per step), chunks cloned " << totals.chunks
              << ", spine and chunk bytes copied " << totals.bytes << ", bounding-rectangle rebuilds " << totals.pointHits << std::endl;
}

// FX6 with one client: the Slider and Splitter gates (plan W8.2). Step k of a phase publishes what step k - 1 did, so the
// slider's 100 keys are published by its steps 1 to 99 and by the splitter phase's step 0; the drag's press by step 1, its 200
// moves by steps 2 to 201, and its release by the word walk's step 0, which also shows the document.
inline void TestFx6IncrementalPublicationGate()
{
    using namespace PerformanceFixtures;
    const bool wasVerifying = DxUi::DebugGetAccessibilityIncrementalVerificationForTest();
    DxUi::DebugResetAccessibilityIncrementalVerificationForTest();
    DxUi::DebugSetAccessibilityIncrementalVerificationForTest(true);
    const RunRecord run = RunEmbedded(Fx6::kDefinition, Fx6::kVariants[0]);
    DxUi::DebugSetAccessibilityIncrementalVerificationForTest(wasVerifying);
    const PhaseRecord& slider    = run.Phase("slider-one-client");
    const PhaseRecord& splitter  = run.Phase("splitter-one-client");
    const PhaseRecord& walk      = run.Phase("wordwalk-one-client");
    PublicationTotals keys       = Sum(slider, 1u, slider.steps.size() - 1u);
    const PublicationTotals last = Sum(splitter, 0u, 0u);
    keys.builds += last.builds;
    keys.incremental += last.incremental;
    keys.records += last.records;
    keys.chunks += last.chunks;
    keys.bytes += last.bytes;
    keys.maxRecords = (std::max)(keys.maxRecords, last.maxRecords);
    keys.maxBuilds  = (std::max)(keys.maxBuilds, last.maxBuilds);
    Print("slider keys", keys, Fx6::kSliderSteps);
    const PublicationTotals press = Sum(splitter, 1u, 1u);
    const PublicationTotals moves = Sum(splitter, 2u, Fx6::kDragPixels + 1u);
    const PublicationTotals up    = Sum(walk, 0u, 0u);
    Print("splitter press", press, 1u);
    Print("splitter moves", moves, Fx6::kDragPixels);
    Print("splitter release (with the document shown)", up, 1u);
    const uint64_t verified   = DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest();
    const uint64_t mismatches = DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest();
    std::cout << "W8.2 FX6 incremental publications verified against a whole build: " << verified << ", differences " << mismatches << std::endl;
    if (mismatches != 0u)
    {
        const std::wstring first = DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest();
        std::wcerr << L"    [W8.2] first difference: " << first << L'\n';
    }
    Check(mismatches == 0u && verified >= Fx6::kSliderSteps + Fx6::kDragPixels, "every incremental FX6 publication equals a whole build of the same tree");
    Check(keys.builds == 0u && keys.incremental == Fx6::kSliderSteps, "FX6, 100 Slider steps: no whole-window build, one incremental publication each");
    Check(keys.maxRecords <= 1u, "FX6, 100 Slider steps: each re-reads at most one record, every other shared");
    Check(press.builds == 0u && press.records <= 2u, "FX6 Splitter press: the focus move re-reads the two records whose focus changed");
    Check(moves.builds == 0u && moves.incremental == Fx6::kDragPixels && moves.maxRecords <= 1u,
          "FX6 Splitter drag: no whole-window build while the separator moves, the splitter's record alone each frame");
    Check(up.builds == 1u, "FX6 Splitter release: one whole build commits the drag");
}

// A composition previewed in an embedded view (the W8.4 record: "after a composition preview, the field's published text is
// still the committed text"). Established with the text-input path an application's text service uses (ApplyTextInput, no
// real IME): what the field publishes while the composition is shown, as the document range, the value and the active
// composition read it, and after the commit. Windows edit controls expose the composition string in their document and value
// while it is composed (it is in the text a user sees), and W8.4's composition range reads its span of the published text, so
// the published text must be the previewed text. Run with whole builds too, as every publication was before W8.2: both
// publish the previewed text, so the record's observation does not reproduce on this path.
inline void TestCompositionPreviewPublishesThePreviewText(bool whole)
{
    using namespace DxUi;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "composition publication WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "composition publication graphics device");
    EmbeddedHost view;
    Hr(view.Attach(std::move(graphics), StoppedTestClock()), "attach the composition publication view");
    const std::optional<bool> outer = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(true);
    DebugSetAccessibilityWholePublicationForTest(whole); // whole: every publication built as before W8.2
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"abc");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 460.0f, 32.0f));
    field->SetAccessibleName(L"Field");
    view.Controls().SetRoot(std::move(root));
    view.Controls().SetFocusControl(field);
    Hr(view.Prepare(480u, 240u), "prepare the composition publication view");
    auto site = std::make_shared<PerformanceFixtures::Fx6::Site>();
    const EmbeddedAccessibilityPlacement placement{{0.0, 0.0, 480.0, 240.0}, true};
    Hr(view.AttachAccessibility(site, 0x5738320000000001ull, placement), "attach the composition publication view's accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Hr(view.GetAccessibilityProvider(provider.put()), "composition publication root provider");
    site->root           = provider.get();
    const auto readField = [&](std::wstring& value, std::wstring& document, std::wstring& composition)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        Hr(provider.query_to(fragment.put()), "composition publication root fragment");
        wil::com_ptr_nothrow<IRawElementProviderFragment> child;
        Hr(fragment->Navigate(NavigateDirection_FirstChild, child.put()), "composition publication field element");
        Check(bool(child), "the view exposes its field");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(child.query_to(simple.put()), "composition publication field provider");
        wil::unique_variant valueProperty;
        Hr(simple->GetPropertyValue(UIA_ValueValuePropertyId, &valueProperty), "the field's value");
        value.assign(valueProperty.vt == VT_BSTR && valueProperty.bstrVal ? valueProperty.bstrVal : L"");
        wil::com_ptr_nothrow<IUnknown> pattern;
        Hr(simple->GetPatternProvider(UIA_TextEditPatternId, pattern.put()), "the field's TextEdit pattern");
        wil::com_ptr_nothrow<ITextEditProvider> edits;
        Hr(pattern.query_to(edits.put()), "the field's TextEdit interface");
        wil::com_ptr_nothrow<ITextRangeProvider> range;
        Hr(edits->get_DocumentRange(range.put()), "the field's document range");
        wil::unique_bstr text;
        Hr(range->GetText(-1, text.put()), "read the document range");
        document.assign(text ? text.get() : L"");
        wil::com_ptr_nothrow<ITextRangeProvider> active;
        composition.clear();
        if (SUCCEEDED(edits->GetActiveComposition(active.put())) && active)
        {
            wil::unique_bstr composed;
            Hr(active->GetText(-1, composed.put()), "read the active composition");
            composition.assign(composed ? composed.get() : L"");
        }
    };
    const auto publish = [&](const char* what)
    {
        HRESULT published = S_FALSE;
        for (size_t turn = 0u; turn < 16u && published == S_FALSE; ++turn)
        {
            Hr(view.Prepare(480u, 240u), what);
            published = view.UpdateAccessibility(placement);
        }
        Check(published == S_OK, what);
    };
    EmbeddedTextInputSnapshot input;
    Hr(view.ReadTextInput(input), "read the field's text input");
    NativeTextInputState preview = input.state;
    preview.text                 = L"aXYbc";
    preview.caretIndex           = 3u;
    preview.selectionAnchorIndex.reset();
    preview.compositionStartIndex      = 1u;
    preview.compositionEndIndex        = 3u;
    preview.conversionTargetStartIndex = 1u;
    preview.conversionTargetEndIndex   = 3u;
    Hr(view.ApplyTextInput(input.revision, preview, EmbeddedTextInputAction::Preview), "preview a composition");
    publish("the previewed composition is published");
    std::wstring value, document, composition;
    readField(value, document, composition);
    std::wcout << (whole ? L"W8.2 composition preview, whole builds, publishes" : L"W8.2 composition preview publishes") << L": document \"" << document
               << L"\", value \"" << value << L"\", active composition \"" << composition << L"\"" << std::endl;
    Check(document == L"aXYbc" && value == L"aXYbc", "while a composition is previewed the field publishes the previewed text, as Windows edit controls do");
    Check(composition == L"XY", "the active composition reads its span of the published text");
    Hr(view.ReadTextInput(input), "read the composing field's text input");
    NativeTextInputState commit = input.state;
    commit.text                 = L"aZbc";
    commit.caretIndex           = 2u;
    commit.selectionAnchorIndex.reset();
    commit.compositionStartIndex.reset();
    commit.compositionEndIndex.reset();
    commit.conversionTargetStartIndex.reset();
    commit.conversionTargetEndIndex.reset();
    Hr(view.ApplyTextInput(input.revision, commit, EmbeddedTextInputAction::Commit), "commit the composition");
    publish("the commit is published");
    readField(value, document, composition);
    Check(document == L"aZbc" && value == L"aZbc" && composition.empty(), "after the commit the field publishes the committed text and no composition");
    site->root = nullptr;
    provider.reset();
    view.DisconnectAccessibility();
    view.Detach();
    DebugSetAccessibilityWholePublicationForTest(false);
    DebugSetAccessibilitySystemListenersForTest(outer);
}

// The cost a layout pays for publication (W8.2 review F3a), on FX12's form shape: one ScrollPanel of rows of a Label, a
// TextField, a Checkbox, a Toggle and a Button, `rows` rows (400 rows: 2,000 controls; 2,000 rows: 10,000), in an embedded
// view with one client (the build gate's override). Measured: the publication of one whole build (after a label's text
// change, every publication whole), and per frame of a 20-frame layout that moves every control, incrementally and whole.
// Each figure is the UpdateAccessibility call alone (the frame's Prepare is outside it), the median of its frames, with the
// C++ allocations it made. Run in Release: Embedded.Tests --measure-incremental-publication.
inline void MeasureLayoutPublication(size_t rows)
{
    using namespace DxUi;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "layout publication WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "layout publication graphics device");
    EmbeddedHost view;
    Hr(view.Attach(std::move(graphics), StoppedTestClock()), "attach the layout publication view");
    const std::optional<bool> outer = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(true);
    constexpr UINT kWidth = 1920u, kHeight = 1080u;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(8.0f, 8.0f, 1912.0f, 1072.0f));
    std::vector<Control*> moved;
    Label* first = nullptr;
    for (size_t row = 0u; row < rows; ++row)
    {
        auto* label = scroll->AddChild<Label>(std::format(L"Field {:04}", row));
        if (! first)
            first = label;
        moved.push_back(label);
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
    Hr(view.Prepare(kWidth, kHeight), "prepare the layout publication view");
    auto site = std::make_shared<PerformanceFixtures::Fx6::Site>();
    const EmbeddedAccessibilityPlacement placement{{0.0, 0.0, static_cast<double>(kWidth), static_cast<double>(kHeight)}, true};
    Hr(view.AttachAccessibility(site, 0x5738320000000002ull, placement), "attach the layout publication view's accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Hr(view.GetAccessibilityProvider(provider.put()), "layout publication root provider");
    site->root = provider.get();
    struct Sample
    {
        double ms           = 0.0;
        size_t allocations_ = 0u;
    };
    const auto publish = [&]() -> Sample
    {
        Hr(view.Prepare(kWidth, kHeight), "prepare a layout publication frame");
        const size_t allocationsBefore = allocations;
        countAllocations               = true;
        const auto started             = std::chrono::steady_clock::now();
        const HRESULT published        = view.UpdateAccessibility(placement);
        const auto ended               = std::chrono::steady_clock::now();
        countAllocations               = false;
        Check(SUCCEEDED(published), "a layout publication frame publishes");
        return {std::chrono::duration<double, std::milli>(ended - started).count(), allocations - allocationsBefore};
    };
    const auto median = [](std::vector<Sample> samples)
    {
        std::ranges::sort(samples, {}, &Sample::ms);
        return samples[samples.size() / 2u];
    };
    static_cast<void>(publish()); // Settles the attachment.
    std::vector<Sample> whole;
    DebugSetAccessibilityWholePublicationForTest(true);
    for (size_t turn = 0u; turn < 7u; ++turn)
    {
        first->SetText(std::format(L"Field {}", turn));
        whole.push_back(publish());
    }
    // Whole: every frame builds the window. Incremental: each frame's update (deferred within 100 ms of the last geometry
    // publication: the view's clock is stopped, so every frame after the first is), then an update with nothing moved since,
    // which publishes the frame's layout as an incremental successor (the settle): the cost coalescing saves per frame.
    std::vector<Sample> wholeLayout, deferredLayout, incrementalLayout;
    uint64_t builds = 0u, collapsed = 0u, records = 0u, deferred = 0u, incrementalPublications = 0u;
    for (size_t mode = 0u; mode < 2u; ++mode)
    {
        DebugSetAccessibilityWholePublicationForTest(mode == 0u);
        for (size_t frame = 1u; frame <= 20u; ++frame)
        {
            const Detail::WorkCounterScope work;
            layout(static_cast<float>(frame % 2u == 0u ? 0u : 4u));
            if (mode == 0u)
            {
                wholeLayout.push_back(publish());
                continue;
            }
            const uint64_t publications = DebugGetAccessibilityPublicationCountForTest();
            const Sample update         = publish();
            (DebugGetAccessibilityPublicationCountForTest() == publications ? deferredLayout : incrementalLayout).push_back(update);
            if (DebugGetAccessibilityPublicationCountForTest() == publications)
                incrementalLayout.push_back(publish());
            const auto delta = work.Delta().accessibility;
            builds += delta.snapshotBuilds;
            collapsed += delta.geometryMarksCollapsed;
            records += delta.recordsCloned;
            deferred += delta.geometryPublicationsDeferred;
            incrementalPublications += delta.incrementalPublications;
        }
    }
    DebugSetAccessibilityWholePublicationForTest(false);
    const Sample build = median(whole), wholeFrame = median(wholeLayout), incrementalFrame = median(incrementalLayout);
    const Sample deferredFrame = deferredLayout.empty() ? Sample{} : median(deferredLayout);
    std::cout << std::format("W8.2 layout publication, {} controls, one client: whole build {:.3f} ms, {} allocations; 20 frames moving every control: "
                             "whole {:.3f} ms and {} allocations per frame, incremental {:.3f} ms and {} allocations per publication ({} publications, "
                             "whole builds {}, marks collapsed {}, records re-read {}), deferred frame {:.3f} ms and {} allocations ({} deferred)",
                             moved.size(),
                             build.ms,
                             build.allocations_,
                             wholeFrame.ms,
                             wholeFrame.allocations_,
                             incrementalFrame.ms,
                             incrementalFrame.allocations_,
                             incrementalPublications,
                             builds,
                             collapsed,
                             records,
                             deferredFrame.ms,
                             deferredFrame.allocations_,
                             deferred)
              << std::endl;
    site->root = nullptr;
    provider.reset();
    view.DisconnectAccessibility();
    view.Detach();
    DebugSetAccessibilitySystemListenersForTest(outer);
}

// The coalescing of geometry-only publications (W8.2): on the application's clock, a layout that moves 40 controls every
// 10 ms for 20 frames publishes at its first frame and at most once per 100 ms after it (the view's clock starts at its
// first advance, two frames in), every other frame deferred with the view's
// animation request due at once (the published deadline of A3); the first update after the layout settles publishes the
// final bounds, and a name change during the layout publishes at once.
inline void TestGeometryPublicationsCoalesce()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "geometry coalescing WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "geometry coalescing graphics device");
    RunningTestClock clock;
    clock.nowMs = 1'000u;
    EmbeddedHost view;
    Hr(view.Attach(std::move(graphics), clock.Callbacks()), "attach the geometry coalescing view");
    const std::optional<bool> outer = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(true);
    const bool wasVerifying = DebugGetAccessibilityIncrementalVerificationForTest();
    DebugResetAccessibilityIncrementalVerificationForTest();
    DebugSetAccessibilityIncrementalVerificationForTest(true);
    auto root    = std::make_unique<Panel>();
    auto* status = root->AddChild<Label>(L"Status");
    status->SetBounds(D2D1::RectF(400.0f, 8.0f, 600.0f, 32.0f));
    std::vector<Button*> pane;
    for (size_t index = 0u; index < 40u; ++index)
        pane.push_back(root->AddChild<Button>(std::format(L"Item {}", index)));
    const auto layout = [&pane](float left)
    {
        for (size_t index = 0u; index < pane.size(); ++index)
            pane[index]->SetBounds(D2D1::RectF(left, 8.0f + 11.0f * static_cast<float>(index), left + 100.0f, 18.0f + 11.0f * static_cast<float>(index)));
    };
    layout(8.0f);
    view.Controls().SetRoot(std::move(root));
    constexpr UINT kWidth = 640u, kHeight = 480u;
    Hr(view.Prepare(kWidth, kHeight), "prepare the geometry coalescing view");
    auto site = std::make_shared<PerformanceFixtures::Fx6::Site>();
    const EmbeddedAccessibilityPlacement placement{{0.0, 0.0, static_cast<double>(kWidth), static_cast<double>(kHeight)}, true};
    Hr(view.AttachAccessibility(site, 0x5738320000000003ull, placement), "attach the geometry coalescing view's accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Hr(view.GetAccessibilityProvider(provider.put()), "geometry coalescing root provider");
    site->root         = provider.get();
    const auto publish = [&]() -> bool
    {
        Hr(view.Prepare(kWidth, kHeight), "prepare a geometry coalescing frame");
        const uint64_t before = DebugGetAccessibilityPublicationCountForTest();
        Check(SUCCEEDED(view.UpdateAccessibility(placement)), "a geometry coalescing frame updates");
        return DebugGetAccessibilityPublicationCountForTest() != before;
    };
    static_cast<void>(publish()); // Settles the attachment.
    const Detail::WorkCounterScope work;
    std::vector<size_t> published;
    bool requestedWhileDeferred = true;
    for (size_t frame = 1u; frame <= 20u; ++frame)
    {
        clock.nowMs += 10u;
        // As an application does: it advances the view whenever the view asks (its clock starts at the first such advance).
        if (view.GetAnimationRequest().kind != EmbeddedAnimationRequest::Kind::None)
            static_cast<void>(view.AdvanceAnimation(clock.nowMs));
        layout(8.0f + 2.0f * static_cast<float>(frame));
        if (publish())
            published.push_back(frame);
        else
            requestedWhileDeferred = requestedWhileDeferred && view.GetAnimationRequest().kind != EmbeddedAnimationRequest::Kind::None;
    }
    const uint64_t deferred = work.Delta().accessibility.geometryPublicationsDeferred;
    clock.nowMs += 10u;
    const bool settledPublishes = publish();
    clock.nowMs += 10u;
    layout(100.0f);
    status->SetText(L"Moving");
    const bool namePublishes  = publish();
    const uint64_t mismatches = DebugGetAccessibilityIncrementalMismatchCountForTest();
    std::cout << "W8.2 geometry coalescing: 20 layout frames 10 ms apart publish at frames";
    for (const size_t frame : published)
        std::cout << ' ' << frame;
    std::cout << ", " << deferred << " deferred; settle publishes " << settledPublishes << ", name change publishes " << namePublishes << ", verified "
              << DebugGetAccessibilityIncrementalVerificationCountForTest() << ", mismatches " << mismatches << std::endl;
    Check(published.size() == 2u && published[0] == 1u && published[1] - published[0] >= 10u && deferred == 18u,
          "geometry-only publications are at most one per 100 ms of the application's clock");
    Check(requestedWhileDeferred, "a deferred geometry publication keeps the view's animation request due");
    Check(settledPublishes, "the first update after the layout settles publishes its final bounds");
    Check(namePublishes, "a name change during a layout publishes at once");
    Check(mismatches == 0u, "every coalesced publication equals a whole build");
    site->root = nullptr;
    provider.reset();
    view.DisconnectAccessibility();
    view.Detach();
    DebugSetAccessibilityIncrementalVerificationForTest(wasVerifying);
    DebugSetAccessibilitySystemListenersForTest(outer);
}

// What ends an embedded view's pending geometry deferral (W8.2 review B1).
enum class DeferralEnd : uint8_t
{
    ListenerLeaves,
    Disconnected,
    RootReplaced,
    DeviceReplaced,
    Hidden,
    Detached,
};

[[nodiscard]] inline const char* DescribeDeferralEnd(DeferralEnd end) noexcept
{
    switch (end)
    {
        case DeferralEnd::ListenerLeaves: return "the last listener leaves";
        case DeferralEnd::Disconnected: return "accessibility is disconnected";
        case DeferralEnd::RootReplaced: return "the root is replaced";
        case DeferralEnd::DeviceReplaced: return "the device is replaced";
        case DeferralEnd::Hidden: return "the view is hidden";
        case DeferralEnd::Detached: return "the view is detached";
    }
    return "?";
}

// No state leaves an embedded view's animation request due with nothing to publish (W8.2 review B1): a geometry-only
// publication is deferred (the request becomes due at once), then `end` happens before the application's next update. The
// application keeps going as documented, advancing when the view asks and preparing and updating every frame: after one
// frame, 200 idle frames see a None request and make no publication.
inline void TestDeferredRequestEnds(DeferralEnd end)
{
    using namespace DxUi;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "deferral end WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "deferral end graphics device");
    RunningTestClock clock;
    clock.nowMs = 1'000u;
    EmbeddedHost view;
    Hr(view.Attach(std::move(graphics), clock.Callbacks()), "attach the deferral end view");
    const std::optional<bool> outer = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(true);
    auto root = std::make_unique<Panel>();
    std::vector<Button*> pane;
    for (size_t index = 0u; index < 8u; ++index)
        pane.push_back(root->AddChild<Button>(std::format(L"Item {}", index)));
    const auto layout = [&pane](float left)
    {
        for (size_t index = 0u; index < pane.size(); ++index)
            pane[index]->SetBounds(D2D1::RectF(left, 8.0f + 20.0f * static_cast<float>(index), left + 100.0f, 26.0f + 20.0f * static_cast<float>(index)));
    };
    layout(8.0f);
    view.Controls().SetRoot(std::move(root));
    constexpr UINT kWidth = 640u, kHeight = 480u;
    Hr(view.Prepare(kWidth, kHeight), "prepare the deferral end view");
    auto site = std::make_shared<PerformanceFixtures::Fx6::Site>();
    const EmbeddedAccessibilityPlacement placement{{0.0, 0.0, static_cast<double>(kWidth), static_cast<double>(kHeight)}, true};
    Hr(view.AttachAccessibility(site, 0x5738320000000004ull, placement), "attach the deferral end view's accessibility");
    bool attached    = true;
    const auto frame = [&](bool move, float left) -> bool
    {
        clock.nowMs += 10u;
        if (view.GetAnimationRequest().kind != EmbeddedAnimationRequest::Kind::None)
            static_cast<void>(view.AdvanceAnimation(clock.nowMs));
        if (! attached)
            return false;
        if (move)
            layout(left);
        const HRESULT prepared = view.Prepare(kWidth, kHeight);
        Check(SUCCEEDED(prepared), "prepare a deferral end frame");
        const uint64_t before = DebugGetAccessibilityPublicationCountForTest();
        Check(SUCCEEDED(view.UpdateAccessibility(placement)), "a deferral end frame updates");
        return DebugGetAccessibilityPublicationCountForTest() != before;
    };
    static_cast<void>(frame(false, 0.0f)); // Settles the attachment.
    const bool first    = frame(true, 12.0f);
    const bool second   = frame(true, 16.0f);
    const bool deferred = view.GetAnimationRequest().kind != EmbeddedAnimationRequest::Kind::None;
    switch (end)
    {
        case DeferralEnd::ListenerLeaves: DebugSetAccessibilitySystemListenersForTest(false); break;
        case DeferralEnd::Disconnected: view.DisconnectAccessibility(); break;
        case DeferralEnd::RootReplaced:
            pane.clear();
            view.Controls().SetRoot(std::make_unique<Panel>());
            break;
        case DeferralEnd::DeviceReplaced:
        {
            std::shared_ptr<GraphicsDevice> replacement;
            Hr(GraphicsDevice::Create(gpu.device.get(), replacement), "deferral end replacement graphics device");
            Hr(view.ReplaceDevice(std::move(replacement)), "replace the deferral end view's device");
            break;
        }
        case DeferralEnd::Hidden: view.SetVisible(false); break;
        case DeferralEnd::Detached:
            view.DisconnectAccessibility();
            view.Detach();
            attached = false;
            break;
    }
    static_cast<void>(frame(false, 0.0f)); // The frame that answers the change (a replaced root publishes its new tree).
    size_t dueFrames = 0u, publications = 0u;
    for (size_t turn = 0u; turn < 200u; ++turn)
    {
        if (view.GetAnimationRequest().kind != EmbeddedAnimationRequest::Kind::None)
            ++dueFrames;
        if (frame(false, 0.0f))
            ++publications;
    }
    std::cout << "W8.2 deferral end, " << DescribeDeferralEnd(end) << ": first move publishes " << first << ", second deferred " << (! second && deferred)
              << "; 200 idle frames: request due on " << dueFrames << ", publications " << publications << std::endl;
    Check(first && ! second && deferred, "the second move within 100 ms is deferred with the view's request due");
    Check(dueFrames == 0u && publications == 0u, "no state leaves the view's request due with nothing to publish");
    if (attached)
    {
        view.DisconnectAccessibility();
        view.Detach();
    }
    DebugSetAccessibilitySystemListenersForTest(outer);
}

inline void Run()
{
    for (const DeferralEnd end : {DeferralEnd::ListenerLeaves,
                                  DeferralEnd::Disconnected,
                                  DeferralEnd::RootReplaced,
                                  DeferralEnd::DeviceReplaced,
                                  DeferralEnd::Hidden,
                                  DeferralEnd::Detached})
        TestDeferredRequestEnds(end);
    TestGeometryPublicationsCoalesce();
    TestFx6IncrementalPublicationGate();
    TestCompositionPreviewPublishesThePreviewText(true);
    TestCompositionPreviewPublishesThePreviewText(false);
}
} // namespace IncrementalPublication
