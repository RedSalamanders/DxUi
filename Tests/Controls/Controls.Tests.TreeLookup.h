#pragma once
// W2.6 Tree data paths in the Tree suite: the id index answers exactly what ITreeModel's default lookup answers (seeded
// models, scripted input, Index against Model mode), a key reads only the focused row, a model with its own lookup is
// always asked, the verified mode catches a model that changes rows silently, and a collapse keeps its removed rows'
// records in place. Included once from DxUi.Tests.Tree.cpp, inside its anonymous namespace, after the shared helpers and
// Support/WorkCounters.h, so it includes nothing itself.

// Differences the suite's verified mode counted on purpose (a test of a model that breaks the notification rule).
inline size_t g_deliberateTreeLookupMismatches = 0u;

// Restores the id lookup mode a test changed, whatever path the test leaves by.
class ScopedTreeIdLookup final
{
public:
    explicit ScopedTreeIdLookup(DxUi::Tree::DebugIdLookup mode) noexcept : _previous(DxUi::Tree::DebugGetIdLookup())
    {
        DxUi::Tree::DebugSetIdLookup(mode);
    }
    ScopedTreeIdLookup(const ScopedTreeIdLookup&)            = delete;
    ScopedTreeIdLookup& operator=(const ScopedTreeIdLookup&) = delete;
    ScopedTreeIdLookup(ScopedTreeIdLookup&&)                 = delete;
    ScopedTreeIdLookup& operator=(ScopedTreeIdLookup&&)      = delete;
    ~ScopedTreeIdLookup()
    {
        DxUi::Tree::DebugSetIdLookup(_previous);
    }

private:
    DxUi::Tree::DebugIdLookup _previous;
};

// A whole tree in preorder with expansion flags; the visible rows skip the descendants of collapsed nodes. It keeps
// ITreeModel's default FindVisibleItemById, so a Tree answers its lookups from the id index. Its delegate side applies a
// toggle and publishes it unless `publishToggles` is off, and records every delegate event in `events`.
class ScriptTreeModel final : public DxUi::ITreeModel, public DxUi::ITreeDelegate
{
public:
    std::vector<DxUi::TreeItemData> nodes;
    DxUi::Tree* tree    = nullptr;
    bool publishToggles = true;
    std::string events;

    void Refresh()
    {
        _visible.clear();
        uint32_t hiddenBelow = UINT32_MAX;
        for (const DxUi::TreeItemData& node : nodes)
        {
            if (node.depth > hiddenBelow)
                continue;
            hiddenBelow = UINT32_MAX;
            _visible.push_back(node);
            if (node.hasChildren && ! node.expanded)
                hiddenBelow = node.depth;
        }
    }
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return _visible.size();
    }
    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& item) const override
    {
        item = _visible.at(visibleIndex);
    }

    void OnTreeSelectionChanged(uint64_t itemId) override
    {
        events += "S" + std::to_string(itemId) + ";";
    }
    void OnTreeFocusedItemChanged(DxUi::Tree&, std::optional<uint64_t> itemId) override
    {
        events += "F" + (itemId ? std::to_string(*itemId) : std::string("-")) + ";";
    }
    void OnTreeItemInvoked(uint64_t itemId) override
    {
        events += "I" + std::to_string(itemId) + ";";
    }
    void OnTreeContextMenu(uint64_t itemId, POINT) override
    {
        events += "C" + std::to_string(itemId) + ";";
    }
    void OnTreeSelectionSetChanged(std::span<const uint64_t> ids) override
    {
        events += "M" + std::to_string(ids.size()) + ";";
    }
    void OnTreeToggleExpanded(uint64_t itemId, bool expanded) override
    {
        events += std::string(expanded ? "E" : "X") + std::to_string(itemId) + ";";
        for (DxUi::TreeItemData& node : nodes)
            if (node.id == itemId)
            {
                node.expanded = expanded;
                break;
            }
        Refresh();
        if (publishToggles && tree)
            tree->NotifyDataChanged();
    }

private:
    std::vector<DxUi::TreeItemData> _visible;
};

