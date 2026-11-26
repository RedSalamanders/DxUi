// Presentation-mode comparison (plan W0.9). See Controls.Tests.PresentationModes.h.
//
// One case = one fixture on one adapter, both swap-chain modes, in one process. Each mode runs on a fresh thread (its own native
// graphics pool, so WARP or the default hardware adapter is chosen cleanly) as a WindowFixtureSession of the performance-fixture
// framework: the one definition of FX5 and FX1, a non-activating off-screen window, the host's own WM_PAINT. Four parts per mode:
//   pixels    the presented image (PrintWindow of the composed window over three backdrops), never the back buffer alone: the
//             alpha every pixel presents, recovered exactly from the black and white backdrops; the text antialiasing mode
//             Direct2D used and the chromatic pixels it left; the back buffer's own alpha;
//   cost      every work counter per full frame, the frame-time breakdown (render, Present, total, UI-thread cycles from the host's
//             public frame metrics), input to present from the trace provider, private bytes and video memory (QueryVideoMemoryInfo);
//   behavior  live resize, minimize and restore, a covering window, device loss, a DPI change (the host's WM_DPICHANGED seam, a
//             simulation), a child HWND host, and the two cases this program cannot run (a monitor move, a remote session);
//   scope     every host a consumer creates without naming a mode, with the code that chooses it (identical in every case).
// Counters and pixels decide; timing advises and is marked not judged unless the caller says the machine was quiet.
#include "Controls.Tests.PresentationModes.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.PerformanceFixtures.h"
#include "Controls.Tests.PresentedFrame.h"
#include "Embedded.Tests.PerformanceFixtureCatalog.h"
#include "Support.Tests.PresentedWindowCapture.h"
#include "Support/Trace.h"
#include "Support/WorkCounters.h"

#include <dwmapi.h>
#include <dxgi1_6.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")

// Results are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4623 4625 4626 5026 5027)
namespace
{
namespace Trace = DxUi::Detail::Trace;
using DxUi::TestSupport::PresentedCaptureStatus;
using DxUi::TestSupport::PresentedWindowImage;
using Mode = DxUi::WindowHost::PresentationMode;

[[nodiscard]] constexpr const char* ModeName(Mode mode) noexcept
{
    return mode == Mode::CompositionSwapChain ? "CompositionSwapChain" : "HwndSwapChain";
}

// ---------------------------------------------------------------------------------------------------------------------------
// A small JSON writer: commas and nesting are tracked, strings are escaped, nothing is parsed.
// ---------------------------------------------------------------------------------------------------------------------------
class Json final
{
public:
    Json& Begin(bool array = false)
    {
        Separate();
        _text += array ? '[' : '{';
        _first.push_back(true);
        return *this;
    }
    Json& End(bool array = false)
    {
        _text += array ? ']' : '}';
        _first.pop_back();
        return *this;
    }
    Json& Array()
    {
        return Begin(true);
    }
    Json& EndArray()
    {
        return End(true);
    }
    Json& Key(std::string_view name)
    {
        Separate();
        Quote(name);
        _text += ':';
        _afterKey = true;
        return *this;
    }
    Json& Str(std::string_view value)
    {
        Separate();
        Quote(value);
        return *this;
    }
    Json& Num(double value)
    {
        Separate();
        if (std::isfinite(value) && value == std::floor(value) && std::fabs(value) < 9.0e15)
            _text += std::format("{}", static_cast<long long>(value));
        else if (std::isfinite(value))
            _text += std::format("{:.6g}", value);
        else
            _text += "null";
        return *this;
    }
    Json& Int(uint64_t value)
    {
        Separate();
        _text += std::format("{}", value);
        return *this;
    }
    Json& Bool(bool value)
    {
        Separate();
        _text += value ? "true" : "false";
        return *this;
    }
    Json& Null()
    {
        Separate();
        _text += "null";
        return *this;
    }
    // A complete value already written as JSON.
    Json& Raw(std::string_view value)
    {
        Separate();
        _text += value;
        return *this;
    }
    Json& Field(std::string_view name, std::string_view value)
    {
        return Key(name).Str(value);
    }
    Json& Field(std::string_view name, const char* value)
    {
        return Key(name).Str(value);
    }
    Json& Field(std::string_view name, const std::string& value)
    {
        return Key(name).Str(value);
    }
    Json& Field(std::string_view name, bool value)
    {
        return Key(name).Bool(value);
    }
    Json& Field(std::string_view name, double value)
    {
        return Key(name).Num(value);
    }
    Json& Field(std::string_view name, uint64_t value)
    {
        return Key(name).Int(value);
    }
    Json& Field(std::string_view name, float value)
    {
        return Key(name).Num(value);
    }
    Json& Field(std::string_view name, unsigned value)
    {
        return Key(name).Int(value);
    }
    Json& Field(std::string_view name, int value)
    {
        return Key(name).Num(value);
    }
    Json& Field(std::string_view name, long value)
    {
        return Key(name).Num(value);
    }
    [[nodiscard]] const std::string& Text() const noexcept
    {
        return _text;
    }

private:
    void Separate()
    {
        if (_afterKey)
        {
            _afterKey = false;
            return;
        }
        if (! _first.empty())
        {
            if (! _first.back())
                _text += ',';
            _first.back() = false;
        }
    }
    void Quote(std::string_view value)
    {
        _text += '"';
        for (const char c : value)
        {
            switch (c)
            {
                case '"': _text += "\\\""; break;
                case '\\': _text += "\\\\"; break;
                case '\n': _text += "\\n"; break;
                case '\r': _text += "\\r"; break;
                case '\t': _text += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20u || static_cast<unsigned char>(c) >= 0x7Fu)
                        _text += '?';
                    else
                        _text += c;
                    break;
            }
        }
        _text += '"';
    }
    std::string _text;
    std::vector<bool> _first;
    bool _afterKey = false;
};

