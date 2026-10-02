#include "DxUiTestHelpers.h"

#include <clocale>
#include <cmath>
#include <fstream>
#include <iterator>

namespace
{

void TestTreeLocalizedEmptyStateRepaintsWithoutSelectionChange()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0, 0, 300, 140));
    window.Host().SetRoot(std::move(root));
    WindowHostBitmapCapture english;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    Require(window.Host().DebugCaptureBitmap(english), "capture default empty Tree");
    std::wstring translation = L"Aucune donnée";
    tree->SetEmptyStateText(translation);
    translation.clear();
    WindowHostBitmapCapture french;
    Require(window.Host().DebugCaptureBitmap(french), "capture translated empty Tree");
    Require(tree->GetEmptyStateText() == L"Aucune donnée", "Tree owns its translated empty-state text");
    Require(CompareWindowHostBitmapCapturesForTest(french, english).differingPixels > 10, "empty-state translation reaches Tree painting");
    Require(! tree->GetSelectedItemId().has_value(), "language change does not synthesize a Tree selection");
    tree->SetEmptyStateText({});
    WindowHostBitmapCapture restored;
    Require(window.Host().DebugCaptureBitmap(restored), "capture restored default empty Tree");
    Require(tree->GetEmptyStateText() == L"No data" && CompareWindowHostBitmapCapturesForTest(restored, english).differingPixels == 0,
            "clearing the override restores default Tree text and layout");
}

void TestTreePointerSelectionNotifiesDelegate()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 10u, .text = L"General"},
        TreeItemData{.id = 20u, .text = L"Viewers"},
        TreeItemData{.id = 30u, .text = L"Themes"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(60, 44), handled));
    Require(handled, "tree pointer row selection is handled");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(60, 44), handled));

    Require(tree->GetSelectedItemId().has_value(), "tree pointer selection produces a selected item");
    Require(tree->GetSelectedItemId().value() == 20u, "tree pointer selection targets the clicked row");
    Require(delegate.selectionChangedCount == 1u, "tree pointer selection notifies the delegate");
    Require(delegate.lastSelectedItemId == 20u, "tree selection delegate receives the clicked item id");
    Require(host.GetFocusControl() == tree, "tree pointer selection focuses the tree");
}

void TestTreeExpanderClickRequestsToggle()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"Plugins", .hasChildren = true, .expanded = false},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(14, 16), handled));
    Require(handled, "tree expander click is handled");

    Require(tree->GetSelectedItemId().has_value(), "tree expander click selects the parent item");
    Require(tree->GetSelectedItemId().value() == 1u, "tree expander click keeps the parent selected");
    Require(delegate.selectionChangedCount == 1u, "tree expander click notifies selection");
    Require(delegate.toggleCount == 1u, "tree expander click requests expansion toggle");
    Require(delegate.lastToggledItemId == 1u, "tree expander click toggles the clicked item");
    Require(delegate.lastExpandedState, "tree expander click requests expansion");
}

void TestTreeExpanderReResolvesStableItemAfterSelectionReorder()
{
    using namespace DxUi;

    class ReorderingDelegate final : public ITreeDelegate
    {
    public:
        explicit ReorderingDelegate(MutableTreeModel& model) noexcept : _model(model)
        {
        }

        ReorderingDelegate(const ReorderingDelegate&)            = delete;
        ReorderingDelegate(ReorderingDelegate&&)                 = delete;
        ReorderingDelegate& operator=(const ReorderingDelegate&) = delete;
        ReorderingDelegate& operator=(ReorderingDelegate&&)      = delete;

        void OnTreeSelectionChanged(uint64_t itemId) override
        {
            selectedItemId = itemId;
            _model.SetVisibleItems({
                TreeItemData{.id = 2u, .text = L"Second", .hasChildren = true, .expanded = false},
                TreeItemData{.id = 1u, .text = L"First", .hasChildren = true, .expanded = false},
            });
        }

        void OnTreeToggleExpanded(uint64_t itemId, bool) override
        {
            toggledItemId = itemId;
        }

        uint64_t selectedItemId = 0u;
        uint64_t toggledItemId  = 0u;

    private:
        MutableTreeModel& _model;
    };

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"First", .hasChildren = true, .expanded = false},
        TreeItemData{.id = 2u, .text = L"Second", .hasChildren = true, .expanded = false},
    });
    ReorderingDelegate delegate(model);
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(14, 16), handled));

    Require(handled, "tree expander click remains handled when selection reorders visible rows");
    Require(delegate.selectedItemId == 1u, "tree selection delegate receives the originally hit stable item id");
    Require(delegate.toggledItemId == 1u, "tree expander re-resolves the originally hit item instead of reusing its old row index");
}

void TestTreeSelectionDelegateCanReplaceRootSafely()
{
    using namespace DxUi;

    class RootReplacingDelegate final : public ITreeDelegate
    {
    public:
        explicit RootReplacingDelegate(WindowHost& host) noexcept : _host(host)
        {
        }

        RootReplacingDelegate(const RootReplacingDelegate&)            = delete;
        RootReplacingDelegate(RootReplacingDelegate&&)                 = delete;
        RootReplacingDelegate& operator=(const RootReplacingDelegate&) = delete;
        RootReplacingDelegate& operator=(RootReplacingDelegate&&)      = delete;

        void OnTreeSelectionChanged(uint64_t itemId) override
        {
            selectedItemId = itemId;
            _host.SetRoot(std::make_unique<Panel>());
        }

        uint64_t selectedItemId = 0u;

    private:
        WindowHost& _host;
    };

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({TreeItemData{.id = 10u, .text = L"General"}});
    RootReplacingDelegate delegate(host);
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(60, 16), handled));

    Require(handled, "tree pointer selection remains handled when its delegate replaces the root");
    Require(delegate.selectedItemId == 10u, "tree selection delegate receives the stable item id before replacing the root");
    Require(host.GetRoot() != nullptr, "tree selection delegate can replace the root without post-dispatch access");
}

void TestTreeKeyboardRightAndLeftHandleExpansionAndParentTraversal()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"Plugins", .hasChildren = true, .expanded = false},
        TreeItemData{.id = 3u, .text = L"Themes"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    host.SetFocusControl(tree);
    tree->SetSelectedItemId(1u);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RIGHT, 0, handled));
    Require(handled, "tree right key is handled on a collapsed parent");
    Require(delegate.toggleCount == 1u, "tree right key requests expansion on a collapsed parent");
    Require(delegate.lastToggledItemId == 1u, "tree right key toggles the selected parent item");
    Require(delegate.lastExpandedState, "tree right key requests the expanded state");

    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"Plugins", .hasChildren = true, .expanded = true},
        TreeItemData{.id = 2u, .parentId = 1u, .text = L"ViewerSqlite", .depth = 1u},
        TreeItemData{.id = 3u, .text = L"Themes"},
    });
    tree->NotifyDataChanged();

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RIGHT, 0, handled));
    Require(handled, "tree right key is handled on an expanded parent");
    Require(tree->GetSelectedItemId().has_value(), "tree expanded-parent right key keeps a selection");
    Require(tree->GetSelectedItemId().value() == 2u, "tree right key on an expanded parent selects the first child");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_LEFT, 0, handled));
    Require(handled, "tree left key is handled on a child item");
    Require(tree->GetSelectedItemId().has_value(), "tree left key on a child keeps a selection");
    Require(tree->GetSelectedItemId().value() == 1u, "tree left key on a child selects the parent");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_LEFT, 0, handled));
    Require(handled, "tree left key is handled on an expanded parent");
    Require(delegate.toggleCount == 2u, "tree left key requests collapse on an expanded parent");
    Require(delegate.lastToggledItemId == 1u, "tree left key collapses the selected parent item");
    Require(! delegate.lastExpandedState, "tree left key requests the collapsed state");
}

void TestTreeTypeaheadSelectsVisibleMatch()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"Alpha"},
        TreeItemData{.id = 2u, .text = L"Beta"},
        TreeItemData{.id = 3u, .text = L"Gamma"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    host.SetFocusControl(tree);
    tree->SetSelectedItemId(1u);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_CHAR, static_cast<WPARAM>(L'g'), 0, handled));
    Require(handled, "tree typeahead character is handled");
    Require(tree->GetSelectedItemId().has_value(), "tree typeahead keeps a selected item");
    Require(tree->GetSelectedItemId().value() == 3u, "tree typeahead selects the matching visible item");
    Require(delegate.selectionChangedCount == 1u, "tree typeahead notifies selection change");
    Require(delegate.lastSelectedItemId == 3u, "tree typeahead delegate receives the matched item id");
}

void TestTreeTypeaheadFallsBackToSingleCharacterAfterPrefixMiss()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"Alpha"},
        TreeItemData{.id = 2u, .text = L"Beta"},
        TreeItemData{.id = 3u, .text = L"Gamma"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    host.SetFocusControl(tree);
    tree->SetSelectedItemId(1u);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_CHAR, static_cast<WPARAM>(L'b'), 0, handled));
    Require(handled, "tree typeahead handles the first prefix character");
    Require(tree->GetSelectedItemId() && tree->GetSelectedItemId().value() == 2u, "tree typeahead selects the first prefix match");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_CHAR, static_cast<WPARAM>(L'g'), 0, handled));
    Require(handled, "tree typeahead falls back to the last character when the accumulated prefix misses");
    Require(tree->GetSelectedItemId() && tree->GetSelectedItemId().value() == 3u, "tree typeahead fallback selects the single-character match");
    Require(delegate.selectionChangedCount == 2u, "tree typeahead fallback notifies only real selection changes");
    Require(delegate.lastSelectedItemId == 3u, "tree typeahead fallback reports the matched item id");
}

void TestTypeaheadUsesInvariantCaseMappingUnderTurkishLocale()
{
    using namespace DxUi;

    const wchar_t* currentLocale      = _wsetlocale(LC_CTYPE, nullptr);
    const std::wstring originalLocale = currentLocale ? currentLocale : L"";
    const auto restoreLocale =
        wil::scope_exit([&]() noexcept { static_cast<void>(_wsetlocale(LC_CTYPE, originalLocale.empty() ? nullptr : originalLocale.c_str())); });

    const wchar_t* turkishLocale = _wsetlocale(LC_CTYPE, L"Turkish_Turkey.1254");
    if (! turkishLocale)
    {
        turkishLocale = _wsetlocale(LC_CTYPE, L"tr-TR");
    }
    if (! turkishLocale)
    {
        turkishLocale = _wsetlocale(LC_CTYPE, L"tr_TR");
    }

    Require(StartsWithInsensitive(L"Index", L"i"), "typeahead prefix matching stays case-insensitive for ASCII prefixes");
    if (turkishLocale)
    {
        Require(StartsWithInsensitive(L"Index", L"i"), "typeahead prefix matching stays invariant under Turkish locale rules");
    }
}

void TestTreeDoubleClickInvokesLeafItem()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers", .badgeText = L"Beta", .badgeTone = AdornmentTone::Warning},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDBLCLK, 0, MAKELPARAM(60, 44), handled));
    Require(handled, "tree double-click is handled");
    Require(delegate.invokedCount == 1u, "tree double-click invokes a leaf item exactly once");
    Require(delegate.lastInvokedItemId == 2u, "tree double-click invokes the clicked leaf item");
}

void TestTreePageDownAdvancesByVisibleRows()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"One"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
        TreeItemData{.id = 4u, .text = L"Four"},
        TreeItemData{.id = 5u, .text = L"Five"},
        TreeItemData{.id = 6u, .text = L"Six"},
        TreeItemData{.id = 7u, .text = L"Seven"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(1u);

    Require(tree.OnKeyDown(host, VK_NEXT, 0), "tree page down is handled");
    Require(tree.GetSelectedItemId().has_value(), "tree page down keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 5u, "tree page down advances by the visible row count");
}

void TestTreePageUpRetreatsByVisibleRows()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"One"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
        TreeItemData{.id = 4u, .text = L"Four"},
        TreeItemData{.id = 5u, .text = L"Five"},
        TreeItemData{.id = 6u, .text = L"Six"},
        TreeItemData{.id = 7u, .text = L"Seven"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(7u);

    Require(tree.OnKeyDown(host, VK_PRIOR, 0), "tree page up is handled");
    Require(tree.GetSelectedItemId().has_value(), "tree page up keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 3u, "tree page up retreats by the visible row count");
}

void TestTreeHomeAndEndNavigateToBoundaries()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"One"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
        TreeItemData{.id = 4u, .text = L"Four"},
        TreeItemData{.id = 5u, .text = L"Five"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(3u);

    Require(tree.OnKeyDown(host, VK_END, 0), "tree end is handled");
    Require(tree.GetSelectedItemId().has_value(), "tree end keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 5u, "tree end selects the last visible item");

    Require(tree.OnKeyDown(host, VK_HOME, 0), "tree home is handled");
    Require(tree.GetSelectedItemId().has_value(), "tree home keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 1u, "tree home selects the first visible item");
}

void TestTreePageAndBoundaryKeysClampAtExtremes()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"One"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
        TreeItemData{.id = 4u, .text = L"Four"},
        TreeItemData{.id = 5u, .text = L"Five"},
        TreeItemData{.id = 6u, .text = L"Six"},
        TreeItemData{.id = 7u, .text = L"Seven"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(1u);

    Require(tree.OnKeyDown(host, VK_HOME, 0), "tree home is handled at the first visible item");
    Require(tree.GetSelectedItemId().has_value(), "tree home at the first visible item keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 1u, "tree home clamps at the first visible item");

    Require(tree.OnKeyDown(host, VK_PRIOR, 0), "tree page up is handled at the first visible item");
    Require(tree.GetSelectedItemId().has_value(), "tree page up at the first visible item keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 1u, "tree page up clamps at the first visible item");

    tree.SetSelectedItemId(7u);

    Require(tree.OnKeyDown(host, VK_END, 0), "tree end is handled at the last visible item");
    Require(tree.GetSelectedItemId().has_value(), "tree end at the last visible item keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 7u, "tree end clamps at the last visible item");

    Require(tree.OnKeyDown(host, VK_NEXT, 0), "tree page down is handled at the last visible item");
    Require(tree.GetSelectedItemId().has_value(), "tree page down at the last visible item keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 7u, "tree page down clamps at the last visible item");
}

void TestTreeMouseWheelScrollAffectsLaterHitTesting()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"One"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
        TreeItemData{.id = 4u, .text = L"Four"},
        TreeItemData{.id = 5u, .text = L"Five"},
        TreeItemData{.id = 6u, .text = L"Six"},
        TreeItemData{.id = 7u, .text = L"Seven"},
    });

    RecordingTreeDelegate delegate;
    tree.SetModel(&model);
    tree.SetDelegate(&delegate);

    Require(tree.OnMouseWheel(host, D2D1::Point2F(40.0f, 16.0f), -120.0f, 0), "tree wheel scroll is handled");
    Require(tree.OnMouseDown(host, D2D1::Point2F(40.0f, 16.0f), false, 0), "tree click after wheel scroll is handled");
    Require(tree.GetSelectedItemId().has_value(), "tree click after wheel scroll keeps a selected item");
    Require(tree.GetSelectedItemId().value() == 4u, "tree wheel scroll updates hit-testing for later pointer selection");
    Require(delegate.selectionChangedCount == 1u, "tree click after wheel scroll notifies selection");
    Require(delegate.lastSelectedItemId == 4u, "tree click after wheel scroll selects the scrolled top row");
}

void TestTreeScrollbarThumbGutterDragThroughWindowHost()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));
    tree->SetRowHeightDip(24.0f);

    MutableTreeModel model;
    std::vector<TreeItemData> items;
    items.reserve(40u);
    for (uint64_t id = 1u; id <= 40u; ++id)
    {
        items.push_back(TreeItemData{.id = id, .text = L"Item " + std::to_wstring(id)});
    }
    model.SetVisibleItems(std::move(items));
    tree->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    const ThemePalette theme             = MakeAnimatedTestThemePalette(true);
    const TreeScrollbarVisualState state = tree->DebugGetScrollbarVisualState(theme);
    Require(state.hasVerticalScrollbar, "tree exposes a vertical scrollbar for thumb gutter drag");
    RequireRectHasArea(state.verticalThumbRect, "tree exposes a visible vertical scrollbar thumb for gutter drag");

    const LONG gutterX = static_cast<LONG>(std::floor(state.verticalTrackRect.left + 1.0f));
    const LONG thumbY  = static_cast<LONG>(std::lround((state.verticalThumbRect.top + state.verticalThumbRect.bottom) * 0.5f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(gutterX, thumbY), handled));
    Require(handled, "tree handles scrollbar thumb gutter mouse-down as a drag");

    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(gutterX, thumbY + 48), handled));
    Require(handled, "tree handles captured scrollbar thumb gutter mouse-move");
    Require(tree->DebugGetVerticalScrollDip() > 0.5f, "tree thumb gutter drag moves the vertical scroll offset");

    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(gutterX, thumbY + 48), handled));
    Require(handled, "tree handles captured scrollbar thumb gutter mouse-up");
}

