// Plan item W2.5, Grid data paths (Specs/UI/UI_ControlsAndLayout.md, "Grid data caches"). Every test here runs under the Grid
// suite's data-path verification (Fatal): each cached group layout, column description, row geometry, keyboard target, range and
// selection epoch the Grid uses is compared with the uncached computation it replaced, and a mismatch ends the run. The tests add
// an independent reference of their own (visible rows computed from the seeded model, not from the Grid) for the public answers.
#include "Controls.Tests.GridDataPaths.h"
#include "Controls.Tests.DxUiTestHelpers.h"

#include "Support/WorkCounters.h"

#include <random>
#include <unordered_set>

namespace
{
using namespace DxUi;
using ::Require;

void Require(bool condition, const std::string& message)
{
    Require(condition, message.c_str());
}

// Rows whose stable ids are not their indices (an XOR permutation, so ascending ids are not row order), in the groups given.
class SeededGridModel final : public IGridModel
{
public:
    struct Group
    {
        size_t start   = 0u;
        size_t count   = 0u;
        bool collapsed = false;
    };
    static constexpr uint64_t kIdMask = 0x5A5Au;

    SeededGridModel(size_t rows, std::vector<Group> groups) : _rows(rows), _groups(std::move(groups))
    {
    }

    [[nodiscard]] static uint64_t IdOf(size_t row) noexcept
    {
        return static_cast<uint64_t>(row) ^ kIdMask;
    }
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rows;
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 3u;
    }
    [[nodiscard]] GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        GridColumnDesc column;
        column.id       = std::format(L"c{}", columnIndex);
        column.title    = std::format(L"Column {}", columnIndex);
        column.widthDip = 80.0f;
        return column;
    }
    void GetCellData(size_t rowIndex, size_t columnIndex, GridCellData& outCell) const override
    {
        outCell.kind = GridCellKind::Text;
        outCell.text = std::format(L"{}:{}", rowIndex, columnIndex);
    }
    [[nodiscard]] size_t GetGroupCount() const noexcept override
    {
        return _groups.size();
    }
    [[nodiscard]] GridGroupDesc GetGroup(size_t groupIndex) const override
    {
        const Group& group = _groups.at(groupIndex);
        return GridGroupDesc{.stableId      = 1000u + groupIndex,
                             .title         = std::format(L"Group {}", groupIndex),
                             .startRowIndex = group.start,
                             .rowCount      = group.count,
                             .collapsed     = group.collapsed};
    }
    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return IdOf(rowIndex);
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        const uint64_t row = rowId ^ kIdMask;
        return row < _rows ? std::optional<size_t>(static_cast<size_t>(row)) : std::nullopt;
    }
    void SetCollapsed(uint64_t groupStableId, bool collapsed)
    {
        _groups.at(static_cast<size_t>(groupStableId - 1000u)).collapsed = collapsed;
    }
    [[nodiscard]] const std::vector<Group>& Groups() const noexcept
    {
        return _groups;
    }

private:
    size_t _rows = 0u;
    std::vector<Group> _groups;
};

// The rows a reader of the model sees, in order: every row outside a collapsed group.
[[nodiscard]] std::vector<size_t> ReferenceVisibleRows(const SeededGridModel& model)
{
    std::vector<bool> hidden(model.GetRowCount(), false);
    for (const SeededGridModel::Group& group : model.Groups())
        if (group.collapsed)
            for (size_t row = group.start; row < group.start + group.count; ++row)
                hidden[row] = true;
    std::vector<size_t> rows;
    for (size_t row = 0u; row < model.GetRowCount(); ++row)
        if (! hidden[row])
            rows.push_back(row);
    return rows;
}

[[nodiscard]] std::vector<uint64_t> IdsOf(std::span<const size_t> rows)
{
    std::vector<uint64_t> ids;
    for (const size_t row : rows)
        ids.push_back(SeededGridModel::IdOf(row));
    return ids;
}