[[nodiscard]] std::string AsciiOf(std::wstring_view text)
{
    std::string out;
    for (const wchar_t c : text)
        out.push_back(c > 0x20 && c < 0x7F ? static_cast<char>(c) : (c == L' ' ? ' ' : '?'));
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Image analysis. Pure functions of presented images: unit-tested without a window.
// ---------------------------------------------------------------------------------------------------------------------------
struct Box
{
    int left   = INT_MAX;
    int top    = INT_MAX;
    int right  = -1; // inclusive
    int bottom = -1;
    void Add(int x, int y) noexcept
    {
        left   = (std::min)(left, x);
        top    = (std::min)(top, y);
        right  = (std::max)(right, x);
        bottom = (std::max)(bottom, y);
    }
    [[nodiscard]] bool Empty() const noexcept
    {
        return right < left;
    }
};

void WriteBox(Json& json, const Box& box)
{
    if (box.Empty())
    {
        json.Null();
        return;
    }
    json.Begin().Field("left", box.left).Field("top", box.top).Field("right", box.right).Field("bottom", box.bottom).End();
}

struct AlphaPlane
{
    UINT width  = 0u;
    UINT height = 0u;
    std::vector<uint8_t> alpha;
    size_t inconsistent = 0u; // pixels whose three channels disagree on the alpha (a backdrop that is not black and white, or a changed frame)
};

// The alpha every pixel presents. A premultiplied pixel composes over an opaque backdrop K as source + (255 - alpha) * K / 255,
// per channel. Over black the image is the premultiplied source; over white it is the source plus (255 - alpha) exactly, so the
// two images differ by 255 - alpha in every channel. An opaque pixel (an HwndSwapChain's, whose alpha the presentation ignores)
// differs by nothing.
[[nodiscard]] AlphaPlane RecoverAlpha(const PresentedWindowImage& overBlack, const PresentedWindowImage& overWhite)
{
    AlphaPlane plane;
    plane.width  = overBlack.widthPx;
    plane.height = overBlack.heightPx;
    if (overBlack.widthPx != overWhite.widthPx || overBlack.heightPx != overWhite.heightPx || overBlack.bgraPixels.size() != overWhite.bgraPixels.size())
        return plane;
    const size_t pixels = static_cast<size_t>(plane.width) * plane.height;
    plane.alpha.resize(pixels);
    for (size_t pixel = 0u; pixel < pixels; ++pixel)
    {
        const size_t at = pixel * 4u;
        const int d0    = static_cast<int>(overWhite.bgraPixels[at + 0u]) - static_cast<int>(overBlack.bgraPixels[at + 0u]);
        const int d1    = static_cast<int>(overWhite.bgraPixels[at + 1u]) - static_cast<int>(overBlack.bgraPixels[at + 1u]);
        const int d2    = static_cast<int>(overWhite.bgraPixels[at + 2u]) - static_cast<int>(overBlack.bgraPixels[at + 2u]);
        if (d0 != d1 || d1 != d2)
            ++plane.inconsistent;
        const int coverage = (std::clamp)((d0 + d1 + d2 + 1) / 3, 0, 255);
        plane.alpha[pixel] = static_cast<uint8_t>(255 - coverage);
    }
    return plane;
}

struct Component
{
    size_t pixels = 0u;
    Box box;
};

struct AlphaSummary
{
    size_t pixels       = 0u;
    size_t opaque       = 0u; // alpha 255
    size_t transparent  = 0u; // alpha 0: the backdrop shows through unchanged
    size_t partial      = 0u; // 0 < alpha < 255: antialiased edges of shapes cleared against transparent, or translucent fills
    size_t inconsistent = 0u;
    Box partialBox;
    Box transparentBox;
    size_t transparentComponents = 0u;
    size_t transparentRows       = 0u;         // rows of the image in which every pixel is fully transparent
    size_t transparentColumns    = 0u;         // columns of which the same holds
    std::vector<Component> largestTransparent; // up to 6, largest first
};

[[nodiscard]] AlphaSummary Summarize(const AlphaPlane& plane)
{
    AlphaSummary summary;
    summary.pixels       = plane.alpha.size();
    summary.inconsistent = plane.inconsistent;
    std::vector<uint8_t> visited(plane.alpha.size(), 0u);
    std::vector<size_t> stack;
    for (UINT y = 0u; y < plane.height; ++y)
        for (UINT x = 0u; x < plane.width; ++x)
        {
            const size_t index = static_cast<size_t>(y) * plane.width + x;
            const uint8_t a    = plane.alpha[index];
            if (a == 255u)
            {
                ++summary.opaque;
                continue;
            }
            if (a != 0u)
            {
                ++summary.partial;
                summary.partialBox.Add(static_cast<int>(x), static_cast<int>(y));
                continue;
            }
            ++summary.transparent;
            summary.transparentBox.Add(static_cast<int>(x), static_cast<int>(y));
            if (visited[index] != 0u)
                continue;
            // A four-connected region of fully transparent pixels.
            Component component;
            visited[index] = 1u;
            stack.assign(1u, index);
            while (! stack.empty())
            {
                const size_t current = stack.back();
                stack.pop_back();
                const int cx = static_cast<int>(current % plane.width);
                const int cy = static_cast<int>(current / plane.width);
                ++component.pixels;
                component.box.Add(cx, cy);
                const std::array<std::pair<int, int>, 4> neighbours{{{cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}}};
                for (const auto& [nx, ny] : neighbours)
                {
                    if (nx < 0 || ny < 0 || nx >= static_cast<int>(plane.width) || ny >= static_cast<int>(plane.height))
                        continue;
                    const size_t next = static_cast<size_t>(ny) * plane.width + static_cast<size_t>(nx);
                    if (plane.alpha[next] != 0u || visited[next] != 0u)
                        continue;
                    visited[next] = 1u;
                    stack.push_back(next);
                }
            }
            ++summary.transparentComponents;
            summary.largestTransparent.push_back(component);
        }
    for (UINT y = 0u; y < plane.height; ++y)
    {
        bool all = true;
        for (UINT x = 0u; x < plane.width && all; ++x)
            all = plane.alpha[static_cast<size_t>(y) * plane.width + x] == 0u;
        summary.transparentRows += all ? 1u : 0u;
    }
    for (UINT x = 0u; x < plane.width; ++x)
    {
        bool all = true;
        for (UINT y = 0u; y < plane.height && all; ++y)
            all = plane.alpha[static_cast<size_t>(y) * plane.width + x] == 0u;
        summary.transparentColumns += all ? 1u : 0u;
    }
    std::ranges::sort(summary.largestTransparent, [](const Component& a, const Component& b) { return a.pixels > b.pixels; });
    if (summary.largestTransparent.size() > 6u)
        summary.largestTransparent.resize(6u);
    return summary;
}

void WriteAlphaSummary(Json& json, const AlphaSummary& summary)
{
    json.Begin()
        .Field("pixels", summary.pixels)
        .Field("opaque", summary.opaque)
        .Field("fullyTransparent", summary.transparent)
        .Field("partiallyTransparent", summary.partial)
        .Field("channelsDisagree", summary.inconsistent)
        .Field("fullyTransparentRows", summary.transparentRows)
        .Field("fullyTransparentColumns", summary.transparentColumns)
        .Field("transparentRegions", summary.transparentComponents);
    json.Key("partialBox");
    WriteBox(json, summary.partialBox);
    json.Key("transparentBox");
    WriteBox(json, summary.transparentBox);
    json.Key("largestTransparentRegions").Array();
    for (const Component& component : summary.largestTransparent)
    {
        json.Begin().Field("pixels", component.pixels).Key("box");
        WriteBox(json, component.box);
        json.End();
    }
    json.EndArray().End();
}

// Colour fringes of subpixel (ClearType) text. Text on a flat ground, drawn with grayscale antialiasing, only ever blends the
// ground colour toward the text colour: every pixel lies on the segment between the two. ClearType blends each colour channel with
// its own coverage, which leaves the segment. In `region`: the ground is the most frequent colour, the text colour the colour
// farthest from it, and a pixel is off the blend line when it is more than 6 levels from its projection on that segment.
struct FringeStats
{
    size_t pixels      = 0u; // pixels of the region
    size_t textPixels  = 0u; // pixels that are not the ground colour
    size_t offTheLine  = 0u; // text pixels off the ground-to-text blend segment: subpixel colour fringes
    uint32_t ground    = 0u; // 0xRRGGBB
    uint32_t textColor = 0u;
};

[[nodiscard]] FringeStats MeasureFringes(const PresentedWindowImage& image, const RECT& region)
{
    FringeStats stats;
    const LONG left   = (std::max)(0L, region.left);
    const LONG top    = (std::max)(0L, region.top);
    const LONG right  = (std::min)(static_cast<LONG>(image.widthPx), region.right);
    const LONG bottom = (std::min)(static_cast<LONG>(image.heightPx), region.bottom);
    if (right <= left || bottom <= top)
        return stats;
    const auto at = [&](LONG x, LONG y) noexcept
    {
        const uint8_t* p = &image.bgraPixels[(static_cast<size_t>(y) * image.widthPx + static_cast<size_t>(x)) * 4u];
        return static_cast<uint32_t>(p[2]) << 16u | static_cast<uint32_t>(p[1]) << 8u | static_cast<uint32_t>(p[0]);
    };
    std::map<uint32_t, size_t> histogram;
    for (LONG y = top; y < bottom; ++y)
        for (LONG x = left; x < right; ++x)
            ++histogram[at(x, y)];
    stats.pixels    = static_cast<size_t>(right - left) * static_cast<size_t>(bottom - top);
    uint32_t ground = 0u;
    size_t best     = 0u;
    for (const auto& [color, count] : histogram)
        if (count > best)
        {
            best   = count;
            ground = color;
        }
    const auto channel  = [](uint32_t color, unsigned shift) noexcept { return static_cast<int>((color >> shift) & 0xFFu); };
    const auto distance = [&](uint32_t a, uint32_t b) noexcept
    {
        int sum = 0;
        for (const unsigned shift : {0u, 8u, 16u})
            sum += (channel(a, shift) - channel(b, shift)) * (channel(a, shift) - channel(b, shift));
        return sum;
    };
    uint32_t text = ground;
    int farthest  = 0;
    for (const auto& [color, count] : histogram)
        if (const int d = distance(color, ground); d > farthest)
        {
            farthest = d;
            text     = color;
        }
    stats.ground    = ground;
    stats.textColor = text;
    if (farthest == 0)
        return stats;
    for (LONG y = top; y < bottom; ++y)
        for (LONG x = left; x < right; ++x)
        {
            const uint32_t color = at(x, y);
            if (color == ground)
                continue;
            ++stats.textPixels;
            int dot = 0;
            for (const unsigned shift : {0u, 8u, 16u})
                dot += (channel(color, shift) - channel(ground, shift)) * (channel(text, shift) - channel(ground, shift));
            const double t = (std::clamp)(static_cast<double>(dot) / static_cast<double>(farthest), 0.0, 1.0);
            double off     = 0.0;
            for (const unsigned shift : {0u, 8u, 16u})
            {
                const double projected = static_cast<double>(channel(ground, shift)) + t * static_cast<double>(channel(text, shift) - channel(ground, shift));
                off += (static_cast<double>(channel(color, shift)) - projected) * (static_cast<double>(channel(color, shift)) - projected);
            }
            if (off > 36.0)
                ++stats.offTheLine;
        }
    return stats;
}

constexpr size_t kGridColumns = 8u;
constexpr size_t kGridRows    = 6u;

struct ImageComparison
{
    bool sameSize               = false;
    size_t pixels               = 0u;
    size_t differing            = 0u;
    size_t differingOpaque      = 0u; // where the composition mode presents alpha 255: text and shapes over opaque fills
    size_t differingPartial     = 0u; // where it presents 0 < alpha < 255: the antialiased edges of rounded shapes
    size_t differingTransparent = 0u;
    uint32_t maxDelta           = 0u;  // the largest per-channel difference
    std::array<size_t, 5> histogram{}; // by largest channel difference: 1, 2-4, 5-16, 17-64, 65-255
    Box box;
    std::array<size_t, kGridColumns * kGridRows> grid{};
    int firstX      = -1;
    int firstY      = -1;
    uint32_t firstA = 0u;
    uint32_t firstB = 0u;
};

// Counts the pixels where two presented images differ, by the alpha the second mode presents there (`alpha` may be null: no class).
[[nodiscard]] ImageComparison CompareImages(const PresentedWindowImage& a, const PresentedWindowImage& b, const AlphaPlane* alpha)
{
    ImageComparison result;
    result.sameSize = a.widthPx == b.widthPx && a.heightPx == b.heightPx && a.bgraPixels.size() == b.bgraPixels.size();
    if (! result.sameSize)
        return result;
    result.pixels = static_cast<size_t>(a.widthPx) * a.heightPx;
    for (UINT y = 0u; y < a.heightPx; ++y)
        for (UINT x = 0u; x < a.widthPx; ++x)
        {
            const size_t index   = static_cast<size_t>(y) * a.widthPx + x;
            const uint8_t* left  = &a.bgraPixels[index * 4u];
            const uint8_t* right = &b.bgraPixels[index * 4u];
            if (std::memcmp(left, right, 3u) == 0)
                continue;
            ++result.differing;
            uint32_t delta = 0u;
            for (size_t channel = 0u; channel < 3u; ++channel)
                delta = (std::max)(delta, static_cast<uint32_t>(std::abs(static_cast<int>(left[channel]) - static_cast<int>(right[channel]))));
            result.maxDelta = (std::max)(result.maxDelta, delta);
            ++result.histogram[delta <= 1u ? 0u : delta <= 4u ? 1u : delta <= 16u ? 2u : delta <= 64u ? 3u : 4u];
            if (alpha && alpha->alpha.size() == result.pixels)
            {
                const uint8_t coverage = alpha->alpha[index];
                if (coverage == 255u)
                    ++result.differingOpaque;
                else if (coverage == 0u)
                    ++result.differingTransparent;
                else
                    ++result.differingPartial;
            }
            result.box.Add(static_cast<int>(x), static_cast<int>(y));
            ++result.grid[(static_cast<size_t>(y) * kGridRows / a.heightPx) * kGridColumns + static_cast<size_t>(x) * kGridColumns / a.widthPx];
            if (result.firstX < 0)
            {
                result.firstX = static_cast<int>(x);
                result.firstY = static_cast<int>(y);
                std::memcpy(&result.firstA, left, 4u);
                std::memcpy(&result.firstB, right, 4u);
            }
        }
    return result;
}

void WriteComparison(Json& json, const ImageComparison& c)
{
    json.Begin()
        .Field("sameSize", c.sameSize)
        .Field("pixels", c.pixels)
        .Field("identical", c.pixels - c.differing)
        .Field("differing", c.differing)
        .Field("differingWhereCompositionIsOpaque", c.differingOpaque)
        .Field("differingWherePartiallyTransparent", c.differingPartial)
        .Field("differingWhereFullyTransparent", c.differingTransparent)
        .Field("maxChannelDifference", static_cast<unsigned>(c.maxDelta));
    json.Key("differenceByLargestChannel").Begin();
    json.Field("1", c.histogram[0]).Field("2-4", c.histogram[1]).Field("5-16", c.histogram[2]).Field("17-64", c.histogram[3]).Field("65-255", c.histogram[4]);
    json.End().Key("differenceBox");
    WriteBox(json, c.box);
    json.Key("differenceGrid").Begin().Field("columns", kGridColumns).Field("rows", kGridRows).Key("counts").Array();
    for (const size_t count : c.grid)
        json.Int(count);
    json.EndArray().End();
    if (c.firstX >= 0)
        json.Key("first")
            .Begin()
            .Field("x", c.firstX)
            .Field("y", c.firstY)
            .Field("hwndBgra", std::format("0x{:08X}", c.firstA))
            .Field("compositionBgra", std::format("0x{:08X}", c.firstB))
            .End();
    json.End();
}

// ---------------------------------------------------------------------------------------------------------------------------
// Statistics, memory, counters.
// ---------------------------------------------------------------------------------------------------------------------------
struct Stats
{
    size_t count   = 0u;
    double minimum = 0.0, median = 0.0, p95 = 0.0, maximum = 0.0;
};

[[nodiscard]] Stats Summarize(std::vector<double> values)
{
    Stats stats;
    stats.count = values.size();
    if (values.empty())
        return stats;
    std::ranges::sort(values);
    const auto at = [&](double share) { return values[(std::min)(values.size() - 1u, static_cast<size_t>(share * static_cast<double>(values.size())))]; };
    stats.minimum = values.front();
    stats.median  = at(0.5);
    stats.p95     = at(0.95);
    stats.maximum = values.back();
    return stats;
}

void WriteStats(Json& json, const Stats& stats)
{
    json.Begin()
        .Field("samples", stats.count)
        .Field("min", stats.minimum)
        .Field("median", stats.median)
        .Field("p95", stats.p95)
        .Field("max", stats.maximum)
        .End();
}

struct MemorySnapshot
{
    uint64_t privateBytes    = 0u;
    uint64_t workingSetBytes = 0u;
    bool videoApplicable     = false;
    uint64_t localBytes      = 0u; // this process's usage of the adapter's local (dedicated) memory
    uint64_t nonLocalBytes   = 0u; // and of its non-local (shared system) memory
};

[[nodiscard]] wil::com_ptr<IDXGIAdapter3> OpenAdapterForMemory(bool hardware)
{
    wil::com_ptr<IDXGIFactory1> factory;
    wil::com_ptr<IDXGIAdapter1> adapter;
    wil::com_ptr<IDXGIAdapter3> adapter3;
    if (! hardware || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.put()))) || FAILED(factory->EnumAdapters1(0u, adapter.put())))
        return adapter3;
    DXGI_ADAPTER_DESC1 description{};
    if (SUCCEEDED(adapter->GetDesc1(&description)) && (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0u)
        static_cast<void>(adapter.query_to(adapter3.put()));
    return adapter3;
}

[[nodiscard]] MemorySnapshot TakeMemory(IDXGIAdapter3* adapter)
{
    MemorySnapshot snapshot;
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    Require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE, "process memory");
    snapshot.privateBytes    = memory.PrivateUsage;
    snapshot.workingSetBytes = memory.WorkingSetSize;
    if (adapter)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO local{};
        DXGI_QUERY_VIDEO_MEMORY_INFO nonLocal{};
        if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0u, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local)) &&
            SUCCEEDED(adapter->QueryVideoMemoryInfo(0u, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocal)))
        {
            snapshot.videoApplicable = true;
            snapshot.localBytes      = local.CurrentUsage;
            snapshot.nonLocalBytes   = nonLocal.CurrentUsage;
        }
    }
    return snapshot;
}

struct CounterRange
{
    uint64_t minimum = UINT64_MAX;
    uint64_t maximum = 0u;
    uint64_t total   = 0u;
};
using CounterTable = std::map<std::string, CounterRange>;

