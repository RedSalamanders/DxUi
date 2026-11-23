// FX13 cold start (plan W0.4): launch to first presented frame of a fresh process, a receipt-only real-time measurement.
// The parent stamps QueryPerformanceCounter just before CreateProcess; the child stamps the start of its command (after the
// test runner installs its failure host), builds the FX13
// application (Embedded.Tests.PerformanceFixtureFx13.h) on a WindowHost that never activates and sits off every monitor at the
// bottom of the z-order, and takes the first Present from the DxUi trace provider's test sink (src/Support/Trace.h). It then
// reads the presented image (Support.Tests.PresentedWindowCapture.h) to confirm the compositor shows a drawn frame. QPC is
// one system-wide clock, so the parent subtracts the child's stamps from its own. Nothing here takes focus or the pointer.
#include "Controls.Tests.PerformanceFixtureColdStart.h"

#include "Controls.Tests.DxUiTestHelpers.h"
// Scenarios are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
#include "Embedded.Tests.PerformanceFixtureFx13.h"
#pragma warning(pop)
#include "Support.Tests.PresentedWindowCapture.h"
#include "Support/Trace.h"

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
// The receipt's inputs, hashed as performance-fixtures.ps1 hashes a fixture's sources (repository-relative, SHA-256).
constexpr std::array kInputs{std::string_view("Tests/Controls/Controls.Tests.PerformanceFixtureColdStart.cpp"),
                             std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx13.h"),
                             std::string_view("Tests/Support/Support.Tests.PresentedWindowCapture.h")};

[[nodiscard]] std::string Sha256Hex(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    Require(bool(input), "the cold-start receipt reads each of its input files");
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::array<UCHAR, 32u> hash{};
    Require(BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,
                                      nullptr,
                                      0u,
                                      reinterpret_cast<PUCHAR>(bytes.data()),
                                      static_cast<ULONG>(bytes.size()),
                                      hash.data(),
                                      static_cast<ULONG>(hash.size()))),
            "SHA-256 of a cold-start input");
    constexpr char kDigits[] = "0123456789abcdef";
    std::string hex;
    for (const UCHAR value : hash)
    {
        hex.push_back(kDigits[value >> 4u]);
        hex.push_back(kDigits[value & 0x0Fu]);
    }
    return hex;
}

constexpr SIZE kClientSize{1920, 1080};
constexpr DWORD kChildDeadlineMs       = 60'000u;
constexpr ULONGLONG kPresentDeadlineMs = 20'000u;

[[nodiscard]] uint64_t Qpc() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return static_cast<uint64_t>(value.QuadPart);
}
[[nodiscard]] double QpcMs(uint64_t ticks) noexcept
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

struct FirstPresent
{
    uint64_t qpc      = 0u;
    uint64_t presents = 0u;
};
void RecordPresent(void* context, const DxUi::Detail::Trace::Record& record) noexcept
{
    auto& first = *static_cast<FirstPresent*>(context);
    if (record.event != DxUi::Detail::Trace::Event::Present || FAILED(record.result))
        return;
    if (first.presents++ == 0u)
        first.qpc = record.qpcTimestamp;
}

struct ChildResult
{
    uint64_t entryQpc   = 0u;
    uint64_t presentQpc = 0u;
    bool drawn          = false;
    UINT widthPx        = 0u;
    UINT heightPx       = 0u;
};

[[nodiscard]] double Median(std::vector<double> values)
{
    std::ranges::sort(values);
    const size_t middle = values.size() / 2u;
    return values.size() % 2u ? values[middle] : (values[middle - 1u] + values[middle]) / 2.0;
}
} // namespace

