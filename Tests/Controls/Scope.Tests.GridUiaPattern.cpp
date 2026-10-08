#include "Scope.Tests.GridUiaPattern.h"

#include "Controls.Tests.DxUiTestHelpers.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

class GridUiaPatternModel final : public DxUi::IGridModel
{
public:
    explicit GridUiaPatternModel(size_t rowCount) noexcept : _rowCount(rowCount)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = columnIndex == 0u ? L"name" : L"details";
        column.title    = columnIndex == 0u ? L"Name" : L"Details";
        column.widthDip = 180.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        _cellReads.fetch_add(1u, std::memory_order_relaxed);
        if (_throwOnRead)
        {
            if (_throwBadAlloc)
            {
                throw std::bad_alloc();
            }
            throw std::runtime_error("requested test callback failure");
        }
        if (_replaceOnRow && rowIndex == _replaceOnRow.value())
        {
            _replaceOnRow.reset();
            auto replace = std::move(_replaceControl);
            if (replace)
            {
                replace();
            }
        }
        if (_reassignOnRow && rowIndex == _reassignOnRow.value())
        {
            _reassignOnRow.reset();
            auto reassign = std::move(_reassignModel);
            if (reassign)
            {
                reassign();
            }
        }
        if (_shrinkOnRow && rowIndex == _shrinkOnRow.value())
        {
            _shrinkOnRow.reset();
            _rowCount.store(_shrinkTo, std::memory_order_release);
        }
        outCell.kind        = DxUi::GridCellKind::Text;
        std::wstring prefix = L"row-";
        if (_contentRevision != 0u)
        {
            prefix = L"revision-" + std::to_wstring(_contentRevision) + L"-row-";
        }
        outCell.text = prefix + std::to_wstring(rowIndex) + (columnIndex == 0u ? L"-name" : L"-details");
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        _stableIdCalls.fetch_add(1u, std::memory_order_relaxed);
        if (_stableIdTriggerRow && rowIndex == _stableIdTriggerRow.value())
        {
            ++_stableIdTriggerOccurrence;
            if (_stableIdTriggerOccurrence == _stableIdTriggerAtOccurrence)
            {
                _stableIdTriggerRow.reset();
                auto callback = std::move(_stableIdCallback);
                if (callback)
                {
                    callback();
                }
            }
        }
        if (rowIndex >= _rowCount.load(std::memory_order_acquire))
        {
            _outOfRangeStableIdCalls.fetch_add(1u, std::memory_order_relaxed);
            return 0u;
        }
        return static_cast<uint64_t>(rowIndex) + 1u;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId == 0u || rowId > _rowCount.load(std::memory_order_acquire))
        {
            return std::nullopt;
        }
        return static_cast<size_t>(rowId - 1u);
    }

    [[nodiscard]] size_t CellReads() const noexcept
    {
        return _cellReads.load(std::memory_order_relaxed);
    }

    void ReplaceControlWhenReading(size_t rowIndex, std::function<void()> callback)
    {
        _replaceOnRow   = rowIndex;
        _replaceControl = std::move(callback);
    }

    void ReassignModelWhenReading(size_t rowIndex, std::function<void()> callback)
    {
        _reassignOnRow = rowIndex;
        _reassignModel = std::move(callback);
    }

    void ReassignModelWhenStableIdRead(size_t rowIndex, size_t occurrence, std::function<void()> callback)
    {
        _stableIdTriggerRow          = rowIndex;
        _stableIdTriggerAtOccurrence = occurrence;
        _stableIdTriggerOccurrence   = 0u;
        _stableIdCallback            = std::move(callback);
    }

    void ShrinkWhenReading(size_t rowIndex, size_t newRowCount)
    {
        _shrinkOnRow = rowIndex;
        _shrinkTo    = newRowCount;
    }

    void SetContentRevision(size_t revision) noexcept
    {
        _contentRevision = revision;
    }

    void SetThrowOnRead(bool enabled, bool badAlloc = false) noexcept
    {
        _throwOnRead   = enabled;
        _throwBadAlloc = badAlloc;
    }

    [[nodiscard]] size_t OutOfRangeStableIdCalls() const noexcept
    {
        return _outOfRangeStableIdCalls.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t StableIdCalls() const noexcept
    {
        return _stableIdCalls.load(std::memory_order_relaxed);
    }

private:
    mutable std::atomic<size_t> _rowCount;
    mutable std::atomic<size_t> _cellReads{0u};
    mutable std::atomic<size_t> _outOfRangeStableIdCalls{0u};
    mutable std::atomic<size_t> _stableIdCalls{0u};
    mutable std::optional<size_t> _replaceOnRow;
    mutable std::function<void()> _replaceControl;
    mutable std::optional<size_t> _reassignOnRow;
    mutable std::function<void()> _reassignModel;
    mutable std::optional<size_t> _stableIdTriggerRow;
    mutable std::function<void()> _stableIdCallback;
    mutable size_t _stableIdTriggerAtOccurrence{};
    mutable size_t _stableIdTriggerOccurrence{};
    mutable std::optional<size_t> _shrinkOnRow;
    size_t _shrinkTo{};
    size_t _contentRevision{};
    bool _throwOnRead{};
    bool _throwBadAlloc{};
};