void AddCounters(CounterTable& table, const DxUi::Detail::WorkCounters& delta)
{
    DxUi::Detail::ForEachWorkCounter(delta,
                                     [&](const char* group, const char* name, uint64_t value)
    {
        CounterRange& range = table[std::string(group) + "." + name];
        range.minimum       = (std::min)(range.minimum, value);
        range.maximum       = (std::max)(range.maximum, value);
        range.total += value;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------
// The fixtures a case can run, and what is specific to their pictures.
// ---------------------------------------------------------------------------------------------------------------------------
struct FixtureCase
{
    std::string_view id;
    std::string_view description;
    const PerformanceFixtures::Definition* definition;
    // The control a wheel notch scrolls (a point in DIPs), and a region of text drawn on a flat ground (DIPs).
    D2D1_POINT_2F wheelPointDip;
    D2D1_RECT_F textRegionDip;
};

[[nodiscard]] const FixtureCase* FindFixtureCase(std::string_view id) noexcept
{
    static const std::array<FixtureCase, 2> cases{{
        {"FX5",
         "FX5 ComplexUi, default variant: the 83-control scene (16 cards of label, toggle, slider and progress bar, a Tree, a Grid), 1280x720 at 96 DPI, after "
         "its 20 warm-up frames",
         &PerformanceFixtures::Fx5::kDefinition,
         {832.0f, 536.0f},
         {16.0f, 10.0f, 163.0f, 34.0f}},
        {"FX1",
         "FX1 idle caret, default variant, read as a text-heavy page: a 64 Ki-unit page of prose in a multiline TextField (63 characters per line) beside two "
         "single-line fields, 800x600 at 96 DPI",
         &PerformanceFixtures::Fx1::kDefinition,
         {396.0f, 348.0f},
         {24.0f, 120.0f, 768.0f, 576.0f}},
    }};
    for (const FixtureCase& entry : cases)
        if (entry.id == id)
            return &entry;
    return nullptr;
}

[[nodiscard]] RECT ToPixels(const D2D1_RECT_F& dip, float dpi) noexcept
{
    const float scale = dpi / 96.0f;
    return RECT{static_cast<LONG>(std::lround(dip.left * scale)),
                static_cast<LONG>(std::lround(dip.top * scale)),
                static_cast<LONG>(std::lround(dip.right * scale)),
                static_cast<LONG>(std::lround(dip.bottom * scale))};
}

[[nodiscard]] COLORREF ToColorRef(const D2D1_COLOR_F& color) noexcept
{
    const auto channel = [](float value) { return static_cast<BYTE>((std::clamp)(static_cast<int>(std::lround(value * 255.0f)), 0, 255)); };
    return RGB(channel(color.r), channel(color.g), channel(color.b));
}

struct Options
{
    size_t counterFrames = 20u;
    size_t timingFrames  = 120u;
    size_t inputCount    = 40u;
    bool judgedTiming    = false;
    std::filesystem::path evidenceDirectory;
};

// ---------------------------------------------------------------------------------------------------------------------------
// One mode on one adapter.
// ---------------------------------------------------------------------------------------------------------------------------
struct AdapterInfo
{
    std::string description;
    UINT vendorId = 0u;
    UINT deviceId = 0u;
    bool software = false;
    std::string driverVersion;
};

struct ModeResult
{
    Mode mode = Mode::HwndSwapChain;
    bool ran  = false;
    std::string notRunReason;
    PresentedCaptureStatus captureStatus = PresentedCaptureStatus::Captured;
    AdapterInfo adapter;
    bool compositionReported = false;
    // Kept for the comparison between the two modes.
    PresentedWindowImage presentedOverBackground; // composed over the window background colour: what the host's own colour shows
    AlphaPlane alpha;
    AlphaSummary alphaSummary;
    CounterTable counters;
    Stats totalUs;
    FringeStats fringes;
    bool clearTypeSetting       = false;
    uint64_t activationsBlocked = 0u;
    uint64_t focusBlocked       = 0u;
    // This mode's sections of the case document, already JSON.
    std::string pixelsJson;
    std::string costJson;
    std::string behaviorJson;
    std::string memoryJson;
    size_t calibrationDifferences = 0u;
    // Behaviour cases that ran, simulated, or did not run (names), for the tests.
    std::vector<std::pair<std::string, std::string>> behaviorStatus;
};

struct ModeContext
{
    const FixtureCase& fixture;
    const PerformanceFixtures::Variant& variant;
    bool hardware;
    Mode mode;
    const Options& options;
    WindowFixtureSession& session;
    ModeResult& result;
    DxUi::WindowHost& host;
    HWND hwnd;
    bool composition;
    float dpi;
    COLORREF windowBackground;
};

// The compositor's capture of the window, over `backdrop` (the window's GDI layer in the print). The capture never renders the
// window: the render count must not move.
[[nodiscard]] PresentedCaptureStatus CapturePresented(ModeContext& m, COLORREF backdrop, PresentedWindowImage& image)
{
    m.session.Frame(); // any posted paint is rendered first: the capture refuses a window with an update region
    const uint64_t renders              = m.host.DebugGetRenderCount();
    const PresentedCaptureStatus status = DxUi::TestSupport::WaitForPresentedFrame(m.hwnd, nullptr, image, backdrop);
    Require(m.host.DebugGetRenderCount() == renders, "capturing the presented image never renders the window");
    return status;
}

// A full render of the window's current state: its back buffer composed over `backdrop` as the compositor composes it.
[[nodiscard]] bool RenderFull(ModeContext& m, PresentedWindowImage& expected)
{
    m.host.Invalidate();
    m.session.Frame();
    DxUi::WindowHostBitmapCapture back;
    if (! m.host.DebugCaptureBitmap(back))
        return false;
    m.session.Frame();
    expected = DxUi::TestSupport::ComposeOverBackdrop(back.widthPx, back.heightPx, back.bgraPixels, m.composition, m.windowBackground);
    return true;
}

// Differences between what the compositor presents after a full render and that render itself: the read-back truth of one state.
// The compositor takes a presented frame some compositor frames after Present returns, and after a restore or a resize a composition
// visual can take longer. The read therefore repeats, one compositor frame (DwmFlush) per read and at most kMaxReads, until the
// image equals the render: `reads` is how many it took, `firstReadDiffering` what the first read still showed.
constexpr uint32_t kMaxReads = 30u;
struct TruthCheck
{
    bool ok                       = false;
    PresentedCaptureStatus status = PresentedCaptureStatus::Captured;
    size_t differing              = 0u; // after the last read
    size_t firstReadDiffering     = 0u;
    uint32_t reads                = 0u;
    UINT width                    = 0u;
    UINT height                   = 0u;
};

[[nodiscard]] TruthCheck CheckPresentedTruth(ModeContext& m)
{
    TruthCheck check;
    PresentedWindowImage expected;
    if (! RenderFull(m, expected))
        return check;
    PresentedWindowImage presented;
    for (uint32_t read = 1u; read <= kMaxReads; ++read)
    {
        check.status = CapturePresented(m, m.windowBackground, presented);
        if (check.status != PresentedCaptureStatus::Captured)
            return check;
        const ImageComparison comparison = CompareImages(presented, expected, nullptr);
        check.ok                         = comparison.sameSize;
        check.differing                  = comparison.differing;
        check.reads                      = read;
        check.width                      = presented.widthPx;
        check.height                     = presented.heightPx;
        if (read == 1u)
            check.firstReadDiffering = comparison.differing;
        if (! comparison.sameSize || comparison.differing == 0u)
            break;
    }
    return check;
}

void WriteTruth(Json& json, const TruthCheck& check)
{
    json.Begin().Field("read", check.ok).Field("captureStatus", DxUi::TestSupport::DescribePresentedCaptureStatus(check.status));
    json.Field("width", check.width).Field("height", check.height).Field("readsUntilThePresentedImageEqualedTheRender", check.reads);
    json.Field("pixelsDifferingOnTheFirstRead", check.firstReadDiffering).Field("pixelsDifferingFromAFullRender", check.differing).End();
}

[[nodiscard]] const char* AntialiasName(D2D1_TEXT_ANTIALIAS_MODE mode) noexcept
{
    if (mode == D2D1_TEXT_ANTIALIAS_MODE_DEFAULT)
        return "DEFAULT";
    if (mode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE)
        return "CLEARTYPE";
    if (mode == D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE)
        return "GRAYSCALE";
    if (mode == D2D1_TEXT_ANTIALIAS_MODE_ALIASED)
        return "ALIASED";
    return "?";
}

void Fail(ModeResult& result, std::string reason)
{
    result.ran          = false;
    result.notRunReason = std::move(reason);
}

// ---------------------------------------------------------------------------------------------------------------------------
// Part 1: pixels.
// ---------------------------------------------------------------------------------------------------------------------------
void MeasurePixels(ModeContext& m)
{
    ModeResult& result = m.result;
    // A frame of the whole client area, read back from the swap chain after it was presented.
    m.host.Invalidate();
    m.session.Frame();
    DxUi::WindowHostBitmapCapture backBuffer;
    Require(m.host.DebugCaptureBitmap(backBuffer), "the host renders and presents a full frame");
    m.session.Frame();

    PresentedWindowImage overBlack;
    PresentedWindowImage overWhite;
    PresentedWindowImage overBackground;
    PresentedCaptureStatus status = CapturePresented(m, RGB(0, 0, 0), overBlack);
    if (status == PresentedCaptureStatus::Captured)
        status = CapturePresented(m, RGB(255, 255, 255), overWhite);
    if (status == PresentedCaptureStatus::Captured)
        status = CapturePresented(m, m.windowBackground, overBackground);
    result.captureStatus = status;
    Json json;
    json.Begin();
    json.Field("presentationModeReported", ModeName(m.host.GetPresentationMode()));
    json.Field("clientPixels", std::format("{}x{}", backBuffer.widthPx, backBuffer.heightPx));
    json.Field("dpi", m.dpi);
    if (status != PresentedCaptureStatus::Captured)
    {
        json.Field("status", "not read").Field("reason", DxUi::TestSupport::DescribePresentedCaptureStatus(status)).End();
        result.pixelsJson = json.Text();
        return;
    }

    // Text antialiasing: the mode the host left on its context, the setting it follows, and the colour fringes in the image.
    BOOL smoothing     = FALSE;
    UINT smoothingType = 0u;
    static_cast<void>(SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0u, &smoothing, 0u));
    static_cast<void>(SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0u, &smoothingType, 0u));
    ID2D1DeviceContext* const dc = m.host.GetDeviceContext();
    wil::com_ptr<IDWriteRenderingParams> bound;
    if (dc)
        dc->GetTextRenderingParams(bound.put());
    const RECT textRegion       = ToPixels(m.fixture.textRegionDip, m.dpi);
    result.fringes              = MeasureFringes(overBackground, textRegion);
    const bool clearTypeSetting = smoothing != FALSE && smoothingType == FE_FONTSMOOTHINGCLEARTYPE;
    result.clearTypeSetting     = clearTypeSetting;
    json.Key("textAntialiasing").Begin();
    json.Field("direct2dTextAntialiasMode", dc ? AntialiasName(dc->GetTextAntialiasMode()) : "no device context");
    json.Field("renderingParametersBound", bound != nullptr);
    json.Field("systemFontSmoothingEnabled", smoothing != FALSE);
    json.Field("systemFontSmoothingType",
               smoothingType == FE_FONTSMOOTHINGCLEARTYPE ? "ClearType" : (smoothingType == FE_FONTSMOOTHINGSTANDARD ? "Standard" : "other"));
    json.Field("expectedFromTargetAndSetting",
               m.composition ? "grayscale (a premultiplied target)"
                             : (clearTypeSetting ? "ClearType (an opaque target and the ClearType setting)" : "grayscale (an opaque target, ClearType off)"));
    json.Key("textRegionPixels")
        .Begin()
        .Field("left", textRegion.left)
        .Field("top", textRegion.top)
        .Field("right", textRegion.right)
        .Field("bottom", textRegion.bottom)
        .End();
    json.Key("textRegionColourFringes")
        .Begin()
        .Field("method",
               "the region's ground is its most frequent colour, the text colour the one farthest from it; a text pixel off the blend segment between them by "
               "more than 6 levels is a subpixel colour fringe")
        .Field("regionPixels", result.fringes.pixels)
        .Field("textPixels", result.fringes.textPixels)
        .Field("pixelsWithColourFringes", result.fringes.offTheLine)
        .Field("groundRgb", std::format("0x{:06X}", result.fringes.ground))
        .Field("textRgb", std::format("0x{:06X}", result.fringes.textColor))
        .End();
    json.End();

    // The alpha each pixel presents, from the two backdrops. Over the window's own background the image is what the user sees.
    result.alpha        = RecoverAlpha(overBlack, overWhite);
    result.alphaSummary = Summarize(result.alpha);
    json.Key("presentedAlpha");
    WriteAlphaSummary(json, result.alphaSummary);
    size_t backOpaque = 0u, backZero = 0u, backPartial = 0u;
    for (size_t index = 3u; index < backBuffer.bgraPixels.size(); index += 4u)
    {
        const uint8_t a = backBuffer.bgraPixels[index];
        (a == 255u ? backOpaque : a == 0u ? backZero : backPartial) += 1u;
    }
    json.Key("backBufferAlphaByte").Begin().Field("255", backOpaque).Field("0", backZero).Field("other", backPartial);
    json.Field("readByPresentation", m.composition).End();

    // Calibration: the presented image over the window background equals the back buffer composed the way the mode composes it.
    const PresentedWindowImage expected =
        DxUi::TestSupport::ComposeOverBackdrop(backBuffer.widthPx, backBuffer.heightPx, backBuffer.bgraPixels, m.composition, m.windowBackground);
    const ImageComparison calibration = CompareImages(overBackground, expected, nullptr);
    result.calibrationDifferences     = calibration.sameSize ? calibration.differing : SIZE_MAX;
    json.Key("captureCalibration")
        .Begin()
        .Field("presentedOverWindowBackgroundVsBackBufferComposed", calibration.sameSize ? calibration.differing : static_cast<size_t>(0u));
    json.Field("sameSize", calibration.sameSize).End();
    json.Field("windowBackgroundRgb",
               std::format("0x{:02X}{:02X}{:02X}", GetRValue(m.windowBackground), GetGValue(m.windowBackground), GetBValue(m.windowBackground)));
    result.presentedOverBackground = std::move(overBackground);

    // Evidence images (not part of the receipt): the three presented images and the alpha plane.
    if (! m.options.evidenceDirectory.empty())
    {
        const std::string prefix = std::format("{}-{}-{}", m.fixture.id, m.hardware ? "hardware" : "warp", m.composition ? "composition" : "hwnd");
        PresentedFrameTests::SaveImage(m.options.evidenceDirectory,
                                       prefix + "-presented-over-window-background",
                                       result.presentedOverBackground.widthPx,
                                       result.presentedOverBackground.heightPx,
                                       result.presentedOverBackground.bgraPixels);
        PresentedFrameTests::SaveImage(
            m.options.evidenceDirectory, prefix + "-presented-over-black", overBlack.widthPx, overBlack.heightPx, overBlack.bgraPixels);
        PresentedFrameTests::SaveImage(
            m.options.evidenceDirectory, prefix + "-presented-over-white", overWhite.widthPx, overWhite.heightPx, overWhite.bgraPixels);
        std::vector<uint8_t> alphaImage(result.alpha.alpha.size() * 4u);
        for (size_t pixel = 0u; pixel < result.alpha.alpha.size(); ++pixel)
        {
            alphaImage[pixel * 4u + 0u] = alphaImage[pixel * 4u + 1u] = alphaImage[pixel * 4u + 2u] = result.alpha.alpha[pixel];
            alphaImage[pixel * 4u + 3u]                                                             = 255u;
        }
        PresentedFrameTests::SaveImage(m.options.evidenceDirectory, prefix + "-presented-alpha", result.alpha.width, result.alpha.height, alphaImage);
    }
    json.End();
    result.pixelsJson = json.Text();
}

// ---------------------------------------------------------------------------------------------------------------------------
// Part 2: cost.
// ---------------------------------------------------------------------------------------------------------------------------
struct FrameMetrics
{
    uint64_t totalUs   = 0u;
    uint64_t renderUs  = 0u;
    uint64_t presentUs = 0u;
    uint32_t frames    = 0u;
};
void CollectFrameMetric(void* context, std::wstring_view metric, uint64_t value) noexcept
{
    auto& metrics = *static_cast<FrameMetrics*>(context);
    if (metric == L"dxui.frame.total_us")
    {
        metrics.totalUs = value;
        ++metrics.frames;
    }
    else if (metric == L"dxui.frame.render_us")
        metrics.renderUs = value;
    else if (metric == L"dxui.frame.present_us")
        metrics.presentUs = value;
}

struct TraceCapture final
{
    std::vector<Trace::Record> records;
    size_t overflow = 0u;
    explicit TraceCapture(size_t capacity)
    {
        records.reserve(capacity);
    }
    static void Sink(void* context, const Trace::Record& record) noexcept
    {
        auto& capture = *static_cast<TraceCapture*>(context);
        if (capture.records.size() == capture.records.capacity())
        {
            ++capture.overflow;
            return;
        }
        capture.records.push_back(record);
    }
};

[[nodiscard]] uint64_t ThreadCycles()
{
    ULONG64 cycles = 0u;
    Require(QueryThreadCycleTime(GetCurrentThread(), &cycles) != FALSE, "thread cycles");
    return cycles;
}

void WriteMemory(Json& json, const MemorySnapshot& memory)
{
    json.Begin().Field("privateBytes", memory.privateBytes).Field("workingSetBytes", memory.workingSetBytes);
    json.Field("videoMemoryApplicable", memory.videoApplicable);
    if (memory.videoApplicable)
        json.Field("localVideoBytes", memory.localBytes).Field("nonLocalVideoBytes", memory.nonLocalBytes);
    json.End();
}

void MeasureCost(ModeContext& m)
{
    ModeResult& result = m.result;
    Json json;
    json.Begin();

    // Counters of full frames (the host invalidated, nothing changed): exactly what one frame of the mode costs in work.
    CounterTable perFrame;
    std::vector<uint64_t> presents;
    for (size_t frame = 0u; frame < m.options.counterFrames; ++frame)
    {
        m.host.Invalidate();
        const DxUi::Detail::WorkCounterScope scope;
        m.session.Frame();
        const DxUi::Detail::WorkCounters delta = scope.Delta();
        AddCounters(perFrame, delta);
        presents.push_back(delta.frame.presents);
    }
    result.counters = perFrame;
    json.Key("fullFrameCounters").Begin().Field("frames", m.options.counterFrames);
    json.Field("framesThatPresented", static_cast<size_t>(std::ranges::count_if(presents, [](uint64_t value) { return value == 1u; })));
    json.Key("perFrame").Begin();
    for (const auto& [name, range] : perFrame)
        if (range.total != 0u)
            json.Key(name).Begin().Field("min", range.minimum).Field("max", range.maximum).Field("total", range.total).End();
    json.End().End();

    // Frame-time breakdown (W0.7's window-host pass): the host's own frame metrics and the UI thread's cycles around the paint.
    FrameMetrics metrics;
    DxUi::SetFrameMetricSink(&CollectFrameMetric, &metrics);
    const auto clearSink = wil::scope_exit([] { DxUi::SetFrameMetricSink(nullptr, nullptr); });
    std::vector<double> render, present, total, cycles;
    size_t withoutMetrics = 0u;
    for (size_t frame = 0u; frame < m.options.timingFrames + 10u; ++frame)
    {
        metrics = {};
        m.host.Invalidate();
        const uint64_t before = ThreadCycles();
        m.session.Frame();
        const uint64_t spent = ThreadCycles() - before;
        static_cast<void>(DwmFlush());
        if (frame < 10u)
            continue; // the first frames after the counter pass settle the pacer
        if (metrics.frames != 1u)
        {
            ++withoutMetrics;
            continue;
        }
        render.push_back(static_cast<double>(metrics.renderUs));
        present.push_back(static_cast<double>(metrics.presentUs));
        total.push_back(static_cast<double>(metrics.totalUs));
        cycles.push_back(static_cast<double>(spent));
    }
    DxUi::SetFrameMetricSink(nullptr, nullptr);
    result.totalUs = Summarize(total);
    json.Key("frameTime").Begin();
    json.Field("judged", m.options.judgedTiming);
    json.Field("status", m.options.judgedTiming ? "judged: the caller attests a quiet machine" : "not judged, machine busy");
    json.Field("note", "wall time of the host's own WM_PAINT (UpdateWindow) with DwmFlush after each frame; microseconds, cycles of the UI thread");
    json.Field("frames", total.size()).Field("framesWithoutMetrics", withoutMetrics);
    json.Key("renderUs");
    WriteStats(json, Summarize(render));
    json.Key("presentUs");
    WriteStats(json, Summarize(present));
    json.Key("totalUs");
    WriteStats(json, result.totalUs);
    json.Key("uiThreadCycles");
    WriteStats(json, Summarize(cycles));
    json.End();

    // Input to present (W0.8): one wheel notch over the scrolling control per input, each taking a frame credit first so the
    // host dispatches it at once; the trace provider's records give the input-received and present events and their times.
    TraceCapture capture(8192u);
    std::vector<double> inputToPresentUs;
    size_t inputEvents = 0u, presentEvents = 0u, withoutPresent = 0u;
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    {
        Trace::InstallSinkForTest(&TraceCapture::Sink, &capture);
        const auto removeSink = wil::scope_exit([] { Trace::InstallSinkForTest(nullptr, nullptr); });
        const float scale     = m.dpi / 96.0f;
        POINT screen{static_cast<LONG>(std::lround(m.fixture.wheelPointDip.x * scale)), static_cast<LONG>(std::lround(m.fixture.wheelPointDip.y * scale))};
        static_cast<void>(ClientToScreen(m.hwnd, &screen));
        for (size_t input = 0u; input < m.options.inputCount; ++input)
        {
            Require(AwaitFrameCreditForTest(m.host), "the host takes a frame credit before an input");
            SendMessageW(m.hwnd,
                         WM_MOUSEWHEEL,
                         MAKEWPARAM(0u, static_cast<short>(-WHEEL_DELTA)),
                         MAKELPARAM(static_cast<short>(screen.x), static_cast<short>(screen.y)));
            m.session.Frame();
        }
    }
    for (size_t index = 0u; index < capture.records.size(); ++index)
    {
        const Trace::Record& record = capture.records[index];
        if (record.event == Trace::Event::Present && SUCCEEDED(record.result))
            ++presentEvents;
        if (record.event != Trace::Event::InputReceived)
            continue;
        ++inputEvents;
        const auto shown = std::find_if(capture.records.begin() + static_cast<std::ptrdiff_t>(index), capture.records.end(), [&](const Trace::Record& later) {
            return later.event == Trace::Event::Present && SUCCEEDED(later.result) && later.input >= record.input;
        });
        if (shown == capture.records.end())
        {
            ++withoutPresent;
            continue;
        }
        inputToPresentUs.push_back(static_cast<double>(shown->qpcTimestamp - record.qpcTimestamp) * 1'000'000.0 / static_cast<double>(frequency.QuadPart));
    }
    DWM_TIMING_INFO timing{};
    timing.cbSize          = sizeof(timing);
    const double refreshUs = SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) && frequency.QuadPart != 0
                                 ? static_cast<double>(timing.qpcRefreshPeriod) * 1'000'000.0 / static_cast<double>(frequency.QuadPart)
                                 : 0.0;
    json.Key("inputToPresent").Begin();
    json.Field("judged", m.options.judgedTiming);
    json.Field("status", m.options.judgedTiming ? "judged: the caller attests a quiet machine" : "not judged, machine busy");
    json.Field("input", "one WM_MOUSEWHEEL notch over the scrolling control per input, sent with a frame credit held");
    json.Field("inputs", m.options.inputCount).Field("inputReceivedEvents", inputEvents).Field("presentEvents", presentEvents);
    json.Field("inputsWithoutPresent", withoutPresent).Field("traceOverflow", capture.overflow);
    json.Field("refreshPeriodUs", refreshUs);
    json.Key("microseconds");
    WriteStats(json, Summarize(inputToPresentUs));
    json.End();

    json.End();
    result.costJson = json.Text();
}