void TestTreeLargeWheelDeltaUsesFullMagnitude()
{
    using namespace DxUi;

    const auto populateTree = [](MutableTreeModel& model) noexcept
    {
        std::vector<TreeItemData> items;
        items.reserve(20u);
        for (uint64_t id = 1u; id <= 20u; ++id)
        {
            items.push_back(TreeItemData{.id = id, .text = L"Item " + std::to_wstring(id)});
        }
        model.SetVisibleItems(std::move(items));
    };

    WindowHost host;

    EnableMotionForTest(host);
    Tree singleStepTree;
    singleStepTree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));
    MutableTreeModel singleStepModel;
    populateTree(singleStepModel);
    singleStepTree.SetModel(&singleStepModel);

    Require(singleStepTree.OnMouseWheel(host, D2D1::Point2F(40.0f, 16.0f), -static_cast<float>(WHEEL_DELTA), 0),
            "tree wheel magnitude test handles a single wheel delta");
    const size_t singleStepFirstVisibleIndex = singleStepTree.DebugGetFirstVisibleIndex();
    Require(singleStepFirstVisibleIndex > 0u, "single tree wheel delta advances the visible tree rows");

    Tree doubleStepTree;
    doubleStepTree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));
    MutableTreeModel doubleStepModel;
    populateTree(doubleStepModel);
    doubleStepTree.SetModel(&doubleStepModel);

    Require(doubleStepTree.OnMouseWheel(host, D2D1::Point2F(40.0f, 16.0f), -static_cast<float>(WHEEL_DELTA * 2), 0),
            "tree wheel magnitude test handles a double wheel delta");
    Require(doubleStepTree.DebugGetFirstVisibleIndex() == singleStepFirstVisibleIndex * 2u,
            "double tree wheel delta advances by twice the single-step row movement");
}

void TestTreeAccumulatesPartialWheelDelta()
{
    using namespace DxUi;

    const auto populateTree = [](MutableTreeModel& model) noexcept
    {
        std::vector<TreeItemData> items;
        items.reserve(20u);
        for (uint64_t id = 1u; id <= 20u; ++id)
        {
            items.push_back(TreeItemData{.id = id, .text = L"Item " + std::to_wstring(id)});
        }
        model.SetVisibleItems(std::move(items));
    };

    WindowHost host;

    EnableMotionForTest(host);
    Tree singleStepTree;
    singleStepTree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));
    MutableTreeModel singleStepModel;
    populateTree(singleStepModel);
    singleStepTree.SetModel(&singleStepModel);

    Require(singleStepTree.OnMouseWheel(host, D2D1::Point2F(40.0f, 16.0f), -static_cast<float>(WHEEL_DELTA), 0),
            "tree partial wheel-delta test handles the full-delta baseline");
    const size_t singleStepFirstVisibleIndex = singleStepTree.DebugGetFirstVisibleIndex();

    Tree partialStepTree;
    partialStepTree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));
    MutableTreeModel partialStepModel;
    populateTree(partialStepModel);
    partialStepTree.SetModel(&partialStepModel);

    Require(partialStepTree.OnMouseWheel(host, D2D1::Point2F(40.0f, 16.0f), -static_cast<float>(WHEEL_DELTA / 2), 0),
            "tree partial wheel-delta test handles the first half wheel delta");
    Require(partialStepTree.DebugGetFirstVisibleIndex() == 0u, "half tree wheel delta alone does not advance the visible rows");

    Require(partialStepTree.OnMouseWheel(host, D2D1::Point2F(40.0f, 16.0f), -static_cast<float>(WHEEL_DELTA / 2), 0),
            "tree partial wheel-delta test handles the second half wheel delta");
    Require(partialStepTree.DebugGetFirstVisibleIndex() == singleStepFirstVisibleIndex,
            "two half tree wheel deltas accumulate to the same row movement as one full step");
}

// Rows sit at whole-row positions offset by the scroll, so after a scrollbar drag the first and last visible rows straddle
// the viewport's edges. Nothing such a row paints, nor the reorder marker on it, may reach past the tree's frame: outside
// the frame the window looks as it does with an empty tree, while inside the viewport the rows' visible parts still paint.
void TestTreeRowsStraddlingTheViewportEdgesPaintOnlyInsideIt()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(16.0f, 40.0f, 240.0f, 136.0f));
    tree->SetReorderEnabled(true);
    MutableTreeModel model;
    std::vector<TreeItemData> items;
    for (uint64_t id = 1u; id <= 20u; ++id)
    {
        // Every row is a parent, so the middle of the row that straddles the bottom edge is an inside drop.
        items.push_back(TreeItemData{.id = id, .text = L"Item " + std::to_wstring(id), .hasChildren = true});
    }
    model.SetVisibleItems(std::move(items));
    tree->SetModel(&model);
    // Selected before the scroll: selecting later would scroll the row wholly into view.
    tree->SetSelectedItemId(2u);
    window.Host().SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    ControlHost& host = window.Host();

    // The row height follows the host's fonts, and the viewport is inset from the frame by the same amount on every side,
    // which the first row's top gives before any scroll. A viewport of 2.9 rows scrolled by 1.5 rows shows half of the
    // second row at its top and four tenths of the fifth at its bottom, and the window has room above and below the frame.
    const std::optional<D2D1_RECT_F> firstRow = tree->GetVisibleItemHitRect(0u);
    Require(firstRow.has_value() && firstRow->bottom > firstRow->top, "the first row has a rectangle");
    const float rowHeight = firstRow->bottom - firstRow->top;
    const float inset     = firstRow->top - tree->GetBounds().top;
    tree->SetBounds(D2D1::RectF(16.0f, 40.0f, 240.0f, 40.0f + (2.0f * inset) + (2.9f * rowHeight)));
    const D2D1_RECT_F frame    = tree->GetBounds();
    const float viewportTop    = frame.top + inset;
    const float viewportBottom = frame.bottom - inset;

    // Only a thumb drag leaves a fraction of a row: the wheel and the keys scroll by whole rows.
    const TreeScrollbarVisualState scrollbar = tree->DebugGetScrollbarVisualState(host.GetTheme());
    Require(scrollbar.hasVerticalScrollbar, "the tree scrolls");
    const D2D1_RECT_F track = scrollbar.verticalTrackRect;
    const D2D1_RECT_F thumb = scrollbar.verticalThumbRect;
    const float available   = (track.bottom - track.top) - (thumb.bottom - thumb.top);
    const float extent      = (20.0f * rowHeight) - (viewportBottom - viewportTop);
    Require(available > 0.0f && extent > 0.0f, "the thumb has room to move");
    const D2D1_POINT_2F grab   = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
    const D2D1_POINT_2F dragTo = D2D1::Point2F(grab.x, grab.y + (1.5f * rowHeight * available / extent));
    Require(tree->OnMouseDown(host, grab, false, 0u) && tree->OnMouseMove(host, dragTo, 0u), "the thumb drag scrolls");
    static_cast<void>(tree->OnMouseUp(host, dragTo, false, 0u));

    std::optional<size_t> topRow;
    std::optional<size_t> bottomRow;
    for (size_t index = 0u; index < 20u; ++index)
    {
        if (const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(index))
        {
            if (rect->top < viewportTop && rect->bottom > viewportTop)
            {
                topRow = index;
            }
            if (rect->top < viewportBottom && rect->bottom > viewportBottom)
            {
                bottomRow = index;
            }
        }
    }
    Require(topRow == 1u && bottomRow.has_value(), "the selected row straddles the top edge and another row the bottom edge");
    const D2D1_RECT_F top    = tree->GetVisibleItemHitRect(topRow.value()).value();
    const D2D1_RECT_F bottom = tree->GetVisibleItemHitRect(bottomRow.value()).value();
    const float bottomShown  = (viewportBottom - bottom.top) / rowHeight;
    Require(viewportTop - top.top > 0.2f * rowHeight && top.bottom - viewportTop > 0.2f * rowHeight && bottomShown > 0.3f && bottomShown < 0.8f,
            "each row straddles its edge by a good part of a row");
    // In the visible part of the bottom row and in its middle half, where a drop goes inside it.
    const D2D1_POINT_2F inBottomRow = D2D1::Point2F((bottom.left + bottom.right) * 0.5f, bottom.top + (0.5f * (0.25f + bottomShown) * rowHeight));
    Require(tree->OnMouseMove(host, inBottomRow, 0u), "the pointer hovers the row that straddles the bottom edge");
    WindowHostBitmapCapture straddling;
    Require(host.DebugCaptureBitmap(straddling), "capture the rows that straddle both edges");

    // A row drag over the bottom row shows the reorder marker on it. The pressed row is wholly visible, so the press
    // scrolls nothing.
    const float scrollDip                    = tree->DebugGetVerticalScrollDip();
    const std::optional<D2D1_RECT_F> pressed = tree->GetVisibleItemHitRect(topRow.value() + 1u);
    Require(pressed.has_value() && pressed->top >= viewportTop && pressed->bottom <= viewportBottom, "a wholly visible row to drag");
    const D2D1_POINT_2F press = D2D1::Point2F((pressed->left + pressed->right) * 0.5f, (pressed->top + pressed->bottom) * 0.5f);
    Require(tree->OnMouseDown(host, press, false, 0u) && tree->OnMouseMove(host, inBottomRow, 0u), "the row drag reaches the bottom row");
    Require(tree->DebugGetVerticalScrollDip() == scrollDip, "the row drag scrolled nothing");
    WindowHostBitmapCapture marker;
    Require(host.DebugCaptureBitmap(marker), "capture the reorder marker on the bottom row");
    Require(tree->OnKeyDown(host, VK_ESCAPE, 0u), "escape cancels the row drag");
    static_cast<void>(tree->OnMouseUp(host, inBottomRow, false, 0u));

    // The same window with an empty tree: the frame alone.
    MutableTreeModel empty;
    tree->SetModel(&empty);
    WindowHostBitmapCapture frameOnly;
    Require(host.DebugCaptureBitmap(frameOnly), "capture the empty tree");
    Require(straddling.widthPx == frameOnly.widthPx && straddling.heightPx == frameOnly.heightPx && marker.widthPx == frameOnly.widthPx &&
                marker.heightPx == frameOnly.heightPx,
            "the captures have one size");

    // Pixel rows kept two pixels clear of the frame's antialiased border and of the clip's edge.
    const float scale          = host.GetDpi() / 96.0f;
    const auto pixelRow        = [scale](float dip) noexcept { return static_cast<UINT>((std::max)(0.0f, std::floor(dip * scale))); };
    const UINT aboveFrameEnd   = pixelRow(frame.top) - 2u;
    const UINT belowFrameBegin = (std::min)(frameOnly.heightPx, pixelRow(frame.bottom) + 3u);
    const auto differingInRows = [](const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected, UINT begin, UINT end) noexcept
    {
        size_t differing = 0u;
        for (UINT y = begin; y < end && y < actual.heightPx; ++y)
        {
            for (UINT x = 0u; x < actual.widthPx; ++x)
            {
                const size_t base = ((static_cast<size_t>(y) * actual.widthPx) + x) * 4u;
                for (size_t channel = 0u; channel < 4u; ++channel)
                {
                    const int delta = static_cast<int>(actual.bgraPixels[base + channel]) - static_cast<int>(expected.bgraPixels[base + channel]);
                    if (delta > 8 || delta < -8)
                    {
                        ++differing;
                        break;
                    }
                }
            }
        }
        return differing;
    };

    Require(differingInRows(straddling, frameOnly, 0u, aboveFrameEnd) == 0u, "nothing of the selected row that straddles the top edge paints above the frame");
    Require(differingInRows(straddling, frameOnly, belowFrameBegin, frameOnly.heightPx) == 0u,
            "nothing of the hovered row that straddles the bottom edge paints below the frame");
    Require(differingInRows(marker, frameOnly, belowFrameBegin, frameOnly.heightPx) == 0u,
            "nothing of the reorder marker on the row that straddles the bottom edge paints below the frame");
    Require(differingInRows(straddling, frameOnly, pixelRow(viewportTop) + 2u, pixelRow(top.bottom) - 1u) > 0u, "the visible part of the selected row paints");
    Require(differingInRows(straddling, frameOnly, pixelRow(bottom.top) + 2u, pixelRow(viewportBottom) - 1u) > 0u,
            "the visible part of the hovered row paints");
    Require(differingInRows(marker, frameOnly, pixelRow(bottom.top) + 2u, pixelRow(viewportBottom) - 1u) > 0u,
            "the reorder marker paints on the visible part of its row");
}
void TestTreeScrollbarFeedbackFollowsHoverAndDragState()
{
    using namespace DxUi;

    const auto thumbCenter = [](const D2D1_RECT_F& thumbRect) noexcept
    { return D2D1::Point2F((thumbRect.left + thumbRect.right) * 0.5f, (thumbRect.top + thumbRect.bottom) * 0.5f); };
    const auto trackPointOutsideThumb = [](const D2D1_RECT_F& trackRect, const D2D1_RECT_F& thumbRect) noexcept
    {
        const float x = (trackRect.left + trackRect.right) * 0.5f;
        const float y = (thumbRect.top - trackRect.top > 2.0f) ? ((trackRect.top + thumbRect.top) * 0.5f) : ((thumbRect.bottom + trackRect.bottom) * 0.5f);
        return D2D1::Point2F(x, y);
    };

    WindowHost host;

    EnableMotionForTest(host);
    const ThemePalette theme = MakeAnimatedTestThemePalette(true);
    host.SetTheme(theme);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 116.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"One"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
        TreeItemData{.id = 4u, .text = L"Four"},
        TreeItemData{.id = 5u, .text = L"Five"},
        TreeItemData{.id = 6u, .text = L"Six"},
        TreeItemData{.id = 7u, .text = L"Seven"},
    });
    tree.SetModel(&model);

    TreeScrollbarVisualState state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.hasVerticalScrollbar, "tree scrollbar feedback test exposes a vertical scrollbar");
    Require(state.verticalTrackArgb == PackColorForTest(theme.scrollbarTrack), "tree scrollbar feedback starts with the idle track color");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumb), "tree scrollbar feedback starts with the idle thumb color");
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "tree scrollbar feedback starts with no track hover progress");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "tree scrollbar feedback starts with no thumb hover progress");

    const D2D1_POINT_2F verticalTrackPoint = trackPointOutsideThumb(state.verticalTrackRect, state.verticalThumbRect);
    Require(tree.OnMouseMove(host, verticalTrackPoint, 0), "tree scrollbar feedback handles track hover");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalTrackHovered, "tree scrollbar feedback marks the track hovered");
    Require(! state.verticalThumbHovered, "tree scrollbar feedback keeps the thumb distinct from track hover");
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "tree scrollbar hover begins from the current track progress");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "tree scrollbar hover begins from the current thumb progress");
    Require(tree.Tick(host, 0u), "tree scrollbar hover animation anchors on the first tick");
    Require(tree.Tick(host, 35u), "tree scrollbar hover animation continues mid-transition");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalTrackHotProgress > 0.0f && state.verticalTrackHotProgress < 1.0f, "tree scrollbar hover exposes an in-flight track progress value");
    Require(state.verticalThumbHotProgress > 0.0f && state.verticalThumbHotProgress < 0.45f, "tree scrollbar hover exposes an in-flight thumb warmup value");
    Require(state.verticalTrackArgb != PackColorForTest(theme.scrollbarTrack), "tree scrollbar hover tints the track mid-transition");
    Require(state.verticalThumbArgb != PackColorForTest(theme.scrollbarThumb), "tree scrollbar hover warms the thumb mid-transition");
    Require(tree.Tick(host, 160u), "tree scrollbar hover requests a final repaint when it settles");
    state = tree.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalTrackHotProgress, 1.0f, 0.0001f, "tree scrollbar hover settles to a fully warm track");
    RequireFloatNear(state.verticalThumbHotProgress, 0.45f, 0.0001f, "tree scrollbar hover settles to the shared thumb warmup strength");

    const D2D1_POINT_2F verticalThumbPoint = thumbCenter(state.verticalThumbRect);
    Require(tree.OnMouseMove(host, verticalThumbPoint, 0), "tree scrollbar feedback handles thumb hover");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbHovered, "tree scrollbar feedback marks the thumb hovered");
    Require(! state.verticalTrackHovered, "tree scrollbar feedback clears track hover when the thumb is hovered");
    RequireFloatNear(state.verticalThumbHotProgress, 0.45f, 0.0001f, "tree scrollbar thumb hover continues from the track-hover warmup level");
    Require(tree.Tick(host, 160u), "tree scrollbar thumb-hover animation anchors on the current tick");
    Require(tree.Tick(host, 230u), "tree scrollbar thumb-hover animation continues mid-transition");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbHotProgress > 0.45f && state.verticalThumbHotProgress < 1.0f,
            "tree scrollbar thumb hover exposes an in-flight hot progress value");
    Require(tree.Tick(host, 320u), "tree scrollbar thumb-hover animation requests a final repaint when settling");
    state = tree.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalThumbHotProgress, 1.0f, 0.0001f, "tree scrollbar thumb hover settles at the hot thumb strength");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumbHot), "tree scrollbar feedback uses the hot thumb color after settling");

    Require(tree.OnMouseDown(host, verticalThumbPoint, false, 0), "tree scrollbar feedback handles thumb drag start");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbDragging, "tree scrollbar feedback marks the thumb as dragging");
    RequireFloatNear(state.verticalThumbHotProgress, 1.0f, 0.0001f, "tree scrollbar feedback keeps the thumb fully hot while dragging");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumbHot), "tree scrollbar feedback keeps the thumb hot while dragging");

    const D2D1_POINT_2F dragPoint = D2D1::Point2F(verticalThumbPoint.x, std::min(state.verticalTrackRect.bottom - 6.0f, verticalThumbPoint.y + 24.0f));
    Require(tree.OnMouseMove(host, dragPoint, 0), "tree scrollbar feedback handles drag movement");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalThumbDragging, "tree scrollbar feedback preserves drag state while the thumb moves");

    const D2D1_POINT_2F dragThumbPoint = thumbCenter(state.verticalThumbRect);
    Require(tree.OnMouseUp(host, dragThumbPoint, false, 0), "tree scrollbar feedback handles thumb drag release");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(! state.verticalThumbDragging, "tree scrollbar feedback clears drag state on release");
    Require(state.verticalThumbHovered, "tree scrollbar feedback keeps the thumb hot when the pointer releases over the thumb");

    Require(tree.OnMouseLeave(host), "tree scrollbar feedback handles mouse leave");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(! state.verticalTrackHovered && ! state.verticalThumbHovered, "tree scrollbar feedback clears hover state on leave");
    RequireFloatNear(state.verticalTrackHotProgress, 1.0f, 0.0001f, "tree scrollbar leave starts from the previously hot track state");
    RequireFloatNear(state.verticalThumbHotProgress, 1.0f, 0.0001f, "tree scrollbar leave starts from the previously hot thumb state");
    Require(tree.Tick(host, 320u), "tree scrollbar leave animation anchors on the first fade-out tick");
    Require(tree.Tick(host, 355u), "tree scrollbar leave animation continues mid-transition");
    state = tree.DebugGetScrollbarVisualState(theme);
    Require(state.verticalTrackHotProgress > 0.0f && state.verticalTrackHotProgress < 1.0f, "tree scrollbar leave exposes an in-flight track fade-out value");
    Require(state.verticalThumbHotProgress > 0.0f && state.verticalThumbHotProgress < 1.0f, "tree scrollbar leave exposes an in-flight thumb fade-out value");
    Require(tree.Tick(host, 480u), "tree scrollbar leave animation requests a final repaint when settling");
    state = tree.DebugGetScrollbarVisualState(theme);
    RequireFloatNear(state.verticalTrackHotProgress, 0.0f, 0.0001f, "tree scrollbar leave settles back to the idle track state");
    RequireFloatNear(state.verticalThumbHotProgress, 0.0f, 0.0001f, "tree scrollbar leave settles back to the idle thumb state");
    Require(state.verticalTrackArgb == PackColorForTest(theme.scrollbarTrack), "tree scrollbar feedback restores the idle track color on leave");
    Require(state.verticalThumbArgb == PackColorForTest(theme.scrollbarThumb), "tree scrollbar feedback restores the idle thumb color on leave");
    Require(! tree.Tick(host, 540u), "settled tree scrollbar animation stops requesting ticks");
}