// Publishes each group toggle with NotifyDataChanged, and counts the selection and focus reports.
class PublishingDelegate final : public IGridDelegate
{
public:
    explicit PublishingDelegate(SeededGridModel& model) : _model(model)
    {
    }
    PublishingDelegate(const PublishingDelegate&)            = delete;
    PublishingDelegate& operator=(const PublishingDelegate&) = delete;
    using IGridDelegate::OnGridGroupToggled;
    using IGridDelegate::OnGridSelectionChanged;
    void OnGridSelectionChanged(Grid& /*sender*/) override
    {
        ++selectionReports;
    }
    void OnGridFocusedRowChanged(Grid& /*sender*/, std::optional<uint64_t> /*rowId*/) override
    {
        ++focusReports;
    }
    void OnGridGroupToggled(Grid& sender, uint64_t groupStableId, bool collapsed) override
    {
        _model.SetCollapsed(groupStableId, collapsed);
        sender.NotifyDataChanged();
    }
    size_t selectionReports = 0u;
    size_t focusReports     = 0u;

private:
    SeededGridModel& _model;
};

struct Seed
{
    const char* name = "";
    size_t rows      = 0u;
    std::vector<SeededGridModel::Group> groups;
    bool rightToLeft = false;
};

[[nodiscard]] std::vector<Seed> Seeds(bool large)
{
    std::vector<Seed> seeds;
    seeds.push_back({"empty", 0u, {}});
    seeds.push_back({"one row", 1u, {}});
    seeds.push_back({"one row in a group of one", 1u, {{0u, 1u, false}}});
    seeds.push_back({"ungrouped", 500u, {}});
    {
        Seed all{"all groups collapsed", 400u, {}};
        for (size_t group = 0u; group < 40u; ++group)
            all.groups.push_back({group * 10u, 10u, true});
        seeds.push_back(std::move(all));
    }
    {
        // Ungrouped runs before, between and after groups of every size from 1 to 9, every other one collapsed.
        Seed alternating{"alternating collapsed with ungrouped runs and groups of one", 0u, {}};
        size_t row = 3u;
        for (size_t group = 0u; group < 30u; ++group)
        {
            const size_t count = 1u + group % 9u;
            alternating.groups.push_back({row, count, group % 2u == 1u});
            row += count + (group % 3u == 0u ? 2u : 0u);
        }
        alternating.rows = row + 4u;
        seeds.push_back(alternating);
        alternating.name        = "alternating collapsed, right to left";
        alternating.rightToLeft = true;
        seeds.push_back(std::move(alternating));
    }
    if (large)
    {
        Seed big{"10,000 groups over 200,000 rows", 200'000u, {}};
        for (size_t group = 0u; group < 10'000u; ++group)
            big.groups.push_back({group * 20u, 20u, group % 7u == 3u});
        seeds.push_back(std::move(big));
    }
    return seeds;
}

struct Scene
{
    WindowHost host;
    Grid* grid = nullptr;
    std::unique_ptr<SeededGridModel> model;
    std::unique_ptr<PublishingDelegate> delegate;

    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;
    explicit Scene(const Seed& seed)
    {
        model     = std::make_unique<SeededGridModel>(seed.rows, seed.groups);
        delegate  = std::make_unique<PublishingDelegate>(*model);
        auto root = std::make_unique<Panel>();
        grid      = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 240.0f));
        grid->SetSelectionMode(GridSelectionMode::Extended);
        if (seed.rightToLeft)
            root->SetFlowDirection(FlowDirection::RightToLeft);
        grid->SetModel(model.get());
        grid->SetDelegate(delegate.get());
        host.SetRoot(std::move(root));
    }
    [[nodiscard]] std::vector<uint64_t> Selection() const
    {
        const auto ids = grid->GetSelectionModel().GetOrderedSelection();
        return {ids.begin(), ids.end()};
    }
};