// ---------------------------------------------------------------------------------------------------------------------------
// Part 3: behaviour.
// ---------------------------------------------------------------------------------------------------------------------------
void WriteWork(Json& json, const DxUi::Detail::WorkCounters& work)
{
    json.Begin()
        .Field("renders", work.frame.renders)
        .Field("presents", work.frame.presents)
        .Field("presentsWithAStatusOtherThanOk", work.frame.presentStatuses)
        .Field("presentsWithoutCredit", work.frame.presentsWithoutCredit)
        .Field("swapChainsCreated", work.frame.swapChains)
        .Field("graphicsResourcesCreated", DxUi::Detail::TotalCreated(work))
        .End();
}

[[nodiscard]] std::string HResultText(HRESULT value)
{
    return std::format("0x{:08X}", static_cast<uint32_t>(value));
}

class BehaviorLog final
{
public:
    BehaviorLog(Json& json, ModeResult& result) noexcept : _json(json), _result(result)
    {
    }
    void Begin(std::string name, std::string status, std::string method)
    {
        _result.behaviorStatus.emplace_back(name, status);
        _json.Begin().Field("name", name).Field("status", status).Field("method", method);
    }
    void End()
    {
        _json.End();
    }

private:
    Json& _json;
    ModeResult& _result;
};

// The compositor's image of the window without letting the host paint first (a resize leaves an update region): the region is
// validated, which only removes the pending WM_PAINT; the caller invalidates and paints afterwards.
[[nodiscard]] PresentedCaptureStatus CapturePresentedUnpainted(ModeContext& m, COLORREF backdrop, PresentedWindowImage& image)
{
    static_cast<void>(ValidateRect(m.hwnd, nullptr));
    const uint64_t renders              = m.host.DebugGetRenderCount();
    const PresentedCaptureStatus status = DxUi::TestSupport::WaitForPresentedFrame(m.hwnd, nullptr, image, backdrop);
    Require(m.host.DebugGetRenderCount() == renders, "capturing the presented image never renders the window");
    return status;
}

// What the compositor shows between a resize and the host's next present, against the last image it showed at the old size.
void WriteInterimFrame(Json& json, const PresentedWindowImage& interim, const PresentedWindowImage& previous, COLORREF backdrop, SIZE swapChain)
{
    const uint8_t bgr[3]{GetBValue(backdrop), GetGValue(backdrop), GetRValue(backdrop)};
    size_t backdropPixels = 0u, backdropOutsideOldArea = 0u, blackPixels = 0u, sameAsPrevious = 0u;
    Box backdropBox;
    for (UINT y = 0u; y < interim.heightPx; ++y)
        for (UINT x = 0u; x < interim.widthPx; ++x)
        {
            const uint8_t* at = &interim.bgraPixels[(static_cast<size_t>(y) * interim.widthPx + x) * 4u];
            if (std::memcmp(at, bgr, 3u) == 0)
            {
                ++backdropPixels;
                backdropOutsideOldArea += (x >= previous.widthPx || y >= previous.heightPx) ? 1u : 0u;
                backdropBox.Add(static_cast<int>(x), static_cast<int>(y));
            }
            else if (at[0] == 0u && at[1] == 0u && at[2] == 0u)
                ++blackPixels;
            if (x < previous.widthPx && y < previous.heightPx &&
                std::memcmp(at, &previous.bgraPixels[(static_cast<size_t>(y) * previous.widthPx + x) * 4u], 3u) == 0)
                ++sameAsPrevious;
        }
    json.Begin()
        .Field("clientPixels", std::format("{}x{}", interim.widthPx, interim.heightPx))
        .Field("previousClientPixels", std::format("{}x{}", previous.widthPx, previous.heightPx))
        .Field("swapChainPixelsAtCapture", std::format("{}x{}", swapChain.cx, swapChain.cy))
        .Field("pixelsShowingTheBackdrop", backdropPixels)
        .Field("pixelsShowingTheBackdropOutsideTheOldClientArea", backdropOutsideOldArea)
        .Field("pixelsBlack", blackPixels)
        .Field("pixelsEqualToThePreviousImageAtTheSameCoordinates", sameAsPrevious);
    json.Key("backdropBox");
    WriteBox(json, backdropBox);
    json.End();
}

