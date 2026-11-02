#pragma once

#include "Support.Tests.PreparedTreeSemanticFixture.h"

#if DXUI_ENABLE_DIAGNOSTICS
static void TestPreparedTreeUnfocusedExpansionAction(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::Tests;
    auto witness = std::make_shared<PreparedTreeSourceWitness>();
    const ThreePreparedTreeRows::Names names{L"Alpha", L"Beta", L"Omega"};
    ThreePreparedTreeModel model(std::make_shared<ThreePreparedTreeRows>(names, witness, true, false));
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "unfocused prepared tree scene");
    auto& view    = scene.view;
    auto controls = std::make_unique<Panel>();
    auto* tree    = controls->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.f, 0.f, 300.f, 120.f));
    tree->SetModel(&model);
    tree->SetFocusedItemId(709u);
    class Delegate final : public ITreeDelegate
    {
    public:
        std::function<void(uint64_t, bool)> toggle;
        void OnTreeToggleExpanded(uint64_t id, bool expanded) override
        {
            toggle(id, expanded);
        }
    } delegate;
    delegate.toggle = [&](uint64_t id, bool expanded)
    {
        Check(id == 101u, "embedded action addresses the unfocused prepared row");
        model.Replace(std::make_shared<ThreePreparedTreeRows>(names, witness, true, expanded));
        tree->NotifyDataChanged();
    };
    tree->SetDelegate(&delegate);
    view.Controls().SetRoot(std::move(controls));
    Hr(view.Prepare(320u, 180u, 96.f), "prepare unfocused tree");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    EmbeddedAccessibilityPlacement placement{{0, 0, 320, 180}, true};
    Hr(view.AttachAccessibility(site, 0x5052455041524545ull, placement), "attach unfocused tree accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "unfocused tree root");
    site->root = root.get();
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment, treeProvider, item;
    Hr(root.query_to(rootFragment.put()), "unfocused tree root fragment");
    Hr(rootFragment->Navigate(NavigateDirection_FirstChild, treeProvider.put()), "unfocused tree control");
    Hr(treeProvider->Navigate(NavigateDirection_FirstChild, item.put()), "unfocused first item");
    wil::com_ptr_nothrow<IExpandCollapseProvider> expand;
    Hr(item.query_to(expand.put()), "unfocused first item expansion pattern");
    size_t changes = 0u;
    DebugSetAccessibilityPropertyEventHookForTest(
        [](long property, void* context) noexcept
    {
        if (property == UIA_ExpandCollapseExpandCollapseStatePropertyId)
            ++*static_cast<size_t*>(context);
    },
        &changes);
    const auto clearHook = wil::scope_exit([]() noexcept { DebugSetAccessibilityPropertyEventHookForTest(nullptr, nullptr); });
    Hr(expand->Expand(), "expand an unfocused embedded prepared item");
    Check(changes == 0u, "embedded state event waits for coherent publication");
    Hr(view.Prepare(320u, 180u, 96.f), "prepare expanded embedded tree");
    Hr(view.UpdateAccessibility(placement), "publish expanded embedded tree");
    ExpandCollapseState state{};
    Hr(expand->get_ExpandCollapseState(&state), "read expanded embedded item");
    Check(changes == 1u && state == ExpandCollapseState_Expanded && tree->GetFocusedItemId() == 709u,
          "unfocused embedded action publishes exactly one current state event without moving focus");
    changes         = 0u;
    delegate.toggle = [](uint64_t, bool) {};
    Hr(expand->Collapse(), "accept a delayed collapse before model replacement");
    ThreePreparedTreeModel replacement(std::make_shared<ThreePreparedTreeRows>(names, witness, true, false));
    tree->SetModel(&replacement);
    Hr(view.Prepare(320u, 180u, 96.f), "prepare replacement tree model");
    Hr(view.UpdateAccessibility(placement), "publish replacement tree model");
    Check(changes == 0u, "an old pending action cannot report a replacement model's state as its own event");
    delegate.toggle = [&](uint64_t id, bool expanded)
    {
        Check(id == 101u, "replacement action addresses the current item");
        replacement.Replace(std::make_shared<ThreePreparedTreeRows>(names, witness, true, expanded));
        tree->NotifyDataChanged();
    };
    Hr(expand->Expand(), "replacement model can retry after old hints retire");
    Hr(view.Prepare(320u, 180u, 96.f), "prepare replacement action");
    Hr(view.UpdateAccessibility(placement), "publish replacement action");
    Check(changes == 1u, "the new model publishes its own unfocused action once");
    tree->SetDelegate(nullptr);
    view.Detach();
    Check(expand->get_ExpandCollapseState(&state) == UIA_E_ELEMENTNOTAVAILABLE, "retained item retires with the embedded owner");
}
#endif

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

    tree->SetMultiSelectEnabled(true);
    const std::array<uint64_t, 2> selectedIds{709u, 9001u};
    tree->SetSelectedItemIds(selectedIds);
    tree->SetFocusedItemId(101u);
    view.Controls().SetFocusControl(tree);
    Hr(view.Prepare(320u, 180u, 96.0f), "prepare independent prepared focus and selection");
    Hr(view.UpdateAccessibility(placement), "publish independent prepared focus and selection");
    wil::com_ptr_nothrow<IRawElementProviderFragment> focused;
    Hr(root->GetFocus(focused.put()), "read prepared focus");
    Check(focused && ReadPreparedTreeName(focused, "prepared focused name") == L"Alpha α", "prepared focus is independent of the selected rows");
    BOOL firstSelected = TRUE;
    wil::com_ptr_nothrow<ISelectionItemProvider> firstSelection;
    Hr(first.query_to(firstSelection.put()), "prepared first SelectionItem");
    Hr(firstSelection->get_IsSelected(&firstSelected), "read independent prepared selection");
    Check(firstSelected == FALSE, "focused prepared row remains unselected");
    wil::com_ptr_nothrow<ISelectionProvider> selection;
    Hr(selectionContainer.query_to(selection.put()), "prepared Selection provider");
    SAFEARRAY* selectedRows = nullptr;
    Hr(selection->GetSelection(&selectedRows), "read multiple prepared selection");
    using SelectionArray = wil::unique_any<SAFEARRAY*, decltype(&SafeArrayDestroy), SafeArrayDestroy>;
    SelectionArray selectedOwner(selectedRows);
    LONG lower = 0, upper = -1;
    Hr(SafeArrayGetLBound(selectedRows, 1, &lower), "prepared selection lower bound");
    Hr(SafeArrayGetUBound(selectedRows, 1, &upper), "prepared selection upper bound");
    Check(upper - lower + 1 == 2, "prepared selection includes both selected rows");
    selectedOwner.reset();
    selection.reset();
    firstSelection.reset();
    focused.reset();

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