// One gesture: its selection report is due exactly when the ordered selection changed, as the copy-and-compare it replaced
// decided, and its focus report exactly when the focused row changed.
template <typename Gesture> void CheckReports(Scene& scene, const char* what, Gesture&& gesture)
{
    const std::vector<uint64_t> before        = scene.Selection();
    const std::optional<uint64_t> focusBefore = scene.grid->GetFocusedRowId();
    const size_t selectionReports             = scene.delegate->selectionReports;
    const size_t focusReports                 = scene.delegate->focusReports;
    gesture();
    const bool selectionChanged = scene.Selection() != before;
    Require((scene.delegate->selectionReports - selectionReports) == (selectionChanged ? 1u : 0u),
            std::format("{}: one selection report exactly when the ordered selection changed", what));
    Require((scene.delegate->focusReports - focusReports) <= 1u &&
                (scene.delegate->focusReports > focusReports) == (scene.grid->GetFocusedRowId() != focusBefore),
            std::format("{}: one focus report exactly when the focused row changed", what));
}

void CheckGeometry(Scene& scene, const std::vector<size_t>& visible, const char* seed)
{
    Grid& grid = *scene.grid;
    Require(grid.GetVisibleRowCount() <= visible.size(), std::format("{}: no more rows on screen than visible rows", seed));
    const size_t stride = (std::max)(size_t{1}, visible.size() / 64u);
    for (size_t ordinal = 0u; ordinal < visible.size(); ordinal += stride)
    {
        const size_t row = visible[ordinal];
        Require(grid.FindVisibleRowOrdinal(row) == std::optional<size_t>(ordinal), std::format("{}: row {} has its reference ordinal", seed, row));
        grid.EnsureRowVisible(row);
        const std::optional<D2D1_RECT_F> rect = grid.GetVisibleRowRect(row);
        Require(rect.has_value(), std::format("{}: a revealed row {} has a rectangle", seed, row));
        const PointDip center{(rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f};
        Require(grid.FindRowAtPoint(center) == std::optional<size_t>(row), std::format("{}: the centre of row {} hits it", seed, row));
        // The rows after it on screen follow at the reference distances: one row height per row, one group header where a group starts.
        for (size_t next = ordinal + 1u; next < visible.size() && next < ordinal + 4u; ++next)
        {
            const std::optional<D2D1_RECT_F> nextRect = grid.GetVisibleRowRect(visible[next]);
            if (! nextRect || nextRect->bottom - nextRect->top < rect->bottom - rect->top)
                break;
            Require(nextRect->top > rect->top, std::format("{}: visible rows descend in reference order", seed));
        }
    }
    for (const SeededGridModel::Group& group : scene.model->Groups())
        if (group.collapsed)
            Require(! grid.FindVisibleRowOrdinal(group.start).has_value(), std::format("{}: a collapsed group's rows have no ordinal", seed));
}

void CheckKeys(Scene& scene, const std::vector<size_t>& visible, const char* seed)
{
    Grid& grid = *scene.grid;
    if (visible.empty())
    {
        for (const UINT key : {UINT{VK_DOWN}, UINT{VK_UP}, UINT{VK_HOME}, UINT{VK_END}, UINT{VK_NEXT}, UINT{VK_PRIOR}})
            CheckReports(scene, "a key over no visible row", [&] { static_cast<void>(grid.OnKeyDown(scene.host, key, 0u)); });
        Require(grid.GetSelectionModel().GetCount() == 0u, std::format("{}: no key selects a hidden row", seed));
        return;
    }
    const auto ordinalOfFocus = [&]() -> size_t
    {
        const std::optional<uint64_t> focus = grid.GetFocusedRowId();
        Require(focus.has_value(), std::format("{}: a key leaves a focused row", seed));
        const auto it = std::ranges::find(visible, static_cast<size_t>(*focus ^ SeededGridModel::kIdMask));
        Require(it != visible.end(), std::format("{}: the focused row is a visible row", seed));
        return static_cast<size_t>(it - visible.begin());
    };
    CheckReports(scene, "a plain click", [&] { Require(grid.RequestSelectRow(visible[visible.size() / 2u], 0u), "the click is handled"); });
    struct Move
    {
        UINT key;
        UINT modifiers;
    };
    constexpr std::array<Move, 16u> moves{{{VK_DOWN, 0u},
                                           {VK_DOWN, 0u},
                                           {VK_UP, 0u},
                                           {VK_NEXT, 0u},
                                           {VK_PRIOR, 0u},
                                           {VK_DOWN, MK_SHIFT},
                                           {VK_DOWN, MK_SHIFT},
                                           {VK_NEXT, MK_SHIFT},
                                           {VK_DOWN, MK_CONTROL},
                                           {VK_DOWN, MK_CONTROL | MK_SHIFT},
                                           {VK_END, 0u},
                                           {VK_PRIOR, 0u},
                                           {VK_HOME, MK_SHIFT},
                                           {VK_HOME, 0u},
                                           {VK_UP, 0u},
                                           {VK_END, MK_CONTROL}}};
    for (const Move& move : moves)
    {
        const size_t before = ordinalOfFocus();
        CheckReports(scene, "a movement key", [&] { Require(grid.OnKeyDown(scene.host, move.key, move.modifiers), "a movement key is handled"); });
        const size_t after = ordinalOfFocus();
        switch (move.key)
        {
            case VK_DOWN: Require(after == (std::min)(before + 1u, visible.size() - 1u), std::format("{}: Down reaches the next visible row", seed)); break;
            case VK_UP: Require(after == (before == 0u ? 0u : before - 1u), std::format("{}: Up reaches the previous visible row", seed)); break;
            case VK_HOME: Require(after == 0u, std::format("{}: Home reaches the first visible row", seed)); break;
            case VK_END: Require(after == visible.size() - 1u, std::format("{}: End reaches the last visible row", seed)); break;
            case VK_NEXT: Require(after >= before && (after > before || before + 1u == visible.size()), std::format("{}: Page Down progresses", seed)); break;
            default: Require(after <= before && (after < before || before == 0u), std::format("{}: Page Up progresses", seed)); break;
        }
        if ((move.modifiers & MK_SHIFT) != 0u && (move.modifiers & MK_CONTROL) == 0u)
        {
            // A Shift range is the reference visible rows from the anchor to the row reached, in visible order.
            const auto anchor = grid.GetSelectionModel().GetAnchor();
            Require(anchor.has_value(), "a Shift range keeps its anchor");
            const size_t anchorOrdinal =
                static_cast<size_t>(std::ranges::find(visible, static_cast<size_t>(*anchor ^ SeededGridModel::kIdMask)) - visible.begin());
            const auto [first, last]             = std::minmax(anchorOrdinal, after);
            const std::vector<uint64_t> expected = IdsOf(std::span<const size_t>(visible).subspan(first, last - first + 1u));
            Require(scene.Selection() == expected, std::format("{}: a Shift key selects the reference range", seed));
        }
    }
}

void CheckSelections(Scene& scene, const std::vector<size_t>& visible, const char* seed)
{
    Grid& grid = *scene.grid;
    CheckReports(scene, "select all", [&] { static_cast<void>(grid.OnSelectAll(scene.host)); });
    Require(scene.Selection() == IdsOf(visible), std::format("{}: select all selects the visible rows in order", seed));
    CheckReports(scene, "select all again", [&] { static_cast<void>(grid.OnSelectAll(scene.host)); });
    if (visible.empty())
        return;
    CheckReports(scene, "a click after select all", [&] { Require(grid.RequestSelectRow(visible.front(), 0u), "the click is handled"); });
    CheckReports(scene, "a click on the selected row", [&] { Require(grid.RequestSelectRow(visible.front(), 0u), "the click is handled"); });
    CheckReports(scene, "a Shift click", [&] { Require(grid.RequestSelectRow(visible[visible.size() * 3u / 4u], MK_SHIFT), "the Shift click is handled"); });
    Require(scene.Selection() == IdsOf(std::span<const size_t>(visible).first(visible.size() * 3u / 4u + 1u)),
            std::format("{}: Shift click selects the reference range", seed));
    CheckReports(scene, "a Shift click back", [&] { Require(grid.RequestSelectRow(visible.front(), MK_SHIFT), "the Shift click is handled"); });
    CheckReports(scene, "a Ctrl click", [&] { Require(grid.RequestSelectRow(visible.back(), MK_CONTROL), "the Ctrl click is handled"); });
    const std::vector<uint64_t> beforeRange = scene.Selection();
    CheckReports(scene, "a Ctrl+Shift click", [&] { Require(grid.RequestSelectRow(visible[visible.size() / 2u], MK_CONTROL | MK_SHIFT), "handled"); });
    {
        // The rows from the anchor (the last row, Ctrl-clicked) back to the clicked row join after the selected ones, from the
        // anchor toward it.
        std::vector<uint64_t> expected = beforeRange;
        for (size_t ordinal = visible.size() - 1u; ordinal + 1u > visible.size() / 2u; --ordinal)
            if (std::ranges::find(expected, SeededGridModel::IdOf(visible[ordinal])) == expected.end())
                expected.push_back(SeededGridModel::IdOf(visible[ordinal]));
        Require(scene.Selection() == expected, std::format("{}: Ctrl+Shift click adds the reference range from the anchor", seed));
    }

    // SetSelectedRowIds: duplicates, ids of hidden rows and ids no row has; the last listed visible row leads.
    std::vector<uint64_t> requested;
    for (size_t ordinal = 0u; ordinal < visible.size(); ordinal += 3u)
        requested.push_back(SeededGridModel::IdOf(visible[ordinal]));
    if (! requested.empty())
        requested.push_back(requested.front());
    requested.push_back(0xFFFF'FFFF'0000ull);
    for (const SeededGridModel::Group& group : scene.model->Groups())
        if (group.collapsed)
            requested.push_back(SeededGridModel::IdOf(group.start));
    requested.push_back(SeededGridModel::IdOf(visible[visible.size() / 2u]));
    grid.SetSelectedRowIds(requested);
    std::vector<uint64_t> expected;
    const std::unordered_set<uint64_t> asked(requested.begin(), requested.end());
    for (const size_t row : visible)
        if (asked.contains(SeededGridModel::IdOf(row)))
            expected.push_back(SeededGridModel::IdOf(row));
    Require(scene.Selection() == expected, std::format("{}: SetSelectedRowIds selects the listed visible rows once each, in visible order", seed));
    Require(grid.GetFocusedRowId() == std::optional<uint64_t>(SeededGridModel::IdOf(visible[visible.size() / 2u])) &&
                grid.GetSelectionModel().GetAnchor() == grid.GetFocusedRowId(),
            std::format("{}: the last listed visible row is the focus and the anchor", seed));
}

void CheckGroupToggles(Scene& scene, const char* seed)
{
    Grid& grid = *scene.grid;
    if (scene.model->Groups().empty())
        return;
    // Collapse the group of the focused row with the collapse key, expand it again, then restore every group through ApplyGroupLayout.
    const std::vector<GridGroupLayoutEntry> layout = grid.CaptureGroupLayout();
    const bool rightToLeft                         = grid.IsRightToLeft();
    for (const UINT key : {rightToLeft ? UINT{VK_RIGHT} : UINT{VK_LEFT}, rightToLeft ? UINT{VK_LEFT} : UINT{VK_RIGHT}})
    {
        CheckReports(scene, "a group toggle key", [&] { static_cast<void>(grid.OnKeyDown(scene.host, key, 0u)); });
        const std::vector<size_t> visible = ReferenceVisibleRows(*scene.model);
        Require(grid.GetVisibleRowCount() <= visible.size() && (visible.empty() || grid.FindVisibleRowOrdinal(visible.back()) == visible.size() - 1u),
                std::format("{}: the toggled layout is the model's", seed));
    }
    CheckReports(scene, "a group layout restore", [&] { grid.ApplyGroupLayout(layout); });
    const std::vector<GridGroupLayoutEntry> restored = grid.CaptureGroupLayout();
    Require(
        restored.size() == layout.size() &&
            std::ranges::equal(restored, layout, [](const auto& a, const auto& b) { return a.groupStableId == b.groupStableId && a.collapsed == b.collapsed; }),
        std::format("{}: ApplyGroupLayout restores every group", seed));
}
} // namespace

