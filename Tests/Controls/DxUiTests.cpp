#include "../../src/Controls/TextClipboard.h"
#include "../../src/Support/AnimationDispatcher.h"
#include "DxUiTestHelpers.h"

#include "../Support/ForegroundThief.h"
#include "../Support/PerformanceCapture.h"
#include <optional>
#include <string>
#include <string_view>

void RunGridTests();
void RunThemeTests();
void RunControlTests();
void RunComboBoxTests();
void RunWindowHostTests();
void RunTreeTests();
void RunTextFieldTests();
void RunNativeTextInputTests();
void RunMultilineTextTests();
void RunReadOnlyTests();
void RunTooltipTests();
void RunRenderingTests();
void RunAnimationTests();
void RunAccessibilityTests();
void RunMenuTests();
void RunMenuResourceTests();
void RunMenuResourceScalingTests();
void RunMenuTextLayoutResourceTests();
void RunMenuExitLifetimeTests();
void RunNewControlTests();
void RunEditorControlTests();
void RunGalleryGenerator(const std::filesystem::path& outputPath);
void RunGalleryGeneratorPerTheme(const std::filesystem::path& outputDirectory);
void RunButtonContrastAuditGenerator(const std::filesystem::path& outputPath);

int wmain(int argc, wchar_t** argv)
{
    class TestClipboard final : public DxUi::TextClipboard
    {
        std::optional<std::wstring> _text;

    public:
        HRESULT Read(HWND, std::wstring& text) noexcept override
        {
            text.clear();
            if (! _text)
                return S_FALSE;
            try
            {
                text = *_text;
                return S_OK;
            }
            catch (const std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }
        }
        HRESULT Write(HWND, std::wstring_view text) noexcept override
        {
            try
            {
                _text = std::wstring(text);
                return S_OK;
            }
            catch (const std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }
        }
    } clipboard;
    DxUi::testTextClipboard     = &clipboard;
    const auto restoreClipboard = wil::scope_exit([]() noexcept { DxUi::testTextClipboard = nullptr; });

    std::optional<std::wstring> suiteFilter;
    std::vector<std::string> testNames;
    std::optional<std::pair<DWORD, DWORD>> foregroundThiefDelayMs;
    std::optional<std::filesystem::path> perfJsonlPath;
    std::optional<std::filesystem::path> galleryOutputPath;
    std::optional<std::filesystem::path> galleryOutputDirectory;
    std::optional<std::filesystem::path> buttonAuditOutputPath;
    bool writeBaselines  = false;
    bool blockActivation = false;
    for (int argIndex = 1; argIndex < argc; ++argIndex)
    {
        const std::wstring_view arg                         = argv[argIndex] ? std::wstring_view(argv[argIndex]) : std::wstring_view{};
        constexpr std::wstring_view kSuitePrefix            = L"--suite=";
        constexpr std::wstring_view kTestPrefix             = L"--test=";
        constexpr std::wstring_view kForegroundThiefFlag    = L"--foreground-thief";
        constexpr std::wstring_view kForegroundThiefPrefix  = L"--foreground-thief=";
        constexpr std::wstring_view kPerfJsonlPrefix        = L"--perf-jsonl=";
        constexpr std::wstring_view kGalleryPrefix          = L"--gallery-output=";
        constexpr std::wstring_view kGalleryDirectoryPrefix = L"--gallery-output-directory=";
        constexpr std::wstring_view kButtonAuditPrefix      = L"--button-audit-output=";
        if (arg.rfind(kSuitePrefix, 0) == 0)
        {
            if (arg.size() == kSuitePrefix.size())
            {
                std::wcerr << L"Missing suite name for --suite.\n";
                return 2;
            }
            suiteFilter = std::wstring(arg.substr(kSuitePrefix.size()));
            continue;
        }
        if (arg.rfind(kTestPrefix, 0) == 0)
        {
            // A comma-separated list of test function names, each a C++ identifier. Repeating the option adds to the list.
            const auto isIdentifierChar = [](wchar_t ch) noexcept
            { return ch == L'_' || (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z'); };
            std::wstring_view list = arg.substr(kTestPrefix.size());
            for (bool more = true; more;)
            {
                const size_t comma           = list.find(L',');
                const std::wstring_view name = list.substr(0, comma);
                if (name.empty() || ! std::all_of(name.begin(), name.end(), isIdentifierChar))
                {
                    std::wcerr << L"Expected --test=<Name>[,<Name>...] with test function names.\n";
                    return 2;
                }
                std::string narrowName;
                for (const wchar_t ch : name)
                    narrowName.push_back(static_cast<char>(ch));
                testNames.push_back(std::move(narrowName));
                more = comma != std::wstring_view::npos;
                if (more)
                    list.remove_prefix(comma + 1u);
            }
            continue;
        }
        if (arg == kForegroundThiefFlag || arg.rfind(kForegroundThiefPrefix, 0) == 0)
        {
            // The delay range in milliseconds between a window of this process becoming the foreground window and the thief
            // taking the foreground; the bare flag reproduces the 30-95 ms of the desktop application that did so.
            foregroundThiefDelayMs = std::pair<DWORD, DWORD>{30u, 95u};
            if (arg != kForegroundThiefFlag)
            {
                const auto parseMilliseconds = [](std::wstring_view digits) -> std::optional<DWORD>
                {
                    if (digits.empty() || digits.size() > 5u ||
                        ! std::all_of(digits.begin(), digits.end(), [](wchar_t ch) noexcept { return ch >= L'0' && ch <= L'9'; }))
                        return std::nullopt;
                    DWORD value = 0u;
                    for (const wchar_t ch : digits)
                        value = value * 10u + static_cast<DWORD>(ch - L'0');
                    return value;
                };
                const std::wstring_view range    = arg.substr(kForegroundThiefPrefix.size());
                const size_t comma               = range.find(L',');
                const std::optional<DWORD> first = parseMilliseconds(range.substr(0, comma));
                const std::optional<DWORD> last  = comma == std::wstring_view::npos ? std::nullopt : parseMilliseconds(range.substr(comma + 1u));
                if (! first || ! last || first.value() > last.value())
                {
                    std::wcerr << L"Expected --foreground-thief[=<minMs>,<maxMs>] with minMs <= maxMs.\n";
                    return 2;
                }
                foregroundThiefDelayMs = std::pair<DWORD, DWORD>{first.value(), last.value()};
            }
            continue;
        }
        if (arg == L"--write-baselines")
        {
            writeBaselines = true;
            continue;
        }
        if (arg == L"--no-activate")
        {
            blockActivation = true;
            continue;
        }
        if (arg.rfind(kPerfJsonlPrefix, 0) == 0)
        {
            if (arg.size() == kPerfJsonlPrefix.size())
            {
                std::wcerr << L"Missing perf JSONL path for --perf-jsonl.\n";
                return 2;
            }
            perfJsonlPath = std::filesystem::path(arg.substr(kPerfJsonlPrefix.size()));
            continue;
        }
        if (arg.rfind(kGalleryPrefix, 0) == 0)
        {
            if (arg.size() == kGalleryPrefix.size())
            {
                std::wcerr << L"Missing output path for --gallery-output.\n";
                return 2;
            }
            galleryOutputPath = std::filesystem::path(arg.substr(kGalleryPrefix.size()));
            continue;
        }
        if (arg.rfind(kGalleryDirectoryPrefix, 0) == 0)
        {
            if (arg.size() == kGalleryDirectoryPrefix.size())
            {
                std::wcerr << L"Missing output directory for --gallery-output-directory.\n";
                return 2;
            }
            galleryOutputDirectory = std::filesystem::path(arg.substr(kGalleryDirectoryPrefix.size()));
            continue;
        }
        if (arg.rfind(kButtonAuditPrefix, 0) == 0)
        {
            if (arg.size() == kButtonAuditPrefix.size())
            {
                std::wcerr << L"Missing output path for --button-audit-output.\n";
                return 2;
            }
            buttonAuditOutputPath = std::filesystem::path(arg.substr(kButtonAuditPrefix.size()));
            continue;
        }
        if (! arg.empty() && arg[0] == L'-')
        {
            std::wcerr << L"Unknown argument: " << arg << L'\n';
            return 2;
        }
        if (! arg.empty())
        {
            suiteFilter = std::wstring(arg);
            continue;
        }
    }

    SetDxUiWriteBaselines(writeBaselines);
    if (! testNames.empty())
    {
        SetDxUiTestFilter(testNames);
    }
    if (perfJsonlPath.has_value())
    {
#if defined(NDEBUG)
        constexpr std::wstring_view kBuildFlavor = L"Release";
#else
        constexpr std::wstring_view kBuildFlavor = L"Debug";
#endif
        TestPerformanceCapture::Start(perfJsonlPath.value(), L"DxUiTests", kBuildFlavor);
    }
    const auto perfCleanup = wil::scope_exit([&] { TestPerformanceCapture::Stop(); });

    const auto shouldRunSuite = [&](const char* name) -> bool
    {
        if (! suiteFilter.has_value())
        {
            return true;
        }

        std::wstring wideName;
        while (*name != '\0')
        {
            wideName.push_back(static_cast<wchar_t>(*name));
            ++name;
        }
        return _wcsicmp(wideName.c_str(), suiteFilter->c_str()) == 0;
    };

    // These suites are one fixture each, with no test functions a name could select.
    if (! testNames.empty() && suiteFilter.has_value())
    {
        for (const char* fixtureSuite : {"MenuTextLayoutResources", "MenuResourceScaling", "MenuResources", "Gallery", "ButtonContrast", "MenuExitLifetime"})
        {
            if (shouldRunSuite(fixtureSuite))
            {
                std::wcerr << L"--test cannot select tests within the " << *suiteFilter << L" suite: it has no individually named tests.\n";
                return 2;
            }
        }
    }

    const auto suiteCanActivate = [](const char* name) noexcept
    {
        return _stricmp(name, "Menu") == 0 || _stricmp(name, "NativeTextInput") == 0 || _stricmp(name, "MenuResources") == 0 ||
               _stricmp(name, "MenuResourceScaling") == 0;
    };
    const bool selectedSuiteCanActivate =
        suiteFilter.has_value() && (_wcsicmp(suiteFilter->c_str(), L"Menu") == 0 || _wcsicmp(suiteFilter->c_str(), L"NativeTextInput") == 0 ||
                                    _wcsicmp(suiteFilter->c_str(), L"MenuResources") == 0 || _wcsicmp(suiteFilter->c_str(), L"MenuResourceScaling") == 0);
    if (blockActivation && (! suiteFilter.has_value() || selectedSuiteCanActivate))
    {
        std::wcerr << L"--no-activate cannot run a DxUi suite whose contract requires real focus.\n";
        return 2;
    }

    if (blockActivation && foregroundThiefDelayMs.has_value())
    {
        // The thief takes the foreground itself, which --no-activate promises no run does.
        std::wcerr << L"--foreground-thief cannot combine with --no-activate.\n";
        return 2;
    }

    DxUi::TestSupport::ScopedWindowActivationBlocker activationBlocker;
    if (blockActivation && ! activationBlocker.Start())
    {
        std::wcerr << L"Failed to install the DxUi no-activation guard.\n";
        return 2;
    }

    std::optional<DxUi::TestSupport::ForegroundThief> foregroundThief;
    if (foregroundThiefDelayMs.has_value())
    {
        foregroundThief.emplace(foregroundThiefDelayMs->first, foregroundThiefDelayMs->second);
    }
    const auto reportForegroundThefts = wil::scope_exit([&]
    {
        if (foregroundThief.has_value())
        {
            std::cerr << "[THIEF] foreground taken " << foregroundThief->TheftCount() << " times";
            if (! foregroundThief->OwnForegroundSeen())
                std::cerr << " (no window of this process ever held the foreground, so no takeover was exercised)";
            std::cerr << '\n' << std::flush;
        }
    });

    auto runSuite = [&](const char* name, void (*fn)())
    {
        SetDxUiTestWindowsCanActivate(suiteCanActivate(name));
        std::cerr << "[START] " << name << '\n' << std::flush;
        fn();
        DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();
        std::cerr << "[DONE] " << name << '\n' << std::flush;
    };

    bool ranAnySuite = false;
    if (suiteFilter.has_value() && shouldRunSuite("MenuTextLayoutResources"))
    {
        runSuite("MenuTextLayoutResources", RunMenuTextLayoutResourceTests);
        ranAnySuite = true;
    }
    if (suiteFilter.has_value() && shouldRunSuite("MenuResourceScaling"))
    {
        runSuite("MenuResourceScaling", RunMenuResourceScalingTests);
        ranAnySuite = true;
    }
    if (suiteFilter.has_value() && shouldRunSuite("MenuResources"))
    {
        runSuite("MenuResources", RunMenuResourceTests);
        ranAnySuite = true;
    }
    if (suiteFilter.has_value() && shouldRunSuite("Gallery"))
    {
        const std::filesystem::path outputPath = galleryOutputPath.value_or(GetDxUiTestArtifactPath(L"DxUiControlGallery.png"));
        std::cerr << "[START] Gallery\n" << std::flush;
        if (galleryOutputDirectory.has_value())
        {
            RunGalleryGeneratorPerTheme(galleryOutputDirectory.value());
        }
        else
        {
            RunGalleryGenerator(outputPath);
        }
        DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();
        std::cerr << "[DONE] Gallery\n" << std::flush;
        ranAnySuite = true;
    }
    if (suiteFilter.has_value() && shouldRunSuite("ButtonContrast"))
    {
        const std::filesystem::path outputPath = buttonAuditOutputPath.value_or(GetDxUiTestArtifactPath(L"DxUiButtonContrast.png"));
        std::cerr << "[START] ButtonContrast\n" << std::flush;
        RunButtonContrastAuditGenerator(outputPath);
        DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();
        std::cerr << "[DONE] ButtonContrast\n" << std::flush;
        ranAnySuite = true;
    }
    if (suiteFilter.has_value() && shouldRunSuite("MenuExitLifetime"))
    {
        runSuite("MenuExitLifetime", RunMenuExitLifetimeTests);
        return 1; // This probe succeeds only through the explicit CRT-exit path.
    }
    if (shouldRunSuite("Grid"))
    {
        runSuite("Grid", RunGridTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Theme"))
    {
        runSuite("Theme", RunThemeTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Control"))
    {
        runSuite("Control", RunControlTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Menu"))
    {
        runSuite("Menu", RunMenuTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("NewControls"))
    {
        runSuite("NewControls", RunNewControlTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("EditorControls"))
    {
        runSuite("EditorControls", RunEditorControlTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("TextField"))
    {
        runSuite("TextField", RunTextFieldTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("NativeTextInput"))
    {
        runSuite("NativeTextInput", RunNativeTextInputTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("ComboBox"))
    {
        runSuite("ComboBox", RunComboBoxTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("WindowHost"))
    {
        runSuite("WindowHost", RunWindowHostTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Tree"))
    {
        runSuite("Tree", RunTreeTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("MultilineText"))
    {
        runSuite("MultilineText", RunMultilineTextTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("ReadOnly"))
    {
        runSuite("ReadOnly", RunReadOnlyTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Tooltip"))
    {
        runSuite("Tooltip", RunTooltipTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Rendering"))
    {
        runSuite("Rendering", RunRenderingTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Animation"))
    {
        runSuite("Animation", RunAnimationTests);
        ranAnySuite = true;
    }
    if (shouldRunSuite("Accessibility"))
    {
        runSuite("Accessibility", RunAccessibilityTests);
        ranAnySuite = true;
    }

    if (! ranAnySuite)
    {
        std::wcerr << L"Unknown suite filter: " << suiteFilter.value_or(L"<empty>") << L'\n';
        return 2;
    }
    if (const std::vector<std::string> unknownTests = UnmatchedDxUiTestNames(); ! unknownTests.empty())
    {
        // A name no selected suite registers would otherwise pass with nothing run.
        std::cerr << "Unknown test name for the selected suites:";
        for (const std::string& name : unknownTests)
            std::cerr << ' ' << name;
        std::cerr << "\nNames are exact, case-sensitive test function names.\n";
        return 2;
    }

    std::cout << "All DxUi tests passed.\n";
    return 0;
}
