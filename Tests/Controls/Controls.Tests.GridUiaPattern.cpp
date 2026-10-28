#include "Controls.Tests.GridUiaPattern.h"

#include "Controls.Tests.DxUiTestHelpers.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
        if (_throwOnRow && rowIndex == _throwOnRow.value())
        {
            throw std::runtime_error("requested test row failure");
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
        if (_notifyOnRow && rowIndex == _notifyOnRow.value())
        {
            _notifyOnRow.reset();
            auto notify = std::move(_notifyModel);
            if (notify)
                notify();
        }
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

    void NotifyModelAfterReading(size_t rowIndex, std::function<void()> callback)
    {
        _notifyOnRow = rowIndex;
        _notifyModel = std::move(callback);
    }

    void SetThrowOnRead(bool enabled, bool badAlloc = false) noexcept
    {
        _throwOnRead   = enabled;
        _throwBadAlloc = badAlloc;
    }

    void SetThrowOnRow(std::optional<size_t> rowIndex) noexcept
    {
        _throwOnRow = rowIndex;
    }

    // An ordinary data refresh of the same model: the caller notifies the grid afterwards.
    void SetRowCount(size_t rowCount) noexcept
    {
        _rowCount.store(rowCount, std::memory_order_release);
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
    std::optional<size_t> _throwOnRow;
    mutable std::optional<size_t> _notifyOnRow;
    mutable std::function<void()> _notifyModel;
    size_t _shrinkTo{};
    size_t _contentRevision{};
    bool _throwOnRead{};
    bool _throwBadAlloc{};
};
#pragma warning(pop)

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

[[nodiscard]] wil::com_ptr_nothrow<IRawElementProviderFragment> NavigateGridElement(IUnknown& from, NavigateDirection direction)
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    wil::com_ptr_nothrow<IRawElementProviderFragment> next;
    if (SUCCEEDED(from.QueryInterface(IID_PPV_ARGS(fragment.put()))))
        static_cast<void>(fragment->Navigate(direction, next.put()));
    return next;
}

[[nodiscard]] std::wstring ReadElementName(IUnknown& element)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(element.QueryInterface(IID_PPV_ARGS(simple.put())), "a grid element exposes properties");
    return ReadGridCellName(*simple.get());
}

[[nodiscard]] bool ReadElementOffscreen(IRawElementProviderSimple& element)
{
    VARIANT value{};
    VariantInit(&value);
    RequireSucceeded(element.GetPropertyValue(UIA_IsOffscreenPropertyId, &value), "a grid element answers IsOffscreen");
    const bool offscreen = value.vt == VT_BOOL && value.boolVal != VARIANT_FALSE;
    VariantClear(&value);
    return offscreen;
}

