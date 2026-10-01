#include "../Support/UiaTestClient.h"
#include "EmbeddedUiaBridge.h"

#include <string_view>

// An embedded view whose only control is a Tree or a Grid, exposed to UI Automation through an application window (see
// EmbeddedUiaBridge.h), and a UI Automation client of that window: the walk from the application's element through the view's
// root and its control to the control's items and back, the events raised on them, and the selection events a multi-select tree's
// view raises itself when the application publishes a change. The embedded host never collapses its root into the control (its
// root element is the application element's child, and the control's elements have it for their parent), so these tests guard
// the chain the window host's collapsed root must match, and they prove the harness: a client that hears the events an embedded
// view raises, and walks the tree its providers expose.

class EmbeddedTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 3u;
    }

    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
    {
        static constexpr const wchar_t* kNames[] = {L"Général", L"Volets", L"Afficheurs"};
        item.id                                  = visibleIndex + 1u;
        item.text                                = kNames[visibleIndex];
    }
};

class EmbeddedStatusGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 3u;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = columnIndex == 0u ? L"name" : L"status";
        column.title    = columnIndex == 0u ? L"Nom" : L"État";
        column.widthDip = 120.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& cell) const override
    {
        static constexpr const wchar_t* kNames[]    = {L"Alpha", L"Beta", L"Gamma"};
        static constexpr const wchar_t* kStatuses[] = {L"Prête", L"Occupée", L"Libre"};
        cell.kind                                   = DxUi::GridCellKind::Text;
        cell.text                                   = columnIndex == 0u ? kNames[rowIndex] : kStatuses[rowIndex];
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId < 3u ? std::optional<size_t>(static_cast<size_t>(rowId)) : std::nullopt;
    }
};

// A client of the application's window, with the expectations a test makes of what it sees. `application` is the application's
// element, whose only child is the view's root element.
class EmbeddedClientWalk final
{
public:
    EmbeddedClientWalk(EmbeddedUiaTest::Bridge& bridge, UiaTest::Subscription subscription = {})
        : client(bridge.Hwnd(), std::move(subscription), [&bridge] { bridge.Pump(); }),
          application(client.Root())
    {
    }

    // An expectation of what the client sees: a failed one first reports the tree it walks and the events it heard.
    void Expect(bool condition, const char* text)
    {
        if (! condition)
        {
            std::cerr << "    [UIA] the client walks from the application's element:\n";
            for (const wchar_t unit : client.Dump(application, 4))
                std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
            client.PrintEvents();
        }
        Check(condition, text);
    }

    [[nodiscard]] std::wstring Name(UiaTest::ElementId element)
    {
        return client.Describe(element).name;
    }

    [[nodiscard]] std::vector<std::wstring> Names(const std::vector<UiaTest::ElementId>& elements)
    {
        std::vector<std::wstring> names;
        for (const UiaTest::ElementId element : elements)
            names.push_back(Name(element));
        return names;
    }

    // The names of `element` and of what follows it by next (or previous) sibling.
    [[nodiscard]] std::vector<std::wstring> Walk(std::optional<UiaTest::ElementId> element, UiaTest::Direction direction)
    {
        std::vector<std::wstring> names;
        for (size_t guard = 0u; element && guard < 64u; ++guard)
        {
            names.push_back(Name(*element));
            element = client.Navigate(*element, direction);
        }
        return names;
    }

    [[nodiscard]] bool HasParent(UiaTest::ElementId element, UiaTest::ElementId parent)
    {
        const std::optional<UiaTest::ElementId> found = client.Navigate(element, UiaTest::Direction::Parent);
        return found && client.Same(*found, parent);
    }

    UiaTest::Client client;
    UiaTest::ElementId application;
};

// A prepared view of one control, attached to an application window.
struct EmbeddedSingleControlView final
{
    template <typename Control> EmbeddedSingleControlView(GraphicsFixture& gpu, std::unique_ptr<Control> control, std::wstring_view name)
    {
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "pool for the single-control view");
        Hr(view.Attach(pool), "single-control view");
        control->SetBounds(D2D1::RectF(0, 0, 240, 96));
        control->SetAccessibleName(std::wstring(name));
        view.Controls().SetRoot(std::move(control));
        Hr(view.Prepare(320, 160, 96), "prepare the single-control view");
    }

    std::shared_ptr<DxUi::GraphicsDevice> pool;
    DxUi::EmbeddedHost view;
};