// Appends one node and, depth first, `width` children per level down to `levels` below it.
inline void AddScriptSubtree(
    std::vector<DxUi::TreeItemData>& nodes, uint64_t& nextId, std::optional<uint64_t> parentId, uint32_t depth, size_t width, uint32_t levels, bool expanded)
{
    const uint64_t id = nextId++;
    nodes.push_back(DxUi::TreeItemData{.id          = id,
                                       .parentId    = parentId,
                                       .text        = (levels > 0u ? L"Folder " : L"Leaf ") + std::to_wstring(id),
                                       .depth       = depth,
                                       .hasChildren = levels > 0u,
                                       .expanded    = levels > 0u && expanded});
    if (levels == 0u)
        return;
    for (size_t child = 0u; child < width; ++child)
        AddScriptSubtree(nodes, nextId, id, depth + 1u, width, levels - 1u, expanded);
}

struct SeededTreeModel
{
    const char* name;
    std::vector<DxUi::TreeItemData> nodes;
};

inline std::vector<SeededTreeModel> SeededTreeModels()
{
    std::vector<SeededTreeModel> models;
    models.push_back({"empty", {}});
    models.push_back({"one row", {DxUi::TreeItemData{.id = 7u, .text = L"Only"}}});
    {
        SeededTreeModel deep{"deep and wide", {}};
        uint64_t nextId = 1u;
        for (size_t root = 0u; root < 3u; ++root)
            AddScriptSubtree(deep.nodes, nextId, std::nullopt, 0u, 3u, 4u, true);
        models.push_back(std::move(deep));
    }
    {
        SeededTreeModel collapsed{"all collapsed", {}};
        uint64_t nextId = 1u;
        for (size_t root = 0u; root < 40u; ++root)
            AddScriptSubtree(collapsed.nodes, nextId, std::nullopt, 0u, 2u, 2u, false);
        models.push_back(std::move(collapsed));
    }
    {
        SeededTreeModel alternating{"alternating", {}};
        uint64_t nextId = 1u;
        for (size_t root = 0u; root < 30u; ++root)
            AddScriptSubtree(alternating.nodes, nextId, std::nullopt, 0u, 3u, 1u, root % 2u == 0u);
        models.push_back(std::move(alternating));
    }
    {
        SeededTreeModel large{"10,000 rows", {}};
        uint64_t nextId = 1u;
        while (large.nodes.size() < 10'000u)
            AddScriptSubtree(large.nodes, nextId, std::nullopt, 0u, 7u, 1u, true);
        large.nodes.resize(10'000u);
        models.push_back(std::move(large));
    }
    {
        // Ids that repeat: the default lookup, and so the index, resolves each to its first row.
        SeededTreeModel duplicates{"duplicate ids", {}};
        for (const uint64_t id : {5u, 6u, 7u, 6u, 8u, 6u, 9u, 5u, 10u})
            duplicates.nodes.push_back(DxUi::TreeItemData{.id = id, .text = L"Row " + std::to_wstring(id)});
        models.push_back(std::move(duplicates));
    }
    return models;
}

// What one script step left: the tree's focus, selection, scroll and first row, and the delegate's events.
inline std::string DescribeTreeState(const DxUi::Tree& tree, const ScriptTreeModel& model)
{
    std::string text = "focus=" + (tree.GetFocusedItemId() ? std::to_string(*tree.GetFocusedItemId()) : std::string("-"));
    text += " row=" + (tree.DebugGetSelectedVisibleIndex() ? std::to_string(*tree.DebugGetSelectedVisibleIndex()) : std::string("-"));
    text += " selected=";
    for (const uint64_t id : tree.GetSelectedItemIds())
        text += std::to_string(id) + ",";
    text += " scroll=" + std::to_string(tree.DebugGetVerticalScrollDip()) + " first=" + std::to_string(tree.GetFirstVisibleItemIndex());
    text += " events=" + model.events;
    return text;
}

// Runs one scripted session over a seeded model and returns the state after every step.
inline std::vector<std::string> RunTreeLookupScript(const SeededTreeModel& seed, bool multiSelect, DxUi::Tree::DebugIdLookup mode)
{
    using namespace DxUi;
    const ScopedTreeIdLookup lookup(mode);
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 200.0f));
    ScriptTreeModel model;
    model.nodes = seed.nodes;
    model.tree  = tree;
    model.Refresh();
    tree->SetMultiSelectEnabled(multiSelect);
    tree->SetModel(&model);
    tree->SetDelegate(&model);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 220.0f));
    host.SetFocusControl(tree);

    std::vector<std::string> states;
    const auto record  = [&](const char* step) { states.push_back(std::string(step) + ": " + DescribeTreeState(*tree, model)); };
    const auto key     = [&](UINT virtualKey, UINT modifiers = 0u) { static_cast<void>(tree->OnKeyDown(host, virtualKey, modifiers)); };
    const size_t count = model.GetVisibleItemCount();
    const auto idAt    = [&](size_t row) -> uint64_t
    {
        TreeItemData item;
        model.GetVisibleItem(row, item);
        return item.id;
    };

    record("start");
    tree->SetFocusedItemId(999'999u);
    record("focus a missing id");
    tree->SetSelectedItemId(999'998u);
    record("select a missing id");
    for (const UINT virtualKey : std::initializer_list<UINT>{VK_DOWN, VK_DOWN, VK_DOWN, VK_UP, VK_NEXT, VK_NEXT, VK_PRIOR, VK_END, VK_UP, VK_HOME, VK_DOWN})
    {
        key(virtualKey);
        record("key");
    }
    if (count > 0u)
    {
        tree->SetFocusedItemId(idAt(count / 2u));
        record("focus the middle row");
        tree->SetSelectedItemId(idAt(count - 1u));
        record("select the last row");
        tree->SetFocusedItemId(idAt(count / 3u));
        record("focus a third of the way");
    }
    for (const UINT virtualKey :
         std::initializer_list<UINT>{VK_RIGHT, VK_RIGHT, VK_LEFT, VK_LEFT, VK_LEFT, VK_DOWN, VK_RIGHT, VK_DOWN, VK_DOWN, VK_LEFT, VK_RETURN, VK_SPACE})
    {
        key(virtualKey);
        record("expansion key");
    }
    key(VK_DOWN, MK_SHIFT);
    record("shift down");
    key(VK_DOWN, MK_CONTROL);
    record("ctrl down");
    key(VK_SPACE, MK_CONTROL);
    record("ctrl space");
    // One character: a longer prefix would depend on the real clock between characters.
    static_cast<void>(tree->OnChar(host, L'L', 0u));
    record("typeahead");
    static_cast<void>(tree->OnContextMenu(host, true, D2D1::Point2F(0.0f, 0.0f)));
    record("keyboard context menu");
    if (count > 0u)
    {
        static_cast<void>(tree->RequestExpandedState(0u, false));
        record("request collapse of row 0");
        static_cast<void>(tree->RequestExpandedState(0u, true));
        record("request expansion of row 0");
        static_cast<void>(tree->RequestFocusVisibleItem(model.GetVisibleItemCount() - 1u));
        record("request focus on the last row");
        static_cast<void>(tree->RequestSelectVisibleItem(0u));
        record("request selection of row 0");
    }
    // The focused row leaves the model, which publishes the change.
    if (const std::optional<uint64_t> focused = tree->GetFocusedItemId())
    {
        std::erase_if(model.nodes, [&](const TreeItemData& node) { return node.id == *focused; });
        model.Refresh();
        tree->NotifyDataChanged();
        record("the focused row is removed");
    }
    key(VK_DOWN);
    record("key after removal");
    // Rows reorder (reversed), published.
    std::reverse(model.nodes.begin(), model.nodes.end());
    for (TreeItemData& node : model.nodes)
    {
        node.depth    = 0u;
        node.parentId = std::nullopt;
    }
    model.Refresh();
    tree->NotifyDataChanged();
    record("reversed");
    for (const UINT virtualKey : std::initializer_list<UINT>{VK_DOWN, VK_DOWN, VK_UP, VK_LEFT, VK_END})
    {
        key(virtualKey);
        record("key after reversal");
    }
    tree->SetModel(nullptr);
    return states;
}

// Decision 3 of the item: equal behavior for seeded models, with the old lookup kept callable (Model mode) for comparison.
inline void TestTreeIdIndexEqualsTheDefaultLookupForSeededModels()
{
    for (const SeededTreeModel& seed : SeededTreeModels())
        for (const bool multiSelect : {false, true})
        {
            const size_t mismatchesBefore           = DxUi::Tree::DebugGetIdLookupMismatchCount();
            const std::vector<std::string> model    = RunTreeLookupScript(seed, multiSelect, DxUi::Tree::DebugIdLookup::Model);
            const std::vector<std::string> indexed  = RunTreeLookupScript(seed, multiSelect, DxUi::Tree::DebugIdLookup::Index);
            const std::vector<std::string> verified = RunTreeLookupScript(seed, multiSelect, DxUi::Tree::DebugIdLookup::Verified);
            const std::string context               = std::string(seed.name) + (multiSelect ? " (multi-select)" : "");
            Require(model.size() == indexed.size() && model.size() == verified.size(), (context + ": every script ran to its end").c_str());
            for (size_t step = 0u; step < model.size(); ++step)
            {
                if (model[step] != indexed[step] || model[step] != verified[step])
                {
                    const std::string message = context + ": step " + std::to_string(step) + " differs\n  model:    " + model[step] +
                                                "\n  index:    " + indexed[step] + "\n  verified: " + verified[step];
                    Require(false, message.c_str());
                }
            }
            Require(DxUi::Tree::DebugGetIdLookupMismatchCount() == mismatchesBefore, (context + ": the verified run found no difference").c_str());
        }
}

// The FX9 gate on a model without a lookup of its own: 10,000 rows, keys from the middle. The first key builds the index
// (one read per row); every later navigation key reads only the row that takes the focus.
inline void TestTreeNavigationKeysReadOnlyTheFocusedRowOf10000Rows()
{
    using namespace DxUi;
    const ScopedTreeIdLookup lookup(Tree::DebugIdLookup::Index);
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 300.0f));
    MutableTreeModel model;
    model.SetVisibleItems(FlatTreeItems(10'000u));
    tree->SetModel(&model);
    host.SetRoot(std::move(root));
    host.SetFocusControl(tree);
    tree->SetFocusedItemId(5'000u); // the first lookup learns that the model keeps the default lookup
    Require(tree->DebugGetIdIndexRowCount() == 0u, "the first lookup asks the model");
    tree->SetFocusedItemId(5'000u);
    Require(tree->DebugGetIdIndexRowCount() == 10'000u, "the next lookup builds the id index over every visible row");
    for (const UINT virtualKey : std::initializer_list<UINT>{VK_DOWN, VK_DOWN, VK_UP, VK_NEXT, VK_PRIOR, VK_END, VK_UP, VK_HOME, VK_DOWN})
    {
        const std::optional<uint64_t> before = tree->GetFocusedItemId();
        const Detail::WorkCounterScope scope;
        Require(tree->OnKeyDown(host, virtualKey, 0u), "a navigation key is handled");
        const Detail::WorkCounters work = scope.Delta();
        Require(tree->GetFocusedItemId() != before, "every navigation key moves the focus");
        Require(work.lists.treeItemReads == 1u, "a navigation key reads only the row that takes the focus");
        Require(work.lists.treeModelLookups == 0u && work.lists.treeIdIndexBuilds == 0u, "a navigation key asks the current index, not the model");
    }
    // The same keys in Model mode read rows up to the focused one, as before W2.6.
    {
        const ScopedTreeIdLookup old(Tree::DebugIdLookup::Model);
        tree->SetFocusedItemId(5'000u);
        const Detail::WorkCounterScope scope;
        Require(tree->OnKeyDown(host, VK_DOWN, 0u), "a navigation key is handled in Model mode");
        Require(scope.Delta().lists.treeItemReads > 5'000u, "the default lookup reads every row up to the focused one");
    }
    tree->SetModel(nullptr);
    Require(tree->DebugGetIdIndexRowCount() == 0u, "SetModel releases the id index");
}

// A model with its own lookup is asked every time and gets no index, whatever its ids.
inline void TestTreeModelWithItsOwnLookupIsAlwaysAsked()
{
    using namespace DxUi;
    const ScopedTreeIdLookup lookup(Tree::DebugIdLookup::Index);
    class OwnLookupModel final : public ITreeModel
    {
    public:
        [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
        {
            return 500u;
        }
        void GetVisibleItem(size_t visibleIndex, TreeItemData& item) const override
        {
            item      = {};
            item.id   = 10'000u + visibleIndex;
            item.text = L"Row " + std::to_wstring(visibleIndex);
        }
        [[nodiscard]] std::optional<size_t> FindVisibleItemById(uint64_t itemId) const noexcept override
        {
            ++asked;
            return itemId >= 10'000u && itemId < 10'500u ? std::optional<size_t>(itemId - 10'000u) : std::nullopt;
        }
        mutable size_t asked = 0u;
    } model;
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 300.0f));
    tree->SetModel(&model);
    host.SetRoot(std::move(root));
    const Detail::WorkCounterScope whole;
    tree->SetFocusedItemId(10'100u);
    for (size_t press = 0u; press < 10u; ++press)
    {
        const size_t askedBefore = model.asked;
        const Detail::WorkCounterScope scope;
        Require(tree->OnKeyDown(host, VK_DOWN, 0u), "a key is handled");
        const Detail::WorkCounters work = scope.Delta();
        Require(model.asked == askedBefore + 1u && work.lists.treeModelLookups == 1u, "the model answers the focused item's lookup itself");
        Require(work.lists.treeItemReads == 1u, "a key reads only the row that takes the focus");
    }
    Require(tree->GetFocusedItemId() == 10'110u, "the keys followed the model's answers");
    Require(tree->DebugGetIdIndexRowCount() == 0u && whole.Delta().lists.treeIdIndexBuilds == 0u, "a model with its own lookup gets no id index");
    tree->SetModel(nullptr);
}

// A model that changes rows without NotifyDataChanged breaks the rule; the verified mode counts the difference.
inline void TestTreeVerifiedLookupCatchesASilentModelChange()
{
    using namespace DxUi;
    const ScopedTreeIdLookup lookup(Tree::DebugIdLookup::Verified);
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 300.0f));
    MutableTreeModel model;
    model.SetVisibleItems(FlatTreeItems(20u));
    tree->SetModel(&model);
    host.SetRoot(std::move(root));
    tree->SetFocusedItemId(10u); // the first lookup learns that the model keeps the default lookup
    tree->SetFocusedItemId(10u); // the second builds the index
    Require(tree->DebugGetIdIndexRowCount() == 20u, "the index holds the 20 rows");
    const size_t before = Tree::DebugGetIdLookupMismatchCount();
    // Two rows swap, silently: the index still has id 10 at row 9.
    std::vector<TreeItemData> items = FlatTreeItems(20u);
    std::swap(items[2], items[9]);
    model.SetVisibleItems(items);
    Require(tree->OnKeyDown(host, VK_DOWN, 0u), "a key is handled after a silent change");
    Require(Tree::DebugGetIdLookupMismatchCount() == before + 1u, "the verified mode counts the stale index answer");
    ++g_deliberateTreeLookupMismatches;
    Require(tree->GetFocusedItemId() == 4u, "the verified mode keeps the model's answer (id 10 at row 2, so Down lands on row 3)");
    tree->NotifyDataChanged();
    Require(tree->OnKeyDown(host, VK_DOWN, 0u) && Tree::DebugGetIdLookupMismatchCount() == before + 1u, "a published change leaves no difference");
    tree->SetModel(nullptr);
}

// A delegate that applies a toggle without publishing it: the tree drops its index when the callback returns, so keys still
// find the rows the toggle revealed.
inline void TestTreeToggleWithoutPublicationDropsTheIdIndex()
{
    using namespace DxUi;
    const ScopedTreeIdLookup lookup(Tree::DebugIdLookup::Verified);
    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 300.0f));
    ScriptTreeModel model;
    uint64_t nextId = 1u;
    for (size_t folder = 0u; folder < 4u; ++folder)
        AddScriptSubtree(model.nodes, nextId, std::nullopt, 0u, 3u, 1u, false);
    model.tree           = tree;
    model.publishToggles = false;
    model.Refresh();
    tree->SetModel(&model);
    tree->SetDelegate(&model);
    host.SetRoot(std::move(root));
    host.SetFocusControl(tree);
    const size_t before = Tree::DebugGetIdLookupMismatchCount();
    tree->SetFocusedItemId(5u); // the second folder, row 1
    Require(tree->OnKeyDown(host, VK_RIGHT, 0u), "Right expands the focused folder");
    Require(model.GetVisibleItemCount() == 7u, "the delegate applied the expansion without publishing it");
    Require(tree->OnKeyDown(host, VK_RIGHT, 0u) && tree->GetFocusedItemId() == 6u, "Right then moves to the folder's first child");
    Require(tree->OnKeyDown(host, VK_DOWN, 0u) && tree->GetFocusedItemId() == 7u, "Down follows the revealed rows");
    Require(Tree::DebugGetIdLookupMismatchCount() == before, "the index was dropped with the toggle, so it never answered stale");
    tree->SetModel(nullptr);
}

// A collapse keeps whole records only for the rows it removes, read in place (no copy), equal to the rows the model had;
// an expansion keeps none. Both happen under the host's animation, at any row count up to the 4,096-row capacity.
inline void TestTreeCollapseKeepsOnlyItsRemovedRowsInPlace()
{
    using namespace DxUi;
    WindowHost host;
    EnableMotionForTest(host);
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 140.0f));
    ScriptTreeModel model;
    uint64_t nextId = 1u;
    for (size_t folder = 0u; folder < 3u; ++folder)
        AddScriptSubtree(model.nodes, nextId, std::nullopt, 0u, 8u, 1u, true);
    model.tree = tree;
    model.Refresh();
    tree->SetModel(&model);
    tree->SetDelegate(&model);
    host.SetRoot(std::move(root));
    std::vector<TreeItemData> folderRows;
    for (size_t row = 1u; row <= 8u; ++row)
    {
        TreeItemData item;
        model.GetVisibleItem(row, item);
        folderRows.push_back(item);
    }
    const Detail::WorkCounterScope collapseScope;
    Require(tree->RequestExpandedState(0u, false), "the first folder collapses");
    const Detail::WorkCounters collapse = collapseScope.Delta();
    Require(tree->DebugGetExpansionAnimationItemId() == 1u, "the collapse animates");
    const std::span<const TreeItemData> records = tree->DebugGetExpansionRecords();
    Require(records.size() == 8u && collapse.lists.treeExpansionRecords == 8u, "the collapse keeps its eight removed rows and nothing else");
    Require(collapse.lists.treeExpansionRecordCopies == 0u, "no record is copied");
    for (size_t index = 0u; index < records.size(); ++index)
        Require(records[index].id == folderRows[index].id && records[index].text == folderRows[index].text && records[index].depth == folderRows[index].depth &&
                    records[index].parentId == folderRows[index].parentId,
                "each record is the model's row before the collapse");
    // A second toggle before the first finishes replaces the records.
    const Detail::WorkCounterScope expandScope;
    Require(tree->RequestExpandedState(0u, true), "the folder expands again before the collapse ends");
    const Detail::WorkCounters expand = expandScope.Delta();
    Require(tree->DebugGetExpansionRecords().empty() && expand.lists.treeExpansionRecords == 0u && expand.lists.treeExpansionRecordCopies == 0u,
            "an expansion keeps no record");
    // The rows read after an animated toggle become the index of a default-lookup model: finding the toggled item reads no row.
    {
        const ScopedTreeIdLookup lookup(Tree::DebugIdLookup::Index);
        const Detail::WorkCounterScope scope;
        Require(tree->RequestExpandedState(0u, false), "the folder collapses again");
        const Detail::WorkCounters work = scope.Delta();
        Require(work.lists.treeIdIndexBuilds == 0u && work.lists.treeModelLookups == 0u, "the toggled item is found in the adopted rows");
        Require(work.lists.treeItemReads == 27u + 1u + 19u + 1u,
                "an animated collapse reads the 27 rows before, the item, the 19 rows after and the item again, and nothing else");
    }
    tree->SetModel(nullptr);
    Require(tree->DebugGetExpansionRecords().empty(), "SetModel releases the records");
}

inline void RunTreeLookupTests()
{
    DXUI_RUN_TEST(TestTreeIdIndexEqualsTheDefaultLookupForSeededModels);
    DXUI_RUN_TEST(TestTreeNavigationKeysReadOnlyTheFocusedRowOf10000Rows);
    DXUI_RUN_TEST(TestTreeModelWithItsOwnLookupIsAlwaysAsked);
    DXUI_RUN_TEST(TestTreeVerifiedLookupCatchesASilentModelChange);
    DXUI_RUN_TEST(TestTreeToggleWithoutPublicationDropsTheIdIndex);
    DXUI_RUN_TEST(TestTreeCollapseKeepsOnlyItsRemovedRowsInPlace);
}