void AttachGrid(AttachedHostWindow& window, GridUiaPatternModel& model, DxUi::Grid*& outGrid)
{
    auto grid = std::make_unique<DxUi::Grid>();
    outGrid   = grid.get();
    outGrid->SetBounds(D2D1::RectF(0.0f, 0.0f, 560.0f, 280.0f));
    outGrid->SetAccessibleName(L"Virtual results");
    outGrid->SetModel(&model);
    window.Host().SetRoot(std::move(grid));
}

[[nodiscard]] std::wstring ReadGridCellName(IRawElementProviderSimple& cell)
{
    VARIANT name{};
    VariantInit(&name);
    const HRESULT hr = cell.GetPropertyValue(UIA_NamePropertyId, &name);
    const auto clear = wil::scope_exit([&]() noexcept { VariantClear(&name); });
    RequireSucceeded(hr, "grid cell exposes its accessible name");
    Require(name.vt == VT_BSTR, "grid cell accessible name is a string");
    return std::wstring(name.bstrVal, SysStringLen(name.bstrVal));
}

[[nodiscard]] std::vector<LONG> ReadProviderRuntimeId(IRawElementProviderFragment& provider)
{
    SAFEARRAY* runtimeId = nullptr;
    RequireSucceeded(provider.GetRuntimeId(&runtimeId), "UIA element exposes a runtime id");
    const auto destroy = wil::scope_exit([&]() noexcept { SafeArrayDestroy(runtimeId); });
    LONG lower         = 0;
    LONG upper         = -1;
    RequireSucceeded(SafeArrayGetLBound(runtimeId, 1, &lower), "runtime id has a lower bound");
    RequireSucceeded(SafeArrayGetUBound(runtimeId, 1, &upper), "runtime id has an upper bound");
    std::vector<LONG> values;
    values.reserve(static_cast<size_t>(upper - lower + 1));
    for (LONG index = lower; index <= upper; ++index)
    {
        LONG value = 0;
        RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &value), "runtime id value is readable");
        values.push_back(value);
    }
    return values;
}

} // namespace