// The elements of a view's tree as its providers hand them out: the root's first child, then its next siblings.
static wil::com_ptr_nothrow<IRawElementProviderFragment> EmbeddedFirstChild(IRawElementProviderFragment& parent)
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> child;
    Hr(parent.Navigate(NavigateDirection_FirstChild, child.put()), "a provider navigates to its first child");
    Check(bool(child), "a provider has a first child");
    return child;
}

static wil::com_ptr_nothrow<IRawElementProviderFragment> EmbeddedNextSibling(IRawElementProviderFragment& element)
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> sibling;
    Hr(element.Navigate(NavigateDirection_NextSibling, sibling.put()), "a provider navigates to its next sibling");
    Check(bool(sibling), "a provider has a next sibling");
    return sibling;
}

static std::wstring EmbeddedElementName(IRawElementProviderFragment& element)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Hr(element.QueryInterface(IID_PPV_ARGS(simple.put())), "an element is a simple provider");
    VARIANT name{};
    VariantInit(&name);
    const auto clearName = wil::scope_exit([&] { VariantClear(&name); });
    Hr(simple->GetPropertyValue(UIA_NamePropertyId, &name), "an element has a name");
    Check(name.vt == VT_BSTR, "the name of an element is a string");
    return name.bstrVal ? std::wstring(name.bstrVal, SysStringLen(name.bstrVal)) : std::wstring{};
}

static std::function<bool(const UiaTest::HeardEvent&)> EmbeddedHeardEvent(EVENTID eventId, long controlType, std::wstring name)
{
    return [=](const UiaTest::HeardEvent& heard)
    {
        return heard.kind == UiaTest::EventKind::Automation && heard.id == static_cast<long>(eventId) && heard.controlType == controlType && heard.name == name;
    };
}

static std::function<bool(const UiaTest::HeardEvent&)> EmbeddedHeardProperty(PROPERTYID propertyId, long controlType, std::wstring name, std::wstring value)
{
    return [=](const UiaTest::HeardEvent& heard)
    {
        return heard.kind == UiaTest::EventKind::Property && heard.id == static_cast<long>(propertyId) && heard.controlType == controlType &&
               heard.name == name && heard.value == value;
    };
}

// Raises, from the test, the events the library will raise about an item, on the element the view hands out for it: what UI
// Automation does with an event is decided by the element's parents.
static void RaiseEmbeddedSelectionItemEvents(IRawElementProviderFragment& element, bool selected, EVENTID eventId)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Hr(element.QueryInterface(IID_PPV_ARGS(simple.put())), "an element is a simple provider");
    VARIANT before{};
    before.vt      = VT_BOOL;
    before.boolVal = selected ? VARIANT_FALSE : VARIANT_TRUE;
    VARIANT after{};
    after.vt      = VT_BOOL;
    after.boolVal = selected ? VARIANT_TRUE : VARIANT_FALSE;
    Hr(UiaRaiseAutomationPropertyChangedEvent(simple.get(), UIA_SelectionItemIsSelectedPropertyId, before, after), "raise the IsSelected change of an element");
    Hr(UiaRaiseAutomationEvent(simple.get(), eventId), "raise a selection event of an element");
}