using namespace DxUi;

void TestGridDataPathsEqualTheUncachedImplementationForSeededModels()
{
#if DXUI_ENABLE_DIAGNOSTICS
    const uint64_t mismatchesBefore = Grid::DebugGetDataPathMismatches();
#endif
#if defined(_DEBUG)
    const bool large = false; // the 200,000-row seed runs in Release, where its uncached comparisons take seconds, not minutes
#else
    const bool large = true;
#endif
    for (const Seed& seed : Seeds(large))
    {
        Scene scene(seed);
        const std::vector<size_t> visible = ReferenceVisibleRows(*scene.model);
        Require(scene.grid->GetVisibleRowCount() <= visible.size(), "the grid is built");
        CheckGeometry(scene, visible, seed.name);
        CheckKeys(scene, visible, seed.name);
        CheckSelections(scene, visible, seed.name);
        CheckGroupToggles(scene, seed.name);
    }
#if DXUI_ENABLE_DIAGNOSTICS
    Require(Grid::DebugGetDataPathMismatches() == mismatchesBefore, "no cached answer differed from the uncached one");
#endif
}

// What the caches cost and when they are filled: geometry, hit tests and keys read them without asking the model for groups or
// columns; NotifyDataChanged, SetModel, a group toggle (published or not) and ApplyColumnLayout fill them again.
void TestGridDataCachesFillOncePerNotification()
{
    Scene scene(Seed{"caches", 200u, {{10u, 20u, false}, {40u, 5u, true}, {60u, 30u, false}}});
    Grid& grid = *scene.grid;
    static_cast<void>(grid.GetVisibleRowRect(0u));
    const Detail::WorkCounterScope steady;
    for (size_t row = 0u; row < 20u; ++row)
    {
        static_cast<void>(grid.GetVisibleRowRect(row));
        static_cast<void>(grid.FindRowAtPoint(PointDip{40.0f, 60.0f + static_cast<float>(row)}));
        static_cast<void>(grid.FindVisibleRowOrdinal(row * 9u));
    }
    Require(grid.RequestSelectRow(0u, 0u), "a click on the first row");
    for (size_t key = 0u; key < 30u; ++key)
        Require(grid.OnKeyDown(scene.host, VK_DOWN, 0u), "Down is handled");
    const Detail::WorkCounters& work     = steady.Delta();
    const size_t verificationCollections = Grid::DebugGetDataPathVerification() == Grid::DataPathVerification::Off ? 0u : work.lists.gridCollectOrderedGroups;
    Require(work.lists.gridGroupLayoutRebuilds == 0u && work.lists.gridColumnDescRebuilds == 0u, "geometry, hits and keys fill no cache");
    Require(work.lists.gridCollectOrderedGroups == verificationCollections, "only verification collects the groups again");
    Require(work.lists.gridSelectionCopies == 0u || Grid::DebugGetDataPathVerification() != Grid::DataPathVerification::Off,
            "a click and keys copy no selection");

    const auto rebuildsOf = [&](auto&& action)
    {
        const Detail::WorkCounterScope scope;
        action();
        static_cast<void>(grid.GetVisibleRowRect(0u));
        return scope.Delta().lists.gridGroupLayoutRebuilds;
    };
    Require(rebuildsOf([&] { grid.NotifyDataChanged(); }) == 1u, "NotifyDataChanged fills the group layout once");
    Require(rebuildsOf([&] { scene.grid->SetSelectionMode(GridSelectionMode::Single); }) == 0u, "a selection mode change keeps it");

    // A delegate that toggles the group without publishing it: the toggle is read afresh.
    class SilentDelegate final : public IGridDelegate
    {
    public:
        explicit SilentDelegate(SeededGridModel& model) : _model(model)
        {
        }
        SilentDelegate(const SilentDelegate&)            = delete;
        SilentDelegate& operator=(const SilentDelegate&) = delete;
        using IGridDelegate::OnGridGroupToggled;
        void OnGridGroupToggled(Grid& /*sender*/, uint64_t groupStableId, bool collapsed) override
        {
            _model.SetCollapsed(groupStableId, collapsed);
        }

    private:
        SeededGridModel& _model;
    } silent(*scene.model);
    grid.SetDelegate(&silent);
    grid.SetSelectionMode(GridSelectionMode::Extended);
    Require(grid.RequestFocusRow(15u), "the focus moves into the first group");
    Require(grid.OnKeyDown(scene.host, VK_LEFT, 0u), "the collapse key is handled");
    Require(scene.model->Groups()[0].collapsed && ! grid.FindVisibleRowOrdinal(15u).has_value(), "an unpublished collapse hides the group's rows");
    Require(grid.FindVisibleRowOrdinal(30u) == std::optional<size_t>(10u), "the rows after it move up by the collapsed rows");
    grid.SetDelegate(scene.delegate.get());

    // Columns: one fill per data revision or column layout, and a changed title is read after NotifyDataChanged.
    const auto columnFillsOf = [&](auto&& action)
    {
        const Detail::WorkCounterScope scope;
        action();
        static_cast<void>(grid.GetCellLayoutMetrics(scene.host, 0u, 1u));
        static_cast<void>(grid.GetCellLayoutMetrics(scene.host, 1u, 2u));
        return scope.Delta().lists.gridColumnDescRebuilds;
    };
    Require(columnFillsOf([] {}) <= 1u && columnFillsOf([] {}) == 0u, "cell metrics read the cached columns");
    Require(columnFillsOf([&] { grid.ApplyColumnLayout(grid.CaptureColumnLayout()); }) == 1u, "a column layout change fills them again");
    Require(columnFillsOf([&] { grid.NotifyDataChanged(); }) == 1u, "a data change fills them again");

    // A new model releases the old one's groups.
    SeededGridModel ungrouped(10u, {});
    grid.SetModel(&ungrouped);
    Require(grid.FindVisibleRowOrdinal(9u) == std::optional<size_t>(9u), "the new model's rows are all visible");
    grid.SetModel(nullptr);
}

