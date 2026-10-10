#include "../Support/Support.Tests.SelectionEventInterruption.h"
#include "../Support/Support.Tests.UiaTestClient.h"
#include "Embedded.Tests.EmbeddedUiaBridge.h"

#include <string_view>

// An embedded view whose only control is a Tree or a Grid, exposed to UI Automation through an application window (see
// Embedded.Tests.EmbeddedUiaBridge.h), and a UI Automation client of that window: the walk from the application's element through the view's
// root and its control to the control's items and back, and the selection events the view raises itself when the application
// publishes a change (a tree's single selection or multi-selection, a grid's rows). The embedded host never collapses its root
// into the control (its root element is the application element's child, and the control's elements have it for their parent),
// so these tests guard the chain the window host's collapsed root must match, and they prove the harness: a client that hears
// the events an embedded view raises, and walks the tree its providers expose.

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

// Thirty numbered rows in one column, "Ligne 1" to "Ligne 30", whose stable ids are their indices.
class EmbeddedNumberedGridModel final : public DxUi::IGridModel
{
public:
    static constexpr size_t kRows = 30u;

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return kRows;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"name";
        column.title    = L"Nom";
        column.widthDip = 200.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t /*columnIndex*/, DxUi::GridCellData& cell) const override
    {
        cell.kind = DxUi::GridCellKind::Text;
        cell.text = L"Ligne " + std::to_wstring(rowIndex + 1u);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId < kRows ? std::optional<size_t>(static_cast<size_t>(rowId)) : std::nullopt;
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

    // Requires that the client heard `expected`, no more and no less, for a step: what UiaTest::HearSelectionEvents returned.
    void ExpectHeard(const std::vector<std::wstring>& heard, std::vector<std::wstring> expected, const char* step)
    {
        expected = UiaTest::SortedEvents(std::move(expected));
        if (heard == expected)
            return;
        const std::string message =
            std::string(step) + ": the client heard" + UiaTest::DescribeEventList(heard) + ", not" + UiaTest::DescribeEventList(expected);
        Expect(false, message.c_str());
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

// What an application does after it changed a view's controls: repaints them (the view prepares) and publishes the change, whose
// events the view raises then. The selection setters are silent, so the view is told to repaint.
static void PublishEmbeddedView(EmbeddedSingleControlView& test, EmbeddedUiaTest::Bridge& bridge, const char* text)
{
    test.view.Controls().Invalidate();
    Check(test.view.Prepare(320, 160, 96) == S_OK, text);
    Check(bridge.Update() == S_OK, text);
}

// A click on a prepared view at a point in its DIPs, which are its pixels at 96 DPI, with the modifiers of the press (MK_CONTROL,
// MK_SHIFT).
static void ClickEmbeddedView(DxUi::EmbeddedHost& view, D2D1_POINT_2F point, UINT modifiers = 0u)
{
    Check(view.DispatchPointer({DxUi::PointerAction::Down, point.x, point.y, modifiers}), "the view takes a press");
    static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, point.x, point.y, modifiers}));
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