__declspec(noinline) static void TestEmbeddedSingleTreeIsNavigableFromTheApplicationsElement(GraphicsFixture& gpu)
{
    using UiaTest::Direction;
    EmbeddedTreeModel model;
    auto control = std::make_unique<DxUi::Tree>();
    control->SetModel(&model);
    control->SetSelectedItemId(2u);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Catégories");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the single-tree view to the application window");
    EmbeddedClientWalk walk(bridge);
    walk.Expect(walk.client.Describe(walk.application).controlType == UIA_PaneControlTypeId, "the client reaches the application's element");

    const std::optional<UiaTest::ElementId> viewRoot = walk.client.Navigate(walk.application, Direction::FirstChild);
    walk.Expect(viewRoot && walk.client.Describe(*viewRoot).controlType == UIA_PaneControlTypeId && walk.HasParent(*viewRoot, walk.application),
                "the view's root element is the application element's child");
    const std::optional<UiaTest::ElementId> tree = viewRoot ? walk.client.Navigate(*viewRoot, Direction::FirstChild) : std::nullopt;
    walk.Expect(tree && walk.client.Describe(*tree).controlType == UIA_TreeControlTypeId && walk.HasParent(*tree, *viewRoot),
                "the tree's element is the view's root element's child");

    const std::vector<std::wstring> items{L"Général", L"Volets", L"Afficheurs"};
    const std::vector<std::wstring> reversed{L"Afficheurs", L"Volets", L"Général"};
    const std::optional<UiaTest::ElementId> first = walk.client.Navigate(*tree, Direction::FirstChild);
    walk.Expect(first && walk.Name(*first) == items.front(), "the first child of the tree's element is its first item");
    const std::optional<UiaTest::ElementId> last = walk.client.Navigate(*tree, Direction::LastChild);
    walk.Expect(last && walk.Name(*last) == items.back(), "the last child of the tree's element is its last item");
    walk.Expect(walk.Walk(first, Direction::NextSibling) == items, "the items follow one another by next sibling");
    walk.Expect(walk.Walk(last, Direction::PreviousSibling) == reversed, "the items follow one another by previous sibling");
    const std::vector<UiaTest::ElementId> children = walk.client.Children(*tree);
    walk.Expect(walk.Names(children) == items, "a search of the tree element's children finds its items");
    for (const UiaTest::ElementId item : children)
    {
        walk.Expect(walk.client.Describe(item).controlType == UIA_TreeItemControlTypeId, "a child of the tree's element is a tree item");
        walk.Expect(walk.HasParent(item, *tree), "the parent of a tree item is the tree's element");
        const std::optional<UiaTest::ElementId> container = walk.client.SelectionContainer(item);
        walk.Expect(container && walk.client.Same(*container, *tree), "the selection container of a tree item is the tree's element");
    }
    const std::vector<UiaTest::ElementId> selection = walk.client.Selection(*tree);
    walk.Expect(selection.size() == 1u && walk.Name(selection.front()) == L"Volets" && walk.HasParent(selection.front(), *tree),
                "the selected item the tree's element reports is a child of that element");
}

__declspec(noinline) static void TestEmbeddedSingleGridIsNavigableFromTheApplicationsElement(GraphicsFixture& gpu)
{
    using UiaTest::Direction;
    EmbeddedStatusGridModel model;
    auto control = std::make_unique<DxUi::Grid>();
    control->SetModel(&model);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Résultats");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the single-grid view to the application window");
    EmbeddedClientWalk walk(bridge);

    const std::optional<UiaTest::ElementId> viewRoot = walk.client.Navigate(walk.application, Direction::FirstChild);
    walk.Expect(viewRoot && walk.HasParent(*viewRoot, walk.application), "the view's root element is the application element's child");
    const std::optional<UiaTest::ElementId> grid = viewRoot ? walk.client.Navigate(*viewRoot, Direction::FirstChild) : std::nullopt;
    walk.Expect(grid && walk.client.Describe(*grid).controlType == UIA_DataGridControlTypeId && walk.HasParent(*grid, *viewRoot),
                "the grid's element is the view's root element's child");

    const std::vector<std::wstring> parts{L"Nom", L"État", L"Alpha | Prête", L"Beta | Occupée", L"Gamma | Libre"};
    const std::vector<std::wstring> reversed(parts.rbegin(), parts.rend());
    const std::optional<UiaTest::ElementId> first = walk.client.Navigate(*grid, Direction::FirstChild);
    walk.Expect(first && walk.Name(*first) == L"Nom", "the first child of the grid's element is its first header");
    const std::optional<UiaTest::ElementId> last = walk.client.Navigate(*grid, Direction::LastChild);
    walk.Expect(last && walk.Name(*last) == L"Gamma | Libre", "the last child of the grid's element is its last row");
    walk.Expect(walk.Walk(first, Direction::NextSibling) == parts, "the headers and the rows follow one another by next sibling");
    walk.Expect(walk.Walk(last, Direction::PreviousSibling) == reversed, "the rows and the headers follow one another by previous sibling");
    const std::vector<UiaTest::ElementId> children = walk.client.Children(*grid);
    walk.Expect(walk.Names(children) == parts, "a search of the grid element's children finds its headers and rows");
    for (const UiaTest::ElementId child : children)
        walk.Expect(walk.HasParent(child, *grid), "the parent of a grid header or row is the grid's element");

    const UiaTest::ElementId row                = children[3];
    const std::vector<UiaTest::ElementId> cells = walk.client.Children(row);
    walk.Expect(walk.Names(cells) == std::vector<std::wstring>({L"Beta", L"Occupée"}), "a grid row's children are its cells");
    for (const UiaTest::ElementId cell : cells)
    {
        walk.Expect(walk.HasParent(cell, row), "the parent of a grid cell is its row");
        const std::optional<UiaTest::ElementId> containing = walk.client.ContainingGrid(cell);
        walk.Expect(containing && walk.client.Same(*containing, *grid), "the containing grid of a grid cell is the grid's element");
    }
}

