#include "DxUi.RetainedCaches.h"

#include "DxUi.Internal.h"
#include "Support/RetainedCaches.Tests.Hooks.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

// The retained-cache registry: see DxUi.RetainedCaches.h. Rows name the member they cover so that
// Tools/tests/Test-RetainedCacheRegistry.ps1 can match every cache-named member against a row or an exemption.
namespace DxUi
{
namespace
{
thread_local bool g_staleLabelLayoutForTest = false;
thread_local bool g_freshPreparationForTest = false;

template <typename Visit> void VisitTree(Control& control, const Visit& visit) noexcept
{
    visit(control);
    const size_t count = control.GetLogicalChildCount();
    for (size_t index = 0u; index < count; ++index)
    {
        if (Control* const child = control.GetLogicalChild(index))
            VisitTree(*child, visit);
    }
}

[[nodiscard]] constexpr size_t Present(bool present) noexcept
{
    return present ? 1u : 0u;
}

// Resident bytes (RetainedCacheCount::residentBytes). The MSVC standard library keeps a string of up to 15 bytes of
// characters inside the object; a longer one owns capacity() + 1 characters on the heap.
template <typename Char> [[nodiscard]] uint64_t StringBytes(const std::basic_string<Char>& text) noexcept
{
    constexpr size_t inlineCapacity = (16u / sizeof(Char)) - 1u;
    return text.capacity() > inlineCapacity ? uint64_t(text.capacity() + 1u) * sizeof(Char) : 0u;
}
template <typename T> [[nodiscard]] uint64_t VectorBytes(const std::vector<T>& values) noexcept
{
    return uint64_t(values.capacity()) * sizeof(T);
}
// An unordered map in the MSVC layout: one list node (two links and the value) per entry, the list's sentinel node and
// two iterators per bucket.
template <typename Map> [[nodiscard]] uint64_t UnorderedMapBytes(const Map& map) noexcept
{
    constexpr uint64_t node = (2u * sizeof(void*)) + sizeof(typename Map::value_type);
    return (uint64_t(map.size()) + 1u) * node + uint64_t(map.bucket_count()) * 2u * sizeof(void*);
}
[[nodiscard]] uint64_t BitmapBytes(ID2D1Bitmap* bitmap) noexcept
{
    if (! bitmap)
        return 0u;
    const D2D1_SIZE_U size = bitmap->GetPixelSize();
    return uint64_t(size.width) * size.height * 4u; // DxUi bitmaps are 32-bit BGRA.
}
} // namespace

// Rows of per-host caches. Each names the member it covers in its comment.
std::span<const RetainedHostCacheRow> RetainedCacheAccess::HostRows() noexcept
{
    {
        static constexpr std::array<RetainedHostCacheRow, 5u> rows{{
            // ControlHost::_brushCache (and _brushFailureLogKeys, the warnings already written for it).
            {"ControlHost._brushCache",
             [](const ControlHost& host) noexcept { return host._brushCache.size(); },
             [](ControlHost& host) noexcept
        {
            host._brushCache.clear();
            host._brushFailureLogKeys.clear();
        },
             [](const ControlHost& host) noexcept { return UnorderedMapBytes(host._brushCache) + VectorBytes(host._brushFailureLogKeys); }},
            // ControlHost::_configuredTextFormats. Controls borrow raw format pointers from it (single-line, menu-bar and
            // tree badge caches); their rows drop those first and every control is prepared again, as TrimCaches does.
            {"ControlHost._configuredTextFormats",
             [](const ControlHost& host) noexcept { return host._configuredTextFormats.size(); },
             [](ControlHost& host) noexcept
        {
            host._configuredTextFormats.clear();
            host._forceAllVisualPreparation = true;
        },
             [](const ControlHost& host) noexcept { return UnorderedMapBytes(host._configuredTextFormats); }},
            // TooltipLayer::_layoutCache of the host's tooltip layer.
            {"TooltipLayer._layoutCache",
             [](const ControlHost& host) noexcept { return Present(host._tooltipLayer._layoutCache.valid || host._tooltipLayer._layoutCache.layout); },
             [](ControlHost& host) noexcept { host._tooltipLayer.InvalidateLayoutCache(); },
             [](const ControlHost&) noexcept { return uint64_t{0u}; }}, // only the layout object, whose storage is DirectWrite's
            // ControlHost::_richTitleHoverCache, the last rich hover title the supplemental tooltip resolved.
            {"ControlHost._richTitleHoverCache",
             [](const ControlHost& host) noexcept { return Present(host._richTitleHoverCache.valid); },
             [](ControlHost& host) noexcept
        {
            host._richTitleHoverCache.valid  = false;
            host._richTitleHoverCache.editor = nullptr;
            host._richTitleHoverCache.lifetime.reset();
            host._richTitleHoverCache.title = std::wstring(0u, L' ');
        },
             [](const ControlHost& host) noexcept { return StringBytes(host._richTitleHoverCache.title); }},
            // The published UI Automation snapshot (WindowHostAccessibilityTarget::snapshot, native or embedded).
            {"Accessibility.snapshot",
             [](const ControlHost& host) noexcept { return CountRetainedAccessibilitySnapshotRecords(host); },
             [](ControlHost& host) noexcept { DropRetainedAccessibilitySnapshot(host); },
             [](const ControlHost& host) noexcept { return RetainedAccessibilitySnapshotBytes(host); }},
        }};
        return rows;
    }
}

// Rows of per-control caches.
std::span<const RetainedControlCacheRow> RetainedCacheAccess::ControlRows() noexcept
{
    {
        static constexpr std::array<RetainedControlCacheRow, 22u> rows{{
            // Label::_typographyLayout, the retained layout of a label with a mnemonic or a typography override.
            {"Label._typographyLayout",
             [](const Control& control) noexcept
        {
            const auto* label = dynamic_cast<const Label*>(&control);
            return label ? Present(label->_typographyLayout != nullptr) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* label = dynamic_cast<Label*>(&control))
                label->Label::CancelVisualContentPreparation();
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // only the layout object
            // Button::_preparedIconTextLayout and its _preparedIconTextFormat.
            {"Button._preparedIconTextLayout",
             [](const Control& control) noexcept
        {
            const auto* button = dynamic_cast<const Button*>(&control);
            return button ? Present(button->_preparedIconTextLayout || button->_preparedIconTextFormat) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* button = dynamic_cast<Button*>(&control))
                button->RetirePreparedIconTextLayout();
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // the layout and format objects only
            // Button::_preparedShortcutWidthDip, the measured shortcut text width.
            {"Button._preparedShortcutWidthDip",
             [](const Control& control) noexcept
        {
            const auto* button = dynamic_cast<const Button*>(&control);
            return button ? Present(button->_preparedShortcutValid) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* button = dynamic_cast<Button*>(&control))
            {
                button->_preparedShortcutValid = false;
                button->_preparedShortcutHost  = nullptr;
            }
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // a width inside the control
            // Button::_preparedHyperlinkWidthDip, the measured hyperlink text width.
            {"Button._preparedHyperlinkWidthDip",
             [](const Control& control) noexcept
        {
            const auto* button = dynamic_cast<const Button*>(&control);
            return button ? Present(button->_preparedHyperlinkValid) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* button = dynamic_cast<Button*>(&control))
            {
                button->_preparedHyperlinkValid = false;
                button->_preparedHyperlinkHost  = nullptr;
            }
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // a width inside the control
            // TextField::_singleLineLayoutCache and _maskedSourceTextElementBoundaries.
            {"TextField._singleLineLayoutCache",
             [](const Control& control) noexcept
        {
            const auto* field = dynamic_cast<const TextField*>(&control);
            return field ? Present(field->_singleLineLayoutCache.layout != nullptr) + Present(! field->_maskedSourceTextElementBoundaries.empty()) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* field = dynamic_cast<TextField*>(&control))
                field->InvalidateSingleLineLayoutCache();
        },
             [](const Control& control) noexcept
        {
            const auto* field = dynamic_cast<const TextField*>(&control);
            return field ? StringBytes(field->_singleLineLayoutCache.text) + VectorBytes(field->_maskedSourceTextElementBoundaries) : 0u;
        }},
            // TextField::_cachedMultilineLayout with _cachedLayoutText, _cachedLayoutSize and _cachedLayoutRole.
            {"TextField._cachedMultilineLayout",
             [](const Control& control) noexcept
        {
            const auto* field = dynamic_cast<const TextField*>(&control);
            return field ? Present(field->_cachedMultilineLayout != nullptr) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* field = dynamic_cast<TextField*>(&control))
                field->InvalidateMultilineLayoutCache();
        },
             [](const Control& control) noexcept
        {
            const auto* field = dynamic_cast<const TextField*>(&control);
            return field ? StringBytes(field->_cachedLayoutText) : 0u;
        }},
            // ComboBox::_singleLineLayoutCache.
            {"ComboBox._singleLineLayoutCache",
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo ? Present(combo->_singleLineLayoutCache.layout != nullptr) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* combo = dynamic_cast<ComboBox*>(&control))
                combo->InvalidateSingleLineLayoutCache();
        },
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo ? StringBytes(combo->_singleLineLayoutCache.text) : 0u;
        }},
            // ComboBox::_popupItemWidthCache: the measured width of every item, the filtered maximum and the no-matches width.
            {"ComboBox._popupItemWidthCache",
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo && combo->_popupItemWidthCache.valid ? combo->_popupItemWidthCache.itemWidthsDip.size() + 1u : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* combo = dynamic_cast<ComboBox*>(&control))
                combo->InvalidatePopupLayoutCaches();
        },
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo ? VectorBytes(combo->_popupItemWidthCache.itemWidthsDip) : 0u;
        }},
            // ComboBox::_popupLayoutCache: the inputs of the last popup placement (_popupBounds, _popupVisibleItemCount).
            {"ComboBox._popupLayoutCache",
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo ? Present(combo->_popupLayoutCache.valid) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* combo = dynamic_cast<ComboBox*>(&control))
                combo->InvalidatePopupLayoutCaches();
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // fixed fields inside the control
            // ComboBox::_popupBackdropBitmap and _popupBackdropDevice: the bitmap made from the captured backdrop. The
            // capture itself (_popupBackdropCapture) is the screen as it was at open and is kept (see the exemptions).
            {"ComboBox._popupBackdropBitmap",
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo ? Present(combo->_popupBackdropBitmap != nullptr) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* combo = dynamic_cast<ComboBox*>(&control))
            {
                combo->_popupBackdropBitmap.reset();
                combo->_popupBackdropDevice.reset();
            }
        },
             [](const Control& control) noexcept
        {
            const auto* combo = dynamic_cast<const ComboBox*>(&control);
            return combo ? BitmapBytes(combo->_popupBackdropBitmap.get()) : 0u;
        }},
            // MenuBar::_menuBarLayoutCache.
            {"MenuBar._menuBarLayoutCache",
             [](const Control& control) noexcept
        {
            const auto* bar = dynamic_cast<const MenuBar*>(&control);
            return bar ? Present(bar->_menuBarLayoutCache.valid) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* bar = dynamic_cast<MenuBar*>(&control))
            {
                bar->InvalidateMenuBarLayoutCache();
                bar->_menuBarLayoutCache.textFormat = nullptr;
            }
        },
             [](const Control& control) noexcept
        {
            const auto* bar = dynamic_cast<const MenuBar*>(&control);
            return bar ? VectorBytes(bar->_menuBarLayoutCache.itemWidthsDip) + VectorBytes(bar->_menuBarLayoutCache.itemRects) : 0u;
        }},
            // TabControl::_tabHeaderLayoutCache.
            {"TabControl._tabHeaderLayoutCache",
             [](const Control& control) noexcept
        {
            const auto* tabs = dynamic_cast<const TabControl*>(&control);
            return tabs ? Present(tabs->_tabHeaderLayoutCache.valid) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* tabs = dynamic_cast<TabControl*>(&control))
                tabs->InvalidateTabHeaderLayoutCache();
        },
             [](const Control& control) noexcept
        {
            const auto* tabs = dynamic_cast<const TabControl*>(&control);
            return tabs ? VectorBytes(tabs->_tabHeaderLayoutCache.tabWidthsDip) + VectorBytes(tabs->_tabHeaderLayoutCache.tabRects) : 0u;
        }},
            // TabControl::TabItem::measuredTitleWidthDip and measuredTitleTextFormat of every tab.
            {"TabControl.TabItem.measuredTitleWidthDip",
             [](const Control& control) noexcept
        {
            const auto* tabs = dynamic_cast<const TabControl*>(&control);
            return tabs ? static_cast<size_t>(std::ranges::count_if(tabs->_tabs,
                                                                    [](const TabControl::TabItem& tab)
            { return tab.measuredTitleWidthDip.has_value() || tab.measuredTitleTextFormat != nullptr; }))
                        : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* tabs = dynamic_cast<TabControl*>(&control))
                tabs->InvalidateTabTitleMeasurements();
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // widths inside the tab records
            // TabControl::_selectedHeaderGeometry and _hoveredHeaderGeometry.
            {"TabControl._selectedHeaderGeometry",
             [](const Control& control) noexcept
        {
            const auto* tabs = dynamic_cast<const TabControl*>(&control);
            return tabs ? Present(tabs->_selectedHeaderGeometry.valid || tabs->_selectedHeaderGeometry.fill) +
                              Present(tabs->_hoveredHeaderGeometry.valid || tabs->_hoveredHeaderGeometry.fill)
                        : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* tabs = dynamic_cast<TabControl*>(&control))
                tabs->TabControl::CancelVisualContentPreparation();
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // geometry objects only
            // Tree::_badgeWidthCache and _tooltipOverflowCache.
            {"Tree._badgeWidthCache",
             [](const Control& control) noexcept
        {
            const auto* tree = dynamic_cast<const Tree*>(&control);
            return tree ? tree->_badgeWidthCache.size() + Present(tree->_tooltipOverflowCache.valid) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* tree = dynamic_cast<Tree*>(&control))
                tree->InvalidateTreeTextMeasurementCaches();
        },
             [](const Control& control) noexcept
        {
            const auto* tree = dynamic_cast<const Tree*>(&control);
            if (! tree)
                return uint64_t{0u};
            uint64_t bytes = VectorBytes(tree->_badgeWidthCache);
            for (const auto& entry : tree->_badgeWidthCache)
                bytes += StringBytes(entry.text);
            const auto& overflow = tree->_tooltipOverflowCache;
            return bytes + StringBytes(overflow.text) + StringBytes(overflow.tooltipText) + StringBytes(overflow.resolvedTooltipText);
        }},
            // Tree::_visibleIdIndexCache, the id index of a model that keeps ITreeModel's default lookup: one entry per
            // distinct visible id. Dropping it makes the next lookup read every row again.
            {"Tree._visibleIdIndexCache",
             [](const Control& control) noexcept
        {
            const auto* tree = dynamic_cast<const Tree*>(&control);
            return tree ? tree->_visibleIdIndexCache.ids.size() : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* tree = dynamic_cast<Tree*>(&control))
                tree->ReleaseVisibleIdIndex();
        },
             [](const Control& control) noexcept
        {
            const auto* tree = dynamic_cast<const Tree*>(&control);
            return tree ? VectorBytes(tree->_visibleIdIndexCache.ids) + VectorBytes(tree->_visibleIdIndexCache.rows) : 0u;
        }},
            // Grid::_cellTextLayouts, _cellDisplayLayouts, _cellEllipsisFormat and _cellEllipsis.
            {"Grid._cellTextLayouts",
             [](const Control& control) noexcept
        {
            const auto* grid = dynamic_cast<const Grid*>(&control);
            if (! grid)
                return size_t{0u};
            const auto live = [](const auto& entry) { return entry.layout != nullptr; };
            return static_cast<size_t>(std::ranges::count_if(grid->_cellTextLayouts, live) + std::ranges::count_if(grid->_cellDisplayLayouts, live)) +
                   Present(grid->_cellEllipsis || grid->_cellEllipsisFormat);
        },
             [](Control& control) noexcept
        {
            if (auto* grid = dynamic_cast<Grid*>(&control))
                grid->ReleaseCellTextResources();
        },
             [](const Control& control) noexcept
        {
            const auto* grid = dynamic_cast<const Grid*>(&control);
            if (! grid)
                return uint64_t{0u};
            uint64_t bytes = VectorBytes(grid->_cellTextLayouts) + VectorBytes(grid->_cellDisplayLayouts);
            for (const auto& entry : grid->_cellTextLayouts)
                bytes += StringBytes(entry.text);
            for (const auto& entry : grid->_cellDisplayLayouts)
                bytes += StringBytes(entry.text);
            return bytes;
        }},
            // Grid::_groupLayoutCache (the model's groups and their collapsed-row prefix) and _columnDescCache (its column
            // descriptions), filled once per data revision; a fresh frame reads the model again.
            {"Grid._groupLayoutCache",
             [](const Control& control) noexcept
        {
            const auto* grid = dynamic_cast<const Grid*>(&control);
            if (! grid)
                return size_t{0u};
            return (grid->_groupLayoutCache.valid ? 1u + grid->_groupLayoutCache.groups.size() : 0u) +
                   (grid->_columnDescCache.valid ? grid->_columnDescCache.columns.size() : 0u);
        },
             [](Control& control) noexcept
        {
            if (auto* grid = dynamic_cast<Grid*>(&control))
                grid->ReleaseDataCaches();
        }},
            // ColorPicker::_saturationBrush, _valueBrush, _hueBrush and their _brushDevice.
            {"ColorPicker._saturationBrush",
             [](const Control& control) noexcept
        {
            const auto* picker = dynamic_cast<const ColorPicker*>(&control);
            return picker ? Present(picker->_saturationBrush != nullptr) + Present(picker->_valueBrush != nullptr) + Present(picker->_hueBrush != nullptr) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* picker = dynamic_cast<ColorPicker*>(&control))
            {
                picker->_saturationBrush.reset();
                picker->_valueBrush.reset();
                picker->_hueBrush.reset();
                picker->_brushDevice.reset();
            }
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // brush objects only
            // FlexPanel::_records (the measured part of each child record) and _lines.
            {"FlexPanel._records",
             [](const Control& control) noexcept
        {
            const auto* flex = dynamic_cast<const FlexPanel*>(&control);
            return flex ? static_cast<size_t>(std::ranges::count_if(flex->_records, [](const auto& record) { return record.measuredValid; })) : 0u;
        },
             [](Control& control) noexcept
        {
            if (auto* flex = dynamic_cast<FlexPanel*>(&control))
            {
                for (auto& record : flex->_records)
                    record.measuredValid = false;
                flex->_lines.clear();
            }
        },
             [](const Control& control) noexcept
        {
            // The child records exist for every child; only the line table is the cache's own storage.
            const auto* flex = dynamic_cast<const FlexPanel*>(&control);
            return flex ? VectorBytes(flex->_lines) : 0u;
        }},
            // FormField::_measureCache and _sharedLabelCache.
            {"FormField._measureCache",
             [](const Control& control) noexcept
        {
            const auto* field = dynamic_cast<const FormField*>(&control);
            if (! field)
                return size_t{0u};
            const auto valid = [](const auto& part) { return part.host != nullptr; };
            return static_cast<size_t>(std::ranges::count_if(field->_measureCache, valid)) + Present(valid(field->_sharedLabelCache));
        },
             [](Control& control) noexcept
        {
            if (auto* field = dynamic_cast<FormField*>(&control))
            {
                field->_measureCache.fill({});
                field->_sharedLabelCache = {};
            }
        },
             [](const Control&) noexcept { return uint64_t{0u}; }}, // fixed records inside the control
            // RichTextEditor::State::layout (prepared document layout and semantic geometry with its hover index), its overlay geometry, inks,
            // brushes and staged parser and builder work: everything RichTextEditor::RetireLayout(false) clears.
            {"RichTextEditor.layout",
             [](const Control& control) noexcept
        {
            const auto* editor = dynamic_cast<const RichTextEditor*>(&control);
            if (! editor)
                return size_t{0u};
            const RichTextEditorDebugState state = editor->DebugGetState();
            return Present(state.layoutReady) + state.retainedGeometryChunks;
        },
             [](Control& control) noexcept
        {
            if (auto* editor = dynamic_cast<RichTextEditor*>(&control))
                editor->RetireLayout(false);
        }}, // No byte count: the prepared document layout keeps no running total of its storage.
        }};
        return rows;
    }
}