void TestTreeFocusVisualsRespectKeyboardFocusVisibilityAndHighContrast()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    const ThemePalette theme = MakeAnimatedTestThemePalette(true);
    Tree tree;

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(2u);
    host.SetFocusControl(&tree);

    TreeDebugRowVisualState state{};
    Require(tree.DebugGetRowVisualState(theme, 1u, false, state), "tree focus visual test resolves the selected row state");
    Require(state.selected, "tree focus visual test keeps the selected row selected");
    Require(! state.showFocus, "tree pointer-focused row stays quiet when keyboard focus visibility is off");
    Require(state.iconArgb == state.textArgb, "tree selected row keeps icon chrome aligned with selected text chrome");
    Require(state.expanderArgb == state.textArgb, "tree selected row keeps expander chrome aligned with selected text chrome");
    Require(state.badgeFillArgb != 0u, "tree selected row exposes resolved badge fill chrome");
    Require(state.badgeTextArgb != 0u, "tree selected row exposes resolved badge text chrome");
    Require(state.focusArgb == 0u, "tree pointer-focused row does not expose focus-ring chrome when focus is hidden");

    Require(tree.DebugGetRowVisualState(theme, 1u, true, state), "tree focus visual test resolves the keyboard-focused row state");
    Require(state.showFocus, "tree keyboard-focused row shows focus chrome");
    Require(state.iconArgb == state.textArgb, "tree keyboard-focused selected row keeps icon chrome aligned with selected text chrome");
    Require(state.expanderArgb == state.textArgb, "tree keyboard-focused selected row keeps expander chrome aligned with selected text chrome");
    Require(state.focusArgb == PackColorForTest(theme.focusStroke), "tree keyboard-focused row uses the palette focus stroke for the focus ring");

    ThemePalette highContrastTheme = theme;
    highContrastTheme.highContrast = true;
    Require(tree.DebugGetRowVisualState(highContrastTheme, 1u, false, state), "tree focus visual test resolves the high-contrast row state");
    Require(state.showFocus, "tree high-contrast focused row keeps a visible focus fallback even without keyboard focus visibility");
    Require(state.focusArgb == PackColorForTest(highContrastTheme.focusStroke),
            "tree high-contrast focused row keeps the palette focus stroke for the focus ring");

    ThemeColors viewerTheme{.sizeBytes = sizeof(ThemeColors)};
    viewerTheme.backgroundArgb             = 0xFF11161Cu;
    viewerTheme.textArgb                   = 0xFFE7EDF8u;
    viewerTheme.selectionBackgroundArgb    = 0xFF4B7ECEu;
    viewerTheme.selectionTextArgb          = 0xFFF7FBFFu;
    viewerTheme.accentArgb                 = 0xFFD06657u;
    viewerTheme.alertErrorBackgroundArgb   = 0xFF5C1F25u;
    viewerTheme.alertErrorTextArgb         = 0xFFFFD8DCu;
    viewerTheme.alertWarningBackgroundArgb = 0xFF5A430Eu;
    viewerTheme.alertWarningTextArgb       = 0xFFFFE3A1u;
    viewerTheme.alertInfoBackgroundArgb    = 0xFF18324Au;
    viewerTheme.alertInfoTextArgb          = 0xFFD6E8FFu;
    viewerTheme.darkMode                   = TRUE;
    viewerTheme.highContrast               = FALSE;
    viewerTheme.rainbowMode                = FALSE;
    viewerTheme.darkBase                   = TRUE;

    const ThemePalette viewerPalette = MakeThemePalette(viewerTheme);
    Require(tree.DebugGetRowVisualState(viewerPalette, 0u, false, state), "tree focus visual test resolves the viewer-derived unselected row state");
    Require(! state.selected, "tree viewer-derived unselected row stays unselected");
    Require(state.iconArgb == PackColorForTest(ResolveListIconColor(viewerPalette, viewerPalette.text, false)),
            "tree viewer-derived unselected row uses the shared list-icon chrome");
    Require(state.expanderArgb == state.textArgb, "tree viewer-derived unselected row keeps expander chrome aligned with row text");
    Require(tree.DebugGetRowVisualState(viewerPalette, 1u, true, state), "tree focus visual test resolves the viewer-derived row state");
    Require(state.showFocus, "tree viewer-derived keyboard-focused row shows focus chrome");
    Require(state.iconArgb == state.textArgb, "tree viewer-derived selected row keeps icon chrome aligned with selected text chrome");
    Require(state.expanderArgb == state.textArgb, "tree viewer-derived selected row keeps expander chrome aligned with selected text chrome");
    Require(state.badgeFillArgb != 0u, "tree viewer-derived selected row exposes resolved badge fill chrome");
    Require(state.badgeTextArgb != 0u, "tree viewer-derived selected row exposes resolved badge text chrome");
    Require(state.badgeFillArgb != PackColorForTest(viewerPalette.accent), "tree viewer-derived selected row badge fill does not fall back to raw accent");
    Require(state.focusArgb == PackColorForTest(viewerPalette.focusStroke),
            "tree viewer-derived keyboard-focused row uses the palette focus stroke for the focus ring");
    Require(state.focusArgb != PackColorForTest(viewerPalette.accent),
            "tree viewer-derived focus ring follows the focus-stroke contract instead of raw accent");
}

void TestTreeSelectedRowUsesRainbowOnlyInRainbowMode()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(2u);
    host.SetFocusControl(&tree);

    ThemePalette rainbowTheme = MakeAnimatedTestThemePalette(true);
    rainbowTheme.rainbowMode  = true;

    TreeDebugRowVisualState state{};
    Require(tree.DebugGetRowVisualState(rainbowTheme, 1u, true, state), "tree rainbow visual test resolves the selected row state");
    Require(state.selected, "tree rainbow visual test keeps the selected row selected");
    Require(state.usesRainbow, "tree selected row uses rainbow tint when rainbow mode is enabled");
    Require(state.fillArgb != PackColorForTest(rainbowTheme.selectionFill),
            "tree rainbow visual test does not fall back to the ordinary selection fill in rainbow mode");
    Require(state.textArgb != 0u, "tree rainbow visual test resolves a legible text color");

    ThemePalette highContrastRainbowTheme = rainbowTheme;
    highContrastRainbowTheme.highContrast = true;
    Require(tree.DebugGetRowVisualState(highContrastRainbowTheme, 1u, true, state), "tree rainbow visual test resolves the high-contrast selected row state");
    Require(! state.usesRainbow, "tree selected row suppresses rainbow tint in high-contrast mode");
    Require(state.fillArgb == PackColorForTest(highContrastRainbowTheme.selectionFill),
            "tree high-contrast selected row falls back to the shared selection fill");
}

void TestTreeNotifyDataChangedClearsMissingSelection()
{
    using namespace DxUi;

    Tree tree;
    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });

    tree.SetModel(&model);
    tree.SetSelectedItemId(2u);

    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
    });
    tree.NotifyDataChanged();

    Require(! tree.GetSelectedItemId().has_value(), "tree data change clears the selection when the selected item disappears");
}

void TestTreeRightClickInvokesContextMenuForHitItem()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 10u, .text = L"General"},
        TreeItemData{.id = 20u, .text = L"Viewers"},
        TreeItemData{.id = 30u, .text = L"Themes"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, 0, MAKELPARAM(60, 44), handled));
    Require(handled, "tree right-click is handled");
    Require(tree->GetSelectedItemId().has_value(), "tree right-click keeps a selected item");
    Require(tree->GetSelectedItemId().value() == 20u, "tree right-click selects the hit item");
    Require(delegate.selectionChangedCount == 1u, "tree right-click notifies selection change when targeting a new item");
    Require(delegate.contextMenuCount == 1u, "tree right-click invokes one context menu");
    Require(delegate.lastContextMenuItemId == 20u, "tree right-click targets the hit item");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{60, 44}, "tree right-click uses the hit point as its screen anchor");
}

void TestTreeKeyboardContextMenuBringsOffscreenSelectionIntoViewBeforeAnchoring()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));
    tree->SetRowHeightDip(28.0f);

    MutableTreeModel model;
    std::vector<TreeItemData> items;
    items.reserve(20u);
    for (uint64_t id = 1u; id <= 20u; ++id)
    {
        items.push_back(TreeItemData{.id = id, .text = L"Item " + std::to_wstring(id)});
    }
    model.SetVisibleItems(std::move(items));

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    tree->SetSelectedItemId(1u);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    host.SetFocusControl(tree);

    Require(tree->OnMouseWheel(host, D2D1::Point2F(40.0f, 60.0f), -static_cast<float>(WHEEL_DELTA), 0),
            "tree keyboard context-menu setup scrolls the selected item offscreen");
    Require(tree->DebugGetVerticalScrollDip() > 0.5f, "tree keyboard context-menu setup has a nonzero scroll offset");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "tree keyboard context menu is handled for an offscreen selection");
    Require(delegate.contextMenuCount == 1u, "tree keyboard context menu invokes the delegate once");
    Require(delegate.lastContextMenuItemId == 1u, "tree keyboard context menu targets the selected item");
    RequireFloatNear(tree->DebugGetVerticalScrollDip(), 0.0f, 0.5f, "tree keyboard context menu scrolls the selected item back into view");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{16, 16}, "tree keyboard context menu anchors on the selected row after scrolling");
}

