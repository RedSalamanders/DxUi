#pragma once

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

// Included by the menu fixture to reuse its owned-window discovery/capture helpers.
// The same probe compiles on the retained baseline: described mode is explicitly
// unsupported there, while plain mode measures identical existing menu behavior.
template <typename Item> bool SetMenuResourceDescription(Item& item, std::wstring_view parent)
{
    if constexpr (requires {
                      item.secondaryText;
                      item.accessibleName;
                  })
    {
        item.secondaryText  = parent;
        item.accessibleName = std::wstring(parent) + L"\\" + item.text;
        return true;
    }
    else
        return false;
}

void RunMenuResourceTests()
{
    using namespace DxUi;
    AttachedHostWindow owner;
    const auto sample = [](const char* mode, int cycle, const char* phase)
    {
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb     = sizeof(memory);
        DWORD handles = 0;
        Require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE &&
                    GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE,
                "menu resource counters are available");
        std::cout << "{\"fixture\":\"dxui-menu-resources-v2\",\"mode\":\"" << mode << "\",\"cycle\":" << cycle << ",\"phase\":\"" << phase
                  << "\",\"privateBytes\":" << memory.PrivateUsage << ",\"workingSetBytes\":" << memory.WorkingSetSize << ",\"handles\":" << handles
                  << ",\"gdi\":" << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) << ",\"user\":" << GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS)
                  << "}\n";
    };
    for (bool described : {false, true})
    {
        const char* mode = described ? "described" : "plain";
        std::vector<MenuFlyoutItem> items;
        for (int index = 0; index < 12; ++index)
        {
            MenuFlyoutItem item{.kind = MenuItemKind::Radio, .text = L"Archives photographiques de la réunion familiale", .commandId = 9000 + index};
            if (described && ! SetMenuResourceDescription(item, std::format(L"D:\\Sauvegardes\\Collection déjà présente\\Génération {}", index)))
                break;
            items.push_back(std::move(item));
        }
        if (items.empty())
        {
            std::cout << "{\"fixture\":\"dxui-menu-resources-v2\",\"mode\":\"described\",\"supported\":false}\n";
            continue;
        }
        ContextMenuSessionCallbacks callbacks{};
        callbacks.maxRootHeightDip = 300.0f;
        // Match physical surfaces: changing widths would confound the retained
        // description cost with larger render targets and capture buffers.
        callbacks.minRootWidthDip = 456.0f;
        for (int cycle = 0; cycle < 96; ++cycle)
        {
            sample(mode, cycle, "before");
            bool closed        = false;
            const POINT anchor = ClientScreenPointForTest(owner.Hwnd(), 20, 20, "resource menu anchor maps to screen");
            Require(
                ContextMenu::ShowAsync(owner.Hwnd(), anchor, items, owner.Host().GetTheme(), [&](std::optional<int>) noexcept { closed = true; }, callbacks),
                "resource menu opens");
            const HWND popup = WaitForOwnedContextMenuPopupWindowByFirstItemText(owner.Hwnd(), items.front().text);
            Require(popup != nullptr, "resource menu is owned by fixture");
            sample(mode, cycle, "rendered");
            WindowHostBitmapCapture capture{};
            Require(DebugCaptureContextMenuPopupBitmap(popup, capture), "resource menu renders a completed owned capture");
            if (cycle == 0)
                std::cout << "{\"fixture\":\"dxui-menu-resources-v2\",\"mode\":\"" << mode << "\",\"widthPx\":" << capture.widthPx
                          << ",\"heightPx\":" << capture.heightPx << ",\"captureBytes\":" << static_cast<uint64_t>(capture.widthPx) * capture.heightPx * 4
                          << "}\n";
            sample(mode, cycle, "open");
            capture = {};
            SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
            owner.PumpMessages();
            Require(closed && WaitForWindowDestroyed(popup), "resource menu closes each cycle");
            sample(mode, cycle, "closed");
        }
    }
}