void RetainedCacheAccess::Drop(ControlHost& host) noexcept
{
    const Failure::Scope failureScope;
    const auto visitPages = [](Control& control, const auto& visit) noexcept
    {
        if (auto* const page = dynamic_cast<PageHost*>(&control); page && page->_outgoingPage && page->_outgoingPage != page->_currentPage)
            VisitTree(*page->_outgoingPage, visit);
    };
    if (Control* const root = host._root.get())
    {
        const auto dropControl = [&](Control& control) noexcept
        {
            for (const RetainedControlCacheRow& cache : ControlRows())
                cache.drop(control);
        };
        VisitTree(*root,
                  [&](Control& control) noexcept
        {
            dropControl(control);
            visitPages(control, dropControl);
        });
    }
    // Content, visual and intrinsic layout preparation run again for the whole tree (a rich editor's layout, label
    // typography, every measured size), as after a device replacement.
    if (Control* const root = host._root.get())
        ControlModelQueryAccess::RequestIntrinsicLayoutOfTree(*root);
    host.CancelPreparedContent();
    host.CancelPreparedVisualContent();
    for (const RetainedHostCacheRow& cache : HostRows())
        cache.drop(host);
    host.Invalidate();
}

void RetainedCacheAccess::Count(ControlHost& host, std::vector<RetainedCacheCount>& out)
{
    for (const RetainedHostCacheRow& cache : HostRows())
        out.push_back({cache.name, cache.entries(host), cache.residentBytes != nullptr, cache.residentBytes ? cache.residentBytes(host) : 0u});
    for (const RetainedControlCacheRow& cache : ControlRows())
    {
        size_t entries = 0u;
        uint64_t bytes = 0u;
        if (Control* const root = host._root.get())
            VisitTree(*root,
                      [&](Control& control) noexcept
            {
                entries += cache.entries(control);
                if (cache.residentBytes)
                    bytes += cache.residentBytes(control);
            });
        out.push_back({cache.name, entries, cache.residentBytes != nullptr, bytes});
    }
}