[[nodiscard]] wil::com_ptr_nothrow<IGridProvider> AcquireGridPattern(AttachedHostWindow& window, wil::com_ptr_nothrow<IRawElementProviderSimple>& outRoot)
{
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(DxUi::CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "the grid's window answers UI Automation");
    RequireSucceeded(root.query_to(outRoot.put()), "the single-grid root exposes its properties");
    wil::com_ptr_nothrow<IUnknown> unknown;
    RequireSucceeded(outRoot->GetPatternProvider(UIA_GridPatternId, unknown.put()), "the single-grid root answers GridPattern");
    wil::com_ptr_nothrow<IGridProvider> grid;
    RequireSucceeded(unknown.query_to(grid.put()), "GridPattern is an IGridProvider");
    return grid;
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
    // CR-19 of the 10 October review: the materialized row's cells are its children, reachable from the row and from one
    // another as on screen. Its cells were reached by GetItem alone: the row and its cells had no children and no siblings.
    {
        const auto navigate = [](IUnknown& from, NavigateDirection direction)
        {
            wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
            wil::com_ptr_nothrow<IRawElementProviderFragment> next;
            if (SUCCEEDED(from.QueryInterface(IID_PPV_ARGS(fragment.put()))))
                static_cast<void>(fragment->Navigate(direction, next.put()));
            return next;
        };
        const auto name = [](IUnknown& element)
        {
            wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
            RequireSucceeded(element.QueryInterface(IID_PPV_ARGS(simple.put())), "a navigated element exposes properties");
            return ReadGridCellName(*simple.get());
        };
        const auto row = navigate(*offscreenCell.get(), NavigateDirection_Parent);
        Require(row != nullptr, "an offscreen cell's parent is its row");
        const auto firstCell = navigate(*row.get(), NavigateDirection_FirstChild);
        Require(firstCell != nullptr && name(*firstCell.get()) == L"row-1500-name", "the offscreen row's first child is its first cell");
        const auto secondCell = navigate(*firstCell.get(), NavigateDirection_NextSibling);
        Require(secondCell != nullptr && name(*secondCell.get()) == L"row-1500-details", "its cells follow one another");
        Require(navigate(*firstCell.get(), NavigateDirection_PreviousSibling) == nullptr, "the first cell has no previous sibling");
    }
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

void TestGridUiaPatternRetriesSupersededSameAddressCapture()
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
        grid->SetModel(&model);       // Invalidates the capture for this new assignment.
        model.SetContentRevision(2u); // A retry must capture the latest acknowledged model state.
    });
    window.Host().RefreshAccessibilitySnapshot();

    wil::com_ptr_nothrow<IRawElementProviderSimple> firstCell;
    RequireSucceeded(pattern->GetItem(0, 0, firstCell.put()), "the nested model assignment remains queryable");
    Require(ReadGridCellName(*firstCell) == L"revision-2-row-0-name", "a superseded same-address capture retries and exposes the current model state");
}

void TestGridUiaPatternStopsWhenCellReadNotifiesSameModel()
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
    RequireSucceeded(root.query_to(rootSimple.put()), "notified model root exposes Simple");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "notified model exposes GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "notified model exposes IGridProvider");
    model.NotifyModelAfterReading(1500u,
                                  [&model, grid]
    {
        model.SetContentRevision(1u);
        grid->NotifyDataChanged();
    });
    wil::com_ptr_nothrow<IRawElementProviderSimple> nameCell;
    wil::com_ptr_nothrow<IRawElementProviderSimple> detailsCell;
    Require(pattern->GetItem(1500, 0, nameCell.put()) == UIA_E_ELEMENTNOTAVAILABLE && ! nameCell,
            "a same-model callback update abandons the superseded requested row");
    RequireSucceeded(pattern->GetItem(1500, 0, nameCell.put()), "a fresh request materializes the current same-model row");
    RequireSucceeded(pattern->GetItem(1500, 1, detailsCell.put()), "the retried row exposes its second cell");
    Require(ReadGridCellName(*nameCell) == L"revision-1-row-1500-name" && ReadGridCellName(*detailsCell) == L"revision-1-row-1500-details",
            "a same-model callback cannot publish cells from different content revisions");
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

    int publishedRowCount = 0;
    RequireSucceeded(pattern->get_RowCount(&publishedRowCount), "an owner query flushes the dirty point-hit snapshot");
    Require(oldModelCallsAfterReplacement != 0u, "point-hit capture reached the scheduled model replacement callback");
    Require(model.StableIdCalls() == oldModelCallsAfterReplacement, "point-hit capture stops immediately after a model callback replaces the grid model");
    wil::com_ptr_nothrow<IRawElementProviderSimple> replacementCell;
    RequireSucceeded(pattern->GetItem(static_cast<int>(firstVisible.value()), 0, replacementCell.put()),
                     "the nested replacement snapshot remains queryable after point-hit capture aborts");
    Require(ReadGridCellName(*replacementCell) == L"revision-1-row-" + std::to_wstring(firstVisible.value()) + L"-name",
            "point-hit capture does not overwrite the replacement model's nested publication");
}