void TestGridUiaPatternSupportsBoundedOffscreenGetItem()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    constexpr size_t kRowCount  = 2000u;
    constexpr int kOffscreenRow = 1500;
    AttachedHostWindow window;
    GridUiaPatternModel model(kRowCount);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    Require(grid != nullptr, "Grid UIA fixture owns a grid control");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "Grid UIA fixture acquires the window root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "single-grid root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "single-grid root advertises GridPattern");
    Require(patternUnknown != nullptr, "single-grid root returns GridPattern");
    wil::com_ptr_nothrow<IGridProvider> gridPattern;
    RequireSucceeded(patternUnknown.query_to(gridPattern.put()), "GridPattern provider supports IGridProvider");

    int rowCount    = 0;
    int columnCount = 0;
    RequireSucceeded(gridPattern->get_RowCount(&rowCount), "GridPattern returns its row count");
    RequireSucceeded(gridPattern->get_ColumnCount(&columnCount), "GridPattern returns its column count");
    Require(rowCount == static_cast<int>(kRowCount) && columnCount == 2, "GridPattern counts match the virtual model");
    IRawElementProviderSimple* invalidCell = nullptr;
    Require(gridPattern->GetItem(-1, 0, &invalidCell) == E_INVALIDARG && invalidCell == nullptr, "GridPattern rejects a negative row");
    Require(gridPattern->GetItem(rowCount, 0, &invalidCell) == E_INVALIDARG && invalidCell == nullptr, "GridPattern rejects the row-count boundary");
    Require(gridPattern->GetItem(0, columnCount, &invalidCell) == E_INVALIDARG && invalidCell == nullptr, "GridPattern rejects the column-count boundary");

    const auto firstVisibleBefore = grid->GetVisibleRowAt(0u);
    const auto selectedBefore     = grid->GetPrimarySelectedRow();
    const size_t cellReadsBefore  = model.CellReads();
    wil::com_ptr_nothrow<IRawElementProviderSimple> offscreenCell;
    RequireSucceeded(gridPattern->GetItem(kOffscreenRow, 1, offscreenCell.put()), "GridPattern materializes one arbitrary offscreen row");
    Require(offscreenCell != nullptr, "GridPattern returns the requested cell provider");
    Require(ReadGridCellName(*offscreenCell) == L"row-1500-details", "offscreen GetItem returns the requested row and column");
    wil::com_ptr_nothrow<IGridItemProvider> gridItem;
    RequireSucceeded(offscreenCell.query_to(gridItem.put()), "GridPattern cell exposes GridItemPattern");
    int itemRow    = -1;
    int itemColumn = -1;
    RequireSucceeded(gridItem->get_Row(&itemRow), "GridItem reports its requested row");
    RequireSucceeded(gridItem->get_Column(&itemColumn), "GridItem reports its requested column");
    Require(itemRow == kOffscreenRow && itemColumn == 1, "GridItem coordinates match GetItem");
    Require(grid->GetVisibleRowAt(0u) == firstVisibleBefore && grid->GetPrimarySelectedRow() == selectedBefore,
            "GetItem does not scroll, select, or focus a row");
    const size_t ordinaryVisibleCellBudget = grid->GetVisibleRowCount() * grid->GetModel()->GetColumnCount() * 2u;
    Require(model.CellReads() - cellReadsBefore <= ordinaryVisibleCellBudget + grid->GetModel()->GetColumnCount(),
            "offscreen GetItem reads only the ordinary viewport and the one requested row");

    // Also exercise the public UIA client contract from a different apartment. The owner pumps only its own hidden,
    // non-activating test window while the client requests an uncached offscreen row.
    std::atomic<bool> clientReady{false};
    std::atomic<bool> clientFinished{false};
    std::atomic<HRESULT> clientResult{E_PENDING};
    std::wstring clientName;
    const HWND hwnd = window.Hwnd();
    std::jthread client([&]
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]() noexcept
        {
            if (SUCCEEDED(initialized))
            {
                CoUninitialize();
            }
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        wil::com_ptr_nothrow<IUIAutomationGridPattern> clientGrid;
        wil::com_ptr_nothrow<IUIAutomationElement> cell;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
        {
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        }
        if (SUCCEEDED(hr))
        {
            hr = automation->ElementFromHandle(hwnd, element.put());
        }
        if (SUCCEEDED(hr))
        {
            hr = element->GetCurrentPatternAs(UIA_GridPatternId, __uuidof(IUIAutomationGridPattern), clientGrid.put_void());
        }
        if (SUCCEEDED(hr))
        {
            hr = clientGrid->GetItem(kOffscreenRow + 1, 0, cell.put());
        }
        if (SUCCEEDED(hr))
        {
            wil::unique_bstr name;
            hr = cell->get_CurrentName(name.put());
            if (SUCCEEDED(hr) && name)
            {
                clientName.assign(name.get(), SysStringLen(name.get()));
            }
        }
        clientResult.store(hr, std::memory_order_release);
        clientReady.store(true, std::memory_order_release);
        clientFinished.store(true, std::memory_order_release);
    });
    const ULONGLONG clientDeadline = GetTickCount64() + 10000u;
    while (! clientFinished.load(std::memory_order_acquire) && GetTickCount64() < clientDeadline)
    {
        window.PumpMessages(5u);
        Sleep(1u);
    }
    Require(clientReady.load(std::memory_order_acquire) && clientFinished.load(std::memory_order_acquire),
            "foreign UIA client completes while the host pumps its bounded action");
    client.join();
    RequireSucceeded(clientResult.load(std::memory_order_acquire), "foreign UIA GridPattern GetItem succeeds");
    Require(clientName == L"row-1501-name", "foreign UIA client reads the requested offscreen cell name");

    for (int index = 0; index < 16; ++index)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> requested;
        RequireSucceeded(gridPattern->GetItem(1600 + index, 0, requested.put()), "GridPattern keeps a bounded recent offscreen-row cache");
    }
    // The cell requested first is now outside the fixed recent-row window and must fail instead of aliasing another row.
    VARIANT evictedName{};
    VariantInit(&evictedName);
    const HRESULT evictedResult = offscreenCell->GetPropertyValue(UIA_NamePropertyId, &evictedName);
    VariantClear(&evictedName);
    Require(evictedResult == UIA_E_ELEMENTNOTAVAILABLE, "an evicted GridItem reports unavailable instead of aliasing another row");
    wil::com_ptr_nothrow<IRawElementProviderSimple> evictedCell;
    RequireSucceeded(gridPattern->GetItem(1500, 0, evictedCell.put()), "a fresh GetItem can re-materialize a row after eviction");
    Require(ReadGridCellName(*evictedCell) == L"row-1500-name", "re-materialized GetItem returns the stable row");

    GridUiaPatternModel replacementModel(kRowCount);
    grid->SetModel(&replacementModel);
    grid->NotifyDataChanged();
    window.Host().RefreshAccessibilitySnapshot();
    VARIANT replacedModelName{};
    VariantInit(&replacedModelName);
    const HRESULT replacedModelResult = evictedCell->GetPropertyValue(UIA_NamePropertyId, &replacedModelName);
    VariantClear(&replacedModelName);
    Require(replacedModelResult == UIA_E_ELEMENTNOTAVAILABLE, "a retained GridItem cannot resolve through a replacement model that reuses its row ids");

    window.Host().SetRoot(nullptr);
    VARIANT detachedName{};
    VariantInit(&detachedName);
    const HRESULT detachedResult = offscreenCell->GetPropertyValue(UIA_NamePropertyId, &detachedName);
    VariantClear(&detachedName);
    Require(detachedResult == UIA_E_ELEMENTNOTAVAILABLE, "retained GridItem disconnects after the grid is detached");
}