void TestTreeLayoutMetricsReserveSpaceForIconAndBadge()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"Plugins", .iconText = L"P", .badgeText = L"12", .hasChildren = true, .expanded = true},
    });
    tree.SetModel(&model);

    const TreeItemLayoutMetrics metrics = tree.GetItemLayoutMetrics(host, 0u);
    Require(metrics.hasExpander, "tree layout reports expander presence");
    Require(metrics.hasIcon, "tree layout reports icon presence");
    Require(metrics.hasBadge, "tree layout reports badge presence");
    RequireRectHasArea(metrics.rowRect, "tree layout row rect has area");
    RequireRectHasArea(metrics.expanderRect, "tree layout expander rect has area");
    RequireRectHasArea(metrics.iconRect, "tree layout icon rect has area");
    RequireRectHasArea(metrics.badgeRect, "tree layout badge rect has area");
    RequireRectHasArea(metrics.textRect, "tree layout text rect has area");
    Require(metrics.iconRect.left >= metrics.expanderRect.right, "tree icon rect starts after the expander");
    Require(metrics.textRect.left >= metrics.iconRect.right, "tree text rect starts after the icon rect");
    Require(metrics.textRect.right <= metrics.badgeRect.left, "tree text rect stops before the badge rect");
    Require(metrics.badgeRect.right <= 250.0f, "tree badge rect stays within the tree content width");
}

void TestTreeLayoutMetricsOmitOptionalAdornmentRectsWhenUnused()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
    });
    tree.SetModel(&model);

    const TreeItemLayoutMetrics metrics = tree.GetItemLayoutMetrics(host, 0u);
    Require(! metrics.hasExpander, "tree layout omits expander when there are no children");
    Require(! metrics.hasIcon, "tree layout omits icon when no icon text is provided");
    Require(! metrics.hasBadge, "tree layout omits badge when no badge text is provided");
    Require(metrics.expanderRect.right <= metrics.expanderRect.left, "tree layout leaves the expander rect empty when unused");
    Require(metrics.iconRect.right <= metrics.iconRect.left, "tree layout leaves the icon rect empty when unused");
    Require(metrics.badgeRect.right <= metrics.badgeRect.left, "tree layout leaves the badge rect empty when unused");
    RequireRectHasArea(metrics.textRect, "tree layout still reserves a text rect when optional adornments are absent");
}

void TestTreeRowMetricsClampToSegoeVariableBodyLineHeight()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    Tree tree;
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));
    tree.SetRowHeightDip(12.0f);

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });
    tree.SetModel(&model);

    const TreeItemLayoutMetrics first  = tree.GetItemLayoutMetrics(host, 0u);
    const TreeItemLayoutMetrics second = tree.GetItemLayoutMetrics(host, 1u);
    RequireFloatNear(first.rowRect.bottom - first.rowRect.top,
                     kMinimumInteractiveTextRowHeightDip,
                     0.5f,
                     "tree rows clamp to the shared Segoe UI Variable body line-height minimum");
    RequireFloatNear(
        second.rowRect.top - first.rowRect.top, kMinimumInteractiveTextRowHeightDip, 0.5f, "tree hit-test row cadence follows the shared row-height minimum");
}

void TestTreeCompactDensityShrinksRowMetrics()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 160.0f));
    tree->SetRowHeightDip(30.0f);

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });
    tree->SetModel(&model);
    host.SetRoot(std::move(root));

    ThemePalette standardTheme = MakeAnimatedTestThemePalette(false);
    standardTheme.density      = Density::Standard;
    host.SetTheme(standardTheme);
    const TreeItemLayoutMetrics standardFirst  = tree->GetItemLayoutMetrics(host, 0u);
    const TreeItemLayoutMetrics standardSecond = tree->GetItemLayoutMetrics(host, 1u);
    const float standardHeight                 = standardFirst.rowRect.bottom - standardFirst.rowRect.top;
    const float standardCadence                = standardSecond.rowRect.top - standardFirst.rowRect.top;

    ThemePalette compactTheme = standardTheme;
    compactTheme.density      = Density::Compact;
    host.SetTheme(compactTheme);
    const TreeItemLayoutMetrics compactFirst  = tree->GetItemLayoutMetrics(host, 0u);
    const TreeItemLayoutMetrics compactSecond = tree->GetItemLayoutMetrics(host, 1u);
    const float compactHeight                 = compactFirst.rowRect.bottom - compactFirst.rowRect.top;
    const float compactCadence                = compactSecond.rowRect.top - compactFirst.rowRect.top;

    Require(compactHeight + 0.5f < standardHeight, "compact tree rows shrink below standard density");
    Require(compactCadence + 0.5f < standardCadence, "compact tree hit-test cadence shrinks below standard density");
    Require(compactHeight >= kMinimumInteractiveTextRowHeightDip, "compact tree rows keep the shared text-row minimum");
}

void TestTreeHoveredClippedTextShowsFullTextTooltip()
{
    using namespace DxUi;

    WindowHost host;

    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 72.0f));

    const std::wstring clippedText = L"Extremely long tree item text that cannot fit in the narrow row";
    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 1u, .text = clippedText},
    });
    tree->SetModel(&model);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 96.0f));

    const D2D1_POINT_2F hoverPoint = D2D1::Point2F(32.0f, 16.0f);
    Require(tree->OnMouseMove(host, hoverPoint, 0), "tree clipped-text hover is handled");
    Require(host.HasTooltip(), "tree clipped-text hover shows a full-text tooltip");
    Require(host.GetTooltipText() == clippedText, "tree clipped-text tooltip uses the full item text");

    Require(tree->OnMouseMove(host, hoverPoint, 0), "tree repeated clipped-text hover remains handled");
    Require(host.GetTooltipText() == clippedText, "tree repeated clipped-text hover keeps the full item tooltip");
}

} // namespace

void TestTreeDragReorderReportsDropAndEscapeCancels()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 160.0f));
    tree->SetReorderEnabled(true);

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 10u, .text = L"Top"},
        TreeItemData{.id = 20u, .text = L"Middle"},
        TreeItemData{.id = 30u, .text = L"Bottom", .hasChildren = true},
    });
    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    host.SetRoot(std::move(root));

    const std::optional<D2D1_RECT_F> top    = tree->GetVisibleItemHitRect(0u);
    const std::optional<D2D1_RECT_F> bottom = tree->GetVisibleItemHitRect(2u);
    Require(top.has_value() && bottom.has_value(), "reorder rows have hit rectangles");
    const D2D1_POINT_2F from         = D2D1::Point2F((top->left + top->right) * 0.5f, (top->top + top->bottom) * 0.5f);
    const D2D1_POINT_2F beforeBottom = D2D1::Point2F((bottom->left + bottom->right) * 0.5f, bottom->top + 2.0f);
    Require(tree->OnMouseDown(host, from, false, 0u), "reorder drag starts on a row");
    Require(tree->OnMouseMove(host, D2D1::Point2F(from.x + 1.0f, from.y + 1.0f), 0u), "a short move stays a click");
    static_cast<void>(tree->OnMouseUp(host, D2D1::Point2F(from.x + 1.0f, from.y + 1.0f), false, 0u));
    Require(delegate.reorderCount == 0u, "a click does not reorder");

    Require(tree->OnMouseDown(host, from, false, 0u), "reorder drag starts again");
    Require(tree->OnMouseMove(host, beforeBottom, 0u), "reorder drag follows the pointer");
    Require(tree->OnKeyDown(host, VK_ESCAPE, 0u), "escape cancels a row drag");
    static_cast<void>(tree->OnMouseUp(host, beforeBottom, false, 0u));
    Require(delegate.reorderCount == 0u, "escape drops the reorder");

    Require(tree->OnMouseDown(host, from, false, 0u), "reorder drag starts for the commit");
    Require(tree->OnMouseMove(host, beforeBottom, 0u), "reorder drag reaches the target");
    static_cast<void>(tree->OnMouseUp(host, beforeBottom, false, 0u));
    Require(delegate.reorderCount == 1u, "release commits one reorder");
    Require(delegate.lastDrop.sourceId == 10u && delegate.lastDrop.targetId == 30u && delegate.lastDrop.place == TreeDropPlace::Before,
            "the drop is before the target row");

    const D2D1_POINT_2F inside = D2D1::Point2F((bottom->left + bottom->right) * 0.5f, (bottom->top + bottom->bottom) * 0.5f);
    Require(tree->OnMouseDown(host, from, false, 0u), "reorder drag starts for an inside drop");
    Require(tree->OnMouseMove(host, inside, 0u), "reorder drag enters a parent row");
    static_cast<void>(tree->OnMouseUp(host, inside, false, 0u));
    Require(delegate.reorderCount == 2u && delegate.lastDrop.place == TreeDropPlace::Inside && delegate.lastDrop.targetId == 30u,
            "the middle of a parent row drops inside it");
}

void TestTreeDragReorderRejectsItsOwnSubtreeAndFollowsModelChanges()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 220.0f));
    tree->SetReorderEnabled(true);

    const TreeItemData group{.id = 10u, .text = L"Group", .depth = 0u, .hasChildren = true, .expanded = true};
    const TreeItemData child{.id = 11u, .parentId = 10u, .text = L"Child", .depth = 1u, .hasChildren = true, .expanded = true};
    const TreeItemData grandchild{.id = 12u, .parentId = 11u, .text = L"Grandchild", .depth = 2u};
    const TreeItemData sibling{.id = 20u, .text = L"Sibling", .depth = 0u};
    MutableTreeModel model;
    model.SetVisibleItems({group, child, grandchild, sibling});
    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    host.SetRoot(std::move(root));
    const auto centre = [&](size_t index)
    {
        const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(index);
        Require(rect.has_value(), "reorder rows have hit rectangles");
        return D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
    };
    const auto drag = [&](size_t from, size_t to)
    {
        Require(tree->OnMouseDown(host, centre(from), false, 0u), "a row drag starts");
        Require(tree->OnMouseMove(host, centre(to), 0u), "the row drag follows the pointer");
        static_cast<void>(tree->OnMouseUp(host, centre(to), false, 0u));
    };

    // A drop into the dragged row's own subtree would make the row its own ancestor.
    drag(0u, 1u);
    drag(0u, 2u);
    Require(delegate.reorderCount == 0u, "drops into the dragged row's own subtree are rejected");
    drag(0u, 3u);
    Require(delegate.reorderCount == 1u && delegate.lastDrop.sourceId == 10u && delegate.lastDrop.targetId == 20u, "a drop outside the subtree reports once");
    drag(2u, 0u);
    Require(delegate.reorderCount == 2u && delegate.lastDrop.sourceId == 12u && delegate.lastDrop.targetId == 10u, "an ancestor stays a valid target");

    // Rows that move mid-drag re-resolve the dragged subtree; a removed row ends the drag without a report.
    Require(tree->OnMouseDown(host, centre(1u), false, 0u), "a child row drag starts");
    Require(tree->OnMouseMove(host, centre(3u), 0u), "the child row drag previews");
    model.SetVisibleItems({TreeItemData{.id = 30u, .text = L"Inserted", .depth = 0u}, group, child, grandchild, sibling});
    tree->NotifyDataChanged();
    Require(tree->OnMouseMove(host, centre(3u), 0u), "the drag continues over the shifted rows");
    static_cast<void>(tree->OnMouseUp(host, centre(3u), false, 0u));
    Require(delegate.reorderCount == 2u, "the shifted grandchild is still inside the dragged subtree");
    Require(tree->OnMouseDown(host, centre(2u), false, 0u), "the child row drag starts again");
    Require(tree->OnMouseMove(host, centre(4u), 0u), "the child row drag previews again");
    model.SetVisibleItems({sibling});
    tree->NotifyDataChanged();
    static_cast<void>(tree->OnMouseUp(host, centre(0u), false, 0u));
    Require(delegate.reorderCount == 2u && host.GetCapturedControl() == nullptr, "a drag whose row was removed ends without a report");
}

void TestTreeIconFontAndReorderReleaseEdgeCases()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 220.0f));
    tree->SetReorderEnabled(true);
    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 0u, .text = L"Zero", .iconText = L"C"},
        TreeItemData{.id = 1u, .text = L"One", .iconText = L"\xE8B7"},
        TreeItemData{.id = 2u, .text = L"Two"},
        TreeItemData{.id = 3u, .text = L"Three"},
    });
    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    host.SetRoot(std::move(root));

    // Like Grid: letters keep the small UI font (the icon font draws them as boxes); private-use glyphs use the icon font.
    TreeDebugRowVisualState state{};
    Require(tree->DebugGetRowVisualState(host.GetTheme(), 0u, false, state) && ! state.iconUsesIconFont, "a letter icon keeps the small text font");
    Require(tree->DebugGetRowVisualState(host.GetTheme(), 1u, false, state) && state.iconUsesIconFont, "a private-use icon glyph uses the icon font");

    const auto centre = [&](size_t index)
    {
        const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(index);
        Require(rect.has_value(), "reorder rows have hit rectangles");
        return D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
    };
    // Model id 0 is an ordinary row, and the release point (not the last move) decides the drop.
    Require(tree->OnMouseDown(host, centre(0u), false, 0u), "the id 0 row drag starts");
    Require(tree->OnMouseMove(host, centre(2u), 0u), "the id 0 row drag moves");
    Require(tree->OnMouseUp(host, centre(3u), false, 0u), "a finished drag reports the release handled");
    Require(delegate.reorderCount == 1u && delegate.lastDrop.sourceId == 0u && delegate.lastDrop.targetId == 3u,
            "the id 0 row drops on the row under the release point");

    // A second button cancels the drag; the left release that follows reports nothing.
    Require(tree->OnMouseDown(host, centre(3u), false, 0u), "another drag starts");
    Require(tree->OnMouseMove(host, centre(1u), 0u), "the drag moves");
    static_cast<void>(tree->OnMouseDown(host, centre(1u), true, 0u));
    Require(host.GetCapturedControl() == nullptr, "a right press releases the drag capture");
    static_cast<void>(tree->OnMouseUp(host, centre(1u), false, 0u));
    Require(delegate.reorderCount == 1u, "the canceled drag reports nothing");

    // A click that never becomes a drag is not a handled drag.
    Require(tree->OnMouseDown(host, centre(2u), false, 0u), "a click presses a row");
    Require(! tree->OnMouseUp(host, centre(2u), false, 0u) && delegate.reorderCount == 1u, "a click without a drag is not reported as a handled drag");
}