void TestGridUiaPatternPreservesAssignmentDuringCachedCellCreation()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(root.query_to(pattern.put()), "cached-cell race obtains GridPattern");
    wil::com_ptr_nothrow<IRawElementProviderSimple> cachedCell;
    RequireSucceeded(pattern->GetItem(0, 0, cachedCell.put()), "cache the row before the peer-creation race");
    wil::unique_event entered(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event release(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(entered && release, "create cached-cell race events");
    DebugSetAccessibilityPeerCreationGateForTest(window.Hwnd(), entered.get(), release.get());
    std::atomic<HRESULT> workerResult{E_PENDING};
    wil::com_ptr_nothrow<IRawElementProviderSimple> racedCell;
    std::jthread worker;
    bool gateArmed     = true;
    const auto cleanup = wil::scope_exit([&]() noexcept
    {
        static_cast<void>(SetEvent(release.get()));
        if (worker.joinable())
            worker.join();
        if (gateArmed)
            DebugSetAccessibilityPeerCreationGateForTest(nullptr, nullptr, nullptr);
    });
    worker             = std::jthread([&]() noexcept { workerResult.store(pattern->GetItem(0, 0, racedCell.put()), std::memory_order_release); });
    Require(WaitForSingleObject(entered.get(), 5000u) == WAIT_OBJECT_0, "pause cached GetItem after its row lookup");
    model.SetContentRevision(1u);
    grid->SetModel(&model);
    grid->NotifyDataChanged();
    window.Host().RefreshAccessibilitySnapshot();
    int rowCount = 0;
    RequireSucceeded(pattern->get_RowCount(&rowCount), "publish a new assignment at the same model address before peer creation resumes");
    Require(SetEvent(release.get()) != FALSE, "resume the cached-cell query");
    worker.join();
    DebugSetAccessibilityPeerCreationGateForTest(nullptr, nullptr, nullptr);
    gateArmed = false;
    RequireSucceeded(workerResult.load(std::memory_order_acquire), "the raced query returns its original-assignment peer");
    Require(racedCell != nullptr, "the raced query produced a cell");
    VARIANT name{};
    VariantInit(&name);
    const HRESULT staleResult = racedCell->GetPropertyValue(UIA_NamePropertyId, &name);
    VariantClear(&name);
    Require(staleResult == UIA_E_ELEMENTNOTAVAILABLE, "a cached GetItem cannot adopt a newer model assignment during peer creation");
    wil::com_ptr_nothrow<IRawElementProviderSimple> currentCell;
    RequireSucceeded(pattern->GetItem(0, 0, currentCell.put()), "a fresh query resolves the current assignment");
    Require(ReadGridCellName(*currentCell) == L"revision-1-row-0-name", "the fresh query reads the new assignment's content");
}

void TestGridUiaPatternPreservesAssignmentDuringRowNavigation()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(root.query_to(pattern.put()), "row-navigation race obtains GridPattern");
    wil::com_ptr_nothrow<IRawElementProviderSimple> cachedCell;
    RequireSucceeded(pattern->GetItem(0, 0, cachedCell.put()), "cache the cell before the parent-row race");
    wil::unique_event entered(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event release(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(entered && release, "create row-navigation race events");
    DebugSetAccessibilityPeerCreationGateForTest(window.Hwnd(), entered.get(), release.get());
    std::atomic<HRESULT> workerResult{E_PENDING};
    wil::com_ptr_nothrow<IRawElementProviderFragment> cachedFragment;
    RequireSucceeded(cachedCell.query_to(cachedFragment.put()), "cached cell exposes Fragment");
    wil::com_ptr_nothrow<IRawElementProviderFragment> racedRow;
    std::jthread worker;
    bool gateArmed     = true;
    const auto cleanup = wil::scope_exit([&]() noexcept
    {
        static_cast<void>(SetEvent(release.get()));
        if (worker.joinable())
            worker.join();
        if (gateArmed)
            DebugSetAccessibilityPeerCreationGateForTest(nullptr, nullptr, nullptr);
    });
    worker =
        std::jthread([&]() noexcept { workerResult.store(cachedFragment->Navigate(NavigateDirection_Parent, racedRow.put()), std::memory_order_release); });
    Require(WaitForSingleObject(entered.get(), 5000u) == WAIT_OBJECT_0, "pause parent navigation after its row lookup");
    model.SetContentRevision(1u);
    grid->SetModel(&model);
    grid->NotifyDataChanged();
    window.Host().RefreshAccessibilitySnapshot();
    int rowCount = 0;
    RequireSucceeded(pattern->get_RowCount(&rowCount), "publish a new assignment at the same model address before peer creation resumes");
    Require(SetEvent(release.get()) != FALSE, "resume the row-navigation query");
    worker.join();
    DebugSetAccessibilityPeerCreationGateForTest(nullptr, nullptr, nullptr);
    gateArmed = false;
    RequireSucceeded(workerResult.load(std::memory_order_acquire), "the raced query returns its original-assignment peer");
    Require(racedRow != nullptr, "the raced query produced a row");
    wil::com_ptr_nothrow<IRawElementProviderSimple> racedSimple;
    RequireSucceeded(racedRow.query_to(racedSimple.put()), "raced row exposes Simple");
    VARIANT name{};
    VariantInit(&name);
    const HRESULT staleResult = racedSimple->GetPropertyValue(UIA_NamePropertyId, &name);
    VariantClear(&name);
    Require(staleResult == UIA_E_ELEMENTNOTAVAILABLE, "a parent-row query cannot adopt a newer model assignment during peer creation");
    wil::com_ptr_nothrow<IRawElementProviderSimple> currentCell;
    RequireSucceeded(pattern->GetItem(0, 0, currentCell.put()), "a fresh query resolves the current assignment");
    Require(ReadGridCellName(*currentCell) == L"revision-1-row-0-name", "the fresh query reads the new assignment's content");
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

void TestGridUiaPatternPublishesAfterAnOrdinaryRefreshDeletesRequestedRows()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "deleted-row fixture acquires the window root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "deleted-row root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "deleted-row fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "deleted-row fixture exposes IGridProvider");
    for (int index = 0; index < 16; ++index)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> requested;
        RequireSucceeded(pattern->GetItem(1500 + index, 0, requested.put()), "a client remembers an offscreen row");
    }

    // An ordinary refresh of the same model deletes every remembered row, as when a long list is filtered while a
    // screen reader walks its cells. Each stale request is dropped; none holds back the window's publication.
    model.SetRowCount(100u);
    model.SetContentRevision(1u);
    grid->NotifyDataChanged();
    window.PumpMessages(20u);
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> acquired;
        acquired.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(acquired != nullptr, "every new client obtains the window root after the remembered rows were deleted");
    }
    int rowCount = 0;
    RequireSucceeded(pattern->get_RowCount(&rowCount), "GridPattern answers its row count after the refresh");
    Require(rowCount == 100, "the refresh that deleted the remembered rows is published");
    wil::com_ptr_nothrow<IRawElementProviderSimple> visibleCell;
    RequireSucceeded(pattern->GetItem(0, 0, visibleCell.put()), "GetItem on a visible row succeeds after the refresh");
    Require(ReadGridCellName(*visibleCell) == L"revision-1-row-0-name", "the visible row exposes the refreshed content");
    wil::com_ptr_nothrow<IRawElementProviderSimple> offscreenCell;
    RequireSucceeded(pattern->GetItem(50, 1, offscreenCell.put()), "GetItem on a remaining offscreen row succeeds after the refresh");
    Require(ReadGridCellName(*offscreenCell) == L"revision-1-row-50-details", "the remaining offscreen row exposes the refreshed content");
    IRawElementProviderSimple* deletedCell = nullptr;
    Require(pattern->GetItem(1500, 0, &deletedCell) == E_INVALIDARG && deletedCell == nullptr, "a deleted row is out of range");
}