void TestGridUiaPatternStopsWhenCellReadReplacesItsControl()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    constexpr size_t kReplaceRow = 63u;
    AttachedHostWindow window;
    GridUiaPatternModel model(80u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    Require(grid != nullptr, "reentrant Grid UIA fixture owns a grid control");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "reentrant Grid UIA fixture acquires a root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "reentrant root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "reentrant fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "reentrant fixture exposes IGridProvider");
    model.ReplaceControlWhenReading(kReplaceRow,
                                    [&window]
    {
        auto replacement = std::make_unique<Button>(L"replacement");
        replacement->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 40.0f));
        window.Host().SetRoot(std::move(replacement));
    });

    IRawElementProviderSimple* requested = nullptr;
    const HRESULT result                 = pattern->GetItem(static_cast<int>(kReplaceRow), 0, &requested);
    wil::com_ptr_nothrow<IRawElementProviderSimple> retained;
    retained.attach(requested);
    Require(result == UIA_E_ELEMENTNOTAVAILABLE && retained == nullptr,
            "a model callback that replaces the grid makes the in-flight GetItem unavailable safely");
}

void TestGridUiaPatternStopsWhenCellReadShrinksItsModel()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    constexpr size_t kShrinkRow   = 63u;
    constexpr size_t kNewRowCount = 10u;
    AttachedHostWindow window;
    GridUiaPatternModel model(80u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    Require(grid != nullptr, "shrinking Grid UIA fixture owns a grid control");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "shrinking Grid UIA fixture acquires a root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "shrinking root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "shrinking fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "shrinking fixture exposes IGridProvider");
    model.ShrinkWhenReading(kShrinkRow, kNewRowCount);

    IRawElementProviderSimple* requested = nullptr;
    const HRESULT result                 = pattern->GetItem(static_cast<int>(kShrinkRow), 0, &requested);
    wil::com_ptr_nothrow<IRawElementProviderSimple> retained;
    retained.attach(requested);
    Require(result == UIA_E_ELEMENTNOTAVAILABLE && retained == nullptr,
            "a same-model row shrink during cell data retrieval makes the in-flight GetItem unavailable");
    Require(model.OutOfRangeStableIdCalls() == 0u, "Grid UIA does not query a row id after a cell callback shrinks the model past that row");

    grid->NotifyDataChanged();
    window.Host().RefreshAccessibilitySnapshot();
    int rowCount = 0;
    RequireSucceeded(pattern->get_RowCount(&rowCount), "GridPattern refreshes its row count after model shrink");
    Require(rowCount == static_cast<int>(kNewRowCount), "GridPattern reports the shrunken model row count");
    IRawElementProviderSimple* outOfRange = nullptr;
    Require(pattern->GetItem(static_cast<int>(kShrinkRow), 0, &outOfRange) == E_INVALIDARG && outOfRange == nullptr,
            "GridPattern rejects the old row index after the model shrink is published");
}