__declspec(noinline) static void TestEmbeddedSingleTreeEventsReachAClientSubscribedToTheApplicationsWindow(GraphicsFixture& gpu)
{
    EmbeddedTreeModel model;
    auto control     = std::make_unique<DxUi::Tree>();
    DxUi::Tree* tree = control.get();
    control->SetModel(&model);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Catégories");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the single-tree view to the application window");
    UiaTest::Subscription subscription;
    subscription.automationEvents = {UIA_SelectionItem_ElementSelectedEventId,
                                     UIA_SelectionItem_ElementAddedToSelectionEventId,
                                     UIA_SelectionItem_ElementRemovedFromSelectionEventId,
                                     UIA_Selection_InvalidatedEventId};
    subscription.properties       = {UIA_SelectionItemIsSelectedPropertyId, UIA_HasKeyboardFocusPropertyId};
    subscription.structure        = true;
    subscription.focus            = true;
    EmbeddedClientWalk walk(bridge, std::move(subscription));

    // What the view raises when it publishes a change: the tree takes the focus, and the application reports it has the window's.
    test.view.Controls().SetFocusControl(tree);
    Hr(test.view.Prepare(320, 160, 96), "prepare the focused tree");
    Hr(bridge.Update(), "publish the focused tree");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardProperty(UIA_HasKeyboardFocusPropertyId, UIA_TreeControlTypeId, L"Catégories", L"true")),
                "a client subscribed to the application's window hears the tree take the keyboard focus");
    walk.Expect(walk.client.WaitForEvent([](const UiaTest::HeardEvent& heard)
    { return heard.kind == UiaTest::EventKind::Focus && heard.controlType == UIA_TreeControlTypeId && heard.name == L"Catégories"; }),
                "a client hears the focus change the view raises for its tree");

    // The events raised on the tree's items, on the elements the view hands out for them.
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> viewRoot;
    Hr(test.view.GetAccessibilityProvider(viewRoot.put()), "the view's root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> viewRootFragment;
    Hr(viewRoot.query_to(viewRootFragment.put()), "the view's root is a fragment");
    const auto treeElement = EmbeddedFirstChild(*viewRootFragment.get());
    const auto first       = EmbeddedFirstChild(*treeElement.get());
    const auto second      = EmbeddedNextSibling(*first.get());
    const auto third       = EmbeddedNextSibling(*second.get());
    Check(EmbeddedElementName(*second.get()) == L"Volets" && EmbeddedElementName(*third.get()) == L"Afficheurs", "the view's providers name the tree's items");
    RaiseEmbeddedSelectionItemEvents(*second.get(), true, UIA_SelectionItem_ElementSelectedEventId);
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementSelectedEventId, UIA_TreeItemControlTypeId, L"Volets")),
                "a client subscribed to the application's window hears the ElementSelected event raised on a tree item");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardProperty(UIA_SelectionItemIsSelectedPropertyId, UIA_TreeItemControlTypeId, L"Volets", L"true")),
                "a client subscribed to the application's window hears the IsSelected change raised on a tree item");
    RaiseEmbeddedSelectionItemEvents(*third.get(), true, UIA_SelectionItem_ElementAddedToSelectionEventId);
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementAddedToSelectionEventId, UIA_TreeItemControlTypeId, L"Afficheurs")),
                "a client subscribed to the application's window hears the ElementAddedToSelection event raised on a tree item");
    RaiseEmbeddedSelectionItemEvents(*first.get(), false, UIA_SelectionItem_ElementRemovedFromSelectionEventId);
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementRemovedFromSelectionEventId, UIA_TreeItemControlTypeId, L"Général")),
                "a client subscribed to the application's window hears the ElementRemovedFromSelection event raised on a tree item");

    wil::com_ptr_nothrow<IRawElementProviderSimple> treeSimple;
    Hr(treeElement.query_to(treeSimple.put()), "the tree's element is a simple provider");
    Hr(UiaRaiseAutomationEvent(treeSimple.get(), UIA_Selection_InvalidatedEventId), "raise the invalidation of the tree's selection");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_Selection_InvalidatedEventId, UIA_TreeControlTypeId, L"Catégories")),
                "a client subscribed to the application's window hears the selection invalidation raised on the tree");
}