int RunFx13ColdStartChild(int argc, wchar_t** argv)
{
    if (argc != 3 || std::wstring_view(argv[1]) != L"--fx13-cold-start-child")
        return -1;
    ChildResult result;
    result.entryQpc = Qpc();
    FirstPresent first;
    DxUi::Detail::Trace::InstallSinkForTest(&RecordPresent, &first);
    {
        const DxUi::TestSupport::ScopedPerMonitorV2Awareness awareness;
        AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        auto theme          = window.Host().GetTheme();
        theme.reducedMotion = true;
        window.Host().SetTheme(theme);
        window.AllowOuterSizeBeyondDesktop(SIZE{8192, 8192});
        static_cast<void>(PerformanceFixtures::Fx13::BuildApplication(window.Host()));
        if (! DxUi::TestSupport::PlacePresentedCaptureWindow(window.Hwnd(), kClientSize))
        {
            DxUi::Detail::Trace::InstallSinkForTest(nullptr, nullptr);
            std::fputs("FX13 cold start: the window could not be shown off-screen without activation.\n", stderr);
            return 1;
        }
        // Real time: this variant measures it. The owner thread waits on its own messages, never busy.
        const ULONGLONG deadline = GetTickCount64() + kPresentDeadlineMs;
        while (first.presents == 0u && GetTickCount64() < deadline)
        {
            window.PumpMessages();
            if (first.presents == 0u)
                static_cast<void>(MsgWaitForMultipleObjects(0u, nullptr, FALSE, 50u, QS_ALLINPUT));
        }
        DxUi::Detail::Trace::InstallSinkForTest(nullptr, nullptr);
        result.presentQpc = first.qpc;
        if (first.presents != 0u)
        {
            window.PumpMessages();
            DxUi::TestSupport::PresentedWindowImage image;
            const auto drawn = [](const DxUi::TestSupport::PresentedWindowImage& shown)
            {
                // A drawn frame has more than one color: the window background and at least the controls' text.
                for (size_t pixel = 4u; pixel + 3u < shown.bgraPixels.size(); pixel += 4u)
                    if (std::memcmp(&shown.bgraPixels[pixel], &shown.bgraPixels[0], 3u) != 0)
                        return true;
                return false;
            };
            const auto status = DxUi::TestSupport::WaitForPresentedFrame(window.Hwnd(), drawn, image, CLR_INVALID, 120u);
            result.drawn      = status == DxUi::TestSupport::PresentedCaptureStatus::Captured;
            result.widthPx    = image.widthPx;
            result.heightPx   = image.heightPx;
            if (! result.drawn)
                std::fprintf(stderr, "FX13 cold start: presented image: %s\n", DxUi::TestSupport::DescribePresentedCaptureStatus(status));
        }
    }
    std::ofstream output{std::filesystem::path(argv[2]), std::ios::binary};
    output << result.entryQpc << ' ' << result.presentQpc << ' ' << (result.drawn ? 1 : 0) << ' ' << result.widthPx << ' ' << result.heightPx << '\n';
    output.close();
    return output && result.presentQpc != 0u && result.drawn ? 0 : 1;
}