void MeasureLiveResize(ModeContext& m, BehaviorLog& log, Json& json)
{
    log.Begin("liveResize",
              "run",
              "WindowFixtureSession::Resize (SetWindowPos on a non-activating off-screen window): two interim reads (grow, shrink) with the update region "
              "validated so the "
              "compositor shows what the host last presented, then a 12-step sweep, one frame per step");
    const UINT w0                     = m.variant.width;
    const UINT h0                     = m.variant.height;
    const PresentedWindowImage before = m.result.presentedOverBackground;
    const COLORREF magenta            = RGB(255, 0, 255);

    json.Key("interimGrow");
    m.session.Resize(w0 + 160u, h0 + 90u, m.dpi);
    PresentedWindowImage interim;
    PresentedCaptureStatus status = CapturePresentedUnpainted(m, magenta, interim);
    if (status == PresentedCaptureStatus::Captured)
        WriteInterimFrame(json, interim, before, magenta, m.host.DebugGetSwapChainSizeForTest());
    else
        json.Str(DxUi::TestSupport::DescribePresentedCaptureStatus(status));
    m.host.Invalidate();
    m.session.Frame();

    json.Key("interimShrink");
    m.session.Resize(w0 - 160u, h0 - 90u, m.dpi);
    status = CapturePresentedUnpainted(m, magenta, interim);
    if (status == PresentedCaptureStatus::Captured)
        WriteInterimFrame(json, interim, before, magenta, m.host.DebugGetSwapChainSizeForTest());
    else
        json.Str(DxUi::TestSupport::DescribePresentedCaptureStatus(status));
    m.host.Invalidate();
    m.session.Frame();

    constexpr std::array<double, 12> factors{0.9, 0.8, 0.7, 0.6, 0.7, 0.85, 1.0, 1.1, 1.2, 1.1, 1.0, 0.95};
    const uint64_t resizesBefore        = m.host.DebugGetResizeCount();
    const uint64_t resizeFailuresBefore = m.host.DebugGetResizeFailureCount();
    size_t stepsWithoutRender = 0u, stepsWithRenderButNoPresent = 0u, sizeMismatches = 0u, nonOkPresents = 0u;
    const DxUi::Detail::WorkCounterScope sweep;
    json.Key("sweep").Begin().Key("steps").Array();
    for (const double factor : factors)
    {
        const UINT width  = static_cast<UINT>(std::lround(static_cast<double>(w0) * factor));
        const UINT height = static_cast<UINT>(std::lround(static_cast<double>(h0) * factor));
        const DxUi::Detail::WorkCounterScope step;
        m.session.Resize(width, height, m.dpi);
        m.session.Frame();
        const DxUi::Detail::WorkCounters delta = step.Delta();
        const SIZE chain                       = m.host.DebugGetSwapChainSizeForTest();
        stepsWithoutRender += delta.frame.renders == 0u ? 1u : 0u;
        stepsWithRenderButNoPresent += delta.frame.renders != 0u && delta.frame.presents == 0u ? 1u : 0u;
        sizeMismatches += (chain.cx != static_cast<LONG>(width) || chain.cy != static_cast<LONG>(height)) ? 1u : 0u;
        nonOkPresents += m.host.DebugGetLastPresentResult() != S_OK ? 1u : 0u;
        json.Begin().Field("client", std::format("{}x{}", width, height)).Field("renders", delta.frame.renders).Field("presents", delta.frame.presents);
        json.Field("swapChainsCreated", delta.frame.swapChains).Field("swapChainPixels", std::format("{}x{}", chain.cx, chain.cy)).End();
    }
    json.EndArray();
    json.Key("total");
    WriteWork(json, sweep.Delta());
    json.Field("stepsWithoutARender", stepsWithoutRender)
        .Field("stepsWithARenderAndNoPresent", stepsWithRenderButNoPresent)
        .Field("stepsWhereTheSwapChainWasNotTheClientSize", sizeMismatches)
        .Field("stepsWhoseLastPresentWasNotOk", nonOkPresents)
        .Field("hostResizes", m.host.DebugGetResizeCount() - resizesBefore)
        .Field("resizeBuffersFailures", m.host.DebugGetResizeFailureCount() - resizeFailuresBefore)
        .End();
    json.Key("afterTheSweep");
    WriteTruth(json, CheckPresentedTruth(m));
    log.End();
}

void MeasureMinimize(ModeContext& m, BehaviorLog& log, Json& json)
{
    log.Begin("minimizeAndRestore", "run", "ShowWindow(SW_SHOWMINNOACTIVE), five invalidate-and-pump rounds, ShowWindow(SW_SHOWNOACTIVATE), three rounds");
    PresentedWindowImage before;
    const bool read = RenderFull(m, before);
    ShowWindow(m.hwnd, SW_SHOWMINNOACTIVE);
    m.session.Frame();
    json.Field("windowIsIconic", IsIconic(m.hwnd) != FALSE);
    json.Field("beforeRead", read);
    {
        const DxUi::Detail::WorkCounterScope scope;
        for (int round = 0; round < 5; ++round)
        {
            m.host.Invalidate();
            m.session.Frame();
        }
        json.Key("whileMinimizedFiveInvalidations");
        WriteWork(json, scope.Delta());
    }
    ShowWindow(m.hwnd, SW_SHOWNOACTIVATE);
    m.session.Frame();
    json.Field("windowIsIconicAfterRestore", IsIconic(m.hwnd) != FALSE);
    {
        const DxUi::Detail::WorkCounterScope scope;
        for (int round = 0; round < 3; ++round)
        {
            m.host.Invalidate();
            m.session.Frame();
        }
        json.Key("afterRestoreThreeInvalidations");
        WriteWork(json, scope.Delta());
    }
    json.Field("lastPresentResult", HResultText(m.host.DebugGetLastPresentResult()));
    json.Key("afterRestore");
    WriteTruth(json, CheckPresentedTruth(m));
    PresentedWindowImage after;
    if (CapturePresented(m, m.windowBackground, after) == PresentedCaptureStatus::Captured && read)
    {
        const ImageComparison same = CompareImages(after, before, nullptr);
        json.Field("pixelsDifferingFromBeforeTheMinimize", same.sameSize ? same.differing : static_cast<size_t>(0u));
    }
    log.End();
}

void MeasureOcclusion(ModeContext& m, BehaviorLog& log, Json& json)
{
    log.Begin("occlusion",
              "run",
              "an opaque WS_POPUP | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW window shown over the host's window without activation (SetWindowPos HWND_TOP, "
              "SWP_NOACTIVATE), "
              "then removed; five rounds covered, three uncovered");
    RECT rect{};
    GetWindowRect(m.hwnd, &rect);
    json.Field("presentResultBeforeCovering", HResultText(m.host.DebugGetLastPresentResult()));
    HWND cover = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                 L"STATIC",
                                 L"",
                                 WS_POPUP,
                                 rect.left,
                                 rect.top,
                                 rect.right - rect.left,
                                 rect.bottom - rect.top,
                                 nullptr,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 nullptr);
    json.Field("coveringWindowCreated", cover != nullptr);
    if (! cover)
    {
        log.End();
        return;
    }
    static_cast<void>(SetWindowPos(cover, HWND_TOP, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW));
    static_cast<void>(DwmFlush());
    {
        const DxUi::Detail::WorkCounterScope scope;
        std::vector<std::string> results;
        for (int round = 0; round < 5; ++round)
        {
            m.host.Invalidate();
            m.session.Frame();
            static_cast<void>(DwmFlush());
            results.push_back(HResultText(m.host.DebugGetLastPresentResult()));
        }
        json.Key("whileCovered");
        WriteWork(json, scope.Delta());
        json.Key("presentResultsWhileCovered").Array();
        for (const std::string& result : results)
            json.Str(result);
        json.EndArray();
    }
    DestroyWindow(cover);
    static_cast<void>(DwmFlush());
    {
        const DxUi::Detail::WorkCounterScope scope;
        for (int round = 0; round < 3; ++round)
        {
            m.host.Invalidate();
            m.session.Frame();
        }
        json.Key("afterUncovering");
        WriteWork(json, scope.Delta());
    }
    json.Key("afterUncoveringRead");
    WriteTruth(json, CheckPresentedTruth(m));
    log.End();
}

void MeasureDeviceLoss(ModeContext& m, BehaviorLog& log, Json& json)
{
    log.Begin("deviceLoss", "run", "ControlHost::DebugSimulateDeviceLoss (the seam the host's own tests use), then invalidate-and-pump until a frame presents");
    const uint64_t generation = m.host.GetResourceGeneration();
    const DxUi::Detail::WorkCounterScope scope;
    size_t rounds     = 0u;
    uint64_t presents = 0u;
    m.host.DebugSimulateDeviceLoss();
    while (rounds < 20u && presents == 0u)
    {
        m.host.Invalidate();
        m.session.Frame();
        ++rounds;
        presents = scope.Delta().frame.presents;
    }
    json.Field("roundsUntilAFramePresented", rounds);
    json.Field("resourceGenerationChanged", m.host.GetResourceGeneration() != generation);
    json.Field("lastPresentResult", HResultText(m.host.DebugGetLastPresentResult()));
    json.Field("pacedAfterRecovery", m.host.DebugGetFramePacerStateForTest().paced);
    json.Key("work");
    WriteWork(json, scope.Delta());
    json.Key("afterRecovery");
    WriteTruth(json, CheckPresentedTruth(m));
    log.End();
}

void MeasureDpiChange(ModeContext& m, BehaviorLog& log, Json& json)
{
    log.Begin("dpiChangeSimulated",
              "simulated",
              "NOT a monitor move: this program delivers WM_DPICHANGED (96 to 144 and back, the client area scaled with it) to the host through the same seam "
              "the performance "
              "fixtures use for a DPI change; no monitor, no system DPI virtualization and no DWM scale change is involved");
    const UINT w0 = m.variant.width;
    const UINT h0 = m.variant.height;
    {
        const DxUi::Detail::WorkCounterScope scope;
        m.session.Resize(w0 * 3u / 2u, h0 * 3u / 2u, 144.0f);
        m.session.Frame();
        json.Field("hostDpiAfter", m.host.GetDpi());
        json.Key("to144Work");
        WriteWork(json, scope.Delta());
        json.Key("to144");
        WriteTruth(json, CheckPresentedTruth(m));
        ID2D1DeviceContext* const dc = m.host.GetDeviceContext();
        json.Field("textAntialiasModeAt144", dc ? AntialiasName(dc->GetTextAntialiasMode()) : "no device context");
    }
    {
        const DxUi::Detail::WorkCounterScope scope;
        m.session.Resize(w0, h0, m.dpi);
        m.session.Frame();
        json.Field("hostDpiBack", m.host.GetDpi());
        json.Key("backTo96Work");
        WriteWork(json, scope.Delta());
        json.Key("backTo96");
        WriteTruth(json, CheckPresentedTruth(m));
    }
    log.End();
}