void TestGridUiaPatternWindowAnswersAfterRequestedRowsAreDeleted()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "deleted-row window fixture acquires the window root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "deleted-row window root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "deleted-row window fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "deleted-row window fixture exposes IGridProvider");
    for (int index = 0; index < 4; ++index)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> requested;
        RequireSucceeded(pattern->GetItem(1500 + index, 0, requested.put()), "a client remembers an offscreen row");
    }
    model.SetRowCount(100u);
    grid->NotifyDataChanged();

    // The window procedure answers WM_GETOBJECT with the root (a null root was dereferenced here before the fix).
    const LRESULT answer = SendMessageW(window.Hwnd(), WM_GETOBJECT, 0, static_cast<LPARAM>(UiaRootObjectId));
    Require(answer != 0, "WM_GETOBJECT returns the window root after the remembered rows were deleted");
}

void TestGridUiaPatternWindowAnswersWhenTheFirstPublicationFails()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);

    // The first client arrives while the model cannot produce its cells, so the first publication fails. The window still
    // answers with its root (a null root was dereferenced here before the fix) instead of failing the client.
    model.SetThrowOnRead(true);
    const LRESULT answer = SendMessageW(window.Hwnd(), WM_GETOBJECT, 0, static_cast<LPARAM>(UiaRootObjectId));
    Require(answer != 0, "WM_GETOBJECT returns the window root even when the first publication fails");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> failingRoot;
    failingRoot.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(failingRoot != nullptr, "a client obtains the window root while publication keeps failing");

    // Once the model recovers, a client acquiring the window gets the grid it shows.
    model.SetThrowOnRead(false);
    grid->NotifyDataChanged();
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "a client obtains the window root after the model recovers");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "the recovered root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "the recovered root advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "the recovered root exposes IGridProvider");
    int rowCount = 0;
    RequireSucceeded(pattern->get_RowCount(&rowCount), "GridPattern answers its row count once the model recovers");
    Require(rowCount == 2000, "the root shows the grid once a publication succeeds");
    wil::com_ptr_nothrow<IRawElementProviderSimple> cell;
    RequireSucceeded(pattern->GetItem(0, 0, cell.put()), "GetItem succeeds once the model recovers");
    Require(ReadGridCellName(*cell) == L"row-0-name", "the recovered root exposes the model content");
}