int RunFx13ColdStartCommand(int argc, wchar_t** argv)
{
    if (argc < 3 || argc > 4)
    {
        std::fputs("--fx13-cold-start requires an output path and optionally a run count.\n", stderr);
        return 2;
    }
    const int runs = argc == 4 ? _wtoi(argv[3]) : 5;
    if (runs < 1 || runs > 50)
    {
        std::fputs("--fx13-cold-start runs 1 to 50 times.\n", stderr);
        return 2;
    }
    const std::filesystem::path outputPath(argv[2]);
    if (outputPath.has_parent_path())
        std::filesystem::create_directories(outputPath.parent_path());
    std::wstring executable(MAX_PATH, L'\0');
    executable.resize(GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size())));
    Require(! executable.empty(), "the cold-start parent finds its own executable");

    std::vector<double> launchToEntry, entryToPresent, launchToPresent;
    std::ostringstream runsJson;
    for (int run = 0; run < runs; ++run)
    {
        const std::filesystem::path childOutput = outputPath.parent_path() / (outputPath.stem().wstring() + L".child" + std::to_wstring(run) + L".txt");
        std::wstring commandLine                = L"\"" + executable + L"\" --fx13-cold-start-child \"" + childOutput.wstring() + L"\"";
        STARTUPINFOW startup{};
        startup.cb          = sizeof(startup);
        startup.dwFlags     = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_SHOWNOACTIVATE;
        PROCESS_INFORMATION process{};
        const uint64_t launchQpc = Qpc();
        Require(CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0u, nullptr, nullptr, &startup, &process) != FALSE,
                "the cold-start child process starts");
        const wil::unique_handle processHandle(process.hProcess);
        const wil::unique_handle threadHandle(process.hThread);
        Require(WaitForSingleObject(processHandle.get(), kChildDeadlineMs) == WAIT_OBJECT_0, "the cold-start child ends in time");
        DWORD exitCode = 1u;
        static_cast<void>(GetExitCodeProcess(processHandle.get(), &exitCode));
        ChildResult result;
        int drawn = 0;
        {
            std::ifstream input{childOutput};
            input >> result.entryQpc >> result.presentQpc >> drawn >> result.widthPx >> result.heightPx;
        }
        std::error_code ignored;
        std::filesystem::remove(childOutput, ignored);
        if (exitCode != 0u || result.presentQpc == 0u || drawn == 0)
        {
            std::fprintf(stderr, "FX13 cold start: child run %d failed (exit %lu).\n", run, static_cast<unsigned long>(exitCode));
            return 1;
        }
        launchToEntry.push_back(QpcMs(result.entryQpc - launchQpc));
        entryToPresent.push_back(QpcMs(result.presentQpc - result.entryQpc));
        launchToPresent.push_back(QpcMs(result.presentQpc - launchQpc));
        runsJson << (run ? "," : "") << "{\"launchToEntryMs\":" << launchToEntry.back() << ",\"entryToFirstPresentMs\":" << entryToPresent.back()
                 << ",\"launchToFirstPresentMs\":" << launchToPresent.back() << ",\"presentedImageDrawn\":true,\"presentedWidthPx\":" << result.widthPx
                 << ",\"presentedHeightPx\":" << result.heightPx << '}';
        std::printf("FX13 cold start run %d: launch to first present %.2f ms\n", run, launchToPresent.back());
    }
    const char* const configuration = PerformanceFixtures::ConfigurationName();
    const double medianEntry        = Median(launchToEntry);
    const double medianPresent      = Median(entryToPresent);
    const double medianTotal        = Median(launchToPresent);
    const double minimumTotal       = *std::ranges::min_element(launchToPresent);
    const std::string runsText      = runsJson.str();
    std::string inputsText;
    for (const std::string_view input : kInputs)
        inputsText += (inputsText.empty() ? "\"" : ",\"") + std::string(input) + "\":\"" + Sha256Hex(FindRepoRootForDxUiTests() / input) + '"';
    std::ofstream output{outputPath, std::ios::binary};
    output << "{\"schema\":1,\"workloadOwner\":\"DxUi\",\"fixture\":\"dxui-fx13-cold-start-v1\",\"fixtureId\":\"FX13\",\"variant\":\"cold-start\","
              "\"receiptOnly\":true,"
              "\"determinismGate\":false,\"host\":\"window\",\"renderer\":\"WARP\",\"configuration\":\""
           << configuration << "\",\"compiler\":" << _MSC_FULL_VER << ",\"clientWidthPx\":" << kClientSize.cx << ",\"clientHeightPx\":" << kClientSize.cy
           << ",\"benchmarkInputs\":{" << inputsText << "}"
           << ",\"method\":\"QueryPerformanceCounter before CreateProcess in the parent; in the child, when its command starts (after the test runner installs "
              "its failure host) and at the first "
              "successful Present from the DxUi trace provider's test sink; the presented image is then read through the compositor "
              "(PrintWindow) and must show a drawn frame. The window is shown without activation, off every monitor, at the bottom of "
              "the z-order.\",\"verdict\":\"none: wall-clock time of fresh processes on a shared machine under load, not a timing "
              "verdict\",\"runs\":["
           << runsText << "],\"medianMs\":{\"launchToEntry\":" << medianEntry << ",\"entryToFirstPresent\":" << medianPresent
           << ",\"launchToFirstPresent\":" << medianTotal << "},\"minimumMs\":{\"launchToFirstPresent\":" << minimumTotal << "}}\n";
    output.close();
    Require(bool(output), "the cold-start receipt is written");
    return 0;
}