__declspec(noinline) static void TestEmbeddedSingleGridEventsReachAClientSubscribedToTheApplicationsWindow(GraphicsFixture& gpu)
{
    EmbeddedStatusGridModel model;
    auto control = std::make_unique<DxUi::Grid>();
    control->SetModel(&model);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Résultats");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the single-grid view to the application window");
    UiaTest::Subscription subscription;
    subscription.automationEvents = {UIA_SelectionItem_ElementSelectedEventId, UIA_Selection_InvalidatedEventId};
    subscription.properties       = {UIA_SelectionItemIsSelectedPropertyId};
    EmbeddedClientWalk walk(bridge, std::move(subscription));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> viewRoot;
    Hr(test.view.GetAccessibilityProvider(viewRoot.put()), "the view's root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> viewRootFragment;
    Hr(viewRoot.query_to(viewRootFragment.put()), "the view's root is a fragment");
    const auto gridElement = EmbeddedFirstChild(*viewRootFragment.get());
    // The grid's children are its headers, then its rows: the second row is the fourth child.
    auto child = EmbeddedFirstChild(*gridElement.get());
    for (size_t index = 0u; index < 3u; ++index)
        child = EmbeddedNextSibling(*child.get());
    Check(EmbeddedElementName(*child.get()) == L"Beta | Occupée", "the view's providers name the grid's second row");
    RaiseEmbeddedSelectionItemEvents(*child.get(), true, UIA_SelectionItem_ElementSelectedEventId);
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementSelectedEventId, UIA_DataItemControlTypeId, L"Beta | Occupée")),
                "a client subscribed to the application's window hears the ElementSelected event raised on a grid row");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardProperty(UIA_SelectionItemIsSelectedPropertyId, UIA_DataItemControlTypeId, L"Beta | Occupée", L"true")),
                "a client subscribed to the application's window hears the IsSelected change raised on a grid row");

    // A cell is a child of its row: an event raised on it reaches the client too.
    const auto cell = EmbeddedFirstChild(*child.get());
    wil::com_ptr_nothrow<IRawElementProviderSimple> cellSimple;
    Hr(cell.query_to(cellSimple.put()), "a grid cell is a simple provider");
    Hr(UiaRaiseAutomationEvent(cellSimple.get(), UIA_SelectionItem_ElementSelectedEventId), "raise an event on a grid cell");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementSelectedEventId, UIA_TextControlTypeId, L"Beta")),
                "a client subscribed to the application's window hears an event raised on a grid cell");
}

// A tree whose rows a test removes: a selected row that leaves it can only be reported as the tree's selection being invalidated.
class EmbeddedShrinkingTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return count;
    }

    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
    {
        static constexpr const wchar_t* kNames[] = {L"Général", L"Volets", L"Afficheurs", L"Barres"};
        item.id                                  = visibleIndex + 1u;
        item.text                                = kNames[visibleIndex];
    }

    size_t count = 4u;
};