namespace
{
// Two groups among loose rows: the visible rows are ids 1 to 7 in that order, so a range crosses a group boundary.
[[nodiscard]] std::vector<DxUi::TreeItemData> GroupedTreeItems()
{
    using namespace DxUi;
    return {
        TreeItemData{.id = 1u, .text = L"Group A", .depth = 0u, .hasChildren = true, .expanded = true},
        TreeItemData{.id = 2u, .parentId = 1u, .text = L"A one", .depth = 1u},
        TreeItemData{.id = 3u, .parentId = 1u, .text = L"A two", .depth = 1u},
        TreeItemData{.id = 4u, .text = L"Group B", .depth = 0u, .hasChildren = true, .expanded = true},
        TreeItemData{.id = 5u, .parentId = 4u, .text = L"B one", .depth = 1u},
        TreeItemData{.id = 6u, .text = L"Loose", .depth = 0u},
        TreeItemData{.id = 7u, .text = L"Other", .depth = 0u},
    };
}

// A tree in a host whose delegate records every callback and reads the tree from inside it. Pointer input goes straight to
// the tree, so clicking one row twice is never taken for a double-click.
class TreeSelectionFixture final
{
public:
    // `multiSelect` enables or disables multi-select; none leaves a tree as `Tree` constructs it, which is how a test of the
    // default sees it.
    explicit TreeSelectionFixture(std::vector<DxUi::TreeItemData> items, std::optional<bool> multiSelect = true, bool reorder = false, float heightDip = 260.0f)
    {
        EnableMotionForTest(host);
        auto root = std::make_unique<DxUi::Panel>();
        tree      = root->AddChild<DxUi::Tree>();
        tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, heightDip));
        model.SetVisibleItems(std::move(items));
        tree->SetModel(&model);
        tree->SetDelegate(&delegate);
        if (multiSelect.has_value())
        {
            tree->SetMultiSelectEnabled(multiSelect.value());
        }
        tree->SetReorderEnabled(reorder);
        delegate.observedTree = tree;
        host.SetRoot(std::move(root));
        static_cast<DxUi::Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, heightDip + 20.0f));
    }

    TreeSelectionFixture(const TreeSelectionFixture&)            = delete;
    TreeSelectionFixture& operator=(const TreeSelectionFixture&) = delete;
    TreeSelectionFixture(TreeSelectionFixture&&)                 = delete;
    TreeSelectionFixture& operator=(TreeSelectionFixture&&)      = delete;

    [[nodiscard]] D2D1_POINT_2F RowPoint(size_t visibleIndex, float fractionDown = 0.5f) const
    {
        const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(visibleIndex);
        Require(rect.has_value(), "the row has a hit rectangle");
        return D2D1::Point2F((rect->left + rect->right) * 0.5f, rect->top + ((rect->bottom - rect->top) * fractionDown));
    }

    void Press(size_t visibleIndex, UINT modifiers = 0u)
    {
        Require(tree->OnMouseDown(host, RowPoint(visibleIndex), false, modifiers), "pressing a row is handled");
    }

    void Release(size_t visibleIndex, UINT modifiers = 0u)
    {
        static_cast<void>(tree->OnMouseUp(host, RowPoint(visibleIndex), false, modifiers));
    }

    void Click(size_t visibleIndex, UINT modifiers = 0u)
    {
        Press(visibleIndex, modifiers);
        Release(visibleIndex, modifiers);
    }

    bool Key(UINT virtualKey, UINT modifiers = 0u)
    {
        return tree->OnKeyDown(host, virtualKey, modifiers);
    }

    [[nodiscard]] TreeIds Selected() const
    {
        return tree->GetSelectedItemIds();
    }

    DxUi::WindowHost host;
    MutableTreeModel model;
    RecordingTreeDelegate delegate;
    DxUi::Tree* tree = nullptr;
};

void TestTreeMultiSelectIsOptInAndSingleSelectIsUnchanged()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(6u), std::nullopt);
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    Require(! tree.MultiSelectEnabled(), "a tree selects one item until multi-select is enabled");
    Require(tree.GetSelectedItemIds().empty() && ! tree.GetSelectedItemId().has_value(), "a new tree selects nothing");

    // Modifiers change nothing while it is off: every click selects its row alone, as it always did.
    fixture.Click(1u);
    fixture.Click(3u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {4u}, "Ctrl+click without multi-select");
    fixture.Click(5u, MK_SHIFT);
    RequireTreeIds(fixture.Selected(), {6u}, "Shift+click without multi-select");
    Require(! tree.IsItemSelected(4u) && tree.IsItemSelected(6u), "a single selection names its one item");
    Require(log.selectionChangedCount == 3u, "every click still notifies the focused item");
    Require(log.selectionSetChangedCount == 0u, "the set callback belongs to multi-select");

    // The keys do too: Shift and Ctrl with a movement key move the selection, and Ctrl+A and Ctrl+Space are not commands.
    Require(fixture.Key(VK_UP, MK_SHIFT), "Shift+Up moves a single selection");
    RequireTreeIds(fixture.Selected(), {5u}, "Shift+Up without multi-select");
    Require(fixture.Key(VK_DOWN, MK_CONTROL), "Ctrl+Down moves a single selection");
    RequireTreeIds(fixture.Selected(), {6u}, "Ctrl+Down without multi-select");
    Require(! fixture.Key('A', MK_CONTROL) && ! tree.OnSelectAll(fixture.host), "Ctrl+A selects nothing without multi-select");
    RequireTreeIds(fixture.Selected(), {6u}, "Ctrl+A without multi-select");
    Require(fixture.Key(VK_SPACE, MK_CONTROL) && log.invokedCount == 1u && log.lastInvokedItemId == 6u, "Ctrl+Space still invokes the item");
    RequireTreeIds(fixture.Selected(), {6u}, "Ctrl+Space without multi-select");
    Require(tree.GetFocusedItemId() == tree.GetSelectedItemId(), "the selected item is the focused item");

    // The setters and requests keep their single meaning.
    Require(! tree.RequestRemoveVisibleItemFromSelection(5u), "there is no membership to remove without multi-select");
    Require(tree.RequestAddVisibleItemToSelection(0u), "adding selects the item without multi-select");
    RequireTreeIds(fixture.Selected(), {1u}, "Add without multi-select");
    tree.SetSelectedItemIds(std::vector<uint64_t>{2u, 3u});
    RequireTreeIds(fixture.Selected(), {3u}, "a list of ids selects its last visible item without multi-select");
    tree.SetSelectedItemIds({});
    Require(fixture.Selected().empty() && ! tree.GetSelectedItemId().has_value(), "an empty list clears the selection");
    Require(log.selectionSetChangedCount == 0u, "no gesture of a single-select tree reaches the set callback");

    // Characters are typeahead whatever modifier came with them, as before: a row that starts with a space is what the
    // space character of Ctrl+Space finds, and only a multi-select tree keeps that character out of typeahead.
    TreeSelectionFixture spaced({TreeItemData{.id = 1u, .text = L" Leading space"}, TreeItemData{.id = 2u, .text = L"Beta"}}, std::nullopt);
    spaced.tree->SetSelectedItemId(2u);
    Require(spaced.tree->OnChar(spaced.host, L' ', MK_CONTROL), "a space with Ctrl is typeahead without multi-select");
    RequireTreeIds(spaced.Selected(), {1u}, "typeahead selected the row that starts with a space");
}

void TestTreeCtrlAndShiftClickBuildTheSelectionInVisibleOrder()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(GroupedTreeItems());
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;

    // A plain click selects one row and is the anchor.
    fixture.Click(1u);
    RequireTreeIds(fixture.Selected(), {2u}, "plain click");
    Require(log.selectionChangedCount == 1u && log.lastSelectedItemId == 2u, "a plain click notifies the focused item");
    Require(log.selectionSetChangedCount == 1u, "a plain click changes the selection set once");
    RequireTreeIds(log.lastSelectionSet, {2u}, "the set callback of a plain click");
    Require(fixture.host.GetFocusControl() == &tree, "a click focuses the tree");

    // Ctrl+click toggles one row and leaves the others; the set keeps the model's order, whichever row joins last.
    fixture.Click(3u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 4u}, "Ctrl+click adds a row");
    fixture.Click(5u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 4u, 6u}, "Ctrl+click adds another row");
    fixture.Click(0u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {1u, 2u, 4u, 6u}, "a row added above the others keeps the model's order");
    fixture.Click(3u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {1u, 2u, 6u}, "Ctrl+click on a selected row removes it");
    Require(! tree.IsItemSelected(4u) && tree.GetSelectedItemId() == 4u, "the row Ctrl+click removed is the focused item but not selected");
    fixture.Click(0u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 6u}, "Ctrl+click removes the first row");
    RequireTreeIds(log.lastSelectionSet, {2u, 6u}, "the set callback names the selection after the toggle");
    Require(log.selectionSetChangedCount == 6u, "each toggle changed the set once");

    // Shift+click selects the visible range from the anchor, the row of the last plain click, and drops the rest.
    fixture.Click(4u, MK_SHIFT);
    RequireTreeIds(fixture.Selected(), {2u, 3u, 4u, 5u}, "Shift+click selects the range across a group boundary");
    Require(tree.GetSelectedItemId() == 5u, "the clicked row of a range is the focused item");
    fixture.Click(0u, MK_SHIFT);
    RequireTreeIds(fixture.Selected(), {1u, 2u}, "Shift+click above the anchor selects the range upward");
    RequireTreeIds(log.lastSelectionSet, {1u, 2u}, "the set callback of a range");

    // A plain click collapses the selection to its row.
    fixture.Click(6u);
    RequireTreeIds(fixture.Selected(), {7u}, "plain click after a range");
    fixture.Click(5u, MK_SHIFT);
    RequireTreeIds(fixture.Selected(), {6u, 7u}, "the plain click moved the anchor");

    // Clicking the one selected row again changes no set: its row is still announced, and the set is not.
    fixture.Click(6u);
    const size_t setChangesBefore = log.selectionSetChangedCount;
    const size_t focusMovesBefore = log.selectionChangedCount;
    fixture.Click(6u);
    RequireTreeIds(fixture.Selected(), {7u}, "the same row clicked again");
    Require(log.selectionSetChangedCount == setChangesBefore, "an unchanged selection is not reported again");
    Require(log.selectionChangedCount == focusMovesBefore + 1u, "a click always announces the row it landed on");

    // Shift+click with no anchor takes the focused row as one.
    TreeSelectionFixture unanchored(FlatTreeItems(5u));
    unanchored.tree->SetFocusedItemId(2u);
    unanchored.Click(3u, MK_SHIFT);
    RequireTreeIds(unanchored.Selected(), {2u, 3u, 4u}, "Shift+click extends from the focused row when nothing is selected");
}

// The host delivers Ctrl and Shift with the press (MK_CONTROL, MK_SHIFT), so the gesture reaches the tree from a message.
void TestTreeCtrlAndShiftClickThroughTheHostMessages()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(6u));
    const auto post = [&](UINT message, size_t visibleIndex, WPARAM flags)
    {
        const D2D1_POINT_2F point = fixture.RowPoint(visibleIndex);
        bool handled              = false;
        static_cast<void>(fixture.host.HandleMessage(nullptr, message, flags, MAKELPARAM(static_cast<int>(point.x), static_cast<int>(point.y)), handled));
        Require(handled, "the host handles the pointer message");
    };
    const auto click = [&](size_t visibleIndex, WPARAM flags)
    {
        post(WM_LBUTTONDOWN, visibleIndex, MK_LBUTTON | flags);
        post(WM_LBUTTONUP, visibleIndex, flags);
    };
    click(1u, 0u);
    click(3u, MK_CONTROL);
    click(5u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 4u, 6u}, "host Ctrl+click");
    click(2u, MK_SHIFT);
    RequireTreeIds(fixture.Selected(), {2u, 3u}, "host Shift+click");
    Require(fixture.delegate.selectionSetChangedCount == 4u, "each host gesture reached the tree once");
}

void TestTreeKeyboardExtendsTogglesAndSelectsAll()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(9u));
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    fixture.Click(2u);
    RequireTreeIds(fixture.Selected(), {3u}, "the click that starts the keyboard run");

    // Shift with a movement key extends from the anchor, and follows the focus back across it.
    Require(fixture.Key(VK_DOWN, MK_SHIFT), "Shift+Down is handled");
    RequireTreeIds(fixture.Selected(), {3u, 4u}, "Shift+Down");
    Require(fixture.Key(VK_DOWN, MK_SHIFT), "Shift+Down again is handled");
    RequireTreeIds(fixture.Selected(), {3u, 4u, 5u}, "Shift+Down twice");
    Require(fixture.Key(VK_UP, MK_SHIFT), "Shift+Up is handled");
    RequireTreeIds(fixture.Selected(), {3u, 4u}, "Shift+Up shrinks the range");
    Require(fixture.Key(VK_UP, MK_SHIFT) && fixture.Key(VK_UP, MK_SHIFT), "Shift+Up twice is handled");
    RequireTreeIds(fixture.Selected(), {2u, 3u}, "Shift+Up reaches past the anchor");
    Require(fixture.Key(VK_UP, MK_SHIFT), "Shift+Up a last time is handled");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u}, "Shift+Up extends upward from the anchor");
    Require(tree.GetFocusedItemId() == 1u, "the focus is at the moving end of the range");

    // Ctrl with a movement key moves the focus alone, and Ctrl+Space toggles the row it reached.
    const size_t setChangesBeforeFocus = log.selectionSetChangedCount;
    for (int step = 0; step < 4; ++step)
    {
        Require(fixture.Key(VK_DOWN, MK_CONTROL), "Ctrl+Down is handled");
    }
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u}, "Ctrl+Down moves no selection");
    Require(tree.GetFocusedItemId() == 5u && ! tree.IsItemSelected(5u), "Ctrl+Down left the focus on a row outside the selection");
    Require(log.selectionSetChangedCount == setChangesBeforeFocus, "moving the focus alone does not change the set");
    Require(fixture.Key(VK_SPACE, MK_CONTROL), "Ctrl+Space is handled");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u, 5u}, "Ctrl+Space adds the focused row");
    Require(log.invokedCount == 0u, "Ctrl+Space toggles instead of invoking");
    Require(fixture.Key(VK_SPACE, MK_CONTROL), "Ctrl+Space a second time is handled");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u}, "Ctrl+Space removes the focused row");
    Require(fixture.Key(VK_SPACE) && log.invokedCount == 1u && log.lastInvokedItemId == 5u, "plain Space still invokes the focused row");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u}, "Space leaves the selection alone");

    // Shift with Home, End and Page Down reaches the ends and the page the keys always did, and the anchor stayed put.
    Require(fixture.Key(VK_END, MK_SHIFT), "Shift+End is handled");
    RequireTreeIds(fixture.Selected(), {3u, 4u, 5u, 6u, 7u, 8u, 9u}, "Shift+End");
    Require(fixture.Key(VK_HOME, MK_SHIFT), "Shift+Home is handled");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u}, "Shift+Home");

    // Ctrl+A selects every visible row and keeps the focused one, which the delegate already knew.
    const size_t focusMovesBeforeAll = log.selectionChangedCount;
    Require(fixture.Key('A', MK_CONTROL), "Ctrl+A is handled");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u}, "Ctrl+A");
    Require(tree.GetFocusedItemId() == 1u && log.selectionChangedCount == focusMovesBeforeAll, "Ctrl+A moves no focus");
    RequireTreeIds(log.lastSelectionSet, {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u}, "the set callback of Ctrl+A");
    const size_t setChangesAfterAll = log.selectionSetChangedCount;
    Require(tree.OnSelectAll(fixture.host), "selecting everything twice is handled");
    Require(log.selectionSetChangedCount == setChangesAfterAll, "selecting everything again reports no change");

    // A movement key without a modifier collapses to the row it reaches, like Grid, and typeahead does too.
    Require(fixture.Key(VK_DOWN), "plain Down is handled");
    RequireTreeIds(fixture.Selected(), {2u}, "plain Down collapses the selection");
    Require(fixture.Key(VK_HOME, MK_CONTROL), "Ctrl+Home is handled");
    RequireTreeIds(fixture.Selected(), {2u}, "Ctrl+Home moves no selection");
    Require(tree.GetFocusedItemId() == 1u, "Ctrl+Home moved the focus");
    Require(fixture.Key(VK_NEXT, MK_SHIFT), "Shift+Page Down is handled");
    RequireTreeIds(fixture.Selected(), {2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u}, "Shift+Page Down extends from the anchor by a page");
    Require(tree.OnChar(fixture.host, L'r', 0u), "typeahead is handled");
    RequireTreeIds(fixture.Selected(), {1u}, "typeahead collapses the selection to its match");
    Require(! tree.OnChar(fixture.host, L' ', MK_CONTROL), "the space character of Ctrl+Space is not typeahead");
    RequireTreeIds(fixture.Selected(), {1u}, "the Ctrl+Space character selected nothing");

    // A row that starts with a space would be found by that character, which is what this keeps out of typeahead.
    TreeSelectionFixture spaced({TreeItemData{.id = 1u, .text = L" Leading space"}, TreeItemData{.id = 2u, .text = L"Beta"}});
    spaced.tree->SetFocusedItemId(2u);
    Require(! spaced.tree->OnChar(spaced.host, L' ', MK_CONTROL), "the space character of Ctrl+Space finds no row");
    Require(spaced.Selected().empty() && spaced.tree->GetFocusedItemId() == 2u, "the Ctrl+Space character moved nothing");
    Require(spaced.tree->OnChar(spaced.host, L' ', 0u), "a space without Ctrl is typeahead");
    RequireTreeIds(spaced.Selected(), {1u}, "typeahead selected the row that starts with a space");
}

