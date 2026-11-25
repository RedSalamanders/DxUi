// W6.1 receipt: CPU cost of IDWriteTextLayout::SetMaxWidth plus metrics against CreateTextLayout plus metrics.
// A standalone console program: no DxUi code, no window. Build and run as README.md in this folder says.
#include <windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#pragma comment(lib, "dwrite.lib")
using Microsoft::WRL::ComPtr;

static unsigned long long Cycles()
{
    ULONG64 c = 0;
    QueryThreadCycleTime(GetCurrentThread(), &c);
    return c;
}
static std::wstring Make(const std::wstring& seed, size_t units)
{
    std::wstring s;
    while (s.size() < units)
        s += seed;
    s.resize(units);
    // never end inside a surrogate pair
    if (!s.empty() && IS_HIGH_SURROGATE(s.back()))
        s.back() = L' ';
    return s;
}
static double Median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

int wmain(int argc, wchar_t** argv)
{
    const double scale = argc > 1 ? _wtof(argv[1]) : 1.0;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    ComPtr<IDWriteFactory> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
        return 1;
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f,
                                         L"en-us", format.GetAddressOf())))
        return 1;
    const std::wstring latin =
        L"The quick brown fox jumps over the lazy dog while the committee reviews quarterly shipments, returns and variances. ";
    const std::wstring mixed = L"Shipments rose 12% \x0627\x0644\x0634\x062D\x0646\x0627\x062A \x0627\x0631\x062A\x0641\x0639\x062A "
                               L"\x51FA\x8377\x91CF\x304C\x5897\x52A0\x3057\x307E\x3057\x305F \x0928\x092E\x0938\x094D\x0924\x0947 "
                               L"\xD55C\xAD6D\xC5B4 \x05E9\x05DC\x05D5\x05DD caf\x00E9 \xD83D\xDE00 \x0E2A\x0E27\x0E31\x0E2A\x0E14\x0E35 ";
    struct Size
    {
        size_t units;
        int reps;
    };
    const Size sizes[] = {{200u, 400}, {4096u, 60}, {65536u, 9}};
    constexpr int kSteps = 30;
    std::printf("script,units,reps,create_med_Mcyc_per_step,inplace_med_Mcyc_per_step,ratio_create_over_inplace,create_lines_last,inplace_lines_last,metrics_equal\n");
    for (int script = 0; script < 2; ++script)
        for (const Size& sz : sizes)
        {
            const std::wstring text = Make(script ? mixed : latin, sz.units);
            const UINT32 len        = static_cast<UINT32>(text.size());
            const int reps          = (std::max)(3, static_cast<int>(sz.reps * scale));
            std::vector<double> createStep, inplaceStep;
            bool equal = true;
            UINT32 lastC = 0, lastI = 0;
            // warm the font and shaping caches once
            {
                ComPtr<IDWriteTextLayout> warm;
                factory->CreateTextLayout(text.c_str(), len, format.Get(), 400.0f, 100000.0f, warm.GetAddressOf());
                DWRITE_TEXT_METRICS m{};
                warm->GetMetrics(&m);
            }
            for (int r = 0; r < reps; ++r)
            {
                // the in-place path starts from a layout already measured at the start width, as a retained one is
                ComPtr<IDWriteTextLayout> kept;
                factory->CreateTextLayout(text.c_str(), len, format.Get(), 300.0f, 100000.0f, kept.GetAddressOf());
                DWRITE_TEXT_METRICS m0{};
                kept->GetMetrics(&m0);
                for (int s = 1; s <= kSteps; ++s)
                {
                    const float w = 300.0f + 11.0f * s; // grows: 30 steps, wrapping changes on most steps
                    DWRITE_TEXT_METRICS mc{}, mi{};
                    const auto c0 = Cycles();
                    ComPtr<IDWriteTextLayout> fresh;
                    factory->CreateTextLayout(text.c_str(), len, format.Get(), w, 100000.0f, fresh.GetAddressOf());
                    fresh->GetMetrics(&mc);
                    const auto c1 = Cycles();
                    kept->SetMaxWidth(w);
                    kept->GetMetrics(&mi);
                    const auto c2 = Cycles();
                    fresh.Reset();
                    createStep.push_back(double(c1 - c0));
                    inplaceStep.push_back(double(c2 - c1));
                    if (mc.lineCount != mi.lineCount || mc.height != mi.height || mc.width != mi.width || mc.widthIncludingTrailingWhitespace != mi.widthIncludingTrailingWhitespace)
                        equal = false;
                    lastC = mc.lineCount;
                    lastI = mi.lineCount;
                }
            }
            // decomposition: create alone, first metrics, metrics again unchanged, SetMaxWidth to the same width + metrics,
            // SetMaxHeight only + metrics, SetMaxWidth by +11 + metrics
            {
                std::vector<double> d[6];
                for (int r = 0; r < reps * 3; ++r)
                {
                    DWRITE_TEXT_METRICS m{};
                    ComPtr<IDWriteTextLayout> l;
                    auto a = Cycles();
                    factory->CreateTextLayout(text.c_str(), len, format.Get(), 400.0f, 100000.0f, l.GetAddressOf());
                    auto b = Cycles();
                    l->GetMetrics(&m);
                    auto c2 = Cycles();
                    l->GetMetrics(&m);
                    auto e = Cycles();
                    l->SetMaxWidth(400.0f);
                    l->GetMetrics(&m);
                    auto f = Cycles();
                    l->SetMaxHeight(90000.0f);
                    l->GetMetrics(&m);
                    auto g = Cycles();
                    l->SetMaxWidth(411.0f);
                    l->GetMetrics(&m);
                    auto h = Cycles();
                    d[0].push_back(double(b - a));
                    d[1].push_back(double(c2 - b));
                    d[2].push_back(double(e - c2));
                    d[3].push_back(double(f - e));
                    d[4].push_back(double(g - f));
                    d[5].push_back(double(h - g));
                }
                std::printf("  decomposition Mcyc: create=%.4f firstMetrics=%.4f metricsAgain=%.4f sameWidth=%.4f heightOnly=%.4f width+11=%.4f\n",
                            Median(d[0]) / 1e6, Median(d[1]) / 1e6, Median(d[2]) / 1e6, Median(d[3]) / 1e6, Median(d[4]) / 1e6, Median(d[5]) / 1e6);
            }
            const double c = Median(createStep) / 1e6, i = Median(inplaceStep) / 1e6;
            std::printf("%s,%zu,%d,%.4f,%.4f,%.2f,%u,%u,%s\n", script ? "mixed" : "latin", sz.units, reps, c, i, c / i, lastC, lastI, equal ? "yes" : "NO");
            std::fflush(stdout);
        }
    return 0;
}