void TestGridUiaPatternDropsARequestedRowItsModelCannotRead()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(root != nullptr, "unreadable-row fixture acquires the window root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(root.query_to(rootSimple.put()), "unreadable-row root exposes the Simple provider interface");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_GridPatternId, patternUnknown.put()), "unreadable-row fixture advertises GridPattern");
    wil::com_ptr_nothrow<IGridProvider> pattern;
    RequireSucceeded(patternUnknown.query_to(pattern.put()), "unreadable-row fixture exposes IGridProvider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> remembered;
    RequireSucceeded(pattern->GetItem(1500, 0, remembered.put()), "a client remembers an offscreen row");

    // The model now fails for that one remembered row. Only the request is lost: the refresh is still published.
    model.SetThrowOnRow(1500u);
    model.SetContentRevision(1u);
    grid->NotifyDataChanged();
    wil::com_ptr_nothrow<IRawElementProviderSimple> visibleCell;
    RequireSucceeded(pattern->GetItem(0, 0, visibleCell.put()), "GetItem on a visible row succeeds while one remembered row cannot be read");
    Require(ReadGridCellName(*visibleCell) == L"revision-1-row-0-name", "the refresh is published despite the unreadable remembered row");
    grid->NotifyDataChanged();
    wil::com_ptr_nothrow<IRawElementProviderSimple> laterCell;
    RequireSucceeded(pattern->GetItem(1, 1, laterCell.put()), "later publications are not held back by the dropped request");
    Require(ReadGridCellName(*laterCell) == L"revision-1-row-1-details", "a later publication exposes the current content");

    model.SetThrowOnRow(std::nullopt);
    wil::com_ptr_nothrow<IRawElementProviderSimple> rereadCell;
    RequireSucceeded(pattern->GetItem(1500, 0, rereadCell.put()), "the row can be requested again once the model reads it");
    Require(ReadGridCellName(*rereadCell) == L"revision-1-row-1500-name", "the re-requested row exposes the current content");
}

// Q15 of the 10 October review: after a column reorder, UI Automation's column numbers, a row's cells and its name, the
// headers and GetItem follow the display order that the headers, the pixels and Ctrl+C use. They followed the model order,
// so a screen reader named a column other than the one the user saw.
void TestGridUiaPatternFollowsTheColumnDisplayOrder()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(50u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    const GridColumnLayoutEntry layout[]{{.columnId = L"details", .displayIndex = 0u, .widthDip = 180.0f},
                                         {.columnId = L"name", .displayIndex = 1u, .widthDip = 180.0f}};
    grid->ApplyColumnLayout(layout);
    window.Host().RefreshAccessibilitySnapshot();

    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    const auto gridPattern = AcquireGridPattern(window, rootSimple);
    wil::com_ptr_nothrow<IRawElementProviderSimple> first;
    wil::com_ptr_nothrow<IRawElementProviderSimple> second;
    RequireSucceeded(gridPattern->GetItem(0, 0, first.put()), "GetItem answers the first displayed column");
    RequireSucceeded(gridPattern->GetItem(0, 1, second.put()), "GetItem answers the second displayed column");
    Require(ReadGridCellName(*first.get()) == L"row-0-details" && ReadGridCellName(*second.get()) == L"row-0-name",
            "GetItem's column counts displayed columns");
    for (const auto& [cell, expected] : {std::pair{first.get(), 0}, std::pair{second.get(), 1}})
    {
        wil::com_ptr_nothrow<IGridItemProvider> item;
        RequireSucceeded(cell->QueryInterface(IID_PPV_ARGS(item.put())), "a cell is a GridItem");
        int column = -1;
        RequireSucceeded(item->get_Column(&column), "GridItem answers its column");
        Require(column == expected, "GridItem.Column is the displayed column");
    }

    const auto row = NavigateGridElement(*first.get(), NavigateDirection_Parent);
    Require(row != nullptr, "a cell's parent is its row");
    Require(ReadElementName(*row.get()) == L"row-0-details | row-0-name", "a row reads its cells in display order");
    const auto firstChild = NavigateGridElement(*row.get(), NavigateDirection_FirstChild);
    Require(firstChild != nullptr && ReadElementName(*firstChild.get()) == L"row-0-details", "a row's first cell is the first displayed one");
    const auto nextCell = NavigateGridElement(*firstChild.get(), NavigateDirection_NextSibling);
    Require(nextCell != nullptr && ReadElementName(*nextCell.get()) == L"row-0-name", "the next cell is the next displayed one");
    Require(NavigateGridElement(*nextCell.get(), NavigateDirection_NextSibling) == nullptr, "the last displayed cell is the last child");
    const auto lastChild = NavigateGridElement(*row.get(), NavigateDirection_LastChild);
    Require(lastChild != nullptr && ReadElementName(*lastChild.get()) == L"row-0-name", "a row's last cell is the last displayed one");

    const auto firstHeader = NavigateGridElement(*rootSimple.get(), NavigateDirection_FirstChild);
    Require(firstHeader != nullptr && ReadElementName(*firstHeader.get()) == L"Details", "the grid's first child is the first displayed header");
    wil::com_ptr_nothrow<IUnknown> tableUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_TablePatternId, tableUnknown.put()), "the grid answers TablePattern");
    wil::com_ptr_nothrow<ITableProvider> table;
    RequireSucceeded(tableUnknown.query_to(table.put()), "TablePattern is an ITableProvider");
    SAFEARRAY* headers = nullptr;
    RequireSucceeded(table->GetColumnHeaders(&headers), "the table lists its column headers");
    const auto destroy = wil::scope_exit([&]() noexcept { SafeArrayDestroy(headers); });
    LONG lower         = 0;
    LONG upper         = -1;
    RequireSucceeded(SafeArrayGetLBound(headers, 1, &lower), "the header list has a lower bound");
    RequireSucceeded(SafeArrayGetUBound(headers, 1, &upper), "the header list has an upper bound");
    std::vector<std::wstring> names;
    for (LONG index = lower; index <= upper; ++index)
    {
        IUnknown* element = nullptr;
        RequireSucceeded(SafeArrayGetElement(headers, &index, &element), "a header is readable");
        wil::com_ptr_nothrow<IUnknown> held;
        held.attach(element);
        names.push_back(ReadElementName(*held.get()));
    }
    Require(names == std::vector<std::wstring>{L"Details", L"Name"}, "the column headers come in display order");
}