// The host keeps Shift and Ctrl as the state of the key messages it saw, and gives it to the focused tree with every key.
void TestTreeKeyboardGesturesThroughTheHostMessages()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(8u));
    fixture.host.SetFocusControl(fixture.tree);
    const auto key = [&](UINT message, WPARAM virtualKey)
    {
        bool handled = false;
        static_cast<void>(fixture.host.HandleMessage(nullptr, message, virtualKey, 0, handled));
        Require(handled || message == WM_KEYUP, "the host handles the key message");
    };
    fixture.Click(1u);
    RequireTreeIds(fixture.Selected(), {2u}, "the click that starts the key messages");

    key(WM_KEYDOWN, VK_SHIFT);
    key(WM_KEYDOWN, VK_DOWN);
    key(WM_KEYDOWN, VK_DOWN);
    key(WM_KEYUP, VK_SHIFT);
    RequireTreeIds(fixture.Selected(), {2u, 3u, 4u}, "Shift held through two Down messages");

    key(WM_KEYDOWN, VK_CONTROL);
    key(WM_KEYDOWN, VK_DOWN);
    key(WM_KEYDOWN, VK_DOWN);
    RequireTreeIds(fixture.Selected(), {2u, 3u, 4u}, "Ctrl held through two Down messages moves the focus alone");
    key(WM_KEYDOWN, VK_SPACE);
    bool handled = false;
    static_cast<void>(fixture.host.HandleMessage(nullptr, WM_CHAR, L' ', 0, handled));
    RequireTreeIds(fixture.Selected(), {2u, 3u, 4u, 6u}, "Ctrl+Space toggles the focused row, and its space character is not typeahead");
    key(WM_KEYDOWN, 'A');
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}, "Ctrl+A selects every row");
    key(WM_KEYUP, VK_CONTROL);

    key(WM_KEYDOWN, VK_UP);
    RequireTreeIds(fixture.Selected(), {5u}, "a key after the modifier was released selects one row");
    Require(fixture.delegate.invokedCount == 0u, "no key of the run invoked a row");
}

void TestTreeKeyboardGesturesStartFromTheFirstRowWhenNothingIsFocused()
{
    using namespace DxUi;

    {
        TreeSelectionFixture fixture(FlatTreeItems(5u));
        Require(fixture.Key(VK_SPACE, MK_CONTROL), "Ctrl+Space is handled on a tree that has no focused row");
        RequireTreeIds(fixture.Selected(), {1u}, "Ctrl+Space selects the first row");
        Require(fixture.tree->GetFocusedItemId() == 1u, "Ctrl+Space focuses the first row");
    }
    {
        TreeSelectionFixture fixture(FlatTreeItems(5u));
        Require(fixture.Key(VK_DOWN, MK_SHIFT), "Shift+Down is handled on a tree that has no focused row");
        RequireTreeIds(fixture.Selected(), {1u, 2u}, "Shift+Down starts its range at the first row");
    }
    {
        TreeSelectionFixture fixture(FlatTreeItems(5u));
        Require(fixture.Key(VK_DOWN, MK_CONTROL), "Ctrl+Down is handled on a tree that has no focused row");
        Require(fixture.Selected().empty() && fixture.tree->GetFocusedItemId() == 2u, "Ctrl+Down selects nothing");
        Require(fixture.Key('A', MK_CONTROL), "Ctrl+A is handled on a tree that has a focused row but no selection");
        Require(fixture.Selected().size() == 5u && fixture.tree->GetFocusedItemId() == 2u, "Ctrl+A keeps the focused row");
    }
    {
        TreeSelectionFixture fixture(FlatTreeItems(5u));
        Require(fixture.Key('A', MK_CONTROL), "Ctrl+A is handled on a tree that has nothing focused");
        Require(fixture.Selected().size() == 5u && fixture.tree->GetFocusedItemId() == 1u, "Ctrl+A focuses the first row");
        Require(fixture.delegate.callOrder == "PS", "the delegate hears of the focus it was given and of the selection, in that order");
    }
}

// Left and Right move between a group and its rows, or expand and collapse the focused group. Moving follows the rules of
// every other movement key; expanding and collapsing never touch the selection, whatever modifier is held.
void TestTreeGroupKeysFollowTheModifierRulesAndLeaveTheSelectionWhenTheyExpand()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(GroupedTreeItems());
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    // The visible rows are ids 1 (Group A), 2, 3, 4 (Group B), 5, 6 and 7.

    fixture.Click(2u);
    fixture.Click(4u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {3u, 5u}, "the multi-selection the group keys start from");
    Require(fixture.Key(VK_LEFT), "Left is handled on a row of a group");
    RequireTreeIds(fixture.Selected(), {4u}, "Left moves to the parent and selects it alone");
    Require(tree.GetFocusedItemId() == 4u, "the parent is the focused row");
    Require(fixture.Key(VK_RIGHT), "Right is handled on an expanded group");
    RequireTreeIds(fixture.Selected(), {5u}, "Right moves to the first row of the group and selects it alone");

    // Shift extends from the anchor to the parent, then to the first row of the group.
    fixture.Click(2u);
    Require(fixture.Key(VK_LEFT, MK_SHIFT), "Shift+Left is handled");
    RequireTreeIds(fixture.Selected(), {1u, 2u, 3u}, "Shift+Left extends the range from the anchor to the parent");
    Require(tree.GetFocusedItemId() == 1u, "the parent is the focused row at the end of the range");
    Require(fixture.Key(VK_RIGHT, MK_SHIFT), "Shift+Right is handled");
    RequireTreeIds(fixture.Selected(), {2u, 3u}, "Shift+Right extends the range from the anchor to the first row of the group");
    Require(tree.GetFocusedItemId() == 2u, "that row is the focused row");

    // Ctrl moves the focus alone.
    fixture.Click(2u);
    const size_t setChanges = log.selectionSetChangedCount;
    Require(fixture.Key(VK_LEFT, MK_CONTROL), "Ctrl+Left is handled");
    RequireTreeIds(fixture.Selected(), {3u}, "Ctrl+Left moves no selection");
    Require(tree.GetFocusedItemId() == 1u, "Ctrl+Left moved the focus to the parent");
    Require(fixture.Key(VK_RIGHT, MK_CONTROL), "Ctrl+Right is handled");
    RequireTreeIds(fixture.Selected(), {3u}, "Ctrl+Right moves no selection");
    Require(tree.GetFocusedItemId() == 2u, "Ctrl+Right moved the focus to the first row of the group");
    Require(log.selectionSetChangedCount == setChanges, "moving the focus alone does not change the set");

    // Left on an expanded group asks for its collapse and changes neither the selection nor the focus, with Shift too.
    Require(fixture.Key(VK_LEFT, MK_CONTROL), "Ctrl+Left returns to the group");
    const size_t togglesBefore = log.toggleCount;
    Require(fixture.Key(VK_LEFT, MK_SHIFT), "Shift+Left is handled on an expanded group");
    Require(log.toggleCount == togglesBefore + 1u && log.lastToggledItemId == 1u && ! log.lastExpandedState, "Left on an expanded group asks for its collapse");
    RequireTreeIds(fixture.Selected(), {3u}, "collapsing a group with Shift held extends nothing");
    Require(tree.GetFocusedItemId() == 1u && log.selectionSetChangedCount == setChanges, "collapsing moves neither the focus nor the selection");

    // Right on a collapsed group asks for its expansion, and leaves the selection alone too.
    TreeSelectionFixture collapsed({TreeItemData{.id = 1u, .text = L"Group", .depth = 0u, .hasChildren = true, .expanded = false},
                                    TreeItemData{.id = 2u, .text = L"Loose", .depth = 0u},
                                    TreeItemData{.id = 3u, .text = L"Other", .depth = 0u}});
    collapsed.Click(1u);
    collapsed.Click(2u, MK_CONTROL);
    RequireTreeIds(collapsed.Selected(), {2u, 3u}, "the selection next to a collapsed group");
    collapsed.tree->SetFocusedItemId(1u);
    const size_t collapsedSetChanges = collapsed.delegate.selectionSetChangedCount;
    Require(collapsed.Key(VK_RIGHT, MK_SHIFT), "Shift+Right is handled on a collapsed group");
    Require(collapsed.delegate.toggleCount == 1u && collapsed.delegate.lastToggledItemId == 1u && collapsed.delegate.lastExpandedState,
            "Right on a collapsed group asks for its expansion");
    RequireTreeIds(collapsed.Selected(), {2u, 3u}, "expanding a group with Shift held changes no selection");
    Require(collapsed.delegate.selectionSetChangedCount == collapsedSetChanges && collapsed.tree->GetFocusedItemId() == 1u,
            "expanding moves neither the focus nor the selection");
}

void TestTreeSelectionCallbacksFireOncePerChangeAndSeeTheFinishedSelection()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(6u));
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    const auto expectCalls     = [&](const char* order, const char* context)
    {
        Require(log.callOrder == order, context);
        log.callOrder.clear();
    };

    fixture.Click(1u);
    expectCalls("PS", "a click that changes the selection announces the row, then the set");
    RequireTreeIds(log.idsSeenByPrimaryCallback, {2u}, "the focused-item callback of a click already sees its selection");
    RequireTreeIds(log.idsSeenBySetCallback, {2u}, "the set callback sees the selection it reports");
    fixture.Click(1u);
    expectCalls("P", "a click that leaves the set alone announces the row only");
    fixture.Click(3u, MK_CONTROL);
    expectCalls("PS", "Ctrl+click that adds a row");
    RequireTreeIds(log.idsSeenBySetCallback, {2u, 4u}, "the set callback of Ctrl+click sees both rows");
    fixture.Click(3u, MK_CONTROL);
    expectCalls("PS", "Ctrl+click that removes a row");
    RequireTreeIds(log.lastSelectionSet, {2u}, "the set after the removal");
    Require(fixture.Key(VK_DOWN, MK_CONTROL), "Ctrl+Down is handled");
    expectCalls("P", "moving the focus alone announces the row only");
    Require(fixture.Key('A', MK_CONTROL), "Ctrl+A is handled");
    expectCalls("S", "Ctrl+A reports the new set only: the focused row stays");
    Require(tree.OnSelectAll(fixture.host), "Ctrl+A again is handled");
    expectCalls("", "an unchanged set reports nothing");

    // The requests a UI Automation client makes notify like the gestures they stand for.
    Require(tree.RequestSelectVisibleItem(4u), "Select is handled");
    expectCalls("PS", "Select replaces the selection");
    RequireTreeIds(fixture.Selected(), {5u}, "Select");
    Require(tree.RequestAddVisibleItemToSelection(1u), "AddToSelection is handled");
    expectCalls("PS", "AddToSelection adds the row");
    Require(tree.RequestAddVisibleItemToSelection(1u), "AddToSelection of a selected row is handled");
    expectCalls("P", "AddToSelection of a selected row changes no set");
    RequireTreeIds(fixture.Selected(), {2u, 5u}, "AddToSelection is not a toggle");
    Require(tree.RequestRemoveVisibleItemFromSelection(1u), "RemoveFromSelection is handled");
    expectCalls("S", "RemoveFromSelection reports the set and leaves the focus");
    RequireTreeIds(fixture.Selected(), {5u}, "RemoveFromSelection");
    Require(tree.RequestRemoveVisibleItemFromSelection(1u), "RemoveFromSelection of a row that is not selected is handled");
    expectCalls("", "removing a row that is not selected reports nothing");
    Require(! tree.RequestRemoveVisibleItemFromSelection(99u) && ! tree.RequestAddVisibleItemToSelection(99u), "a row that does not exist is refused");

    // The setters are silent: the application that calls them knows what it set.
    tree.SetSelectedItemId(3u);
    tree.SetSelectedItemIds(std::vector<uint64_t>{1u, 4u});
    tree.SetFocusedItemId(2u);
    tree.SetMultiSelectEnabled(false);
    tree.SetMultiSelectEnabled(true);
    expectCalls("", "no setter notifies");
}

void TestTreeSelectionSetCallbackMayDestroyTheTree()
{
    using namespace DxUi;

    class RootReplacingDelegate final : public ITreeDelegate
    {
    public:
        explicit RootReplacingDelegate(WindowHost& host) noexcept : _host(host)
        {
        }

        RootReplacingDelegate(const RootReplacingDelegate&)            = delete;
        RootReplacingDelegate(RootReplacingDelegate&&)                 = delete;
        RootReplacingDelegate& operator=(const RootReplacingDelegate&) = delete;
        RootReplacingDelegate& operator=(RootReplacingDelegate&&)      = delete;

        void OnTreeSelectionSetChanged(std::span<const uint64_t> selectedItemIds) override
        {
            ++setChanges;
            selected.assign(selectedItemIds.begin(), selectedItemIds.end());
            _host.SetRoot(std::make_unique<Panel>());
        }

        size_t setChanges = 0u;
        std::vector<uint64_t> selected;

    private:
        WindowHost& _host;
    };

    // Every gesture that reaches the set callback: each leaves nothing of the destroyed tree to touch afterwards.
    for (int gesture = 0; gesture < 5; ++gesture)
    {
        WindowHost host;
        EnableMotionForTest(host);
        auto root  = std::make_unique<Panel>();
        auto* tree = root->AddChild<Tree>();
        tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 260.0f));
        MutableTreeModel model;
        model.SetVisibleItems(FlatTreeItems(5u));
        RootReplacingDelegate delegate(host);
        tree->SetModel(&model);
        tree->SetDelegate(&delegate);
        tree->SetMultiSelectEnabled(true);
        host.SetRoot(std::move(root));
        static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 280.0f));
        const auto rowPoint = [&](size_t index)
        {
            const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(index);
            Require(rect.has_value(), "the row has a hit rectangle");
            return D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
        };
        switch (gesture)
        {
            case 0: Require(tree->OnMouseDown(host, rowPoint(1u), false, 0u), "a click is handled"); break;
            case 1: Require(tree->OnMouseDown(host, rowPoint(1u), false, MK_CONTROL), "a Ctrl+click is handled"); break;
            case 2: Require(tree->OnKeyDown(host, VK_SPACE, MK_CONTROL), "Ctrl+Space is handled"); break;
            case 3: Require(tree->OnSelectAll(host), "Ctrl+A is handled"); break;
            default: static_cast<void>(tree->RequestSelectVisibleItem(2u)); break; // False once the delegate destroyed the tree.
        }
        Require(delegate.setChanges == 1u && ! delegate.selected.empty(), "the set callback ran once for the gesture");
        Require(host.GetRoot() != nullptr, "the callback replaced the root without the tree touching itself afterwards");
    }

    // A release that collapses a multi-selection reaches the callback too.
    WindowHost host;
    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 260.0f));
    MutableTreeModel model;
    model.SetVisibleItems(FlatTreeItems(5u));
    RootReplacingDelegate delegate(host);
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    tree->SetMultiSelectEnabled(true);
    tree->SetReorderEnabled(true);
    tree->SetSelectedItemIds(std::vector<uint64_t>{2u, 3u});
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 280.0f));
    const std::optional<D2D1_RECT_F> rect = tree->GetVisibleItemHitRect(2u);
    Require(rect.has_value(), "the third row has a hit rectangle");
    const D2D1_POINT_2F point = D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
    Require(tree->OnMouseDown(host, point, false, 0u), "a press on a selected row is handled");
    Require(delegate.setChanges == 0u, "the press leaves the selection for the release");
    static_cast<void>(tree->OnMouseUp(host, point, false, 0u));
    Require(delegate.setChanges == 1u && host.GetRoot() != nullptr, "the collapsing release reached the callback, and the tree did not touch itself after it");
}

