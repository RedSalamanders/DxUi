#pragma once

#include "../Support/PreparedTreeSemanticFixture.h"

static std::wstring ReadPreparedTreeName(const wil::com_ptr_nothrow<IRawElementProviderFragment>& provider, const char* context)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Hr(provider.query_to(simple.put()), context);
    Check(bool(simple), context);
    wil::unique_variant name;
    Hr(simple->GetPropertyValue(UIA_NamePropertyId, &name), context);
    Check(name.vt == VT_BSTR, context);
    return name.bstrVal ? std::wstring(name.bstrVal, SysStringLen(name.bstrVal)) : std::wstring{};
}

// Semantic and source-lifetime qualification only: no client-reentrant event, large-count, or G1 claim.
static void TestPreparedTreeAccessibilitySemantics(GraphicsFixture& gpu)
{
    using namespace DxUi::Tests;
    auto oldWitness = std::make_shared<PreparedTreeSourceWitness>();
    auto oldSource  = std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta β", L"Omega Ω"}, oldWitness);
    Check(! oldSource->GetItem(3u) && ! oldSource->FindItem(42u), "prepared tree source rejects absent rows and ids");
    ThreePreparedTreeModel model(oldSource);

    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "prepared tree supplied-device scene");
    auto& view    = scene.view;
    auto controls = std::make_unique<DxUi::Panel>();
    auto* tree    = controls->AddChild<DxUi::Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    tree->SetModel(&model);
    view.Controls().SetRoot(std::move(controls));
    Hr(view.Prepare(320u, 180u, 96.0f), "prepare semantic tree");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, 320, 180}, true};
    Hr(view.AttachAccessibility(site, 0x5052455041524544ull, placement), "attach semantic tree accessibility");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "semantic tree embedded root");
    Check(bool(root), "semantic tree embedded root exists");
    site->root = root.get();
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    Hr(root.query_to(rootFragment.put()), "semantic tree embedded root fragment");
    wil::com_ptr_nothrow<IRawElementProviderFragment> treeProvider;
    Hr(rootFragment->Navigate(NavigateDirection_FirstChild, treeProvider.put()), "navigate to semantic tree");
    Check(bool(treeProvider), "semantic tree provider exists");
    wil::com_ptr_nothrow<IRawElementProviderFragment> first, middle, last;
    Hr(treeProvider->Navigate(NavigateDirection_FirstChild, first.put()), "navigate to first prepared row");
    Check(bool(first), "first prepared row provider exists");
    Hr(first->Navigate(NavigateDirection_NextSibling, middle.put()), "navigate to middle prepared row");
    Check(bool(middle), "middle prepared row provider exists");
    Hr(middle->Navigate(NavigateDirection_NextSibling, last.put()), "navigate to last prepared row");
    Check(bool(last), "last prepared row provider exists");
    Check(ReadPreparedTreeName(first, "read first prepared row name") == L"Alpha α", "first row exposes full literal Unicode name");
    Check(ReadPreparedTreeName(last, "read last prepared row name") == L"Omega Ω", "last row exposes full literal Unicode name");
    wil::com_ptr_nothrow<IRawElementProviderFragment> previousMiddle, noNext;
    Hr(last->Navigate(NavigateDirection_PreviousSibling, previousMiddle.put()), "navigate back to middle prepared row");
    Hr(last->Navigate(NavigateDirection_NextSibling, noNext.put()), "navigate beyond last prepared row");
    Check(previousMiddle && ReadPreparedTreeName(previousMiddle, "read previous middle name") == L"Beta β" && ! noNext,
          "prepared row sibling navigation is ordered and bounded");

    wil::com_ptr_nothrow<ISelectionItemProvider> selectionItem;
    Hr(middle.query_to(selectionItem.put()), "middle row SelectionItem pattern");
    Check(bool(selectionItem), "middle row SelectionItem provider exists");
    Hr(selectionItem->Select(), "select middle prepared row");
    Check(tree->GetSelectedItemId() == std::optional<uint64_t>{709u} && site->completions == 1u, "SelectionItem action reaches the live tree exactly once");
    Hr(view.Prepare(320u, 180u, 96.0f), "prepare selected semantic tree");
    Hr(view.UpdateAccessibility(placement), "publish acknowledged semantic tree selection");
    BOOL selected = FALSE;
    Hr(selectionItem->get_IsSelected(&selected), "read middle prepared row selection");
    wil::com_ptr_nothrow<IRawElementProviderSimple> selectionContainer;
    Hr(selectionItem->get_SelectionContainer(selectionContainer.put()), "read prepared row selection container");
    Check(bool(selectionContainer), "prepared row selection container exists");
    wil::unique_variant containerType;
    Hr(selectionContainer->GetPropertyValue(UIA_ControlTypePropertyId, &containerType), "read prepared row selection container type");
    Check(selected == TRUE && tree->GetSelectedItemId() == std::optional<uint64_t>{709u} && site->completions == 1u,
          "SelectionItem selection is acknowledged by the tree and provider");
    Check(containerType.vt == VT_I4 && containerType.lVal == UIA_TreeControlTypeId, "SelectionItem container is the public tree provider");

    oldSource.reset();
    auto newWitness = std::make_shared<PreparedTreeSourceWitness>();
    auto newSource  = std::make_shared<ThreePreparedTreeRows>(ThreePreparedTreeRows::Names{L"Alpha α", L"Beta nouveau β", L"Omega nouveau Ω"}, newWitness);
    model.Replace(newSource);
    tree->NotifyDataChanged();
    view.MarkDirty(); // The embedding application schedules a changed preparation after its model publication.
    Check(view.Prepare(320u, 180u, 96.0f) == S_OK, "prepare replacement semantic tree changes the displayed revision");
    Check(view.UpdateAccessibility(placement) == S_OK, "publish replacement semantic tree creates a new snapshot");
    Check(ReadPreparedTreeName(middle, "read retained middle replacement") == L"Beta nouveau β" &&
              ReadPreparedTreeName(last, "read retained last replacement") == L"Omega nouveau Ω",
          "retained providers read acknowledged same-id replacement names");
    Check(oldWitness->destructions == 0u, "retained embedded providers keep the replaced source snapshot alive");

    view.Controls().SetRoot(std::make_unique<DxUi::Panel>());
    wil::com_ptr_nothrow<IRawElementProviderSimple> staleMiddle;
    Hr(middle.query_to(staleMiddle.put()), "retain replaced-root middle provider");
    wil::unique_variant staleName;
    Check(staleMiddle->GetPropertyValue(UIA_NamePropertyId, &staleName) == UIA_E_ELEMENTNOTAVAILABLE,
          "root replacement invalidates the old prepared row provider");
    Hr(view.Prepare(320u, 180u, 96.0f), "prepare replacement root");
    Hr(view.UpdateAccessibility(placement), "publish replacement root");
    site->root = nullptr;
    view.Detach();
    wil::com_ptr_nothrow<IRawElementProviderFragment> detachedChild;
    Check(rootFragment->Navigate(NavigateDirection_FirstChild, detachedChild.put()) == UIA_E_ELEMENTNOTAVAILABLE && ! detachedChild,
          "detaching with retained prepared providers is safe");

    staleMiddle.reset();
    selectionContainer.reset();
    selectionItem.reset();
    previousMiddle.reset();
    last.reset();
    middle.reset();
    first.reset();
    treeProvider.reset();
    rootFragment.reset();
    root.reset();
    Check(oldWitness->destructions == 1u, "replaced source is destroyed after its final snapshot reader releases it");
}