// Q15 and CR-19 of the 10 October review: rows and cells have ScrollItem, so a client scrolls an item it reached through
// GetItem into the grid's viewport, as a screen reader does before reading it in place.
void TestGridUiaPatternScrollsAnOffscreenItemIntoView()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(2000u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    const auto gridPattern = AcquireGridPattern(window, rootSimple);
    wil::com_ptr_nothrow<IRawElementProviderSimple> cell;
    RequireSucceeded(gridPattern->GetItem(1500, 1, cell.put()), "GetItem materializes an offscreen row");
    Require(ReadElementOffscreen(*cell.get()), "the materialized cell is offscreen");
    const auto isShown = [&](size_t rowIndex)
    {
        for (size_t visible = 0u; visible < grid->GetVisibleRowCount(); ++visible)
            if (grid->GetVisibleRowAt(visible) == rowIndex)
                return true;
        return false;
    };
    Require(! isShown(1500u), "the grid shows its first rows");

    wil::com_ptr_nothrow<IUnknown> unknown;
    RequireSucceeded(cell->GetPatternProvider(UIA_ScrollItemPatternId, unknown.put()), "a cell answers ScrollItem");
    wil::com_ptr_nothrow<IScrollItemProvider> scroll;
    RequireSucceeded(unknown.query_to(scroll.put()), "ScrollItem is an IScrollItemProvider");
    RequireSucceeded(scroll->ScrollIntoView(), "ScrollIntoView scrolls the cell in");
    Require(isShown(1500u), "the cell's row is in the grid's viewport");
    window.Host().RefreshAccessibilitySnapshot();
    window.PumpMessages();
    Require(! ReadElementOffscreen(*cell.get()), "the cell is on screen once the grid republishes");

    const auto row = NavigateGridElement(*cell.get(), NavigateDirection_Parent);
    Require(row != nullptr, "the cell's parent is its row");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rowSimple;
    RequireSucceeded(row.query_to(rowSimple.put()), "the row exposes properties");
    wil::com_ptr_nothrow<IUnknown> rowUnknown;
    RequireSucceeded(rowSimple->GetPatternProvider(UIA_ScrollItemPatternId, rowUnknown.put()), "a row answers ScrollItem");
    Require(rowUnknown != nullptr, "a row has ScrollItem");
}