// The selection events a multi-select tree's view raises itself, when the application publishes a change (EmbeddedHost::
// UpdateAccessibility), as a client subscribed to the application's window hears them: through the chain from the item to the
// application's element, which the item's events need to reach it.
__declspec(noinline) static void TestEmbeddedMultiSelectTreeRaisesItsSelectionEventsToAClientOfTheApplicationsWindow(GraphicsFixture& gpu)
{
    EmbeddedShrinkingTreeModel model;
    auto control     = std::make_unique<DxUi::Tree>();
    DxUi::Tree* tree = control.get();
    control->SetModel(&model);
    control->SetMultiSelectEnabled(true);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Catégories");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the multi-select tree view to the application window");
    UiaTest::Subscription subscription;
    subscription.automationEvents = {UIA_SelectionItem_ElementSelectedEventId,
                                     UIA_SelectionItem_ElementAddedToSelectionEventId,
                                     UIA_SelectionItem_ElementRemovedFromSelectionEventId,
                                     UIA_Selection_InvalidatedEventId};
    subscription.properties       = {UIA_SelectionItemIsSelectedPropertyId};
    EmbeddedClientWalk walk(bridge, std::move(subscription));
    // What an application does after it changed the control: repaints it (the view prepares), then publishes the change. The
    // selection setters are silent, so the view is told to repaint; an unchanged view would prepare nothing and publish nothing.
    const auto publish = [&](const char* text)
    {
        test.view.Controls().Invalidate();
        Check(test.view.Prepare(320, 160, 96) == S_OK, text);
        Check(bridge.Update() == S_OK, text);
    };

    // A selection that became one new item is that item being selected.
    tree->SetSelectedItemIds(std::vector<uint64_t>{2u});
    publish("publish a selection of one item");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementSelectedEventId, UIA_TreeItemControlTypeId, L"Volets")),
                "a client subscribed to the application's window hears the ElementSelected event the view raises for a tree item");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardProperty(UIA_SelectionItemIsSelectedPropertyId, UIA_TreeItemControlTypeId, L"Volets", L"true")),
                "a client hears the IsSelected change the view raises for a tree item that became selected");

    // A publish that changed no selection raises none, though the tree has one. The client hears each event again a moment after
    // the first time, so the stream is waited out (nothing arriving for a second) before and after the publish.
    const auto waitOutTheStream = [&]
    {
        size_t arrived        = walk.client.Events().size();
        ULONGLONG lastArrival = GetTickCount64();
        static_cast<void>(walk.client.WaitUntil(10000u,
                                                [&]
        {
            if (const size_t now = walk.client.Events().size(); now != arrived)
            {
                arrived     = now;
                lastArrival = GetTickCount64();
            }
            return GetTickCount64() - lastArrival >= 1000u;
        }));
        return arrived;
    };
    const size_t heardBefore = waitOutTheStream();
    publish("publish an unchanged selection");
    walk.Expect(waitOutTheStream() == heardBefore, "a publish that changed no selection raises no selection event");

    // An item added to a selection is added, and one taken out is removed.
    Check(tree->RequestAddVisibleItemToSelection(2u), "add the third item to the selection");
    publish("publish an item added to the selection");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementAddedToSelectionEventId, UIA_TreeItemControlTypeId, L"Afficheurs")),
                "a client hears the ElementAddedToSelection event the view raises for a tree item");
    Check(tree->RequestRemoveVisibleItemFromSelection(1u), "remove the second item from the selection");
    publish("publish an item removed from the selection");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementRemovedFromSelectionEventId, UIA_TreeItemControlTypeId, L"Volets")),
                "a client hears the ElementRemovedFromSelection event the view raises for a tree item");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardProperty(UIA_SelectionItemIsSelectedPropertyId, UIA_TreeItemControlTypeId, L"Volets", L"false")),
                "a client hears the IsSelected change the view raises for a tree item that left the selection");

    // A selected item that left the tree cannot be named: the selection of the tree itself is invalidated.
    model.count = 2u;
    tree->NotifyDataChanged();
    publish("publish a selected item that left the tree");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_Selection_InvalidatedEventId, UIA_TreeControlTypeId, L"Catégories")),
                "a client hears the Selection_Invalidated event the view raises for its tree");
}

__declspec(noinline) static void TestEmbeddedUiaEventHarness(GraphicsFixture& gpu)
{
    TestEmbeddedSingleTreeIsNavigableFromTheApplicationsElement(gpu);
    TestEmbeddedSingleGridIsNavigableFromTheApplicationsElement(gpu);
    TestEmbeddedSingleTreeEventsReachAClientSubscribedToTheApplicationsWindow(gpu);
    TestEmbeddedSingleGridEventsReachAClientSubscribedToTheApplicationsWindow(gpu);
    TestEmbeddedMultiSelectTreeRaisesItsSelectionEventsToAClientOfTheApplicationsWindow(gpu);
}