// A host on a child window of a top-level window, as NativeMenuBar's host is: ControlHost::Attach with the mode under test.
struct ChildHostWindow final
{
    HWND parent = nullptr;
    HWND child  = nullptr;
    DxUi::ControlHost host;
    ~ChildHostWindow()
    {
        host.Detach();
        if (child)
            DestroyWindow(child);
        if (parent)
            DestroyWindow(parent);
    }
    static LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<const CREATESTRUCTW*>(lParam)->lpCreateParams));
            return TRUE;
        }
        auto* self = reinterpret_cast<ChildHostWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
        {
            bool handled         = false;
            const LRESULT result = self->host.HandleMessage(hwnd, message, wParam, lParam, handled);
            if (message == WM_NCDESTROY)
            {
                self->host.Detach();
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            if (handled)
                return result;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
};

void MeasureChildHost(ModeContext& m, BehaviorLog& log, Json& json)
{
    log.Begin("childHwnd",
              "run",
              "ControlHost::Attach(child, {presentationMode}) on a WS_CHILD window of a non-activating top-level window (the way NativeMenuBar attaches its "
              "host), a card with "
              "text, a toggle, a slider and a progress bar; presented image of the child over black and white, and of the parent over a marker colour");
    static const ATOM childClass = []() noexcept
    {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc   = &ChildHostWindow::Procedure;
        windowClass.hInstance     = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"DxUiTests.PresentationModeChild";
        return RegisterClassW(&windowClass);
    }();
    static const ATOM parentClass = []() noexcept
    {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc   = &DefWindowProcW;
        windowClass.hInstance     = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"DxUiTests.PresentationModeParent";
        return RegisterClassW(&windowClass);
    }();
    if (childClass == 0u || parentClass == 0u)
    {
        json.Field("status", "window classes not registered");
        log.End();
        return;
    }
    ChildHostWindow window;
    constexpr int kParentW = 360, kParentH = 260, kChildX = 24, kChildY = 20, kChildW = 312, kChildH = 100;
    window.parent = CreateWindowExW(WS_EX_NOACTIVATE,
                                    L"DxUiTests.PresentationModeParent",
                                    L"",
                                    WS_POPUP,
                                    -32000,
                                    -32000,
                                    kParentW,
                                    kParentH,
                                    nullptr,
                                    nullptr,
                                    GetModuleHandleW(nullptr),
                                    nullptr);
    window.child  = window.parent ? CreateWindowExW(0,
                                                    L"DxUiTests.PresentationModeChild",
                                                    L"",
                                                    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                                    kChildX,
                                                    kChildY,
                                                    kChildW,
                                                    kChildH,
                                                    window.parent,
                                                    nullptr,
                                                    GetModuleHandleW(nullptr),
                                                    &window)
                                  : nullptr;
    json.Field("windowsCreated", window.parent != nullptr && window.child != nullptr);
    if (! window.child)
    {
        log.End();
        return;
    }
    const bool attached = window.host.Attach(window.child, DxUi::ControlHost::AttachOptions{.presentationMode = m.mode, .forceWarp = ! m.hardware});
    json.Field("attached", attached);
    json.Field("attachLastError", attached ? 0u : static_cast<unsigned>(GetLastError()));
    if (! attached)
    {
        log.End();
        return;
    }
    auto theme          = DxUi::MakeDefaultThemePalette(true);
    theme.reducedMotion = true;
    window.host.SetTheme(theme);
    auto root = std::make_unique<DxUi::Panel>();
    {
        auto* card = root->AddChild<DxUi::CardPanel>();
        card->SetBounds(D2D1::RectF(8.0f, 8.0f, 304.0f, 92.0f));
        card->AddChild<DxUi::Label>(L"Child HWND host")->SetBounds(D2D1::RectF(16.0f, 10.0f, 163.0f, 34.0f));
        card->AddChild<DxUi::Toggle>(L"Enabled")->SetBounds(D2D1::RectF(174.0f, 10.0f, 296.0f, 38.0f));
        card->AddChild<DxUi::Slider>()->SetBounds(D2D1::RectF(16.0f, 42.0f, 296.0f, 66.0f));
        auto* bar = card->AddChild<DxUi::ProgressBar>();
        bar->SetBounds(D2D1::RectF(16.0f, 70.0f, 296.0f, 82.0f));
        bar->SetValue(40.0);
    }
    window.host.SetRoot(std::move(root));
    ShowWindow(window.parent, SW_SHOWNOACTIVATE);
    ShowWindow(window.child, SW_SHOWNOACTIVATE);
    const DxUi::Detail::WorkCounterScope scope;
    for (int round = 0; round < 4; ++round)
    {
        Require(AwaitFrameCreditForTest(window.host), "the child host takes a frame credit");
        window.host.Invalidate();
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, WM_TIMER - 1u, PM_REMOVE) != FALSE)
            static_cast<void>(DispatchQueuedMessageForTest(message));
        static_cast<void>(UpdateWindow(window.child));
    }
    json.Field("hostReportsMode", ModeName(window.host.GetPresentationMode()));
    json.Field("lastPresentResult", HResultText(window.host.DebugGetLastPresentResult()));
    json.Key("work");
    WriteWork(json, scope.Delta());
    PresentedWindowImage overBlack, overWhite, parentView;
    PresentedCaptureStatus status = DxUi::TestSupport::WaitForPresentedFrame(window.child, nullptr, overBlack, RGB(0, 0, 0));
    if (status == PresentedCaptureStatus::Captured)
        status = DxUi::TestSupport::WaitForPresentedFrame(window.child, nullptr, overWhite, RGB(255, 255, 255));
    json.Field("childCaptureStatus", DxUi::TestSupport::DescribePresentedCaptureStatus(status));
    if (status == PresentedCaptureStatus::Captured)
    {
        json.Field("childClientPixels", std::format("{}x{}", overBlack.widthPx, overBlack.heightPx));
        json.Key("childPresentedAlpha");
        WriteAlphaSummary(json, Summarize(RecoverAlpha(overBlack, overWhite)));
        size_t content = 0u;
        for (size_t index = 0u; index + 3u < overBlack.bgraPixels.size(); index += 4u)
            content += (overBlack.bgraPixels[index] | overBlack.bgraPixels[index + 1u] | overBlack.bgraPixels[index + 2u]) != 0u ? 1u : 0u;
        json.Field("childPixelsNotBlackOverBlack", content);
    }
    const COLORREF marker               = RGB(255, 0, 255);
    const PresentedCaptureStatus parent = DxUi::TestSupport::WaitForPresentedFrame(window.parent, nullptr, parentView, marker);
    json.Field("parentCaptureStatus", DxUi::TestSupport::DescribePresentedCaptureStatus(parent));
    if (parent == PresentedCaptureStatus::Captured)
    {
        size_t childAreaMarker = 0u, childAreaOther = 0u;
        for (int y = kChildY; y < kChildY + kChildH && y < static_cast<int>(parentView.heightPx); ++y)
            for (int x = kChildX; x < kChildX + kChildW && x < static_cast<int>(parentView.widthPx); ++x)
            {
                const uint8_t* at = &parentView.bgraPixels[(static_cast<size_t>(y) * parentView.widthPx + static_cast<size_t>(x)) * 4u];
                (at[0] == 255u && at[1] == 0u && at[2] == 255u ? childAreaMarker : childAreaOther) += 1u;
            }
        json.Field("parentPrintPixelsInChildAreaShowingTheMarkerColour", childAreaMarker);
        json.Field("parentPrintPixelsInChildAreaShowingOtherColours", childAreaOther);
    }
    log.End();
}

void MeasureNotRun(ModeContext& m, BehaviorLog& log)
{
    log.Begin("monitorMoveThatChangesDpi",
              "not run",
              "cannot be driven from this program: it needs a second monitor of another scale and moves a window across them");
    log.End();
    log.Begin("remoteSession", "not run", "cannot be driven from this program: it needs a Remote Desktop session");
    static_cast<void>(m);
    log.End();
}

void MeasureBehavior(ModeContext& m)
{
    Json json;
    json.Begin();
    json.Field("remoteSessionAtRun", GetSystemMetrics(SM_REMOTESESSION) != 0);
    json.Key("cases").Array();
    BehaviorLog log(json, m.result);
    MeasureLiveResize(m, log, json);
    MeasureMinimize(m, log, json);
    MeasureOcclusion(m, log, json);
    MeasureDeviceLoss(m, log, json);
    MeasureDpiChange(m, log, json);
    MeasureChildHost(m, log, json);
    MeasureNotRun(m, log);
    json.EndArray().End();
    m.result.behaviorJson = json.Text();
}

// ---------------------------------------------------------------------------------------------------------------------------
// One mode, one fixture, one adapter, on a fresh thread.
// ---------------------------------------------------------------------------------------------------------------------------
void WriteMemoryTimeline(Json& json, const std::vector<std::pair<std::string, MemorySnapshot>>& snapshots)
{
    json.Begin().Key("snapshots").Array();
    for (const auto& [name, snapshot] : snapshots)
    {
        json.Begin().Field("at", name);
        json.Key("memory");
        WriteMemory(json, snapshot);
        json.End();
    }
    json.EndArray();
    if (snapshots.size() >= 4u)
    {
        const MemorySnapshot& before = snapshots[0].second;
        const MemorySnapshot& frames = snapshots[2].second;
        json.Key("growthFromBeforeTheHostToAfterTheFrames")
            .Begin()
            .Field("privateBytes", static_cast<double>(frames.privateBytes) - static_cast<double>(before.privateBytes))
            .Field("workingSetBytes", static_cast<double>(frames.workingSetBytes) - static_cast<double>(before.workingSetBytes));
        if (before.videoApplicable && frames.videoApplicable)
            json.Field("localVideoBytes", static_cast<double>(frames.localBytes) - static_cast<double>(before.localBytes))
                .Field("nonLocalVideoBytes", static_cast<double>(frames.nonLocalBytes) - static_cast<double>(before.nonLocalBytes));
        json.End();
    }
    json.End();
}