// The events a view of a tree with a single selection raises itself when the application publishes a change, as a client
// subscribed to the application's window hears them: the tree taking the keyboard focus the application reports, then its
// selection moved by a click, a key and the application (the item becomes selected, which says that the other left the selection,
// and each item whose state changed reports its IsSelected change) and cleared (the item is removed).
__declspec(noinline) static void TestEmbeddedSingleTreeEventsReachAClientSubscribedToTheApplicationsWindow(GraphicsFixture& gpu)
{
    EmbeddedTreeModel model;
    auto control     = std::make_unique<DxUi::Tree>();
    DxUi::Tree* tree = control.get();
    control->SetModel(&model);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Catégories");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the single-tree view to the application window");
    UiaTest::Subscription subscription = UiaTest::SelectionEventsSubscription();
    subscription.properties.push_back(UIA_HasKeyboardFocusPropertyId);
    subscription.structure = true;
    subscription.focus     = true;
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

    // The tree's selection: each step changes it, the application publishes, and the view raises the events.
    const auto hear = [&](size_t expected, const char* step, const std::function<void()>& change)
    {
        return UiaTest::HearSelectionEvents(walk.client,
                                            expected,
                                            [&]
        {
            change();
            PublishEmbeddedView(test, bridge, step);
        });
    };
    const auto item  = [](std::wstring_view what, std::wstring_view name) { return std::wstring(what) + L" TreeItem '" + std::wstring(name) + L"'"; };
    const auto click = [&](size_t visibleIndex)
    {
        const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(visibleIndex);
        Check(rect.has_value(), "the tree has a rectangle for the item");
        ClickEmbeddedView(test.view, D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f));
    };
    walk.ExpectHeard(hear(2u, "a click on an item", [&] { click(2u); }),
                     {item(L"Selected", L"Afficheurs"), item(L"IsSelected:true", L"Afficheurs")},
                     "a click on an item of an empty selection");
    walk.ExpectHeard(hear(3u, "Up", [&] { Check(test.view.DispatchKey(VK_UP, true), "the tree takes Up"); }),
                     {item(L"Selected", L"Volets"), item(L"IsSelected:true", L"Volets"), item(L"IsSelected:false", L"Afficheurs")},
                     "Up");
    walk.ExpectHeard(hear(3u, "the application selecting an item", [&] { tree->SetSelectedItemId(1u); }),
                     {item(L"Selected", L"Général"), item(L"IsSelected:true", L"Général"), item(L"IsSelected:false", L"Volets")},
                     "the application selecting an item");
    walk.ExpectHeard(hear(2u, "the application clearing the selection", [&] { tree->SetSelectedItemId(std::nullopt); }),
                     {item(L"Removed", L"Général"), item(L"IsSelected:false", L"Général")},
                     "the application clearing the selection");
}