void TestTreeMultiSelectionSurvivesModelChangesExpandCollapseAndScrolling()
{
    using namespace DxUi;

    const TreeItemData group{.id = 1u, .text = L"Group", .depth = 0u, .hasChildren = true, .expanded = true};
    const TreeItemData collapsedGroup{.id = 1u, .text = L"Group", .depth = 0u, .hasChildren = true, .expanded = false};
    const auto row      = [](uint64_t id, uint32_t depth = 0u) { return TreeItemData{.id = id, .text = L"Row " + std::to_wstring(id), .depth = depth}; };
    const auto expanded = [&]() { return std::vector<TreeItemData>{group, row(2u, 1u), row(3u, 1u), row(4u), row(5u), row(6u), row(7u), row(8u)}; };

    TreeSelectionFixture fixture(expanded());
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    fixture.Click(1u);
    fixture.Click(2u, MK_CONTROL);
    fixture.Click(4u, MK_CONTROL);
    fixture.Click(6u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 3u, 5u, 7u}, "the selection the model changes start from");
    const size_t setChanges = log.selectionSetChangedCount;

    // Collapsing the group hides two selected rows: they leave the selection, the others stay, nothing else is reported.
    fixture.model.SetVisibleItems({collapsedGroup, row(4u), row(5u), row(6u), row(7u), row(8u)});
    tree.NotifyDataChanged();
    RequireTreeIds(fixture.Selected(), {5u, 7u}, "collapsing drops the rows it hides");
    Require(log.selectionSetChangedCount == setChanges + 1u, "the delegate hears of the dropped rows once");
    RequireTreeIds(log.lastSelectionSet, {5u, 7u}, "the set callback of a collapse");
    Require(tree.GetSelectedItemId() == 7u, "the focused row survived");

    // Expanding shows them again, unselected.
    fixture.model.SetVisibleItems(expanded());
    tree.NotifyDataChanged();
    RequireTreeIds(fixture.Selected(), {5u, 7u}, "expanding selects nothing");
    Require(log.selectionSetChangedCount == setChanges + 1u, "an unchanged selection is not reported");

    // Rows that move keep their selection and the order follows the model; moving is not a change of selection.
    fixture.model.SetVisibleItems({row(7u), group, row(2u, 1u), row(3u, 1u), row(4u), row(5u), row(6u), row(8u)});
    tree.NotifyDataChanged();
    RequireTreeIds(fixture.Selected(), {7u, 5u}, "the selection follows the model's order");
    Require(log.selectionSetChangedCount == setChanges + 1u, "rows that only moved change no selection");

    // A removed row leaves it, and so does the focused row.
    fixture.model.SetVisibleItems({row(7u), group, row(2u, 1u), row(3u, 1u), row(4u), row(6u), row(8u)});
    tree.NotifyDataChanged();
    RequireTreeIds(fixture.Selected(), {7u}, "a removed row leaves the selection");
    Require(log.selectionSetChangedCount == setChanges + 2u, "the removal was reported once");
    const size_t focusMoves = log.selectionChangedCount;
    fixture.model.SetVisibleItems({group, row(2u, 1u), row(3u, 1u), row(4u), row(6u), row(8u)});
    tree.NotifyDataChanged();
    Require(fixture.Selected().empty() && ! tree.GetSelectedItemId().has_value(), "the last selected row left");
    Require(log.selectionSetChangedCount == setChanges + 3u && log.lastSelectionSet.empty(), "an empty selection is reported");
    Require(log.selectionChangedCount == focusMoves, "a focused row that left is not announced, as before");
    Require(tree.GetFocusedItemId() == tree.GetSelectedItemId(), "focus and selection agree that nothing is left");

    // Another model keeps the ids it also shows.
    tree.SetSelectedItemIds(std::vector<uint64_t>{2u, 4u, 8u});
    RequireTreeIds(fixture.Selected(), {2u, 4u, 8u}, "the selection set for the model swap");
    const size_t beforeSwap = log.selectionSetChangedCount;
    MutableTreeModel other;
    other.SetVisibleItems({row(8u), row(4u), row(100u)});
    tree.SetModel(&other);
    RequireTreeIds(fixture.Selected(), {8u, 4u}, "a new model keeps the selected ids it shows, in its order");
    Require(log.selectionSetChangedCount == beforeSwap + 1u, "the model swap reported its dropped row once");
    tree.SetModel(nullptr);
    Require(fixture.Selected().empty() && log.selectionSetChangedCount == beforeSwap + 2u, "a tree without a model selects nothing");
    tree.SetModel(&fixture.model);

    // Scrolling changes no selection, and a click after it toggles the row that is under the pointer.
    TreeSelectionFixture tall(FlatTreeItems(40u), true, false, 140.0f);
    tall.Click(1u);
    tall.Click(3u, MK_CONTROL);
    Require(tall.tree->OnMouseWheel(tall.host, D2D1::Point2F(40.0f, 40.0f), -static_cast<float>(WHEEL_DELTA) * 2.0f, 0u), "the wheel scrolls");
    Require(tall.tree->GetFirstVisibleItemIndex() > 0u, "the tree scrolled");
    RequireTreeIds(tall.Selected(), {2u, 4u}, "scrolling keeps the selection");
    const size_t first = tall.tree->GetFirstVisibleItemIndex();
    Require(tall.tree->OnMouseDown(tall.host, D2D1::Point2F(40.0f, 16.0f), false, MK_CONTROL), "a Ctrl+click after scrolling is handled");
    RequireTreeIds(tall.Selected(), {2u, 4u, static_cast<uint64_t>(first + 1u)}, "a click after scrolling toggles the row under the pointer");
    Require(tall.Key(VK_END, MK_CONTROL), "Ctrl+End is handled");
    Require(tall.tree->GetFocusedItemId() == 40u && tall.Selected().size() == 3u, "Ctrl+End scrolls to the last row and keeps the selection");
    Require(tall.tree->GetFirstVisibleItemIndex() > first, "moving the focus scrolls it into view");
}

void TestTreeMultiSelectPaintsEverySelectedRowWithTheSelectionColors()
{
    using namespace DxUi;

    const ThemePalette theme = MakeAnimatedTestThemePalette(false);
    TreeSelectionFixture fixture(FlatTreeItems(5u));
    Tree& tree = *fixture.tree;
    fixture.host.SetTheme(theme);
    tree.SetSelectedItemIds(std::vector<uint64_t>{1u, 3u, 4u});
    fixture.host.SetFocusControl(&tree);
    TreeDebugRowVisualState state{};

    // Every selected row takes the selection colors, as Grid rows do; only the focused row shows the focus ring.
    for (const size_t index : {0u, 2u, 3u})
    {
        Require(tree.DebugGetRowVisualState(theme, index, true, state) && state.selected, "a selected row reports itself selected");
        Require(state.fillArgb == PackColorForTest(theme.selectionFill), "a selected row of a focused tree is filled with the selection color");
        Require(state.textArgb == PackColorForTest(theme.selectionText), "a selected row of a focused tree uses the selection text color");
        Require(state.current == (index == 3u) && state.showFocus == (index == 3u), "only the focused row owns the focus ring");
    }
    for (const size_t index : {1u, 4u})
    {
        Require(tree.DebugGetRowVisualState(theme, index, true, state) && ! state.selected && ! state.current && ! state.showFocus,
                "a row outside the selection is plain");
        Require(state.fillArgb == 0u && state.textArgb == PackColorForTest(theme.text), "a row outside the selection keeps the row colors");
    }

    // A focused row outside the selection owns the ring, and the selected row it left has none.
    tree.SetFocusedItemId(2u);
    Require(tree.DebugGetRowVisualState(theme, 1u, true, state) && ! state.selected && state.current && state.showFocus,
            "the focused row shows the ring without being selected");
    Require(tree.DebugGetRowVisualState(theme, 3u, true, state) && state.selected && ! state.showFocus, "a selected row that is not focused has no ring");
    Require(tree.DebugGetRowVisualState(theme, 1u, false, state) && ! state.showFocus, "the ring follows keyboard-focus visibility");

    // An unfocused tree shows the selection with the inactive fill and normal text, on every selected row.
    fixture.host.SetFocusControl(nullptr);
    for (const size_t index : {0u, 2u, 3u})
    {
        Require(tree.DebugGetRowVisualState(theme, index, true, state) && state.selected, "a selected row of an unfocused tree reports itself selected");
        Require(state.fillArgb == PackColorForTest(theme.selectionInactiveFill),
                "a selected row of an unfocused tree is filled with the inactive selection color");
        Require(state.textArgb == PackColorForTest(theme.text), "a selected row of an unfocused tree keeps the text color");
    }

    // The same rows reach the screen: sample the rendered rows of a window.
    AttachedHostWindow window;
    window.Host().SetTheme(theme);
    auto root  = std::make_unique<Panel>();
    auto* live = root->AddChild<Tree>();
    live->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 150.0f));
    MutableTreeModel model;
    model.SetVisibleItems(FlatTreeItems(5u));
    live->SetModel(&model);
    live->SetMultiSelectEnabled(true);
    live->SetSelectedItemIds(std::vector<uint64_t>{1u, 3u, 4u});
    window.Host().SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.Host().SetFocusControl(live);
    window.PumpMessages();
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), "capture the multi-select tree");
    const auto pixelAt = [&](size_t visibleIndex)
    {
        const std::optional<D2D1_RECT_F> rect = live->GetVisibleItemHitRect(visibleIndex);
        Require(rect.has_value(), "the painted row has a hit rectangle");
        // Near the right end of the row: no text, icon or badge there, only the row's fill.
        const float xPx = window.Host().DipsToPixels(rect->right - 24.0f);
        const float yPx = window.Host().DipsToPixels((rect->top + rect->bottom) * 0.5f);
        const size_t x  = static_cast<size_t>(xPx);
        const size_t y  = static_cast<size_t>(yPx);
        Require(x < capture.widthPx && y < capture.heightPx, "the sampled pixel lies in the capture");
        const uint8_t* bgra = capture.bgraPixels.data() + (((y * capture.widthPx) + x) * 4u);
        return D2D1::ColorF(static_cast<float>(bgra[2]) / 255.0f, static_cast<float>(bgra[1]) / 255.0f, static_cast<float>(bgra[0]) / 255.0f);
    };
    const auto nearSelectionFill = [&](const D2D1_COLOR_F& color)
    {
        return std::abs(color.r - theme.selectionFill.r) < 0.02f && std::abs(color.g - theme.selectionFill.g) < 0.02f &&
               std::abs(color.b - theme.selectionFill.b) < 0.02f;
    };
    for (const size_t index : {0u, 2u, 3u})
    {
        Require(nearSelectionFill(pixelAt(index)), "every selected row is painted with the selection color");
    }
    for (const size_t index : {1u, 4u})
    {
        Require(! nearSelectionFill(pixelAt(index)), "a row outside the selection is not painted with the selection color");
    }
}

void TestTreeDragReorderKeepsItsSourceAndTheMultiSelection()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(5u, 10u), true, true);
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    // The rows are ids 10, 11, 12, 13 and 14 from the top.
    fixture.Click(1u);
    fixture.Click(3u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "the multi-selection the drags start from");

    // A press with Ctrl or Shift is a selection gesture, never a drag, however far the pointer goes.
    Require(tree.OnMouseDown(fixture.host, fixture.RowPoint(4u), false, MK_CONTROL), "a Ctrl+press is handled");
    Require(tree.OnMouseMove(fixture.host, fixture.RowPoint(0u, 0.2f), MK_CONTROL), "the pointer moves after a Ctrl+press");
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(0u, 0.2f), false, MK_CONTROL));
    Require(log.reorderCount == 0u && fixture.host.GetCapturedControl() == nullptr, "Ctrl+press does not start a row drag");
    RequireTreeIds(fixture.Selected(), {11u, 13u, 14u}, "the Ctrl+press toggled its row");
    Require(tree.OnMouseDown(fixture.host, fixture.RowPoint(4u), false, MK_SHIFT), "a Shift+press is handled");
    Require(tree.OnMouseMove(fixture.host, fixture.RowPoint(0u, 0.2f), MK_SHIFT), "the pointer moves after a Shift+press");
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(0u, 0.2f), false, MK_SHIFT));
    Require(log.reorderCount == 0u, "Shift+press does not start a row drag");
    fixture.Click(1u);
    fixture.Click(3u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "the multi-selection again");

    // A drag of a selected row keeps the whole selection, and the drop names the dragged row alone.
    const size_t setChangesBeforeDrag = log.selectionSetChangedCount;
    fixture.Press(3u);
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "a press on a selected row keeps the multi-selection while the pointer is down");
    Require(log.selectionSetChangedCount == setChangesBeforeDrag, "the press reports no change of selection");
    Require(tree.GetFocusedItemId() == 13u, "the press focuses the row it landed on");
    Require(tree.OnMouseMove(fixture.host, fixture.RowPoint(0u, 0.2f), 0u), "the drag follows the pointer");
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(0u, 0.2f), false, 0u));
    Require(log.reorderCount == 1u, "the release commits one reorder");
    Require(log.lastDrop.sourceId == 13u && log.lastDrop.targetId == 10u && log.lastDrop.place == TreeDropPlace::Before,
            "the drop names the dragged row alone");
    RequireTreeIds(log.idsSeenByReorderCallback, {11u, 13u}, "the delegate can apply the drop to the selection that still holds the source");
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "the drag left the selection as it was");
    Require(log.selectionSetChangedCount == setChangesBeforeDrag, "the drag reported no change of selection");

    // A click on a selected row of a multi-selection selects that row alone once the pointer is up.
    log.callOrder.clear();
    fixture.Press(3u);
    Require(log.callOrder == "P", "the press announces the row it landed on");
    fixture.Release(3u);
    RequireTreeIds(fixture.Selected(), {13u}, "the release of a click collapses the selection to the row");
    Require(log.callOrder == "PS", "the release reports the set once and does not announce the row twice");
    Require(log.reorderCount == 1u, "a click is not a reorder");

    // Escape and capture loss cancel the drag and its collapse: the selection stays as it was.
    fixture.Click(1u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "the multi-selection before Escape");
    fixture.Press(1u);
    Require(tree.OnMouseMove(fixture.host, fixture.RowPoint(4u, 0.8f), 0u), "the drag moves before Escape");
    Require(tree.OnKeyDown(fixture.host, VK_ESCAPE, 0u), "Escape cancels the drag");
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(4u, 0.8f), false, 0u));
    Require(log.reorderCount == 1u, "Escape reports no reorder");
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "Escape leaves the multi-selection alone");
    fixture.Press(1u);
    tree.OnCaptureLost(fixture.host);
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(1u), false, 0u));
    RequireTreeIds(fixture.Selected(), {11u, 13u}, "capture loss leaves the multi-selection alone");
    Require(log.reorderCount == 1u, "capture loss reports no reorder");

    // A press on a row outside the selection selects it alone first, and that row is the source.
    fixture.Press(2u);
    RequireTreeIds(fixture.Selected(), {12u}, "a press outside the selection selects its row alone");
    Require(tree.OnMouseMove(fixture.host, fixture.RowPoint(4u, 0.8f), 0u), "the drag moves");
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(4u, 0.8f), false, 0u));
    Require(log.reorderCount == 2u && log.lastDrop.sourceId == 12u && log.lastDrop.targetId == 14u && log.lastDrop.place == TreeDropPlace::After,
            "the row that was pressed is the dragged row");
    RequireTreeIds(log.idsSeenByReorderCallback, {12u}, "the selection at that drop is the dragged row alone");

    // Without reordering a press on a selected row collapses the selection at once, as Grid does.
    TreeSelectionFixture still(FlatTreeItems(5u, 10u));
    still.Click(1u);
    still.Click(3u, MK_CONTROL);
    still.Press(3u);
    RequireTreeIds(still.Selected(), {13u}, "a tree that cannot reorder collapses the selection on press");
}