// Q15 and CR-19 of the 10 October review: an action on a cell of a column scrolled out of the viewport scrolls it in
// first. SetFocus failed there, because resolving the cell needs its column in the viewport.
void TestGridUiaPatternFocusesACellOfAScrolledOutColumn()
{
    using namespace DxUi;
    ScopedNonActivatingTestWindows nonActivating;
    AttachedHostWindow window;
    GridUiaPatternModel model(50u);
    Grid* grid = nullptr;
    AttachGrid(window, model, grid);
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 150.0f, 280.0f));
    window.Host().RefreshAccessibilitySnapshot();
    Require(! grid->FindVisibleColumnOrdinal(1u), "the second column is scrolled out of the narrow grid");

    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    const auto gridPattern = AcquireGridPattern(window, rootSimple);
    wil::com_ptr_nothrow<IRawElementProviderSimple> cell;
    RequireSucceeded(gridPattern->GetItem(3, 1, cell.put()), "GetItem answers a cell of the scrolled-out column");
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    RequireSucceeded(cell.query_to(fragment.put()), "the cell is a fragment");
    RequireSucceeded(fragment->SetFocus(), "SetFocus on the cell succeeds");
    Require(grid->FindVisibleColumnOrdinal(1u).has_value(), "the cell's column was scrolled into the viewport first");
    Require(grid->GetFocusedRowId() == std::optional<uint64_t>{4u}, "the cell's row is the focused row");

    wil::com_ptr_nothrow<ITableItemProvider> tableItem;
    RequireSucceeded(cell.query_to(tableItem.put()), "the cell is a TableItem");
    SAFEARRAY* headers = nullptr;
    RequireSucceeded(tableItem->GetColumnHeaderItems(&headers), "the cell names its column header");
    const auto destroy = wil::scope_exit([&]() noexcept { SafeArrayDestroy(headers); });
    LONG index         = 0;
    IUnknown* header   = nullptr;
    RequireSucceeded(SafeArrayGetElement(headers, &index, &header), "the header is readable");
    wil::com_ptr_nothrow<IUnknown> held;
    held.attach(header);
    Require(ReadElementName(*held.get()) == L"Details", "the cell's header is its column's");
}