// The events a view of a grid raises itself when the application publishes a change of its selection, as a client subscribed to
// the application's window hears them: a click selects a row, Ctrl+click adds one, Shift+click selects the range from the anchor
// (each row that joined is added and each that left removed), clearing removes the rows, and Ctrl+A is more changes than a client
// is told of one by one, an invalidation of the grid's selection. A cell is a child of its row: an event raised on it reaches the
// client too.
__declspec(noinline) static void TestEmbeddedSingleGridEventsReachAClientSubscribedToTheApplicationsWindow(GraphicsFixture& gpu)
{
    EmbeddedNumberedGridModel model;
    auto control     = std::make_unique<DxUi::Grid>();
    DxUi::Grid* grid = control.get();
    control->SetModel(&model);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Résultats");
    grid->SetBounds(D2D1::RectF(0, 0, 240, 160)); // Four rows on screen.
    Hr(test.view.Prepare(320, 160, 96), "prepare the grid at its full height");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the single-grid view to the application window");
    EmbeddedClientWalk walk(bridge, UiaTest::SelectionEventsSubscription());

    const auto hear = [&](size_t expected, const char* step, const std::function<void()>& change)
    {
        return UiaTest::HearSelectionEvents(walk.client,
                                            expected,
                                            [&]
        {
            change();
            PublishEmbeddedView(test, bridge, step);
        });
    };
    const auto row   = [](std::wstring_view what, size_t number) { return std::wstring(what) + L" DataItem 'Ligne " + std::to_wstring(number) + L"'"; };
    const auto click = [&](size_t number, UINT modifiers = 0u)
    {
        const std::optional<D2D1_RECT_F> cell = grid->GetVisibleCellRect(number - 1u, 0u);
        Check(cell.has_value(), "the row to click is on screen");
        ClickEmbeddedView(test.view, D2D1::Point2F((cell->left + cell->right) * 0.5f, (cell->top + cell->bottom) * 0.5f), modifiers);
    };
    walk.ExpectHeard(hear(2u, "a click on a row", [&] { click(1u); }), {row(L"Selected", 1u), row(L"IsSelected:true", 1u)}, "a click on a row");
    walk.ExpectHeard(hear(2u, "Ctrl+click", [&] { click(3u, MK_CONTROL); }), {row(L"Added", 3u), row(L"IsSelected:true", 3u)}, "Ctrl+click");
    walk.ExpectHeard(hear(4u, "Shift+click", [&] { click(2u, MK_SHIFT); }),
                     {row(L"Removed", 1u), row(L"IsSelected:false", 1u), row(L"Added", 2u), row(L"IsSelected:true", 2u)},
                     "Shift+click from the last Ctrl+click anchor, Ligne 3");
    walk.ExpectHeard(hear(4u,
                          "clearing the selection",
                          [&]
    {
        grid->GetSelectionModel().Clear();
        grid->NotifyDataChanged();
    }),
                     {row(L"Removed", 2u), row(L"Removed", 3u), row(L"IsSelected:false", 2u), row(L"IsSelected:false", 3u)},
                     "clearing the selection");
    walk.ExpectHeard(hear(1u,
                          "Ctrl+A",
                          [&]
    {
        test.view.Controls().SetFocusControl(grid);
        Check(test.view.DispatchKey('A', true, MK_CONTROL), "the grid takes Ctrl+A");
    }),
                     {L"Invalidated DataGrid 'Résultats'"},
                     "Ctrl+A");

    // A cell is a child of its row: an event raised on it reaches the client too. The grid's children are its header, then its
    // rows: the second row is the third child.
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> viewRoot;
    Hr(test.view.GetAccessibilityProvider(viewRoot.put()), "the view's root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> viewRootFragment;
    Hr(viewRoot.query_to(viewRootFragment.put()), "the view's root is a fragment");
    const auto gridElement = EmbeddedFirstChild(*viewRootFragment.get());
    auto child             = EmbeddedFirstChild(*gridElement.get());
    for (size_t index = 0u; index < 2u; ++index)
        child = EmbeddedNextSibling(*child.get());
    Check(EmbeddedElementName(*child.get()) == L"Ligne 2", "the view's providers name the grid's second row");
    const auto cell = EmbeddedFirstChild(*child.get());
    wil::com_ptr_nothrow<IRawElementProviderSimple> cellSimple;
    Hr(cell.query_to(cellSimple.put()), "a grid cell is a simple provider");
    Hr(UiaRaiseAutomationEvent(cellSimple.get(), UIA_SelectionItem_ElementSelectedEventId), "raise an event on a grid cell");
    walk.Expect(walk.client.WaitForEvent(EmbeddedHeardEvent(UIA_SelectionItem_ElementSelectedEventId, UIA_TextControlTypeId, L"Ligne 2")),
                "a client subscribed to the application's window hears an event raised on a grid cell");
}

// When the view whose selection events are being raised is hidden (which disconnects its accessibility) or its root replaced, by
// something that runs while they are raised (see UiaTest::SelectionEventInterruption), the raising ends there. Each case adds two
// rows to a selection, four events.
__declspec(noinline) static void TestEmbeddedSelectionEventsEndWhenTheViewLeavesWhileTheyAreRaised(GraphicsFixture& gpu)
{
    const auto run = [&](const char* what, bool replaceTheRoot)
    {
        EmbeddedNumberedGridModel model;
        auto control     = std::make_unique<DxUi::Grid>();
        DxUi::Grid* grid = control.get();
        control->SetModel(&model);
        EmbeddedSingleControlView test(gpu, std::move(control), L"Résultats");
        EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
        Hr(bridge.Attach(), "attach the grid view to the application window");
        EmbeddedClientWalk walk(bridge, UiaTest::SelectionEventsSubscription()); // A client listens, so the view raises them.
        grid->GetSelectionModel().SetSingle(0u);
        PublishEmbeddedView(test, bridge, "publish a selection of one row");
        std::vector<uint64_t> rows;
        for (uint64_t rowId = 0u; rowId < EmbeddedNumberedGridModel::kRows; ++rowId)
            rows.push_back(rowId);
        auto replacement = std::make_unique<DxUi::Panel>();
        size_t events    = 0u;
        {
            UiaTest::SelectionEventInterruption interruption([&]
            {
                if (replaceTheRoot)
                    test.view.Controls().SetRoot(std::move(replacement));
                else
                    test.view.SetVisible(false);
            });
            grid->GetSelectionModel().SetRange(rows, 0u, 2u);
            test.view.Controls().Invalidate();
            Check(test.view.Prepare(320, 160, 96) == S_OK, "prepare two rows added to the selection");
            static_cast<void>(bridge.Update()); // The grid may be gone by now.
            events = interruption.Events();
        }
        Check(events == 1u, what);
    };
    run("hiding the view while its grid's selection events are raised ends them", false);
    run("replacing the view's root while its grid's selection events are raised ends them", true);
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

__declspec(noinline) static void TestEmbeddedRowFocusChangesReachSubscribedClient(GraphicsFixture& gpu)
{
    const auto heardFocus = [](long type, std::wstring name)
    { return [=](const UiaTest::HeardEvent& event) { return event.kind == UiaTest::EventKind::Focus && event.controlType == type && event.name == name; }; };
    {
        EmbeddedTreeModel model;
        auto control = std::make_unique<DxUi::Tree>();
        auto* tree   = control.get();
        tree->SetModel(&model);
        tree->SetMultiSelectEnabled(true);
        tree->SetSelectedItemIds(std::vector<uint64_t>{1u, 2u});
        tree->SetFocusedItemId(1u);
        EmbeddedSingleControlView test(gpu, std::move(control), L"Categories");
        EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
        Hr(bridge.Attach(), "attach the row-focus tree");
        UiaTest::Subscription subscription;
        subscription.focus = true;
        EmbeddedClientWalk walk(bridge, std::move(subscription));
        test.view.Controls().SetFocusControl(tree);
        PublishEmbeddedView(test, bridge, "publish tree focus");
        walk.Expect(walk.client.WaitForEvent(heardFocus(UIA_TreeItemControlTypeId, L"Général")), "gaining tree focus announces the focused item");
        tree->SetFocusedItemId(2u);
        PublishEmbeddedView(test, bridge, "publish a focus-only move within the selected tree items");
        walk.Expect(walk.client.WaitForEvent(heardFocus(UIA_TreeItemControlTypeId, L"Volets")), "a focus-only tree move announces its item");
        Check(tree->GetSelectedItemIds().size() == 2u, "focus events do not change the multi-selection");
    }
    {
        EmbeddedNumberedGridModel model;
        auto control = std::make_unique<DxUi::Grid>();
        auto* grid   = control.get();
        grid->SetModel(&model);
        Check(grid->RequestSelectRow(2u, 0u), "select the initial grid row");
        EmbeddedSingleControlView test(gpu, std::move(control), L"Results");
        EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
        Hr(bridge.Attach(), "attach the row-focus grid");
        UiaTest::Subscription subscription;
        subscription.focus = true;
        EmbeddedClientWalk walk(bridge, std::move(subscription));
        test.view.Controls().SetFocusControl(grid);
        PublishEmbeddedView(test, bridge, "publish grid focus");
        walk.Expect(walk.client.WaitForEvent(heardFocus(UIA_DataItemControlTypeId, L"Ligne 3")), "gaining grid focus announces the focused row");
        Check(grid->RequestSelectRow(1u, MK_SHIFT), "extend a grid range upward");
        PublishEmbeddedView(test, bridge, "publish the moving grid range endpoint");
        walk.Expect(walk.client.WaitForEvent(heardFocus(UIA_DataItemControlTypeId, L"Ligne 2")), "a grid range announces its moving endpoint");
        walk.client.Settle();
        Hr(bridge.Update(false), "the application loses keyboard focus");
        walk.client.Settle();
        const auto rowFocusCount = [&]
        {
            return std::ranges::count_if(walk.client.Events(), [](const UiaTest::HeardEvent& event) {
                return event.kind == UiaTest::EventKind::Focus && event.controlType == UIA_DataItemControlTypeId && event.name.starts_with(L"Ligne ");
            });
        };
        const auto eventsBefore = rowFocusCount();
        Check(grid->RequestSelectRow(0u, 0u), "change a background grid row");
        PublishEmbeddedView(test, bridge, "publish the background row");
        walk.client.Settle();
        walk.Expect(rowFocusCount() == eventsBefore, "a view without application keyboard focus raises no row focus event");
        Hr(bridge.Update(true), "the application regains keyboard focus");
        walk.Expect(walk.client.WaitForEvent(heardFocus(UIA_DataItemControlTypeId, L"Ligne 1")), "regaining focus announces the current grid row");
    }
}

__declspec(noinline) static void TestEmbeddedRangeMetadataChangesWakeAndReachSubscribedClient(GraphicsFixture& gpu)
{
    auto control = std::make_unique<DxUi::Slider>();
    auto* slider = control.get();
    slider->SetValue(42.0);
    EmbeddedSingleControlView test(gpu, std::move(control), L"Opacity");
    EmbeddedUiaTest::Bridge bridge(test.view, 320, 160);
    Hr(bridge.Attach(), "attach the range-metadata slider");
    UiaTest::Subscription subscription;
    subscription.properties = {UIA_RangeValueValuePropertyId,
                               UIA_RangeValueMinimumPropertyId,
                               UIA_RangeValueMaximumPropertyId,
                               UIA_RangeValueSmallChangePropertyId,
                               UIA_RangeValueLargeChangePropertyId};
    EmbeddedClientWalk walk(bridge, std::move(subscription));
    const auto publish = [&]
    {
        Check(test.view.Prepare(320, 160, 96) == S_OK, "a range setter requests changed preparation without an explicit invalidation");
        Hr(bridge.Update(), "publish the changed range metadata");
    };
    const auto expect = [&](PROPERTYID property, double value)
    {
        walk.Expect(walk.client.WaitForEvent(EmbeddedHeardProperty(property, UIA_SliderControlTypeId, L"Opacity", std::to_wstring(value))),
                    "an application-window subscriber receives the embedded range property change");
    };
    slider->SetMinimum(44.0);
    publish();
    expect(UIA_RangeValueValuePropertyId, 44.0);
    expect(UIA_RangeValueMinimumPropertyId, 44.0);
    slider->SetMaximum(45.0);
    publish();
    expect(UIA_RangeValueMaximumPropertyId, 45.0);
    slider->SetStep(12.0);
    publish();
    expect(UIA_RangeValueSmallChangePropertyId, 12.0);
    expect(UIA_RangeValueLargeChangePropertyId, 12.0);
    slider->SetLargeStep(15.0);
    publish();
    expect(UIA_RangeValueLargeChangePropertyId, 15.0);
}

__declspec(noinline) static void TestEmbeddedUiaEventHarness(GraphicsFixture& gpu)
{
    TestEmbeddedSingleTreeIsNavigableFromTheApplicationsElement(gpu);
    TestEmbeddedSingleGridIsNavigableFromTheApplicationsElement(gpu);
    TestEmbeddedSingleTreeEventsReachAClientSubscribedToTheApplicationsWindow(gpu);
    TestEmbeddedSingleGridEventsReachAClientSubscribedToTheApplicationsWindow(gpu);
    TestEmbeddedMultiSelectTreeRaisesItsSelectionEventsToAClientOfTheApplicationsWindow(gpu);
    TestEmbeddedSelectionEventsEndWhenTheViewLeavesWhileTheyAreRaised(gpu);
    TestEmbeddedRowFocusChangesReachSubscribedClient(gpu);
    TestEmbeddedRangeMetadataChangesWakeAndReachSubscribedClient(gpu);
}