// The verification catches a model that changes its groups or a column without NotifyDataChanged (the rule the caches rest on),
// and reads nothing amiss once the change is published. Diagnostics builds only: elsewhere there is no verification.
void TestGridDataPathVerificationCatchesAModelThatDoesNotNotify()
{
#if DXUI_ENABLE_DIAGNOSTICS
    const Grid::DataPathVerification previous = Grid::DebugGetDataPathVerification();
    Grid::DebugSetDataPathVerification(Grid::DataPathVerification::Count);
    const auto restore = wil::scope_exit([previous] { Grid::DebugSetDataPathVerification(previous); });
    Scene scene(Seed{"unpublished", 60u, {{10u, 10u, false}, {30u, 10u, false}}});
    Grid& grid = *scene.grid;
    static_cast<void>(grid.GetVisibleRowRect(0u));
    const uint64_t before = Grid::DebugGetDataPathMismatches();
    static_cast<void>(grid.FindVisibleRowOrdinal(35u));
    Require(Grid::DebugGetDataPathMismatches() == before, "a model that changed nothing reads no mismatch");
    scene.model->SetCollapsed(1000u, true); // not published
    static_cast<void>(grid.FindVisibleRowOrdinal(35u));
    Require(Grid::DebugGetDataPathMismatches() > before, "a collapse the model did not publish is caught");
    grid.NotifyDataChanged();
    const uint64_t published = Grid::DebugGetDataPathMismatches();
    Require(grid.FindVisibleRowOrdinal(35u) == std::optional<size_t>(25u) && Grid::DebugGetDataPathMismatches() == published,
            "once published, the collapse is read and nothing is amiss");
    static_cast<void>(grid.GetCellLayoutMetrics(scene.host, 0u, 0u));
    Require(Grid::DebugGetDataPathMismatches() == published, "the cached columns match the model");
#endif
}

