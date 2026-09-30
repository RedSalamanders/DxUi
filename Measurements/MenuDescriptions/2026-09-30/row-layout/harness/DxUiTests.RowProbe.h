#pragma once

// SCRATCH evaluation harness for "one formatted layout per described row". Never committed.
// Included at the end of DxUiTests.Menu.cpp, so it sees that file's anonymous-namespace helpers.
//   --suite=MenuRowProbe        capture matrix: PNG + manifest (geometry, hashes) into DXUI_ROW_PROBE_DIR
//   --suite=MenuRowLiveBytes    live heap bytes of an open menu for repeated and distinct captions
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
[[nodiscard]] std::optional<std::wstring> RowProbeEnv(const wchar_t* name)
{
    wchar_t buffer[1024]{};
    const DWORD n = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    if (n == 0 || n >= std::size(buffer))
        return std::nullopt;
    return std::wstring(buffer, n);
}

struct RowProbeTheme
{
    std::string slug;
    DxUi::ThemePalette palette;
};

[[nodiscard]] std::vector<RowProbeTheme> BuildRowProbeThemes()
{
    using namespace DxUi;
    std::vector<RowProbeTheme> themes;
    const char* slugs[] = {"light", "dark", "rainbow-light", "rainbow-dark", "high-contrast"};
    for (int i = 0; i < 5; ++i)
    {
        const bool dark       = i == 1 || i == 3 || i == 4;
        auto palette          = MakeDefaultThemePalette(dark);
        palette.reducedMotion = true;
        if (i == 2 || i == 3)
            palette.rainbowMode = true;
        if (i == 4)
        {
            ThemeColors colors{};
            colors.backgroundArgb          = 0xFF000000;
            colors.textArgb                = 0xFFFFFFFF;
            colors.selectionBackgroundArgb = 0xFF003B80;
            colors.selectionTextArgb       = 0xFFFFFFFF;
            colors.accentArgb              = 0xFFFFFF00;
            colors.darkMode                = TRUE;
            colors.darkBase                = TRUE;
            colors.highContrast            = TRUE;
            palette                        = MakeThemePalette(colors);
            palette.reducedMotion          = true;
        }
        themes.push_back({slugs[i], palette});
    }
    return themes;
}

struct RowProbeFixture
{
    std::string name;
    std::wstring firstText; // displayed text of item 0 (popup discovery)
    std::vector<DxUi::MenuFlyoutItem> items;
    float maxRootHeightDip = 0.0f;
    float minRootWidthDip  = 0.0f;
};