void TestGridUiaPatternKeepsNestedSameAddressPublication()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(80u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    Require(grid != nullptr, "nested publication fixture owns a grid control");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "nested root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "nested publication fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "nested publication fixture exposes IGridProvider");

    model.ReassignModelWhenReading(0u,
                                   [&model, grid]
    {
        model.SetContentRevision(1u);
        grid->SetModel(&model);       // Publishes a nested snapshot for this new assignment.
        model.SetContentRevision(2u); // The outer, older capture must not overwrite that nested publication.
    });
    window.Host().RefreshAccessibilitySnapshot();

    wil::com_ptr_nothrow<IRawElementProviderSimple> firstCell;
    RequireSucceeded(pattern->GetItem(0, 0, firstCell.put()), "the nested model assignment remains queryable");
    Require(ReadGridCellName(*firstCell) == L"revision-1-row-0-name",
            "an unwinding outer capture does not overwrite the newer nested same-address model publication");
}

void TestGridUiaPatternStopsPointHitsAfterModelReplacement()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(80u);
    GridUiaPatternModel replacementModel(80u);
    replacementModel.SetContentRevision(1u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    Require(grid != nullptr, "point-hit replacement fixture owns a grid control");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "point-hit replacement root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "point-hit replacement fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "point-hit replacement fixture exposes IGridProvider");

    const std::optional<size_t> firstVisible = grid->GetVisibleRowAt(0u);
    Require(firstVisible.has_value(), "point-hit replacement fixture has a visible row");
    size_t oldModelCallsAfterReplacement = 0u;
    model.ReassignModelWhenStableIdRead(firstVisible.value(),
                                        2u,
                                        [&model, &replacementModel, grid, &oldModelCallsAfterReplacement]
    {
        grid->SetModel(&replacementModel);
        oldModelCallsAfterReplacement = model.StableIdCalls();
    });
    window.Host().RefreshAccessibilitySnapshot();

    Require(oldModelCallsAfterReplacement != 0u, "point-hit capture reached the scheduled model replacement callback");
    Require(model.StableIdCalls() == oldModelCallsAfterReplacement, "point-hit capture stops immediately after a model callback replaces the grid model");
    wil::com_ptr_nothrow<IRawElementProviderSimple> replacementCell;
    RequireSucceeded(pattern->GetItem(static_cast<int>(firstVisible.value()), 0, replacementCell.put()),
                     "the nested replacement snapshot remains queryable after point-hit capture aborts");
    Require(ReadGridCellName(*replacementCell) == L"revision-1-row-" + std::to_wstring(firstVisible.value()) + L"-name",
            "point-hit capture does not overwrite the replacement model's nested publication");
}