void TestTreeMultiSelectContextMenuExpanderAndDoubleClickKeepTheSelectionRules()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(GroupedTreeItems());
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;
    fixture.Click(1u);
    fixture.Click(5u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 6u}, "the multi-selection the menus start from");

    // A right-click on a selected row keeps the selection for the command; any other row is selected alone.
    Require(tree.OnMouseDown(fixture.host, fixture.RowPoint(5u), true, 0u), "a right-click on a selected row is handled");
    Require(log.contextMenuCount == 1u && log.lastContextMenuItemId == 6u, "the menu opens for the row under the pointer");
    RequireTreeIds(fixture.Selected(), {2u, 6u}, "a right-click on a selected row keeps the multi-selection");
    Require(tree.GetFocusedItemId() == 6u, "the right-click focuses its row");
    Require(tree.OnMouseDown(fixture.host, fixture.RowPoint(6u), true, 0u), "a right-click on another row is handled");
    Require(log.contextMenuCount == 2u && log.lastContextMenuItemId == 7u, "the menu opens for that row");
    RequireTreeIds(fixture.Selected(), {7u}, "a right-click outside the selection selects its row alone");
    Require(log.selectionSetChangedCount >= 3u, "the delegate heard of the selection the right-click made");

    // The expander expands without touching the selection.
    fixture.Click(1u);
    fixture.Click(6u, MK_CONTROL);
    RequireTreeIds(fixture.Selected(), {2u, 7u}, "the selection before the expander");
    const std::optional<D2D1_RECT_F> rect = tree.GetVisibleItemHitRect(3u);
    Require(rect.has_value(), "the group row has a hit rectangle");
    const TreeItemLayoutMetrics metrics = tree.GetItemLayoutMetrics(fixture.host, 3u);
    Require(metrics.hasExpander, "the group row has an expander");
    const size_t setChanges = log.selectionSetChangedCount;
    Require(tree.OnMouseDown(
                fixture.host,
                D2D1::Point2F((metrics.expanderRect.left + metrics.expanderRect.right) * 0.5f, (metrics.expanderRect.top + metrics.expanderRect.bottom) * 0.5f),
                false,
                MK_CONTROL),
            "the expander is pressed");
    static_cast<void>(tree.OnMouseUp(fixture.host, fixture.RowPoint(3u), false, MK_CONTROL));
    Require(log.toggleCount == 1u && log.lastToggledItemId == 4u, "the expander asks for the expansion of its row");
    RequireTreeIds(fixture.Selected(), {2u, 7u}, "the expander leaves the selection alone, even with Ctrl held");
    Require(log.selectionSetChangedCount == setChanges && tree.GetFocusedItemId() == 4u, "the expander moves the focus only");

    // A double-click activates; with Ctrl or Shift it leaves the selection alone, without them it selects the row.
    const size_t invokedBefore = log.invokedCount;
    Require(tree.OnMouseDoubleClick(fixture.host, fixture.RowPoint(6u), false, MK_CONTROL), "a Ctrl+double-click is handled");
    Require(log.invokedCount == invokedBefore + 1u, "the double-click invokes the row");
    RequireTreeIds(fixture.Selected(), {2u, 7u}, "a Ctrl+double-click cannot toggle the row the first click added off again");
    Require(tree.OnMouseDoubleClick(fixture.host, fixture.RowPoint(6u), false, 0u), "a double-click is handled");
    RequireTreeIds(fixture.Selected(), {7u}, "a plain double-click selects its row alone");
}

void TestTreeSelectionSettersAndModeSwitchKeepTheSelectionCoherent()
{
    using namespace DxUi;

    TreeSelectionFixture fixture(FlatTreeItems(6u));
    Tree& tree                 = *fixture.tree;
    RecordingTreeDelegate& log = fixture.delegate;

    // The list keeps the rows it names, in the model's order; the last one it names is the focused row and the anchor.
    tree.SetSelectedItemIds(std::vector<uint64_t>{2u, 99u, 5u, 2u});
    RequireTreeIds(fixture.Selected(), {2u, 5u}, "a list of ids selects the rows that exist, in the model's order, once each");
    Require(tree.GetFocusedItemId() == 2u, "the last listed row that exists is the focused row");
    Require(fixture.Key(VK_DOWN, MK_SHIFT), "Shift+Down is handled");
    RequireTreeIds(fixture.Selected(), {2u, 3u}, "the range starts at the focused row");
    tree.SetSelectedItemIds({});
    Require(fixture.Selected().empty() && ! tree.GetFocusedItemId().has_value(), "an empty list clears the selection and the focus");
    tree.SetSelectedItemIds(std::vector<uint64_t>{77u});
    Require(fixture.Selected().empty() && ! tree.GetFocusedItemId().has_value(), "a list of rows that do not exist selects nothing");

    // SetSelectedItemId selects one row; SetFocusedItemId moves only the focus.
    tree.SetSelectedItemIds(std::vector<uint64_t>{1u, 3u, 4u});
    tree.SetFocusedItemId(6u);
    RequireTreeIds(fixture.Selected(), {1u, 3u, 4u}, "moving the focus keeps the selection");
    Require(tree.GetFocusedItemId() == 6u && tree.GetSelectedItemId() == 6u, "the focused row is what GetSelectedItemId reports with multi-select");
    tree.SetSelectedItemId(2u);
    RequireTreeIds(fixture.Selected(), {2u}, "SetSelectedItemId selects one row alone");
    tree.SetSelectedItemId(std::nullopt);
    Require(fixture.Selected().empty(), "SetSelectedItemId with no row clears the selection");

    // Turning multi-select off keeps the focused row if it is selected, else the last selected one, else nothing.
    tree.SetSelectedItemIds(std::vector<uint64_t>{1u, 3u, 4u});
    tree.SetFocusedItemId(3u);
    tree.SetMultiSelectEnabled(false);
    RequireTreeIds(fixture.Selected(), {3u}, "off keeps the focused row when it is selected");
    tree.SetMultiSelectEnabled(true);
    RequireTreeIds(fixture.Selected(), {3u}, "on starts from the selected row");
    tree.SetSelectedItemIds(std::vector<uint64_t>{1u, 3u, 4u});
    tree.SetFocusedItemId(2u);
    tree.SetMultiSelectEnabled(false);
    RequireTreeIds(fixture.Selected(), {4u}, "off keeps the last selected row when the focused one is not selected");
    tree.SetMultiSelectEnabled(true);
    tree.SetSelectedItemIds(std::vector<uint64_t>{1u, 3u});
    tree.RequestRemoveVisibleItemFromSelection(0u);
    tree.RequestRemoveVisibleItemFromSelection(2u);
    Require(fixture.Selected().empty() && tree.GetFocusedItemId() == 3u, "removing the last selected row leaves the focus where it was");
    tree.SetMultiSelectEnabled(false);
    Require(fixture.Selected().empty() && ! tree.GetSelectedItemId().has_value(), "off with nothing selected selects nothing");
    Require(log.selectionSetChangedCount == 3u, "only the Shift+Down and the two removals reported a set");
}


// A tree press or double click on an item that focuses the tree, given a focus callback that replaces every control,
// touches the destroyed tree no further (AddressSanitizer catches one that does).
void TestTreeInputLeavesATreeTheFocusCallbackDestroyed()
{
    using namespace DxUi;
    MutableTreeModel model;
    model.SetVisibleItems({TreeItemData{.id = 10u, .text = L"General"}, TreeItemData{.id = 20u, .text = L"Viewers"}});
    RecordingTreeDelegate delegate;
    const auto add = [&](Panel& root)
    {
        auto* tree = root.AddChild<Tree>();
        tree->SetBounds(D2D1::RectF(0.0f, 40.0f, 240.0f, 200.0f));
        tree->SetModel(&model);
        tree->SetDelegate(&delegate);
        return tree;
    };
    const auto firstItem = [](Control& tree) { return D2D1::Point2F(60.0f, tree.GetBounds().top + 8.0f); };
    RequireFocusReplacementLeavesControlAlone(
        "Tree press", add, [&](WindowHost& host, Control& tree) { static_cast<void>(tree.OnMouseDown(host, firstItem(tree), false, 0u)); });
    RequireFocusReplacementLeavesControlAlone(
        "Tree double click", add, [&](WindowHost& host, Control& tree) { static_cast<void>(tree.OnMouseDoubleClick(host, firstItem(tree), false, 0u)); });
}

} // namespace

void RunTreeTests()
{
    DXUI_RUN_TEST(TestTreeInputLeavesATreeTheFocusCallbackDestroyed);
    DXUI_RUN_TEST(TestTreeLocalizedEmptyStateRepaintsWithoutSelectionChange);
    DXUI_RUN_TEST(TestTreeIconFontAndReorderReleaseEdgeCases);
    DXUI_RUN_TEST(TestTreePointerSelectionNotifiesDelegate);
    DXUI_RUN_TEST(TestTreeDragReorderReportsDropAndEscapeCancels);
    DXUI_RUN_TEST(TestTreeDragReorderRejectsItsOwnSubtreeAndFollowsModelChanges);
    DXUI_RUN_TEST(TestTreeExpanderClickRequestsToggle);
    DXUI_RUN_TEST(TestTreeExpanderReResolvesStableItemAfterSelectionReorder);
    DXUI_RUN_TEST(TestTreeSelectionDelegateCanReplaceRootSafely);
    DXUI_RUN_TEST(TestTreeKeyboardRightAndLeftHandleExpansionAndParentTraversal);
    DXUI_RUN_TEST(TestTreeTypeaheadSelectsVisibleMatch);
    DXUI_RUN_TEST(TestTreeTypeaheadFallsBackToSingleCharacterAfterPrefixMiss);
    DXUI_RUN_TEST(TestTypeaheadUsesInvariantCaseMappingUnderTurkishLocale);
    DXUI_RUN_TEST(TestTreeDoubleClickInvokesLeafItem);
    DXUI_RUN_TEST(TestTreePageDownAdvancesByVisibleRows);
    DXUI_RUN_TEST(TestTreePageUpRetreatsByVisibleRows);
    DXUI_RUN_TEST(TestTreeHomeAndEndNavigateToBoundaries);
    DXUI_RUN_TEST(TestTreePageAndBoundaryKeysClampAtExtremes);
    DXUI_RUN_TEST(TestTreeMouseWheelScrollAffectsLaterHitTesting);
    DXUI_RUN_TEST(TestTreeScrollbarThumbGutterDragThroughWindowHost);
    DXUI_RUN_TEST(TestTreeLargeWheelDeltaUsesFullMagnitude);
    DXUI_RUN_TEST(TestTreeAccumulatesPartialWheelDelta);
    DXUI_RUN_TEST(TestTreeScrollbarFeedbackFollowsHoverAndDragState);
    DXUI_RUN_TEST(TestTreeRowsStraddlingTheViewportEdgesPaintOnlyInsideIt);
    DXUI_RUN_TEST(TestTreeFocusVisualsRespectKeyboardFocusVisibilityAndHighContrast);
    DXUI_RUN_TEST(TestTreeSelectedRowUsesRainbowOnlyInRainbowMode);
    DXUI_RUN_TEST(TestTreeNotifyDataChangedClearsMissingSelection);
    DXUI_RUN_TEST(TestTreeRightClickInvokesContextMenuForHitItem);
    DXUI_RUN_TEST(TestTreeKeyboardContextMenuBringsOffscreenSelectionIntoViewBeforeAnchoring);
    DXUI_RUN_TEST(TestTreeLayoutMetricsReserveSpaceForIconAndBadge);
    DXUI_RUN_TEST(TestTreeLayoutMetricsOmitOptionalAdornmentRectsWhenUnused);
    DXUI_RUN_TEST(TestTreeRowMetricsClampToSegoeVariableBodyLineHeight);
    DXUI_RUN_TEST(TestTreeCompactDensityShrinksRowMetrics);
    DXUI_RUN_TEST(TestTreeHoveredClippedTextShowsFullTextTooltip);
    DXUI_RUN_TEST(TestTreeMultiSelectIsOptInAndSingleSelectIsUnchanged);
    DXUI_RUN_TEST(TestTreeCtrlAndShiftClickBuildTheSelectionInVisibleOrder);
    DXUI_RUN_TEST(TestTreeCtrlAndShiftClickThroughTheHostMessages);
    DXUI_RUN_TEST(TestTreeKeyboardExtendsTogglesAndSelectsAll);
    DXUI_RUN_TEST(TestTreeKeyboardGesturesThroughTheHostMessages);
    DXUI_RUN_TEST(TestTreeKeyboardGesturesStartFromTheFirstRowWhenNothingIsFocused);
    DXUI_RUN_TEST(TestTreeGroupKeysFollowTheModifierRulesAndLeaveTheSelectionWhenTheyExpand);
    DXUI_RUN_TEST(TestTreeSelectionCallbacksFireOncePerChangeAndSeeTheFinishedSelection);
    DXUI_RUN_TEST(TestTreeSelectionSetCallbackMayDestroyTheTree);
    DXUI_RUN_TEST(TestTreeMultiSelectionSurvivesModelChangesExpandCollapseAndScrolling);
    DXUI_RUN_TEST(TestTreeMultiSelectPaintsEverySelectedRowWithTheSelectionColors);
    DXUI_RUN_TEST(TestTreeDragReorderKeepsItsSourceAndTheMultiSelection);
    DXUI_RUN_TEST(TestTreeMultiSelectContextMenuExpanderAndDoubleClickKeepTheSelectionRules);
    DXUI_RUN_TEST(TestTreeSelectionSettersAndModeSwitchKeepTheSelectionCoherent);
}