// A GridSelectionModel's epoch moves exactly when its ordered ids change, reads 0 exactly when it is empty, travels with a copy
// and leaves a moved-from model at 0.
void TestSelectionModelEpochMovesExactlyWhenTheOrderedSelectionChanges()
{
    std::mt19937_64 random(0x5EED0025u);
    GridSelectionModel model;
    Require(model.GetEpoch() == 0u, "an empty model reads epoch 0");
    std::vector<uint64_t> universe;
    for (uint64_t id = 0u; id < 40u; ++id)
        universe.push_back(id * 3u + 1u);
    for (size_t step = 0u; step < 4000u; ++step)
    {
        const std::vector<uint64_t> before(model.GetOrderedSelection().begin(), model.GetOrderedSelection().end());
        const uint64_t epoch = model.GetEpoch();
        const uint64_t id    = universe[random() % universe.size()];
        switch (random() % 7u)
        {
            case 0u: model.Clear(); break;
            case 1u: model.SetSingle(id); break;
            case 2u: model.Toggle(id); break;
            case 3u: model.SetRange(universe, universe[random() % universe.size()], id); break;
            case 4u:
            {
                std::vector<uint64_t> requested;
                for (size_t count = random() % 12u; count > 0u; --count)
                    requested.push_back(random() % 4u == 0u ? 9999u : universe[random() % universe.size()]);
                model.AssignInOrder(universe, requested, id);
                break;
            }
            case 5u:
            {
                std::vector<uint64_t> kept;
                for (const uint64_t candidate : universe)
                    if (random() % 5u != 0u)
                        kept.push_back(candidate);
                model.PreserveOrdered(kept);
                break;
            }
            default: model.SetAnchor(id); break;
        }
        const bool changed = ! std::ranges::equal(before, model.GetOrderedSelection());
        Require((model.GetEpoch() != epoch) == changed, "the epoch moves exactly when the ordered ids change");
        Require((model.GetEpoch() == 0u) == (model.GetCount() == 0u), "the epoch reads 0 exactly when the selection is empty");
    }
    model.SetSingle(universe[3]);
    GridSelectionModel copy = model;
    Require(copy.GetEpoch() == model.GetEpoch(), "a copy keeps the epoch of the selection it copies");
    copy.Toggle(universe[4]);
    Require(copy.GetEpoch() != model.GetEpoch(), "a change of the copy takes an epoch of its own");
    GridSelectionModel moved = std::move(copy);
    Require(copy.GetEpoch() == 0u && copy.GetCount() == 0u && ! copy.GetAnchor().has_value(), "a moved-from model is empty with epoch 0");
    Require(moved.GetCount() == 2u && moved.GetEpoch() != 0u, "the moved-to model keeps the selection and its epoch");
}

// AssignInOrder's membership (an ascending copy behind a table of bits) answers as the hash set it replaced: repeated ids, ids
// absent from the visible list and repeated visible ids included.
void TestSelectionModelAssignInOrderMatchesTheHashSetMembership()
{
    std::mt19937_64 random(0x5EED0026u);
    for (size_t trial = 0u; trial < 300u; ++trial)
    {
        std::vector<uint64_t> visible;
        for (size_t count = random() % 3000u; count > 0u; --count)
            visible.push_back(random() % 5000u);
        std::vector<uint64_t> requested;
        for (size_t count = random() % 3000u; count > 0u; --count)
            requested.push_back(random() % 6000u);
        std::unordered_set<uint64_t> asked(requested.begin(), requested.end());
        std::vector<uint64_t> expected;
        for (const uint64_t id : visible)
            if (asked.erase(id) != 0u)
                expected.push_back(id);
        GridSelectionModel model;
        model.AssignInOrder(visible, requested, std::nullopt);
        Require(std::ranges::equal(model.GetOrderedSelection(), expected), "AssignInOrder keeps each requested visible id once, in visible order");
        for (const uint64_t id : requested)
            Require(model.IsSelected(id) == std::ranges::contains(expected, id), "membership answers for every requested id");
    }
}