void TestGridUiaPatternInvalidatesProvidersAfterSameAddressModelAssignment()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    constexpr size_t kRow = 1500u;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "same-address root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "same-address fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "same-address fixture exposes IGridProvider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> previousCell;
    RequireSucceeded(pattern->GetItem(static_cast<int>(kRow), 0, previousCell.put()), "first assignment returns an offscreen cell");
    Require(ReadGridCellName(*previousCell) == L"row-1500-name", "first assignment exposes its original content");
    wil::com_ptr_nothrow<IRawElementProviderFragment> previousCellFragment;
    RequireSucceeded(previousCell.query_to(previousCellFragment.put()), "first assignment cell is a fragment");
    wil::com_ptr_nothrow<IRawElementProviderFragment> previousRow;
    RequireSucceeded(previousCellFragment->Navigate(NavigateDirection_Parent, previousRow.put()), "first assignment cell has a row parent");
    Require(previousRow != nullptr, "first assignment row provider exists");
    const std::vector<LONG> previousRowRuntimeId = ReadProviderRuntimeId(*previousRow);

    model.SetContentRevision(1u);
    const uint64_t oldAssignment = grid->GetModelAssignmentGeneration();
    grid->SetModel(&model);
    Require(grid->GetModelAssignmentGeneration() != oldAssignment, "reassigning the same model address advances its generation");
    VARIANT staleName{};
    VariantInit(&staleName);
    const HRESULT staleResult = previousCell->GetPropertyValue(UIA_NamePropertyId, &staleName);
    VariantClear(&staleName);
    Require(staleResult == UIA_E_ELEMENTNOTAVAILABLE, "a retained cell provider is invalid after the same model address is assigned as a new model revision");
    SAFEARRAY* staleRowRuntimeId = nullptr;
    const HRESULT staleRowResult = previousRow->GetRuntimeId(&staleRowRuntimeId);
    if (staleRowRuntimeId)
    {
        SafeArrayDestroy(staleRowRuntimeId);
    }
    Require(staleRowResult == UIA_E_ELEMENTNOTAVAILABLE, "a retained row provider is invalid after the same model address is assigned as a new model revision");

    wil::com_ptr_nothrow<IRawElementProviderSimple> currentCell;
    RequireSucceeded(pattern->GetItem(static_cast<int>(kRow), 0, currentCell.put()), "new assignment rematerializes its offscreen cell");
    Require(ReadGridCellName(*currentCell) == L"revision-1-row-1500-name", "the new assignment exposes updated content at the same address and row id");
    wil::com_ptr_nothrow<IRawElementProviderFragment> currentCellFragment;
    RequireSucceeded(currentCell.query_to(currentCellFragment.put()), "new assignment cell is a fragment");
    wil::com_ptr_nothrow<IRawElementProviderFragment> currentRow;
    RequireSucceeded(currentCellFragment->Navigate(NavigateDirection_Parent, currentRow.put()), "new assignment cell has a row parent");
    Require(currentRow && ReadProviderRuntimeId(*currentRow) != previousRowRuntimeId,
            "new model assignment produces a distinct row runtime id even when address and stable row id match");
}

void TestGridUiaPatternContainsModelExceptionsAtTheProviderBoundary()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "throwing root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "throwing fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "throwing fixture exposes IGridProvider");
    model.SetThrowOnRead(true);
    IRawElementProviderSimple* failedCell = nullptr;
    const HRESULT result                  = pattern->GetItem(1500, 0, &failedCell);
    wil::com_ptr_nothrow<IRawElementProviderSimple> retainedFailure;
    retainedFailure.attach(failedCell);
    Require(result == E_FAIL && retainedFailure == nullptr, "a standard model exception becomes a failed HRESULT at the COM boundary");

    model.SetThrowOnRead(true, true);
    IRawElementProviderSimple* outOfMemoryCell = nullptr;
    const HRESULT outOfMemoryResult            = pattern->GetItem(1501, 0, &outOfMemoryCell);
    wil::com_ptr_nothrow<IRawElementProviderSimple> retainedOutOfMemory;
    retainedOutOfMemory.attach(outOfMemoryCell);
    Require(outOfMemoryResult == E_OUTOFMEMORY && retainedOutOfMemory == nullptr, "a model allocation failure maps to E_OUTOFMEMORY at the COM boundary");

    model.SetThrowOnRead(false);
    wil::com_ptr_nothrow<IRawElementProviderSimple> recoveredCell;
    RequireSucceeded(pattern->GetItem(1500, 0, recoveredCell.put()), "a later valid GetItem succeeds after a model callback exception");
    Require(ReadGridCellName(*recoveredCell) == L"row-1500-name", "failed callback publication does not expose a partial cell");
}