[[nodiscard]] std::vector<RowProbeFixture> BuildRowProbeFixtures()
{
    using namespace DxUi;
    std::vector<RowProbeFixture> f;
    const std::wstring leaf = L"Sélection définitive pour impression et archivage — photographies familiales";
    f.push_back({"gallery",
                 leaf,
                 {{.kind          = MenuItemKind::Radio,
                   .text          = leaf,
                   .checked       = true,
                   .commandId     = 801,
                   .secondaryText = L"D:\\Sauvegardes\\Archives photographiques personnelles\\Exposition annuelle de la médiathèque"},
                  {.kind          = MenuItemKind::Radio,
                   .text          = leaf,
                   .commandId     = 802,
                   .secondaryText = L"D:\\Sauvegardes\\Archives photographiques personnelles\\Collection permanente du musée"}}});

    // Distinct captions, scrolled viewport (scaling-suite geometry).
    {
        RowProbeFixture fx{"distinct12", L"Archives photographiques 0 de la réunion familiale", {}, 300.0f, 456.0f};
        for (int i = 0; i < 12; ++i)
            fx.items.push_back({.kind          = MenuItemKind::Radio,
                                .text          = std::format(L"Archives photographiques {} de la réunion familiale", i),
                                .commandId     = 9000 + i,
                                .secondaryText = std::format(L"D:\\Sauvegardes\\Collection déjà présente\\Génération {}", i)});
        f.push_back(std::move(fx));
    }
    // Repeated captions, scrolled viewport.
    {
        RowProbeFixture fx{"repeated12", L"Archives photographiques de la réunion familiale", {}, 300.0f, 456.0f};
        for (int i = 0; i < 12; ++i)
            fx.items.push_back({.kind          = MenuItemKind::Radio,
                                .text          = L"Archives photographiques de la réunion familiale",
                                .commandId     = 9000 + i,
                                .secondaryText = std::format(L"D:\\Sauvegardes\\Collection déjà présente\\Génération {}", i)});
        f.push_back(std::move(fx));
    }
    // Mixed kinds: icons, accelerators, submenu, info, disabled, separators, headers, plain rows, mnemonics.
    {
        RowProbeFixture fx{"mixed", L"Copier vers Documents", {}, 0.0f, 0.0f};
        fx.items.push_back({.text = L"Copier vers &Documents", .acceleratorText = L"Ctrl+D", .iconGlyph = L"\xE8C8", .commandId = 1, .secondaryText = L"C:\\Utilisateurs\\Éric\\Documents"});
        fx.items.push_back({.kind = MenuItemKind::Toggle, .text = L"Afficher les fichiers cachés", .checked = true, .commandId = 2, .secondaryText = L"Inclut les éléments dont l’attribut caché est défini."});
        fx.items.push_back({.kind = MenuItemKind::Separator});
        fx.items.push_back({.kind = MenuItemKind::Header, .text = L"Destinations récentes"});
        fx.items.push_back({.kind = MenuItemKind::Radio, .text = L"Téléchargements", .checked = true, .commandId = 3, .secondaryText = L"C:\\Utilisateurs\\Éric\\Téléchargements"});
        fx.items.push_back({.text = L"Bureau", .enabled = false, .commandId = 4, .secondaryText = L"Cette destination n’est plus accessible."});
        fx.items.push_back({.text = L"Plain row without description", .commandId = 5});
        fx.items.push_back({.kind = MenuItemKind::Info, .text = L"Espace utilisé :", .acceleratorText = L"561 Go", .secondaryText = L"Sur tous les volumes de cette bibliothèque"});
        fx.items.push_back({.text = L"Ouvrir avec",
                            .acceleratorText = L"Ctrl+Entrée",
                            .iconGlyph = L"\xE8A7",
                            .commandId = 6,
                            .children = {{.text = L"Éditeur de texte", .commandId = 61, .secondaryText = L"C:\\Programmes\\Éditeur\\editeur.exe"}},
                            .secondaryText = L"Choisit l’application pour ce type de fichier."});
        fx.items.push_back({.text = L"Disabled described plain", .iconGlyph = L"\xE711", .enabled = false, .commandId = 7, .secondaryText = L"Short."});
        f.push_back(std::move(fx));
    }
    f.push_back({"short",
                 L"Documents",
                 {{.text = L"Documents", .commandId = 11, .secondaryText = L"C:\\Users\\Eric\\Documents"},
                  {.text = L"Downloads", .commandId = 12, .secondaryText = L"C:\\Users\\Eric\\Downloads"},
                  {.text = L"Pictures", .commandId = 13, .secondaryText = L"C:\\Users\\Eric\\Pictures"}}});
    f.push_back({"scripts",
                 L"Photos familiales",
                 {{.text = L"Photos familiales", .commandId = 21, .secondaryText = L"Archive 🎉 été 👨‍👩‍👧 et 📷"},
                  {.text = L"שלום עולם!", .commandId = 22, .secondaryText = L"D:\\Backup\\תמונות"},
                  {.text = L"مرحبا بالعالم (2026)", .commandId = 23, .secondaryText = L"المسار: الصور\\العائلة."},
                  {.text = L"abc שלום 123 def", .commandId = 24, .secondaryText = L"123 שלום abc"},
                  {.text = L"写真のアーカイブ 한국어", .commandId = 25, .secondaryText = L"D:\\バックアップ\\写真 中文"},
                  {.text = L"สวัสดีครับ ภาษาไทย", .commandId = 26, .secondaryText = L"สวัสดี ประเทศไทย"},
                  {.text = L"e\u0301\u0302\u0303\u0304\u0305 stack", .commandId = 27, .secondaryText = L"a\u0300\u0301\u0302\u0303\u0304\u0305\u0306\u0307\u0308 tall"}}});
    f.push_back({"edges",
                 L"First row",
                 {{.text = L"First row", .commandId = 31, .secondaryText = L"Line one\nLine two\r\nLine three"},
                  {.text = L"Tab in description", .commandId = 32, .secondaryText = L"C:\\a\tb\tc\td\te"},
                  {.text = L"Trailing newline", .commandId = 33, .secondaryText = L"Line\n"},
                  {.text = L"Leading newline", .commandId = 34, .secondaryText = L"\nLine"},
                  {.text = L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", .commandId = 35, .secondaryText = L"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"},
                  {.text = L"Line one\nline two of the label", .commandId = 36, .secondaryText = L"Description"},
                  {.text = L"", .commandId = 37, .secondaryText = L"Empty label, description only"},
                  {.text = L"Ends with ampersand &&", .commandId = 38, .secondaryText = L"&& is literal in a description"}}});
    return f;
}

[[nodiscard]] uint64_t HashRowProbeCapture(const DxUi::WindowHostBitmapCapture& capture)
{
    uint64_t hash = 1469598103934665603ull;
    for (const uint8_t byte : capture.bgraPixels)
    {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash ^ (static_cast<uint64_t>(capture.widthPx) << 32) ^ static_cast<uint64_t>(capture.heightPx);
}

void RunRowProbeScenario(AttachedHostWindow& owner,
                         const RowProbeTheme& theme,
                         const RowProbeFixture& fx,
                         const std::string& stateName,
                         UINT dpi,
                         const std::filesystem::path& outDir,
                         std::ofstream& manifest)
{
    using namespace DxUi;
    ContextMenuSessionCallbacks callbacks{};
    callbacks.maxRootHeightDip = fx.maxRootHeightDip;
    callbacks.minRootWidthDip  = fx.minRootWidthDip;
    bool closed                = false;
    const POINT anchor         = ClientScreenPointForTest(owner.Hwnd(), 20, 20, "row probe anchor maps to screen");
    Require(ContextMenu::ShowAsync(owner.Hwnd(), anchor, fx.items, theme.palette, [&](std::optional<int>) noexcept { closed = true; }, callbacks), "row probe menu opens");
    const HWND popup = WaitForOwnedContextMenuPopupWindowByFirstItemText(owner.Hwnd(), fx.firstText);
    Require(popup != nullptr, "row probe popup appears");
    ContextMenuPopupDebugState state{};
    Require(DebugGetContextMenuPopupState(popup, state), "row probe popup state");
    if (dpi != 96u)
    {
        RECT suggested = state.windowRectPx;
        SendMessageW(popup, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
        Require(DebugGetContextMenuPopupState(popup, state) && state.dpi == dpi, "row probe DPI change applies");
    }
    // Settle first, then pin the state with delivered messages only: nothing pumps between the state and the capture, so a
    // pointer move the system synthesizes for the physical cursor cannot change it.
    RedrawWindow(popup, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    owner.PumpMessages();
    if (stateName != "hover")
        static_cast<void>(SendSettledClientMouseMoveForMenuSuite(popup, 2, 2));
    if (stateName == "kbd")
    {
        // The first key can race popup activation: resend it until the popup has a keyboard target.
        for (int attempt = 0; attempt < 50 && ! (DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex.has_value()); ++attempt)
        {
            SendMessageW(popup, WM_KEYDOWN, VK_DOWN, 0);
            if (! (DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex.has_value()))
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        Require(state.keyboardIndex.has_value(), "row probe keyboard selects a row");
        const size_t first = *state.keyboardIndex;
        SendMessageW(popup, WM_KEYDOWN, VK_DOWN, 0);
        Require(DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex.has_value() && *state.keyboardIndex != first, "row probe keyboard advances one row");
    }
    else if (stateName == "end")
    {
        for (int attempt = 0; attempt < 50 && ! (DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex.has_value()); ++attempt)
        {
            SendMessageW(popup, WM_KEYDOWN, VK_END, 0);
            if (! (DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex.has_value()))
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        Require(state.keyboardIndex.has_value(), "row probe End selects the last row");
    }
    else if (stateName == "wheel")
    {
        const WPARAM wheelTowardUser = MAKEWPARAM(0, static_cast<WORD>(0x10000 - WHEEL_DELTA));
        const LPARAM wheelPoint      = MAKELPARAM((state.surfaceRectPx.left + state.surfaceRectPx.right) / 2, (state.surfaceRectPx.top + state.surfaceRectPx.bottom) / 2);
        for (int notch = 0; notch < 2; ++notch)
            SendMessageW(popup, WM_MOUSEWHEEL, wheelTowardUser, wheelPoint);
    }
    else if (stateName == "hover")
    {
        // Hover the second navigable row that is at least partly visible.
        size_t found = 0;
        for (size_t index = 0; index < fx.items.size(); ++index)
        {
            D2D1_RECT_F rect{};
            if (! DebugGetContextMenuPopupItemRect(popup, index, rect))
                continue;
            ContextMenuPopupDebugState s{};
            if (! DebugGetContextMenuPopupState(popup, s) || s.itemKinds[index] == MenuItemKind::Separator || s.itemKinds[index] == MenuItemKind::Header ||
                s.itemKinds[index] == MenuItemKind::Info || ! s.itemEnabled[index])
                continue;
            if (rect.top < s.viewportRectDip.top || rect.bottom > s.viewportRectDip.bottom)
                continue;
            if (++found == 2)
            {
                const LONG x = static_cast<LONG>(DipToPixelForPopup((rect.left + rect.right) * 0.5f, s.dpi));
                const LONG y = static_cast<LONG>(DipToPixelForPopup((rect.top + rect.bottom) * 0.5f, s.dpi));
                for (int attempt = 0; attempt < 50; ++attempt)
                {
                    Require(SendSettledClientMouseMoveForMenuSuite(popup, x, y), "row probe hovers a row");
                    if (DebugGetContextMenuPopupState(popup, s) && s.hoveredIndex == std::optional<size_t>{index})
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                Require(s.hoveredIndex == std::optional<size_t>{index}, "row probe hover takes effect");
                break;
            }
        }
    }
    WindowHostBitmapCapture capture{};
    Require(DebugCaptureContextMenuPopupBitmap(popup, capture), "row probe captures the popup");
    Require(DebugGetContextMenuPopupState(popup, state), "row probe final popup state");
    const std::string name = std::format("{}_{}_{}_{}", theme.slug, fx.name, stateName, dpi);
    Require(SaveWindowHostBitmapCaptureAsPngForTest(outDir / (name + ".png"), capture), "row probe writes its PNG");
    manifest << name << '\t' << capture.widthPx << 'x' << capture.heightPx << '\t' << std::hex << HashRowProbeCapture(capture) << std::dec << "\tdpi=" << state.dpi
             << "\thover=" << (state.hoveredIndex ? static_cast<long long>(*state.hoveredIndex) : -1) << "\tkbd=" << (state.keyboardIndex ? static_cast<long long>(*state.keyboardIndex) : -1)
             << "\tscroll=" << std::format("{:.4f}", state.scrollOffsetDip) << "\tcontent=" << std::format("{:.4f}", state.contentHeightDip)
             << "\tscrollbar=" << state.hasScrollbar;
    for (size_t index = 0; index < fx.items.size(); ++index)
    {
        ContextMenuPopupItemLayoutDebugState layout{};
        if (DebugGetContextMenuPopupItemLayout(popup, index, layout))
            manifest << std::format("\t[{} item={:.4f}..{:.4f} text={:.4f}..{:.4f} x={:.4f}..{:.4f} sec={:.4f}..{:.4f} lines={}/{} w={:.4f}/{:.4f}]",
                                    index,
                                    layout.itemRectDip.top,
                                    layout.itemRectDip.bottom,
                                    layout.textRectDip.top,
                                    layout.textRectDip.bottom,
                                    layout.textRectDip.left,
                                    layout.textRectDip.right,
                                    layout.secondaryTextRectDip.top,
                                    layout.secondaryTextRectDip.bottom,
                                    layout.primaryLineCount,
                                    layout.secondaryLineCount,
                                    layout.primaryLayoutWidthDip,
                                    layout.secondaryLayoutWidthDip);
    }
    manifest << '\n';
    manifest.flush();
    SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
    owner.PumpMessages();
    Require(closed && WaitForWindowDestroyed(popup, std::chrono::milliseconds(2000)), "row probe menu closes");
}
} // namespace

void RunMenuRowProbeTests()
{
    const auto dirEnv = RowProbeEnv(L"DXUI_ROW_PROBE_DIR");
    const std::filesystem::path outDir = dirEnv ? std::filesystem::path(*dirEnv) : std::filesystem::path(".build/row-probe");
    std::filesystem::create_directories(outDir);
    std::ofstream manifest(outDir / "manifest.tsv", std::ios::binary | std::ios::trunc);
    Require(static_cast<bool>(manifest), "row probe manifest opens");
    const auto themes   = BuildRowProbeThemes();
    const auto fixtures = BuildRowProbeFixtures();
    size_t count        = 0;
    for (const auto& theme : themes)
    {
        AttachedHostWindow owner;
        ShowWindow(owner.Hwnd(), SW_SHOWNOACTIVATE);
        for (const auto& fx : fixtures)
        {
            for (const UINT dpi : {96u, 120u, 144u, 192u})
            {
                std::vector<std::string> states = {"rest"};
                if (dpi == 96u || dpi == 144u)
                    states = {"rest", "kbd", "hover", "end", "wheel"};
                for (const auto& stateName : states)
                {
                    NoteDxUiTestProgress();
                    RunRowProbeScenario(owner, theme, fx, stateName, dpi, outDir, manifest);
                    ++count;
                }
            }
        }
    }
    std::cout << "row probe: " << count << " captures written to " << outDir.string() << '\n';
}

// ---------------------------------------------------------------------------------------------------------------------
// Live bytes of an open menu for repeated and distinct captions (same geometry as the scaling suite: 300 DIP, 456 wide).
// ---------------------------------------------------------------------------------------------------------------------
void RunMenuRowLiveBytesTests()
{
    using namespace DxUi;
    AttachedHostWindow owner;
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    Require(GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitorInfo) != FALSE, "live-bytes fixture resolves its fixed monitor");
    Require(SetWindowPos(owner.Hwnd(), nullptr, monitorInfo.rcWork.left + 64, monitorInfo.rcWork.top + 64, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "live-bytes fixture positions its owner");
    ShowWindow(owner.Hwnd(), SW_SHOWNOACTIVATE);
    owner.PumpMessages();
    struct Variant
    {
        size_t rows;
        bool distinct;
        bool described;
    };
    constexpr std::array<Variant, 7> variants{{{12, false, false}, {12, false, true}, {12, true, true}, {24, false, true}, {24, true, true}, {48, false, true}, {48, true, true}}};
    const auto cyclesEnv = RowProbeEnv(L"DXUI_ROW_LIVE_CYCLES");
    const int cycles     = cyclesEnv ? std::stoi(*cyclesEnv) : 24;
    const auto sample = [&](const Variant& v, int cycle, const char* phase)
    {
        NoteDxUiTestProgress();
        const auto resources = DebugGetContextMenuResources();
        std::cout << "{\"fixture\":\"dxui-menu-row-live-bytes-v1\",\"rows\":" << v.rows << ",\"captions\":\"" << (v.distinct ? "distinct" : "repeated") << "\",\"described\":"
                  << (v.described ? 1 : 0) << ",\"cycle\":" << cycle << ",\"phase\":\"" << phase << "\",\"liveBytes\":" << MeasureLiveHeapBytesForMenuSuite()
                  << ",\"popups\":" << resources.popups << ",\"rowLayouts\":" << resources.rowLayouts << ",\"accessibilityRecords\":" << resources.accessibilityRecords << "}\n";
    };
    for (int cycle = 0; cycle < cycles; ++cycle)
    {
        for (size_t step = 0; step < variants.size(); ++step)
        {
            const size_t index = (static_cast<size_t>(cycle) + (cycle % 2 == 0 ? step : variants.size() - 1u - step)) % variants.size();
            const Variant v    = variants[index];
            std::vector<MenuFlyoutItem> items;
            for (size_t row = 0; row < v.rows; ++row)
            {
                MenuFlyoutItem item{.kind = MenuItemKind::Radio,
                                    .text = v.distinct ? std::format(L"Archives photographiques {} de la réunion familiale", row) : std::wstring(L"Archives photographiques de la réunion familiale"),
                                    .commandId = static_cast<int>(9000 + row)};
                if (v.described)
                    item.secondaryText = std::format(L"D:\\Sauvegardes\\Collection déjà présente\\Génération {}", row);
                items.push_back(std::move(item));
            }
            ContextMenuSessionCallbacks callbacks{};
            callbacks.maxRootHeightDip = 300.0f;
            callbacks.minRootWidthDip  = 456.0f;
            bool closed                = false;
            const POINT anchor         = ClientScreenPointForTest(owner.Hwnd(), 20, 20, "live-bytes anchor maps to screen");
            sample(v, cycle, "before");
            Require(ContextMenu::ShowAsync(owner.Hwnd(), anchor, items, owner.Host().GetTheme(), [&](std::optional<int>) noexcept { closed = true; }, callbacks),
                    "live-bytes menu opens");
            const HWND popup = WaitForOwnedContextMenuPopupWindowByFirstItemText(owner.Hwnd(), items.front().text);
            Require(popup != nullptr, "live-bytes menu belongs to the fixture");
            Require(RedrawWindow(popup, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW) != FALSE, "live-bytes menu completes its paint");
            sample(v, cycle, "rendered");
            SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
            owner.PumpMessages();
            Require(closed && WaitForWindowDestroyed(popup), "live-bytes menu closes");
            sample(v, cycle, "closed");
        }
    }
}