[[nodiscard]] ModeResult RunMode(const FixtureCase& fixture, bool hardware, Mode mode, const Options& options)
{
    ModeResult result;
    result.mode = mode;
    std::exception_ptr failure;
    std::jthread worker([&]
    {
        // Runner assertions cross this thread-entry boundary; named failures are relayed after joining.
        try
        {
            const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
            DxUi::TestSupport::ScopedWindowActivationBlocker blocker;
            Require(blocker.Start(), "the presentation-mode comparison cannot activate or focus desktop windows");
            const DxUi::TestSupport::ScopedPerMonitorV2Awareness awareness;
            Require(awareness.Applied(), "the comparison window is created per-monitor DPI aware");
            if (! DxUi::TestSupport::HasInteractiveWindowStation())
            {
                Fail(result, "no interactive window station: there is no compositor to read");
                return;
            }
            std::vector<std::pair<std::string, MemorySnapshot>> snapshots;
            const wil::com_ptr<IDXGIAdapter3> adapter3 = OpenAdapterForMemory(hardware);
            snapshots.emplace_back("beforeTheHost", TakeMemory(adapter3.get()));
            const PerformanceFixtures::Variant& variant = fixture.definition->variants.front();
            {
                auto session                           = OpenWindowFixtureSession(*fixture.definition, variant, ! hardware, mode);
                DxUi::WindowHost& host                 = session->Host();
                const PerformanceFixtures::Phase& warm = fixture.definition->phases.front();
                for (size_t step = 0u; step < warm.steps; ++step)
                {
                    session->Step(0u, step);
                    session->Frame();
                }
                host.Invalidate();
                session->Frame();
                const PresentedFrameTests::AdapterIdentity identity = PresentedFrameTests::ReadAdapterIdentity(host);
                result.adapter = AdapterInfo{identity.description, identity.vendorId, identity.deviceId, identity.software, identity.driverVersion};
                if (hardware && identity.software)
                {
                    Fail(result, "the default adapter is a software adapter: no hardware Direct3D adapter");
                    return;
                }
                Require(! (! hardware && ! identity.software), "the WARP case renders on a software adapter");
                snapshots.emplace_back("afterTheSceneIsBuiltAndWarm", TakeMemory(adapter3.get()));
                ModeContext context{fixture,
                                    variant,
                                    hardware,
                                    mode,
                                    options,
                                    *session,
                                    result,
                                    host,
                                    session->Hwnd(),
                                    mode == Mode::CompositionSwapChain,
                                    variant.dpi,
                                    ToColorRef(host.GetTheme().windowBackground)};
                result.compositionReported = host.GetPresentationMode() == Mode::CompositionSwapChain;
                MeasurePixels(context);
                MeasureCost(context);
                snapshots.emplace_back("afterTheFrames", TakeMemory(adapter3.get()));
                MeasureBehavior(context);
                snapshots.emplace_back("afterTheBehaviourCases", TakeMemory(adapter3.get()));
                result.ran = true;
            }
            snapshots.emplace_back("afterTheHostIsDestroyed", TakeMemory(adapter3.get()));
            Json memory;
            WriteMemoryTimeline(memory, snapshots);
            result.memoryJson         = memory.Text();
            result.activationsBlocked = blocker.BlockedActivationCount();
            result.focusBlocked       = blocker.BlockedFocusCount();
        }
        catch (const std::exception&)
        {
            failure = std::current_exception(); // Test-only cross-thread result.
        }
    });
    worker.join();
    if (failure)
        std::rethrow_exception(failure);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Part 4: scope. Every host a consumer creates without naming a mode, and where the default is chosen.
// ---------------------------------------------------------------------------------------------------------------------------
struct Anchor
{
    std::string_view file;
    std::string_view text; // a distinctive substring of the line
};

struct ScopeEntry
{
    std::string_view host;
    std::string_view how;
    std::string_view modeToday;
    bool namesTheMode;
    std::vector<Anchor> anchors;
};

[[nodiscard]] const std::vector<ScopeEntry>& ScopeEntries()
{
    static const std::vector<ScopeEntry> entries{
        {"ControlHost::Attach(HWND hwnd): a consumer's own window, any top-level or child HWND, through the one-argument overload",
         "the overload forwards AttachOptions{}, whose presentationMode member defaults to HwndSwapChain",
         "HwndSwapChain",
         false,
         {{"src/Controls/DxUi.WindowHost.cpp", "return Attach(hwnd, AttachOptions{});"},
          {"include/DxUi/DxUi.h", "PresentationMode presentationMode = PresentationMode::HwndSwapChain;"}}},
        {"ControlHost::Attach(HWND hwnd, const AttachOptions&) with AttachOptions that set forceWarp or batchInitialRootChildren but not presentationMode",
         "designated initializers leave presentationMode at its default member initializer",
         "HwndSwapChain",
         false,
         {{"include/DxUi/DxUi.h", "PresentationMode presentationMode = PresentationMode::HwndSwapChain;"},
          {"src/Controls/DxUi.WindowHost.cpp", "= options.presentationMode;"}}},
        {"A ControlHost that is never attached",
         "the host's own member is initialized to the same default, and a detached host creates no swap chain",
         "HwndSwapChain (no swap chain exists)",
         false,
         {{"include/DxUi/DxUi.h", "PresentationMode _presentationMode = PresentationMode::HwndSwapChain;"}}},
        {"NativeMenuBar host (DxUi::NativeMenuBar, include/DxUi/NativeMenuInterop.h): a WS_CHILD window the library creates under the application's owner "
         "window",
         "Attach(hwnd) with no options",
         "HwndSwapChain",
         false,
         {{"src/Controls/NativeMenuInterop.cpp", "! _host.Attach(hwnd)"},
          {"src/Controls/NativeMenuInterop.cpp", "WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP"}}},
        {"Context-menu and menu-flyout popup windows (MenuPopup): top-level popups the library creates with WS_EX_NOREDIRECTIONBITMAP",
         "names its mode: CompositionSwapChain",
         "CompositionSwapChain",
         true,
         {{"src/Controls/DxUi.Menu.cpp", ".presentationMode = ControlHost::PresentationMode::CompositionSwapChain"}}},
        {"EmbeddedHost (the application's own device and surface)",
         "owns no swap chain and no window; ControlHost::Attach refuses a host that is embedded, so no mode applies",
         "none",
         false,
         {{"src/Rendering/Embedded.cpp", "_host._embedded"}, {"src/Controls/DxUi.WindowHost.cpp", "if (_embedded)"}}},
        {"The test window of every WindowHost test, fixture and harness capture (AttachedHostWindow)",
         "its constructor's presentationMode argument defaults to HwndSwapChain; a test that wants composition says so",
         "HwndSwapChain",
         false,
         {{"Tests/Controls/Controls.Tests.DxUiTestHelpers.h",
           "explicit AttachedHostWindow(DxUi::WindowHost::PresentationMode presentationMode = DxUi::WindowHost::PresentationMode::HwndSwapChain,"}}},
    };
    return entries;
}

// The line of the file that holds `text`, 0 when none does.
[[nodiscard]] size_t FindAnchorLine(const std::filesystem::path& root, const Anchor& anchor)
{
    std::ifstream input(root / std::filesystem::path(anchor.file));
    std::string line;
    for (size_t number = 1u; std::getline(input, line); ++number)
        if (line.find(anchor.text) != std::string::npos)
            return number;
    return 0u;
}

// A consumer's own Attach(hwnd), run: the mode the host reports.
[[nodiscard]] std::string RunDefaultAttach()
{
    std::string reported = "not run";
    std::jthread worker([&]
    {
        const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
        DxUi::ControlHost host;
        HWND hwnd = CreateWindowExW(
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, -32000, -32000, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (! hwnd)
            return;
        if (host.Attach(hwnd))
            reported = ModeName(host.GetPresentationMode());
        else
            reported = "Attach failed";
        host.Detach();
        DestroyWindow(hwnd);
    });
    worker.join();
    return reported;
}

void WriteScope(Json& json)
{
    const std::filesystem::path root = FindRepoRootForDxUiTests();
    const std::string defaultAttach  = RunDefaultAttach();
    json.Begin();
    json.Field("question", "every host a consumer creates without naming a mode, the mode it gets today, and where in the code that default is chosen");
    json.Field("defaultAttachOptionsPresentationMode", ModeName(DxUi::ControlHost::AttachOptions{}.presentationMode));
    json.Field("consumerStyleAttachReports", defaultAttach);
    json.Field("consumerStyleAttach",
               "ControlHost::Attach(hwnd) on a window of this program, run in this process: the mode ControlHost::GetPresentationMode reports");
    json.Key("hosts").Array();
    for (const ScopeEntry& entry : ScopeEntries())
    {
        json.Begin().Field("host", entry.host).Field("how", entry.how).Field("modeToday", entry.modeToday).Field("namesTheModeItself", entry.namesTheMode);
        json.Key("chosenAt").Array();
        for (const Anchor& anchor : entry.anchors)
            json.Begin().Field("file", anchor.file).Field("text", anchor.text).Field("line", FindAnchorLine(root, anchor)).End();
        json.EndArray().End();
    }
    json.EndArray();
    json.Field("changingTheDefaultReaches",
               "the first four rows and every consumer's own Attach(hwnd): the library's one default is AttachOptions::presentationMode; MenuPopup already "
               "names composition and the embedded host has no mode");
    json.End();
}

// ---------------------------------------------------------------------------------------------------------------------------
// A case: both modes, compared.
// ---------------------------------------------------------------------------------------------------------------------------
struct CaseResult
{
    std::array<ModeResult, 2> modes; // [0] HwndSwapChain, [1] CompositionSwapChain
    ImageComparison pixels;
    std::vector<std::string> counterDifferences;
    std::string json;
    bool ran = false;
};

[[nodiscard]] CaseResult RunCase(const FixtureCase& fixture, bool hardware, bool compositionFirst, const Options& options)
{
    CaseResult out;
    const std::array<Mode, 2> order = compositionFirst ? std::array<Mode, 2>{Mode::CompositionSwapChain, Mode::HwndSwapChain}
                                                       : std::array<Mode, 2>{Mode::HwndSwapChain, Mode::CompositionSwapChain};
    for (const Mode mode : order)
        out.modes[mode == Mode::CompositionSwapChain ? 1u : 0u] = RunMode(fixture, hardware, mode, options);
    const ModeResult& hwnd        = out.modes[0];
    const ModeResult& composition = out.modes[1];
    out.ran                       = hwnd.ran && composition.ran;

    const PerformanceFixtures::Variant& variant = fixture.definition->variants.front();
    Json json;
    json.Begin();
    json.Field("schema", 1u).Field("kind", "presentation-mode-case");
    json.Key("fixture").Begin().Field("id", fixture.id).Field("description", fixture.description);
    json.Field("variant", variant.name).Field("width", variant.width).Field("height", variant.height).Field("dpi", variant.dpi).End();
    json.Field("adapterRequested", hardware ? "Hardware" : "Warp");
    json.Field("order", compositionFirst ? "composition-first" : "hwnd-first");
    json.Field("configuration", PresentedFrameTests::ConfigurationName()).Field("platform", PresentedFrameTests::PlatformName());
    json.Field("compiler", static_cast<uint64_t>(_MSC_FULL_VER));
    json.Key("adapter")
        .Begin()
        .Field("description", hwnd.adapter.description)
        .Field("vendorId", hwnd.adapter.vendorId)
        .Field("deviceId", hwnd.adapter.deviceId);
    json.Field("software", hwnd.adapter.software).Field("driverVersion", hwnd.adapter.driverVersion).End();
    json.Field("windowsActivationsBlocked", hwnd.activationsBlocked + composition.activationsBlocked)
        .Field("focusMovesBlocked", hwnd.focusBlocked + composition.focusBlocked);
    json.Key("modes").Begin();
    for (const ModeResult& mode : out.modes)
    {
        json.Key(ModeName(mode.mode)).Begin().Field("ran", mode.ran);
        if (! mode.ran)
            json.Field("notRunReason", mode.notRunReason);
        else
        {
            json.Key("pixels").Raw(mode.pixelsJson);
            json.Key("cost").Raw(mode.costJson);
            json.Key("memory").Raw(mode.memoryJson);
            json.Key("behavior").Raw(mode.behaviorJson);
        }
        json.End();
    }
    json.End();

    json.Key("comparison").Begin();
    if (! out.ran)
        json.Field("status", "not run");
    else
    {
        out.pixels = CompareImages(hwnd.presentedOverBackground, composition.presentedOverBackground, &composition.alpha);
        json.Key("pixels").Begin();
        json.Field("basis", "the presented image of each mode over the window background colour, HwndSwapChain against CompositionSwapChain");
        json.Key("presentedImageDifference");
        WriteComparison(json, out.pixels);
        json.Key("compositionAlpha");
        WriteAlphaSummary(json, composition.alphaSummary);
        json.Key("hwndAlpha");
        WriteAlphaSummary(json, hwnd.alphaSummary);
        json.Key("textPixelsWithColourFringesInTheTextRegion")
            .Begin()
            .Field("HwndSwapChain", hwnd.fringes.offTheLine)
            .Field("CompositionSwapChain", composition.fringes.offTheLine)
            .Field("textPixelsHwnd", hwnd.fringes.textPixels)
            .Field("textPixelsComposition", composition.fringes.textPixels)
            .End();
        json.End();

        // Counters of a full frame that differ between the modes.
        json.Key("cost").Begin().Key("counterDifferencesPerFullFrame").Array();
        std::vector<std::string> names;
        for (const auto& [name, range] : hwnd.counters)
            names.push_back(name);
        for (const auto& [name, range] : composition.counters)
            if (! hwnd.counters.contains(name))
                names.push_back(name);
        for (const std::string& name : names)
        {
            const CounterRange none{};
            const CounterRange& a = hwnd.counters.contains(name) ? hwnd.counters.at(name) : none;
            const CounterRange& b = composition.counters.contains(name) ? composition.counters.at(name) : none;
            if (a.total == b.total && a.minimum == b.minimum && a.maximum == b.maximum)
                continue;
            out.counterDifferences.push_back(name);
            json.Begin().Field("counter", name);
            json.Key("HwndSwapChain").Begin().Field("min", a.minimum == UINT64_MAX ? 0u : a.minimum).Field("max", a.maximum).Field("total", a.total).End();
            json.Key("CompositionSwapChain")
                .Begin()
                .Field("min", b.minimum == UINT64_MAX ? 0u : b.minimum)
                .Field("max", b.maximum)
                .Field("total", b.total)
                .End();
            json.End();
        }
        json.EndArray();
        json.Key("frameTotalMedianUs")
            .Begin()
            .Field("HwndSwapChain", hwnd.totalUs.median)
            .Field("CompositionSwapChain", composition.totalUs.median)
            .Field("compositionOverHwnd", hwnd.totalUs.median > 0.0 ? composition.totalUs.median / hwnd.totalUs.median : 0.0)
            .Field("judged", options.judgedTiming)
            .End();
        json.End();
    }
    json.End();
    json.Key("scope");
    WriteScope(json);
    json.End();
    out.json = json.Text();
    return out;
}

[[nodiscard]] bool WriteFile(const std::filesystem::path& path, const std::string& text)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text << '\n';
    return static_cast<bool>(output);
}

[[nodiscard]] Options OptionsFor(size_t timingFrames, bool judged, const std::string& evidenceName)
{
    Options options;
    options.timingFrames      = timingFrames;
    options.judgedTiming      = judged;
    options.evidenceDirectory = FindRepoRootForDxUiTests() / L".build" / L"test-artifacts" / L"presentation-modes" / std::filesystem::path(evidenceName);
    return options;
}

[[nodiscard]] std::string EvidenceName()
{
    return std::format("{}-{}", PresentedFrameTests::PlatformName(), PresentedFrameTests::ConfigurationName());
}
} // namespace

