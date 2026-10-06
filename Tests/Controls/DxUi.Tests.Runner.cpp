#include "../../src/Controls/TextClipboard.h"
#include "../../src/Support/AnimationDispatcher.h"
#include "Controls.Tests.DxUiTestHelpers.h"

#include "../Support/Support.Tests.FailureReports.h"
#include "../Support/Support.Tests.ForegroundThief.h"
#include "../Support/Support.Tests.PerformanceCapture.h"
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
void RunInteractiveLeaseTests();
void RunGalleryGenerator(const std::filesystem::path& outputPath);
void RunGalleryGeneratorPerTheme(const std::filesystem::path& outputDirectory);
void RunButtonContrastAuditGenerator(const std::filesystem::path& outputPath);

namespace
{
// Suites that are one fixture each, with no test functions a name could select: --test cannot select within them, and the
// watchdog bounds each as one unit (the resource fixtures report every cycle with NoteDxUiTestProgress). The last two belong
// to the watchdog's self-test.
constexpr std::array<const char*, 8> kFixtureSuites{"MenuTextLayoutResources",
                                                    "MenuResourceScaling",
                                                    "MenuResources",
                                                    "Gallery",
                                                    "ButtonContrast",
                                                    "MenuExitLifetime",
                                                    "WatchdogSelfTestFixture",
                                                    "WatchdogSelfTestProgress"};

// The watchdog's self-test, reachable only through --watchdog-self-test[=test|fixture|progress], which
// Tools/tests/Test-TestWatchdog.ps1 runs with a two-second deadline: without the watchdog the first two never return.
[[nodiscard]] wil::unique_event_nothrow MakeEventNobodySets()
{
    wil::unique_event_nothrow never(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(static_cast<bool>(never), "the watchdog self-test creates its event");
    return never;
}

// A test that never returns, as the hung Menu test did.
void TestWatchdogSelfTestBlocksForever()
{
    const wil::unique_event_nothrow never = MakeEventNobodySets();
    static_cast<void>(WaitForSingleObject(never.get(), INFINITE));
}

void RunWatchdogSelfTest()
{
    DXUI_RUN_TEST(TestWatchdogSelfTestBlocksForever);
}

// A fixture suite that never returns. It has no named test for DXUI_RUN_TEST to arm, so the runner arms the suite.
void RunWatchdogSelfTestFixture()
{
    const wil::unique_event_nothrow never = MakeEventNobodySets();
    static_cast<void>(WaitForSingleObject(never.get(), INFINITE));
}

// A fixture whose run outlives the deadline as a whole but whose cycles do not: each waits a second on an event nobody
// sets, then reports its progress, so the deadline starts over and the run ends by itself.
void RunWatchdogSelfTestProgress()
{
    const wil::unique_event_nothrow never = MakeEventNobodySets();
    for (int cycle = 0; cycle < 4; ++cycle)
    {
        Require(WaitForSingleObject(never.get(), 1000u) == WAIT_TIMEOUT, "the watchdog self-test's event is never set");
        NoteDxUiTestProgress();
    }
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    DxUiTestFailureReports::RouteAwayFromDialogs();
    // The failure-report self-test, reachable only through --failure-report-self-test, which Tools/tests/Test-TestWatchdog.ps1
    // runs: a failed runtime check must end the run with its report and exit code 3, never wait on a dialog.
    if (argc == 2 && std::wstring_view(argv[1]) == L"--failure-report-self-test")
    {
#if defined(_DEBUG)
        _ASSERTE(! L"the failure-report self-test fails this check on purpose");
        std::fputs("The failed check returned instead of ending the run.\n", stderr);
        return 1;
#else
        std::fputs("A Release build has no runtime checks to report.\n", stdout);
        return 0;
#endif
    }

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
    std::optional<std::wstring> watchdogSelfTest;
    unsigned testTimeoutSeconds = DxUi::TestSupport::kDefaultTestTimeoutSeconds;
    bool writeBaselines         = false;
    bool blockActivation        = false;
    // A whole number of at most `maximumDigits` decimal digits.
    const auto parseWholeNumber = [](std::wstring_view digits, size_t maximumDigits) -> std::optional<DWORD>
    {
        if (digits.empty() || digits.size() > maximumDigits ||
            ! std::all_of(digits.begin(), digits.end(), [](wchar_t ch) noexcept { return ch >= L'0' && ch <= L'9'; }))
            return std::nullopt;
        DWORD value = 0u;
        for (const wchar_t ch : digits)
            value = value * 10u + static_cast<DWORD>(ch - L'0');
        return value;
    };
    for (int argIndex = 1; argIndex < argc; ++argIndex)
    {
        const std::wstring_view arg                         = argv[argIndex] ? std::wstring_view(argv[argIndex]) : std::wstring_view{};
        constexpr std::wstring_view kSuitePrefix            = L"--suite=";
        constexpr std::wstring_view kTestPrefix             = L"--test=";
        constexpr std::wstring_view kTestTimeoutPrefix      = L"--test-timeout=";
        constexpr std::wstring_view kWatchdogSelfTestFlag   = L"--watchdog-self-test";
        constexpr std::wstring_view kWatchdogSelfTestPrefix = L"--watchdog-self-test=";
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
        if (arg.rfind(kTestTimeoutPrefix, 0) == 0)
        {
            // The seconds each test may run before the watchdog ends the run; 0 turns the watchdog off.
            const std::optional<DWORD> seconds = parseWholeNumber(arg.substr(kTestTimeoutPrefix.size()), 6u);
            if (! seconds)
            {
                std::wcerr << L"Expected --test-timeout=<seconds> with a whole number of seconds (0 turns the watchdog off).\n";
                return 2;
            }
            testTimeoutSeconds = seconds.value();
            continue;
        }
        if (arg == kWatchdogSelfTestFlag || arg.rfind(kWatchdogSelfTestPrefix, 0) == 0)
        {
            // Hidden: one test (or fixture suite) that never returns, for Tools/tests/Test-TestWatchdog.ps1 to prove the
            // watchdog ends it. It is in no suite and no run reaches it without this switch.
            const std::wstring_view mode = arg == kWatchdogSelfTestFlag ? std::wstring_view(L"test") : arg.substr(kWatchdogSelfTestPrefix.size());
            if (mode != L"test" && mode != L"fixture" && mode != L"progress")
            {
                std::wcerr << L"Expected --watchdog-self-test[=test|fixture|progress].\n";
                return 2;
            }
            watchdogSelfTest = std::wstring(mode);
            continue;
        }
        if (arg == kForegroundThiefFlag || arg.rfind(kForegroundThiefPrefix, 0) == 0)
        {
            // The delay range in milliseconds between a window of this process becoming the foreground window and the thief
            // taking the foreground; the bare flag reproduces the 30-95 ms of the desktop application that did so.
            foregroundThiefDelayMs = std::pair<DWORD, DWORD>{30u, 95u};
            if (arg != kForegroundThiefFlag)
            {
                const std::wstring_view range    = arg.substr(kForegroundThiefPrefix.size());
                const size_t comma               = range.find(L',');
                const std::optional<DWORD> first = parseWholeNumber(range.substr(0, comma), 5u);
                const std::optional<DWORD> last  = comma == std::wstring_view::npos ? std::nullopt : parseWholeNumber(range.substr(comma + 1u), 5u);
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
        for (const char* fixtureSuite : kFixtureSuites)
        {
            if (shouldRunSuite(fixtureSuite))
            {
                std::wcerr << L"--test cannot select tests within the " << *suiteFilter << L" suite: it has no individually named tests.\n";
                return 2;
            }
        }
    }
    if (watchdogSelfTest.has_value() && (suiteFilter.has_value() || ! testNames.empty()))
    {
        std::wcerr << L"--watchdog-self-test cannot combine with --suite or --test.\n";
        return 2;
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

    // Every test runs under the watchdog's deadline, and so does a fixture suite, which has no named tests to arm it (see
    // Tests/Support/Support.Tests.TestWatchdog.h). The line says what a run allows, so a log shows the deadline that ended it.
    DxUi::TestSupport::TestWatchdog::Instance().SetTimeout(std::chrono::seconds(testTimeoutSeconds));
    if (testTimeoutSeconds != 0u)
        std::cerr << "[WATCHDOG] each test may run " << testTimeoutSeconds << " s; a test that outlives it ends the run with exit code "
                  << DxUi::TestSupport::kTestTimeoutExitCode << '\n'
                  << std::flush;
    else
        std::cerr << "[WATCHDOG] off (--test-timeout=0)\n" << std::flush;

    const auto isFixtureSuite = [](const char* name) noexcept
    { return std::any_of(kFixtureSuites.begin(), kFixtureSuites.end(), [name](const char* fixture) noexcept { return _stricmp(name, fixture) == 0; }); };
    auto runSuite = [&](const char* name, auto&& suite)
    {
        SetDxUiTestWindowsCanActivate(suiteCanActivate(name));
        std::cerr << "[START] " << name << '\n' << std::flush;
        const auto started = std::chrono::steady_clock::now();
        {
            std::optional<DxUi::TestSupport::ScopedTestDeadline> fixtureDeadline;
            if (isFixtureSuite(name))
                fixtureDeadline.emplace(name);
            suite();
        }
        DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();
        std::cerr << std::format("[DONE] {} ({:.3f} s)\n", name, std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count())
                  << std::flush;
    };

    if (watchdogSelfTest.has_value())
    {
        // Two of the three never return, so with the watchdog on the run ends inside them; returning from either is the failure.
        if (watchdogSelfTest.value() == L"progress")
        {
            runSuite("WatchdogSelfTestProgress", RunWatchdogSelfTestProgress);
            std::cout << "The watchdog self-test ended by itself.\n";
            return 0;
        }
        if (watchdogSelfTest.value() == L"fixture")
            runSuite("WatchdogSelfTestFixture", RunWatchdogSelfTestFixture);
        else
            runSuite("WatchdogSelfTest", RunWatchdogSelfTest);
        std::cerr << "FAILED: the watchdog self-test returned, so nothing ended it.\n";
        return 1;
    }

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
        runSuite("Gallery",
                 [&]
        {
            if (galleryOutputDirectory.has_value())
            {
                RunGalleryGeneratorPerTheme(galleryOutputDirectory.value());
            }
            else
            {
                RunGalleryGenerator(outputPath);
            }
        });
        ranAnySuite = true;
    }
    if (suiteFilter.has_value() && shouldRunSuite("ButtonContrast"))
    {
        const std::filesystem::path outputPath = buttonAuditOutputPath.value_or(GetDxUiTestArtifactPath(L"DxUiButtonContrast.png"));
        runSuite("ButtonContrast", [&] { RunButtonContrastAuditGenerator(outputPath); });
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
    if (shouldRunSuite("InteractiveLease"))
    {
        runSuite("InteractiveLease", RunInteractiveLeaseTests);
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
