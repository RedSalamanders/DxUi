#pragma once

#include "PreparedTreeAccessibilityRowsFixture.h"

#include <chrono>
#include <iomanip>

// Identical source drives baseline and candidate. Model construction, paint preparation and provider reads
// stay outside the timed snapshot updates; every raw round includes allocation and model-call counts.
static void RunPreparedTreeAccessibilityBenchmark(GraphicsFixture& gpu, bool prepared)
{
    DxUi::Tests::PreparedCountingLargeTreeModel adapter(prepared);
    auto& model = adapter.model;
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "large Tree supplied-device scene");
    auto controls = std::make_unique<DxUi::Panel>();
    auto* tree    = controls->AddChild<DxUi::Tree>();
    tree->SetModel(&adapter);
    tree->SetBounds(D2D1::RectF(12, 12, 450, 225));
    tree->SetAccessibleName(L"Large synthetic tree");
    scene.view.Controls().SetRoot(std::move(controls));
    Hr(scene.view.Prepare(480, 240, 96), "prepare large synthetic Tree");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, 480, 240}, false};
    Hr(scene.view.AttachAccessibility(site, 0x3333, placement), "attach large Tree accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(scene.view.GetAccessibilityProvider(root.put()), "large Tree provider root");
    site->root = root.get();

    for (unsigned int round = 0; round < 12; ++round)
    {
        placement.viewport.left = round + 1;
        model.ResetCallCounts();
        allocations           = 0;
        allocationBytes       = 0;
        countAllocations      = true;
        const auto begin      = std::chrono::steady_clock::now();
        const HRESULT updated = scene.view.UpdateAccessibility(placement);
        const auto end        = std::chrono::steady_clock::now();
        countAllocations      = false;
        Hr(updated, "publish moved large Tree snapshot");
        const auto microseconds = std::chrono::duration<double, std::micro>(end - begin).count();
        std::cout << "TREE_ACCESSIBILITY_COST round=" << round << " prepared=" << prepared << " supported=" << DXUI_TEST_HAS_PREPARED_TREE_ROWS
                  << " rows=10000 name_units=1024 us=" << std::fixed << std::setprecision(3) << microseconds << " allocations=" << allocations
                  << " allocation_bytes=" << allocationBytes << " count_calls=" << model.visibleItemCountCalls << " item_calls=" << model.visibleItemCalls
                  << " lookup_calls=" << model.findVisibleItemByIdCalls << '\n';
    }

    wil::com_ptr_nothrow<IRawElementProviderFragment> first;
    Hr(root->ElementProviderFromPoint(44, 32, first.put()), "large Tree first row hit");
    Check(bool(first), "large Tree first row remains accessible");
    wil::com_ptr_nothrow<IRawElementProviderFragment> parent;
    Hr(first->Navigate(NavigateDirection_Parent, parent.put()), "large Tree row parent");
    Check(bool(parent), "large Tree exposes its selection container");
    wil::com_ptr_nothrow<IRawElementProviderFragment> last;
    Hr(parent->Navigate(NavigateDirection_LastChild, last.put()), "large Tree offscreen last row navigation");
    Check(bool(last), "offscreen row is present in navigation");
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Hr(last.query_to(simple.put()), "large Tree offscreen row property interface");
    VARIANT name{};
    const auto clearName = wil::scope_exit([&]() noexcept { VariantClear(&name); });
    Hr(simple->GetPropertyValue(UIA_NamePropertyId, &name), "large Tree offscreen full name");
    Check(name.vt == VT_BSTR && SysStringLen(name.bstrVal) == 1024 && std::wstring_view(name.bstrVal, 1024).ends_with(L"#09999"),
          "offscreen last row has its complete literal name");
    scene.view.Detach();
    VARIANT disconnected{};
    const auto clearDisconnected = wil::scope_exit([&]() noexcept { VariantClear(&disconnected); });
    Check(simple->GetPropertyValue(UIA_NamePropertyId, &disconnected) == UIA_E_ELEMENTNOTAVAILABLE, "retained large Tree row disconnects on detach");
    site->root = nullptr;
    std::cout << "TREE_ACCESSIBILITY_COST summary rounds=12 semantic_checks=passed\n";
}