// Keep the surface constrained even with no descriptions. The first description
// activates the semantic menu tree; subsequent descriptions add two layouts each.
// Rotating/reversing the case order separates that fixed activation cost from
// per-row cost without charging PNG capture/readback to normal menu presentation.
void RunMenuResourceScalingTests()
{
    using namespace DxUi;
    AttachedHostWindow owner;
    struct Variant
    {
        int entries;
        int descriptions;
    };
    constexpr std::array<Variant, 10> variants{{{12, 0}, {12, 1}, {12, 2}, {12, 4}, {12, 8}, {12, 12}, {24, 0}, {24, 24}, {48, 0}, {48, 48}}};
    std::optional<SIZE> surfaceSize;
    const auto sample = [&](const Variant& variant, int cycle, const char* phase)
    {
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb     = sizeof(memory);
        DWORD handles = 0u;
        Require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE &&
                    GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE,
                "menu scaling counters are available");
        std::cout << "{\"fixture\":\"dxui-menu-resource-scaling-v1\",\"entries\":" << variant.entries << ",\"descriptions\":" << variant.descriptions
                  << ",\"cycle\":" << cycle << ",\"phase\":\"" << phase << "\",\"privateBytes\":" << memory.PrivateUsage
                  << ",\"workingSetBytes\":" << memory.WorkingSetSize << ",\"handles\":" << handles
                  << ",\"gdi\":" << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) << ",\"user\":" << GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS)
                  << "}\n";
    };
    std::cout << "{\"fixture\":\"dxui-menu-resource-scaling-v1\",\"menuItemObjectBytes\":" << sizeof(MenuFlyoutItem)
              << ",\"toggleObjectBytes\":" << sizeof(Toggle) << ",\"capture\":false}\n";
    for (int cycle = 0; cycle < 32; ++cycle)
    {
        for (size_t step = 0u; step < variants.size(); ++step)
        {
            const size_t index = (static_cast<size_t>(cycle) + (cycle % 2 == 0 ? step : variants.size() - 1u - step)) % variants.size();
            const auto variant = variants[index];
            std::vector<MenuFlyoutItem> items;
            items.reserve(static_cast<size_t>(variant.entries));
            bool supported = true;
            for (int row = 0; row < variant.entries; ++row)
            {
                MenuFlyoutItem item{.kind = MenuItemKind::Radio, .text = L"Archives photographiques de la réunion familiale", .commandId = 9000 + row};
                if (row < variant.descriptions &&
                    ! SetMenuResourceDescription(item, std::format(L"D:\\Sauvegardes\\Collection déjà présente\\Génération {}", row)))
                {
                    supported = false;
                    break;
                }
                items.push_back(std::move(item));
            }
            if (! supported)
            {
                if (cycle == 0)
                    std::cout << "{\"fixture\":\"dxui-menu-resource-scaling-v1\",\"entries\":" << variant.entries
                              << ",\"descriptions\":" << variant.descriptions << ",\"supported\":false}\n";
                continue;
            }
            ContextMenuSessionCallbacks callbacks{};
            callbacks.maxRootHeightDip = 300.0f;
            callbacks.minRootWidthDip  = 456.0f;
            bool closed                = false;
            const POINT anchor         = ClientScreenPointForTest(owner.Hwnd(), 20, 20, "scaling menu anchor maps to screen");
            sample(variant, cycle, "before");
            Require(
                ContextMenu::ShowAsync(owner.Hwnd(), anchor, items, owner.Host().GetTheme(), [&](std::optional<int>) noexcept { closed = true; }, callbacks),
                "scaling menu opens");
            const HWND popup = WaitForOwnedContextMenuPopupWindowByFirstItemText(owner.Hwnd(), items.front().text);
            Require(popup != nullptr, "scaling menu belongs to fixture");
            Require(RedrawWindow(popup, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW) != FALSE, "scaling menu completes its ordinary paint");
            RECT rect{};
            Require(GetWindowRect(popup, &rect) != FALSE, "scaling menu exposes its window extent");
            const SIZE size{rect.right - rect.left, rect.bottom - rect.top};
            if (! surfaceSize)
                surfaceSize = size;
            Require(size.cx == surfaceSize->cx && size.cy == surfaceSize->cy, "scaling cases retain the same window extent");
            if (cycle == 0)
                std::cout << "{\"fixture\":\"dxui-menu-resource-scaling-v1\",\"entries\":" << variant.entries << ",\"descriptions\":" << variant.descriptions
                          << ",\"widthPx\":" << size.cx << ",\"heightPx\":" << size.cy << ",\"dpi\":" << GetDpiForWindow(popup) << "}\n";
            sample(variant, cycle, "rendered");
            SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
            owner.PumpMessages();
            Require(closed && WaitForWindowDestroyed(popup), "scaling menu closes each cycle");
            sample(variant, cycle, "closed");
        }
    }
}