// DxUi.ControlTests.exe --presentation-modes <output.json> <FX5|FX1> Warp|Hardware hwnd-first|composition-first <timingFrames> <judged 0|1>
int RunPresentationModeCommand(int argc, wchar_t** argv)
{
    size_t timingFrames = 0u;
    const auto number   = [](std::wstring_view text, size_t& value)
    {
        value = 0u;
        if (text.empty() || text.size() > 7u)
            return false;
        for (const wchar_t c : text)
        {
            if (c < L'0' || c > L'9')
                return false;
            value = value * 10u + static_cast<size_t>(c - L'0');
        }
        return true;
    };
    // A trailing --no-activate is accepted: this executable's windows never activate, with or without it.
    if ((argc != 8 && ! (argc == 9 && std::wstring_view(argv[8]) == L"--no-activate")) || ! number(argv[6], timingFrames) || timingFrames == 0u ||
        (std::wstring_view(argv[4]) != L"Warp" && std::wstring_view(argv[4]) != L"Hardware") ||
        (std::wstring_view(argv[5]) != L"hwnd-first" && std::wstring_view(argv[5]) != L"composition-first") ||
        (std::wstring_view(argv[7]) != L"0" && std::wstring_view(argv[7]) != L"1"))
    {
        std::fputs("--presentation-modes requires an output path, FX5|FX1, Warp|Hardware, hwnd-first|composition-first, timing frames and 0|1 (judged), "
                   "and accepts a trailing --no-activate.\n",
                   stderr);
        return 2;
    }
    const std::string fixtureId = AsciiOf(argv[3]);
    const FixtureCase* fixture  = FindFixtureCase(fixtureId);
    if (! fixture)
    {
        std::fputs("--presentation-modes names a fixture other than FX5 and FX1.\n", stderr);
        return 2;
    }
    const bool hardware   = std::wstring_view(argv[4]) == L"Hardware";
    const Options options = OptionsFor(timingFrames, std::wstring_view(argv[7]) == L"1", EvidenceName());
    if (! DxUi::TestSupport::HasInteractiveWindowStation())
    {
        std::fputs("--presentation-modes needs an interactive window station: there is no compositor to read.\n", stderr);
        return 4;
    }
    const CaseResult result = RunCase(*fixture, hardware, std::wstring_view(argv[5]) == L"composition-first", options);
    if (! result.ran)
    {
        for (const ModeResult& mode : result.modes)
            if (! mode.ran)
                std::fprintf(stderr, "%s did not run: %s\n", ModeName(mode.mode), mode.notRunReason.c_str());
        return 3;
    }
    Require(WriteFile(std::filesystem::path(argv[2]), result.json), "the case document is written");
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Tests.
// ---------------------------------------------------------------------------------------------------------------------------
namespace
{
[[nodiscard]] PresentedWindowImage MakeImage(UINT width, UINT height, uint8_t b, uint8_t g, uint8_t r)
{
    PresentedWindowImage image;
    image.widthPx  = width;
    image.heightPx = height;
    image.bgraPixels.resize(static_cast<size_t>(width) * height * 4u);
    for (size_t pixel = 0u; pixel < static_cast<size_t>(width) * height; ++pixel)
    {
        image.bgraPixels[pixel * 4u + 0u] = b;
        image.bgraPixels[pixel * 4u + 1u] = g;
        image.bgraPixels[pixel * 4u + 2u] = r;
        image.bgraPixels[pixel * 4u + 3u] = 255u;
    }
    return image;
}

void RequireCaseStructure(const CaseResult& result, bool hardware)
{
    for (const ModeResult& mode : result.modes)
    {
        Require(mode.ran, "both modes ran");
        Require(mode.captureStatus == PresentedCaptureStatus::Captured, "the presented image was read in both modes");
        Require(mode.adapter.software == ! hardware, "the adapter is the requested kind");
        Require(mode.compositionReported == (mode.mode == Mode::CompositionSwapChain), "the host reports the mode it was attached with");
        // The capture is calibrated against the back buffer each run: an unexplained difference is the test's failure.
        Require(mode.calibrationDifferences == 0u, "the presented image over the window background equals the composed back buffer");
        Require(! mode.pixelsJson.empty() && ! mode.costJson.empty() && ! mode.behaviorJson.empty() && ! mode.memoryJson.empty(), "all four parts are written");
        Require(mode.activationsBlocked == 0u && mode.focusBlocked == 0u, "the comparison never tried to activate or focus a window");
        // Every behaviour case is accounted for: run, simulated, or named as not run.
        const std::array<const char*, 8> names{
            "liveResize", "minimizeAndRestore", "occlusion", "deviceLoss", "dpiChangeSimulated", "childHwnd", "monitorMoveThatChangesDpi", "remoteSession"};
        for (const char* name : names)
            Require(std::ranges::any_of(mode.behaviorStatus, [&](const auto& entry) { return entry.first == name; }), "every behaviour case is recorded");
    }
    const ModeResult& hwnd        = result.modes[0];
    const ModeResult& composition = result.modes[1];
    // An HwndSwapChain presents every pixel opaque; a CompositionSwapChain presents what it cleared to transparent as transparent.
    Require(hwnd.alphaSummary.opaque == hwnd.alphaSummary.pixels && hwnd.alphaSummary.inconsistent == 0u, "an HwndSwapChain presents no transparent pixel");
    Require(composition.alphaSummary.transparent != 0u, "a CompositionSwapChain leaves the gaps between the scene's controls transparent");
    Require(result.pixels.sameSize, "both modes present an image of the same size");
    // Text: grayscale on a premultiplied target always; ClearType fringes on the opaque target when the user's setting is ClearType.
    Require(composition.fringes.offTheLine * 20u <= composition.fringes.textPixels,
            "a CompositionSwapChain draws grayscale text: (almost) no pixel leaves the ground-to-text blend line");
    if (hwnd.clearTypeSetting)
        Require(hwnd.fringes.offTheLine * 4u >= hwnd.fringes.textPixels && hwnd.fringes.textPixels != 0u,
                "an HwndSwapChain draws ClearType text under the ClearType setting");
}
} // namespace

void TestPresentationModeAlphaRecoveryIsExact()
{
    // A premultiplied buffer holding every alpha with a colour of the same intensity: composed over black and over white as the
    // compositor does, the two images give the alpha back exactly, whatever the colour.
    constexpr UINT kWidth = 256u;
    std::vector<uint8_t> premultiplied(static_cast<size_t>(kWidth) * 3u * 4u);
    for (UINT alpha = 0u; alpha < kWidth; ++alpha)
        for (UINT row = 0u; row < 3u; ++row)
        {
            const size_t at        = (static_cast<size_t>(row) * kWidth + alpha) * 4u;
            const uint8_t channel  = static_cast<uint8_t>(row == 0u ? 0u : (row == 1u ? alpha : alpha / 2u));
            premultiplied[at + 0u] = channel;
            premultiplied[at + 1u] = static_cast<uint8_t>(row == 2u ? alpha : channel);
            premultiplied[at + 2u] = channel;
            premultiplied[at + 3u] = static_cast<uint8_t>(alpha);
        }
    const PresentedWindowImage overBlack = DxUi::TestSupport::ComposeOverBackdrop(kWidth, 3u, premultiplied, true, RGB(0, 0, 0));
    const PresentedWindowImage overWhite = DxUi::TestSupport::ComposeOverBackdrop(kWidth, 3u, premultiplied, true, RGB(255, 255, 255));
    const AlphaPlane plane               = RecoverAlpha(overBlack, overWhite);
    Require(plane.alpha.size() == static_cast<size_t>(kWidth) * 3u && plane.inconsistent == 0u, "the recovered plane has every pixel and no disagreement");
    for (UINT row = 0u; row < 3u; ++row)
        for (UINT alpha = 0u; alpha < kWidth; ++alpha)
            Require(plane.alpha[static_cast<size_t>(row) * kWidth + alpha] == alpha, "the alpha of every premultiplied pixel is recovered exactly");
    // An opaque target ignores its alpha: the backdrop never shows, so both reads agree and every pixel recovers as opaque.
    const PresentedWindowImage opaqueA = DxUi::TestSupport::ComposeOverBackdrop(kWidth, 3u, premultiplied, false, RGB(0, 0, 0));
    const PresentedWindowImage opaqueB = DxUi::TestSupport::ComposeOverBackdrop(kWidth, 3u, premultiplied, false, RGB(255, 255, 255));
    const AlphaSummary opaque          = Summarize(RecoverAlpha(opaqueA, opaqueB));
    Require(opaque.opaque == opaque.pixels && opaque.transparent == 0u && opaque.partial == 0u, "an opaque target recovers as fully opaque");
    const AlphaSummary mixed = Summarize(plane);
    Require(mixed.transparent == 3u && mixed.opaque == 3u && mixed.partial == 3u * 254u, "the summary counts transparent, opaque and partial pixels");
}

void TestPresentationModeImageComparisonClassifiesPixels()
{
    // 8 x 6: composition alpha 0 in the left column, partial in the second, opaque elsewhere.
    AlphaPlane alpha;
    alpha.width  = 8u;
    alpha.height = 6u;
    alpha.alpha.assign(48u, 255u);
    for (UINT y = 0u; y < 6u; ++y)
    {
        alpha.alpha[static_cast<size_t>(y) * 8u + 0u] = 0u;
        alpha.alpha[static_cast<size_t>(y) * 8u + 1u] = 128u;
    }
    const AlphaSummary summary = Summarize(alpha);
    Require(summary.transparent == 6u && summary.partial == 6u && summary.opaque == 36u && summary.transparentComponents == 1u,
            "alpha classes and regions are counted");
    Require(summary.transparentBox.left == 0 && summary.transparentBox.right == 0 && summary.transparentBox.top == 0 && summary.transparentBox.bottom == 5,
            "the transparent box spans the column");

    PresentedWindowImage a = MakeImage(8u, 6u, 0x30u, 0x30u, 0x30u);
    PresentedWindowImage b = a;
    const auto set         = [](PresentedWindowImage& image, UINT x, UINT y, uint8_t blue, uint8_t green, uint8_t red)
    {
        const size_t at           = (static_cast<size_t>(y) * image.widthPx + x) * 4u;
        image.bgraPixels[at]      = blue;
        image.bgraPixels[at + 1u] = green;
        image.bgraPixels[at + 2u] = red;
    };
    set(b, 1u, 2u, 0x31u, 0x30u, 0x30u); // a partial-alpha pixel off by 1
    set(b, 5u, 3u, 0x30u, 0x40u, 0x30u); // an opaque pixel off by 16, chromatic
    set(b, 7u, 5u, 0xFFu, 0x30u, 0x30u); // an opaque pixel off by 207
    set(b, 0u, 0u, 0x30u, 0x30u, 0x35u); // a transparent pixel off by 5
    const ImageComparison comparison = CompareImages(a, b, &alpha);
    Require(comparison.sameSize && comparison.pixels == 48u && comparison.differing == 4u, "four pixels differ");
    Require(comparison.differingPartial == 1u && comparison.differingOpaque == 2u && comparison.differingTransparent == 1u,
            "the differences are classed by the alpha the second mode presents");
    Require(comparison.maxDelta == 207u && comparison.histogram[0] == 1u && comparison.histogram[2] == 2u && comparison.histogram[4] == 1u,
            "the differences are bucketed by their largest channel");
    Require(comparison.box.left == 0 && comparison.box.right == 7 && comparison.box.top == 0 && comparison.box.bottom == 5 && comparison.firstX == 0 &&
                comparison.firstY == 0,
            "the difference box and the first difference are reported");
    const PresentedWindowImage other = MakeImage(4u, 4u, 0u, 0u, 0u);
    Require(! CompareImages(a, other, &alpha).sameSize, "images of different sizes are reported as such");
}

void TestPresentationModeScopeAnchorsResolve()
{
    // Names a file and a distinctive line, never a line number: the receipt records the line it found at the commit it measured.
    const std::filesystem::path root = FindRepoRootForDxUiTests();
    for (const ScopeEntry& entry : ScopeEntries())
    {
        Require(! entry.anchors.empty(), "every scope row names the code that chooses its mode");
        for (const Anchor& anchor : entry.anchors)
        {
            if (FindAnchorLine(root, anchor) == 0u)
                std::cerr << "    [SCOPE] anchor not found: " << anchor.file << ": " << anchor.text << '\n';
            Require(FindAnchorLine(root, anchor) != 0u, "a scope anchor is found in its file");
        }
    }
    Require(DxUi::ControlHost::AttachOptions{}.presentationMode == Mode::HwndSwapChain, "AttachOptions defaults to HwndSwapChain");
    Require(RunDefaultAttach() == std::string("HwndSwapChain"), "a consumer's Attach(hwnd) gets HwndSwapChain today");
}

namespace
{
void RunComparisonCases(bool hardware)
{
    if (! DxUi::TestSupport::HasInteractiveWindowStation())
    {
        SkipDxUiTest("presentation-mode comparison needs an interactive desktop compositor");
        return;
    }
    // Small counts: the test decides on counters and pixels, not on timing.
    Options options       = OptionsFor(30u, false, EvidenceName());
    options.counterFrames = 8u;
    options.inputCount    = 12u;
    for (const std::string_view id : {std::string_view("FX5"), std::string_view("FX1")})
    {
        const FixtureCase* fixture = FindFixtureCase(id);
        Require(fixture != nullptr, "the fixture is known");
        const CaseResult first = RunCase(*fixture, hardware, false, options);
        if (hardware && ! first.ran && first.modes[0].notRunReason.find("no hardware Direct3D adapter") != std::string::npos)
        {
            SkipDxUiTest("presentation-mode hardware case needs a hardware Direct3D adapter");
            return;
        }
        RequireCaseStructure(first, hardware);
        const CaseResult second = RunCase(*fixture, hardware, true, options);
        RequireCaseStructure(second, hardware);
        // Counters and pixels do not depend on which mode ran first.
        Require(first.counterDifferences == second.counterDifferences, "the counters that differ between the modes do not depend on the order");
        Require(first.pixels.differing == second.pixels.differing && first.pixels.maxDelta == second.pixels.maxDelta,
                "the pixel difference between the modes does not depend on the order");
        for (size_t mode = 0u; mode < 2u; ++mode)
            Require(first.modes[mode].alphaSummary.transparent == second.modes[mode].alphaSummary.transparent &&
                        first.modes[mode].alphaSummary.partial == second.modes[mode].alphaSummary.partial,
                    "the alpha each mode presents does not depend on the order");
        std::cerr << std::format("    [MODES] {} {}: {} of {} pixels differ between the modes (max channel {}); composition presents {} transparent and {} "
                                 "partial pixels; {} counters differ\n",
                                 id,
                                 hardware ? "hardware" : "warp",
                                 first.pixels.differing,
                                 first.pixels.pixels,
                                 first.pixels.maxDelta,
                                 first.modes[1].alphaSummary.transparent,
                                 first.modes[1].alphaSummary.partial,
                                 first.counterDifferences.size());
    }
}
} // namespace

void TestPresentationModeComparisonOnWarp()
{
    RunComparisonCases(false);
}

void TestPresentationModeComparisonOnHardware()
{
    RunComparisonCases(true);
}
#pragma warning(pop)