void RetainedCacheAccess::AddHostTotals(const ControlHost& host, RetainedCacheTotals& totals) noexcept
{
    for (const RetainedHostCacheRow& cache : HostRows())
    {
        totals.entries += cache.entries(host);
        if (cache.residentBytes)
            totals.bytes += cache.residentBytes(host);
    }
}

void RetainedCacheAccess::AddControlTotals(const Control& control, RetainedCacheTotals& totals) noexcept
{
    for (const RetainedControlCacheRow& cache : ControlRows())
    {
        totals.entries += cache.entries(control);
        if (cache.residentBytes)
            totals.bytes += cache.residentBytes(control);
    }
}

std::span<const std::string_view> RetainedCacheAccess::Names() noexcept
{
    static const auto names = []
    {
        std::vector<std::string_view> all;
        for (const RetainedHostCacheRow& cache : HostRows())
            all.push_back(cache.name);
        for (const RetainedControlCacheRow& cache : ControlRows())
            all.push_back(cache.name);
        all.push_back("EmbeddedHost.State.texture");
        return all;
    }();
    return names;
}

std::span<const RetainedCacheExemption> RetainedCacheAccess::Exemptions() noexcept
{
    static constexpr std::array<RetainedCacheExemption, 14u> exemptions{{
        {"ControlHost._bodyTextFormat",
         "The fourteen role formats (_bodyTextFormat .. _monoTextFormat) are built once per DirectWrite factory from fixed role specs; "
         "no theme, DPI or model input keys them, and an embedded Prepare has no path that would rebuild them."},
        {"ControlHost._fluentIconFontAvailable", "Whether the system has the icon font; not derived from any DxUi input."},
        {"ControlHost._fallbackBrush", "One brush whose color every use sets first; it holds no value a frame could read stale."},
        {"ControlHost._nativeTextInputStateCache",
         "The native text session's copy of the text service state (with _nativeTextInputStateCacheValid); session state, not paint state."},
        {"ControlHost._nativeTextInputCacheText",
         "The document of _nativeTextInputStateCache, shared with the session control (Shared input text); session state, not paint state."},
        {"ControlHost._nativeTextInputCacheSource",
         "A rich editor's document of _nativeTextInputStateCache as its immutable source revision (Rich text-input snapshots); session "
         "state, not paint state."},
        {"ControlHost._dwriteFactory", "The borrowed shared DirectWrite factory; device replacement owns it."},
        {"ComboBox._popupBackdropCapture",
         "The screen pixels captured when the popup opened: an input from outside DxUi that a fresh frame could not rebuild. "
         "Oracle scenes pin OverlayMaterial::Solid, which captures nothing."},
        {"Grid._cellVisibleText", "Scratch string reused within one paint; ReleaseCellTextResources frees it."},
        {"Accessibility.eventBaseline",
         "eventBaseline and diffBaseline decide which UI Automation events a publication raises, not what the snapshot holds; "
         "events are outside the oracle's comparison."},
        {"Accessibility.rootProvider", "The canonical root provider object: its identity must survive for clients that hold it."},
        {"RichTextEditor.State.images",
         "Decoded images come from the application's asynchronous decoder; dropping them shows alternative text until the "
         "application decodes again. Oracle scripts use no images; W2.3 keys image reuse itself."},
        {"MenuPopup.descriptionLayouts",
         "Native menu popups (descriptionLayouts, liveRowLayouts, backdropSnapshot, the popup host) run only in the focus-taking "
         "Menu suites; their resources are reported by DebugGetContextMenuResources."},
        {"MenuFlyoutItem.BitmapIcon.cachedBitmap", "Bitmap made from an application HBITMAP for a native menu popup; see MenuPopup."},
    }};
    return exemptions;
}

bool IsStaleLabelLayoutInjectedForTest() noexcept
{
    return g_staleLabelLayoutForTest;
}

bool IsFreshPreparationForTest() noexcept
{
    return g_freshPreparationForTest;
}

namespace Detail
{
void SetStaleLabelLayoutInjectionForTest(bool enabled) noexcept
{
    g_staleLabelLayoutForTest = enabled;
}
void SetFreshPreparationForTest(bool enabled) noexcept
{
    g_freshPreparationForTest = enabled;
}
} // namespace Detail

#if DXUI_ENABLE_DIAGNOSTICS
void ControlHost::DebugDropRetainedStateForTest() noexcept
{
    RetainedCacheAccess::Drop(*this);
}
#endif
} // namespace DxUi
