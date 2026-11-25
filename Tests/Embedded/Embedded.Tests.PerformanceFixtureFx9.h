#pragma once
// FX9 Large-list scroll (plan W0.4): a Grid over a synthetic 100,000-row or 1,000,000-row model beside a Tree over a
// synthetic 10,000-row model, each scrolled continuously for 10 s of the frozen fixture clock at 120 Hz, one wheel notch
// per frame. Both models answer every row arithmetically and store no rows, so any memory that grows with the row count is
// the library's. Missed frames (G6) are a hardware timing verdict and stay "not measured" here; the receipt records the
// work of every scroll step and the memory of each row count. It runs on the embedded host only: the window runner
// presents every frame of its off-screen window (about 90 ms per frame on WARP in x64 Debug), so 2,440 frames twice
// outlast the Rendering suite's 300 s test deadline; native presented-frame evidence for G6 comes from the W0.8 trace.
// The last phase (W2.6) presses navigation keys on the focused Tree: its model overrides FindVisibleItemById, so the gate
// is that each key reads the model's row for the focused item and no other.
#include "Embedded.Tests.PerformanceFixture.h"

#include <cmath>
#include <format>

namespace PerformanceFixtures::Fx9
{
inline constexpr size_t kGridRows      = 100'000u;
inline constexpr size_t kMillionRows   = 1'000'000u;
inline constexpr size_t kTreeRows      = 10'000u;
inline constexpr size_t kGridColumns   = 5u;
inline constexpr size_t kWarmSteps     = 4u;
inline constexpr size_t kScrollSteps   = 1'200u; // 10 s at 120 Hz
inline constexpr float kWheelNotchDown = -120.0f;

inline constexpr std::array kBudgets{std::string_view("G6"), std::string_view("G15"), std::string_view("G13")};
inline constexpr std::array kVariants{
    // The suites' variant: the 100,000-row Grid.
    Variant{"default", 1280u, 720u, 96.0f, false, false},
    // The same screen over 1,000,000 rows: its memory beside default's shows what grows with the row count.
    Variant{"rows-1m", 1280u, 720u, 96.0f, false, true},
    // W9.4 reads cell memory at 1080p and 4K; G6 is judged at 120 Hz on hardware (W0.8, W4.9, W5).
    Variant{"fhd", 1920u, 1080u, 96.0f, false, true},
    Variant{"uhd-192dpi", 3840u, 2160u, 192.0f, false, true},
};
inline constexpr size_t kHoverSteps = 20u;
inline constexpr size_t kHoverPhase = 1u;
inline constexpr size_t kGridPhase  = 2u;
inline constexpr size_t kTreePhase  = 3u;
// W2.6: keys on the focused Tree after the scroll phases. Step 0 focuses it and focuses its first visible row (a scripted
// SetFocusedItemId, no key); every other step is one key press and release, in the order below.
inline constexpr size_t kTreeKeysPhase = 4u;
inline constexpr std::array<UINT, 24> kTreeKeys{VK_DOWN, VK_DOWN, VK_DOWN,  VK_DOWN, VK_DOWN, VK_DOWN, VK_DOWN, VK_DOWN, VK_UP, VK_UP,   VK_UP,   VK_UP,
                                                VK_NEXT, VK_NEXT, VK_PRIOR, VK_DOWN, VK_DOWN, VK_UP,   VK_END,  VK_UP,   VK_UP, VK_HOME, VK_DOWN, VK_DOWN};
inline constexpr std::array kPhases{
    Phase{"warm", kWarmSteps, PhaseKind::Warmup, "reveal the middle row of the Grid; one wheel notch on the Grid and one on the Tree; two unchanged frames"},
    Phase{"hover", kHoverSteps, PhaseKind::Measured, "the pointer moves between two Grid rows 40 DIP apart: one hover change per frame"},
    Phase{"grid", kScrollSteps, PhaseKind::Measured, "the clock advances 8 or 9 ms (120 Hz) and one wheel notch down scrolls the Grid"},
    Phase{"tree", kScrollSteps, PhaseKind::Measured, "the clock advances 8 or 9 ms (120 Hz) and one wheel notch down scrolls the Tree"},
    Phase{"tree-keys",
          1u + kTreeKeys.size(),
          PhaseKind::Measured,
          "the Tree takes focus on its first visible row, then one key per frame: Down, Up, Page Down, Page Up, End and Home (W2.6)"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx9.h")};

// Rows are computed from their index; nothing per row is stored.
class SyntheticGridModel final : public DxUi::IGridModel
{
public:
    explicit SyntheticGridModel(size_t rows) noexcept : _rows(rows)
    {
    }
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rows;
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return kGridColumns;
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        static constexpr std::array<const wchar_t*, kGridColumns> kTitles{L"Name", L"Size", L"Kind", L"Modified", L"Owner"};
        static constexpr std::array<float, kGridColumns> kWidths{260.0f, 110.0f, 120.0f, 130.0f, 130.0f};
        DxUi::GridColumnDesc desc;
        desc.id        = std::to_wstring(column);
        desc.title     = kTitles[column % kGridColumns];
        desc.widthDip  = kWidths[column % kGridColumns];
        desc.multiline = false;
        return desc;
    }
    void GetCellData(size_t row, size_t column, DxUi::GridCellData& cell) const override
    {
        static constexpr std::array<const wchar_t*, 7> kKinds{L"Document", L"Image", L"Archive", L"Spreadsheet", L"Video", L"Folder", L"Text"};
        static constexpr std::array<const wchar_t*, 5> kOwners{L"Ana", L"Bao", L"Chidi", L"Dana", L"Emil"};
        switch (column)
        {
            case 0u: cell.text = std::format(L"Record {:07} report.dat", row); break;
            case 1u: cell.text = std::format(L"{} KB", (static_cast<uint64_t>(row) * 2'654'435'761u) % 1'000'000u); break;
            case 2u: cell.text = kKinds[row % kKinds.size()]; break;
            case 3u: cell.text = std::format(L"2026-{:02}-{:02}", 1u + row % 12u, 1u + (row / 12u) % 28u); break;
            default: cell.text = kOwners[(row / 3u) % kOwners.size()]; break;
        }
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
    {
        return id < _rows ? std::optional<size_t>(static_cast<size_t>(id)) : std::nullopt;
    }
    [[nodiscard]] uint64_t GetStableRowId(size_t row) const noexcept override
    {
        return row;
    }

private:
    size_t _rows = 0u;
};

// A fully expanded tree in preorder: every 50 rows one folder, then seven groups of one group row and six items.
class SyntheticTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return kTreeRows;
    }
    void GetVisibleItem(size_t index, DxUi::TreeItemData& item) const override
    {
        const size_t inFolder = index % 50u;
        const size_t inGroup  = inFolder == 0u ? 0u : (inFolder - 1u) % 7u;
        item.id               = index;
        if (inFolder == 0u)
        {
            item.depth    = 0u;
            item.parentId = std::nullopt;
            item.text     = std::format(L"Folder {}", index / 50u);
        }
        else if (inGroup == 0u)
        {
            item.depth    = 1u;
            item.parentId = index - inFolder;
            item.text     = std::format(L"Group {}", index);
        }
        else
        {
            item.depth    = 2u;
            item.parentId = index - inGroup;
            item.text     = std::format(L"Item {} of group {}", inGroup, index - inGroup);
        }
        item.hasChildren = item.depth < 2u;
        item.expanded    = item.hasChildren;
    }
    [[nodiscard]] std::optional<size_t> FindVisibleItemById(uint64_t id) const noexcept override
    {
        return id < kTreeRows ? std::optional<size_t>(static_cast<size_t>(id)) : std::nullopt;
    }
};

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        _grid.emplace(context.variant.name == "rows-1m" ? kMillionRows : kGridRows);
        const float width  = static_cast<float>(context.variant.width) * 96.0f / context.variant.dpi;
        const float height = static_cast<float>(context.variant.height) * 96.0f / context.variant.dpi;
        const float split  = std::floor(width * 0.66f);
        auto root          = std::make_unique<DxUi::Panel>();
        _gridControl       = root->AddChild<DxUi::Grid>();
        _gridControl->SetBounds(D2D1::RectF(8.0f, 8.0f, split - 4.0f, height - 8.0f));
        _gridControl->SetModel(&*_grid);
        _treeControl = root->AddChild<DxUi::Tree>();
        _treeControl->SetBounds(D2D1::RectF(split + 4.0f, 8.0f, width - 8.0f, height - 8.0f));
        _treeControl->SetModel(&_tree);
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        if (phase == 0u)
        {
            switch (step)
            {
                case 0u: _gridControl->EnsureRowVisible(_grid->GetRowCount() / 2u); break;
                case 1u: Wheel(context, *_gridControl); break;
                case 2u: Wheel(context, *_treeControl); break;
                default: break;
            }
            return;
        }
        if (phase == kHoverPhase)
        {
            const D2D1_RECT_F bounds = _gridControl->GetBounds();
            const float scale        = context.variant.dpi / 96.0f;
            const float y            = (bounds.top + bounds.bottom) * 0.5f + (step % 2u == 0u ? 0.0f : 40.0f);
            static_cast<void>(context.port.Pointer({DxUi::PointerAction::Move, (bounds.left + 120.0f) * scale, y * scale}));
            return;
        }
        if (phase == kTreeKeysPhase)
        {
            if (step == 0u)
            {
                context.controls.SetFocusControl(_treeControl, false);
                _treeControl->SetFocusedItemId(static_cast<uint64_t>(_treeControl->GetFirstVisibleItemIndex()));
                return;
            }
            const UINT key = kTreeKeys[(step - 1u) % kTreeKeys.size()];
            static_cast<void>(context.port.Key(key, true));
            static_cast<void>(context.port.Key(key, false));
            return;
        }
        // 8, 8, 9 ms: three frames per 25 ms, so 1,200 steps are exactly 10 s of the fixture clock.
        context.port.AdvanceTime(step % 3u == 2u ? 9u : 8u);
        if (phase == kGridPhase)
            Wheel(context, *_gridControl);
        else if (phase == kTreePhase)
            Wheel(context, *_treeControl);
    }

private:
    // One notch down at the control's centre, in device pixels; a window host receives WM_MOUSEWHEEL there.
    static void Wheel(Context& context, const DxUi::Control& control)
    {
        const D2D1_RECT_F bounds = control.GetBounds();
        const float scale        = context.variant.dpi / 96.0f;
        DxUi::PointerEvent event{DxUi::PointerAction::Wheel, (bounds.left + bounds.right) * 0.5f * scale, (bounds.top + bounds.bottom) * 0.5f * scale};
        event.wheelDelta = kWheelNotchDown;
        static_cast<void>(context.port.Pointer(event));
    }

    std::optional<SyntheticGridModel> _grid;
    SyntheticTreeModel _tree;
    DxUi::Grid* _gridControl = nullptr;
    DxUi::Tree* _treeControl = nullptr;
};

inline constexpr Definition kDefinition{
    .id         = "FX9",
    .title      = "Large-list scroll",
    .scenario   = "A Grid (five single-line text columns) over a synthetic 100,000-row model (variant rows-1m: 1,000,000 rows), revealed at its middle row "
                  "(variants default and rows-1m at 1280x720, fhd at 1920x1080, uhd-192dpi at 3840x2160 and 192 DPI), "
                  "beside a Tree over a synthetic, fully expanded 10,000-row model; absolute layout, reduced motion. A hover phase moves the pointer between two "
                  "Grid rows (20 frames); then each list is scrolled for 10 s of the "
                  "frozen fixture clock at 120 Hz, one wheel notch down per frame (1,200 frames each); a last phase focuses the Tree and presses 24 "
                  "navigation keys, one per frame. The models compute every row from its index and store "
                  "none. G6 (missed frames) is a hardware timing verdict: not measured here; the receipt records the work of every scroll step and memory "
                  "per row count.",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "grid",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx9
