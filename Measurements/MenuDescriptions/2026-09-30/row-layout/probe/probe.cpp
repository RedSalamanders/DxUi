// Feasibility probe for one formatted DirectWrite layout per described menu row (DxUi).
// Scratch tool, never committed. Compares main's two layouts (primary Body + secondary smallf, 3 DIP gap after the
// ceiled primary height) with one layout: primary paragraph, a spacer paragraph, secondary paragraph.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using Microsoft::WRL::ComPtr;

#define CHECK(x)                                                                                 \
    do                                                                                           \
    {                                                                                            \
        HRESULT _hr = (x);                                                                       \
        if (FAILED(_hr))                                                                         \
        {                                                                                        \
            std::printf("FAILED %s hr=0x%08X line %d\n", #x, static_cast<unsigned>(_hr), __LINE__); \
            std::exit(1);                                                                        \
        }                                                                                        \
    } while (0)

static uint64_t LiveHeapBytes()
{
    HANDLE heaps[128]{};
    const DWORD n = GetProcessHeaps(128, heaps);
    uint64_t busy = 0;
    for (DWORD i = 0; i < n; ++i)
    {
        if (HeapLock(heaps[i]))
        {
            PROCESS_HEAP_ENTRY e{};
            while (HeapWalk(heaps[i], &e))
                if (e.wFlags & PROCESS_HEAP_ENTRY_BUSY)
                    busy += e.cbData;
            HeapUnlock(heaps[i]);
        }
    }
    return busy;
}

struct Gfx
{
    ComPtr<IDWriteFactory> dw;
    ComPtr<ID2D1Factory1> d2d;
    ComPtr<ID2D1DeviceContext> dc;
    ComPtr<ID2D1Bitmap1> target, staging;
    UINT w = 0, h = 0;

    void Init(UINT width, UINT height)
    {
        w = width;
        h = height;
        CHECK(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dw.GetAddressOf())));
        ComPtr<ID3D11Device> d3d;
        CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &d3d, nullptr, nullptr));
        ComPtr<IDXGIDevice> dxgi;
        CHECK(d3d.As(&dxgi));
        CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr, reinterpret_cast<void**>(d2d.GetAddressOf())));
        ComPtr<ID2D1Device> dev;
        CHECK(d2d->CreateDevice(dxgi.Get(), &dev));
        CHECK(dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc));
        D2D1_BITMAP_PROPERTIES1 tp = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        CHECK(dc->CreateBitmap(D2D1::SizeU(w, h), nullptr, 0, tp, &target));
        D2D1_BITMAP_PROPERTIES1 sp = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                             D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        CHECK(dc->CreateBitmap(D2D1::SizeU(w, h), nullptr, 0, sp, &staging));
        dc->SetTarget(target.Get());
    }

    std::vector<uint8_t> Read()
    {
        CHECK(staging->CopyFromBitmap(nullptr, target.Get(), nullptr));
        D2D1_MAPPED_RECT m{};
        CHECK(staging->Map(D2D1_MAP_OPTIONS_READ, &m));
        std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4u);
        for (UINT y = 0; y < h; ++y)
            std::memcpy(out.data() + static_cast<size_t>(y) * w * 4u, m.bits + static_cast<size_t>(y) * m.pitch, static_cast<size_t>(w) * 4u);
        staging->Unmap();
        return out;
    }
};

static bool FamilyAvailable(IDWriteFactory* dw, const wchar_t* family)
{
    ComPtr<IDWriteFontCollection> c;
    if (FAILED(dw->GetSystemFontCollection(&c, TRUE)))
        return false;
    UINT32 idx = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(c->FindFamilyName(family, &idx, &exists)) && exists;
}

static ComPtr<IDWriteTextFormat> MakeFormat(IDWriteFactory* dw, const wchar_t* family, float size)
{
    const wchar_t* resolved = FamilyAvailable(dw, family) ? family : L"Segoe UI";
    ComPtr<IDWriteTextFormat> f;
    CHECK(dw->CreateTextFormat(resolved, nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &f));
    return f;
}

struct SepRow
{
    ComPtr<IDWriteTextLayout> p, s;
    DWRITE_TEXT_METRICS mp{}, ms{};
};

static void ApplyPolicy(IDWriteTextLayout* layout)
{
    CHECK(layout->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));
    CHECK(layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    CHECK(layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
    CHECK(layout->SetTrimming(&trimming, nullptr));
}

static void PrepSep(IDWriteFactory* dw, IDWriteTextFormat* fmt, const std::wstring& text, float width, ComPtr<IDWriteTextLayout>& layout, DWRITE_TEXT_METRICS& m)
{
    CHECK(dw->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), fmt, width, 1000000.0f, &layout));
    ApplyPolicy(layout.Get());
    CHECK(layout->GetMetrics(&m));
}

static SepRow MakeSep(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf, const std::wstring& P, const std::wstring& S, float width)
{
    SepRow r;
    PrepSep(g.dw.Get(), body, P, width, r.p, r.mp);
    PrepSep(g.dw.Get(), smallf, S, width, r.s, r.ms);
    return r;
}

// One layout: P \n <spacer> \n S.  The spacer paragraph is the second newline; its font size sets the gap.
struct Single
{
    ComPtr<IDWriteTextLayout> layout;
    UINT32 pLen = 0, spacerPos = 0, sStart = 0, sLen = 0;
    float hp = 0, hs = 0, spacerHeight = 0, spacerSize = 0;
    UINT32 pLines = 0, sLines = 0;
    DWRITE_TEXT_METRICS m{};
    bool ok = false;
};

static std::vector<DWRITE_LINE_METRICS> Lines(IDWriteTextLayout* l)
{
    UINT32 n = 0;
    l->GetLineMetrics(nullptr, 0, &n);
    std::vector<DWRITE_LINE_METRICS> v(n);
    if (n)
        CHECK(l->GetLineMetrics(v.data(), n, &n));
    v.resize(n);
    return v;
}

static float g_spacerGuess = 10.0f;
static bool g_useGuess = false;
static Single MakeSingle(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf, const std::wstring& P, const std::wstring& S, float width, bool verbose = false)
{
    Single r;
    r.pLen = static_cast<UINT32>(P.size());
    r.spacerPos = r.pLen + 1;
    r.sStart = r.pLen + 2;
    r.sLen = static_cast<UINT32>(S.size());
    const std::wstring text = P + L"\n\n" + S;
    CHECK(g.dw->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), body, width, 1000000.0f, &r.layout));
    ApplyPolicy(r.layout.Get());
    // Secondary range takes the smallf format's font.
    const DWRITE_TEXT_RANGE sec{r.sStart, r.sLen};
    {
        UINT32 len = smallf->GetFontFamilyNameLength() + 1;
        std::vector<wchar_t> fam(len);
        CHECK(smallf->GetFontFamilyName(fam.data(), len));
        if (r.sLen)
        {
            CHECK(r.layout->SetFontFamilyName(fam.data(), sec));
            CHECK(r.layout->SetFontSize(smallf->GetFontSize(), sec));
            CHECK(r.layout->SetFontWeight(smallf->GetFontWeight(), sec));
            CHECK(r.layout->SetFontStyle(smallf->GetFontStyle(), sec));
            CHECK(r.layout->SetFontStretch(smallf->GetFontStretch(), sec));
        }
        // the spacer newline: same family as smallf, reference size 10
        const DWRITE_TEXT_RANGE sp{r.spacerPos, 1};
        CHECK(r.layout->SetFontFamilyName(fam.data(), sp));
        CHECK(r.layout->SetFontSize(g_useGuess ? g_spacerGuess : 10.0f, sp));
        CHECK(r.layout->SetIncrementalTabStop(smallf->GetIncrementalTabStop()));
    }
    auto lines = Lines(r.layout.Get());
    // partition lines by text position
    UINT32 pos = 0;
    float hp = 0, hs = 0, hsp = 0;
    UINT32 pl = 0, sl = 0;
    size_t spacerLineIdx = SIZE_MAX;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (pos < r.spacerPos)
        {
            hp += lines[i].height;
            ++pl;
        }
        else if (pos == r.spacerPos)
        {
            hsp = lines[i].height;
            spacerLineIdx = i;
        }
        else
        {
            hs += lines[i].height;
            ++sl;
        }
        pos += lines[i].length;
    }
    r.hp = hp;
    r.pLines = pl;
    r.hs = hs;
    r.sLines = sl;
    if (verbose)
        std::printf("    [single] lines=%zu hp=%.6f spacer@10=%.6f hs=%.6f pl=%u sl=%u spacerIdx=%zd\n", lines.size(), hp, hsp, hs, pl, sl, (ptrdiff_t)spacerLineIdx);
    if (spacerLineIdx == SIZE_MAX || hsp <= 0)
        return r;
    // calibrate the spacer to (ceil(hp) - hp) + 3
    const float target = std::ceil(hp) - hp + 3.0f;
    const float startSize = g_useGuess ? g_spacerGuess : 10.0f;
    if (g_useGuess && std::fabs(hsp - target) < 1e-4f)
    {
        r.spacerHeight = hsp;
        r.spacerSize = startSize;
        r.ok = true;
        CHECK(r.layout->GetMetrics(&r.m));
        return r;
    }
    float size = startSize * target / hsp;
    for (int it = 0; it < 4; ++it)
    {
        const DWRITE_TEXT_RANGE sp{r.spacerPos, 1};
        CHECK(r.layout->SetFontSize(size, sp));
        lines = Lines(r.layout.Get());
        // find the spacer line again
        pos = 0;
        float cur = 0;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            if (pos == r.spacerPos)
                cur = lines[i].height;
            pos += lines[i].length;
        }
        if (verbose)
            std::printf("    [single] calib it=%d size=%.6f spacerHeight=%.6f target=%.6f\n", it, size, cur, target);
        if (std::fabs(cur - target) < 1e-4f)
        {
            r.spacerHeight = cur;
            r.spacerSize = size;
            r.ok = true;
            break;
        }
        size *= target / cur;
    }
    CHECK(r.layout->GetMetrics(&r.m));
    g_spacerGuess = r.spacerSize > 0 ? r.spacerSize : g_spacerGuess;
    return r;
}

struct Px
{
    std::vector<uint8_t> v;
};

static const D2D1_COLOR_F kBg = D2D1::ColorF(0.13f, 0.13f, 0.14f, 1.0f);
static const D2D1_COLOR_F kP = D2D1::ColorF(0.96f, 0.96f, 0.96f, 1.0f);
static const D2D1_COLOR_F kS = D2D1::ColorF(0.62f, 0.66f, 0.72f, 1.0f);

static std::vector<uint8_t> RenderMain(Gfx& g, const SepRow& r, float x, float y, float dpi)
{
    g.dc->SetDpi(dpi, dpi);
    ComPtr<ID2D1SolidColorBrush> bp, bs;
    CHECK(g.dc->CreateSolidColorBrush(kP, &bp));
    CHECK(g.dc->CreateSolidColorBrush(kS, &bs));
    g.dc->BeginDraw();
    g.dc->Clear(kBg);
    g.dc->DrawTextLayout(D2D1::Point2F(x, y), r.p.Get(), bp.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    g.dc->DrawTextLayout(D2D1::Point2F(x, y + std::ceil(r.mp.height) + 3.0f), r.s.Get(), bs.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    CHECK(g.dc->EndDraw());
    return g.Read();
}

static std::vector<uint8_t> RenderSingle(Gfx& g, const Single& r, float x, float y, float dpi)
{
    g.dc->SetDpi(dpi, dpi);
    ComPtr<ID2D1SolidColorBrush> bp, bs;
    CHECK(g.dc->CreateSolidColorBrush(kP, &bp));
    CHECK(g.dc->CreateSolidColorBrush(kS, &bs));
    if (r.sLen)
        CHECK(r.layout->SetDrawingEffect(bs.Get(), DWRITE_TEXT_RANGE{r.sStart, r.sLen}));
    g.dc->BeginDraw();
    g.dc->Clear(kBg);
    g.dc->DrawTextLayout(D2D1::Point2F(x, y), r.layout.Get(), bp.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    CHECK(g.dc->EndDraw());
    return g.Read();
}

struct DiffStat
{
    size_t diffPixels = 0, inkPixels = 0;
    int maxDelta = 0;
    int minX = 1 << 30, minY = 1 << 30, maxX = -1, maxY = -1;
};

static DiffStat Diff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, UINT w)
{
    DiffStat d;
    for (size_t i = 0; i + 3 < a.size(); i += 4)
    {
        const bool ink = std::abs(int(a[i]) - 33) > 2 || std::abs(int(a[i + 1]) - 33) > 2 || std::abs(int(a[i + 2]) - 36) > 2;
        d.inkPixels += ink;
        int md = 0;
        for (int c = 0; c < 4; ++c)
            md = std::max(md, std::abs(int(a[i + c]) - int(b[i + c])));
        if (md)
        {
            ++d.diffPixels;
            d.maxDelta = std::max(d.maxDelta, md);
            const int px = static_cast<int>((i / 4) % w), py = static_cast<int>((i / 4) / w);
            d.minX = std::min(d.minX, px);
            d.maxX = std::max(d.maxX, px);
            d.minY = std::min(d.minY, py);
            d.maxY = std::max(d.maxY, py);
        }
    }
    return d;
}

struct Case
{
    const char* name;
    std::wstring p, s;
    float width;
};

static std::vector<Case> Cases()
{
    std::vector<Case> c;
    c.push_back({"short", L"Documents", L"C:\\Users\\Eric\\Documents", 396.0f});
    c.push_back({"french-wrap", L"Sélection définitive pour impression et archivage — réunion familiale été 2026 & photographies originales",
                 L"D:\\Sauvegardes\\Archives photographiques personnelles de plusieurs générations\\Exposition annuelle de la médiathèque", 300.0f});
    c.push_back({"french-fixture", L"Archives photographiques de la réunion familiale", L"D:\\Sauvegardes\\Collection déjà présente\\Génération 3", 396.0f});
    c.push_back({"emoji", L"Photos \U0001F4F7 famille \U0001F389", L"Archive \U0001F389 été \U0001F468\u200D\U0001F469\u200D\U0001F467", 396.0f});
    c.push_back({"hebrew", L"\u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD!", L"D:\\Backup\\\u05EA\u05DE\u05D5\u05E0\u05D5\u05EA", 396.0f});
    c.push_back({"rtl-rtl-punct", L"\u05E9\u05DC\u05D5\u05DD!", L"\u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD", 396.0f});
    c.push_back({"arabic", L"\u0645\u0631\u062D\u0628\u0627 \u0628\u0627\u0644\u0639\u0627\u0644\u0645 (2026)", L"\u0627\u0644\u0645\u0633\u0627\u0631: \u0627\u0644\u0635\u0648\u0631\\\u0627\u0644\u0639\u0627\u0626\u0644\u0629.", 396.0f});
    c.push_back({"mixed-bidi", L"abc \u05E9\u05DC\u05D5\u05DD 123 def", L"123 \u05E9\u05DC\u05D5\u05DD abc", 396.0f});
    c.push_back({"cjk", L"\u5199\u771F\u306E\u30A2\u30FC\u30AB\u30A4\u30D6 \uD55C\uAD6D\uC5B4", L"D:\\\u30D0\u30C3\u30AF\u30A2\u30C3\u30D7\\\u5199\u771F \u4E2D\u6587", 396.0f});
    c.push_back({"thai", L"\u0E2A\u0E27\u0E31\u0E2A\u0E14\u0E35\u0E04\u0E23\u0E31\u0E1A \u0E20\u0E32\u0E29\u0E32\u0E44\u0E17\u0E22", L"\u0E2A\u0E27\u0E31\u0E2A\u0E14\u0E35 \u0E1B\u0E23\u0E30\u0E40\u0E17\u0E28\u0E44\u0E17\u0E22", 396.0f});
    c.push_back({"marks", L"e\u0301\u0302\u0303\u0304\u0305\u0306 stack", L"a\u0300\u0301\u0302\u0303\u0304\u0305\u0306\u0307\u0308 tall", 396.0f});
    c.push_back({"empty-primary", L"", L"Only a description here", 396.0f});
    c.push_back({"long-token", L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", L"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", 300.0f});
    c.push_back({"multi-paragraph-primary", L"Line one\nLine two is here", L"Desc one\nDesc two", 396.0f});
    c.push_back({"narrow", L"Archives photographiques de la réunion familiale", L"D:\\Sauvegardes\\Collection déjà présente\\Génération 3", 96.0f});
    return c;
}


// ---------------------------------------------------------------------------------------------------------------
// Memory experiment: live heap bytes held by N described rows for each layout strategy.
// ---------------------------------------------------------------------------------------------------------------
static double Median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    if (v.empty())
        return 0;
    return v.size() % 2 ? v[v.size() / 2] : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
}

static int64_t RunMemoryMode(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf, int mode, size_t rows, bool distinctPrimary, float width, bool drawEffect)
{
    std::vector<std::wstring> P(rows), S(rows);
    for (size_t i = 0; i < rows; ++i)
    {
        P[i] = distinctPrimary ? (L"Archives photographiques " + std::to_wstring(i) + L" de la réunion familiale") : std::wstring(L"Archives photographiques de la réunion familiale");
        S[i] = L"D:\\Sauvegardes\\Collection déjà présente\\Génération " + std::to_wstring(i);
    }
    ComPtr<ID2D1SolidColorBrush> bs;
    CHECK(g.dc->CreateSolidColorBrush(kS, &bs));
    const int64_t before = static_cast<int64_t>(LiveHeapBytes());
    std::vector<SepRow> sep;
    std::vector<Single> one;
    std::map<std::wstring, std::pair<ComPtr<IDWriteTextLayout>, DWRITE_TEXT_METRICS>> intern;
    for (size_t i = 0; i < rows; ++i)
    {
        if (mode == 0)
            sep.push_back(MakeSep(g, body, smallf, P[i], S[i], width));
        else if (mode == 1)
        {
            SepRow r;
            auto prep = [&](const std::wstring& text, IDWriteTextFormat* fmt, int role, ComPtr<IDWriteTextLayout>& layout, DWRITE_TEXT_METRICS& m)
            {
                const std::wstring key = std::to_wstring(role) + L"|" + text;
                auto it = intern.find(key);
                if (it != intern.end())
                {
                    layout = it->second.first;
                    m = it->second.second;
                    return;
                }
                PrepSep(g.dw.Get(), fmt, text, width, layout, m);
                intern.emplace(key, std::make_pair(layout, m));
            };
            prep(P[i], body, 0, r.p, r.mp);
            prep(S[i], smallf, 1, r.s, r.ms);
            sep.push_back(std::move(r));
        }
        else
        {
            one.push_back(MakeSingle(g, body, smallf, P[i], S[i], width));
            if (drawEffect)
                CHECK(one.back().layout->SetDrawingEffect(bs.Get(), DWRITE_TEXT_RANGE{one.back().sStart, one.back().sLen}));
        }
    }
    const int64_t prepared = static_cast<int64_t>(LiveHeapBytes());
    sep.clear();
    one.clear();
    intern.clear();
    return prepared - before;
}

static void MemoryExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    struct Cfg
    {
        const char* name;
        int mode;
        bool effect;
    };
    const Cfg cfgs[] = {{"separate (no sharing)", 0, false}, {"main: shared identical text", 1, false}, {"single layout per row", 2, false}, {"single layout + secondary drawing effect", 2, true}};
    for (bool distinct : {false, true})
    {
        for (size_t rows : {12u, 24u, 48u})
        {
            std::printf("\nrows=%zu primary=%s\n", rows, distinct ? "distinct" : "repeated");
            std::map<int, std::vector<double>> samples;
            // warm-up
            for (const auto& c : cfgs)
                RunMemoryMode(g, body, smallf, c.mode, rows, distinct, 396.0f, c.effect);
            for (int cycle = 0; cycle < 24; ++cycle)
                for (int step = 0; step < 4; ++step)
                {
                    const int idx = (cycle + (cycle % 2 == 0 ? step : 3 - step)) % 4;
                    samples[idx].push_back(static_cast<double>(RunMemoryMode(g, body, smallf, cfgs[idx].mode, rows, distinct, 396.0f, cfgs[idx].effect)));
                }
            for (int i = 0; i < 4; ++i)
            {
                std::vector<double> last(samples[i].end() - 16, samples[i].end());
                const double mn = *std::min_element(last.begin(), last.end()), mx = *std::max_element(last.begin(), last.end());
                std::printf("  %-44s median live bytes %10.1f  (min %.0f max %.0f)  per row %.1f\n", cfgs[i].name, Median(last), mn, mx, Median(last) / static_cast<double>(rows));
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Extra probes
// ---------------------------------------------------------------------------------------------------------------
static uint32_t Bits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}

// Bitwise comparison of the height a separate layout reports with the float sum of the single layout's line heights.
static void HeightsExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    int total = 0, mismatchP = 0, mismatchS = 0, lineCountMismatch = 0, totalMismatch = 0;
    const std::vector<std::wstring> texts = {L"Archives photographiques de la réunion familiale", L"Sélection définitive pour impression et archivage — réunion familiale été 2026 & photographies originales",
                                             L"D:\\Sauvegardes\\Collection déjà présente\\Génération 3", L"Short", L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                                             L"\u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD \u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD \u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD \u05E9\u05DC\u05D5\u05DD", L"Photos \U0001F4F7 famille \U0001F389 Photos \U0001F4F7 famille \U0001F389 Photos"};
    for (float width : {1.0f, 20.0f, 37.0f, 64.0f, 96.0f, 123.5f, 180.0f, 240.0f, 300.0f, 380.0f, 396.0f, 500.0f})
        for (const auto& p : texts)
            for (const auto& s : texts)
            {
                auto sep = MakeSep(g, body, smallf, p, s, width);
                auto one = MakeSingle(g, body, smallf, p, s, width);
                ++total;
                if (Bits(one.hp) != Bits(sep.mp.height))
                    ++mismatchP;
                if (Bits(one.hs) != Bits(sep.ms.height))
                    ++mismatchS;
                if (one.pLines != sep.mp.lineCount || one.sLines != sep.ms.lineCount)
                    ++lineCountMismatch;
                const float expectedTotal = std::ceil(sep.mp.height) + 3.0f + sep.ms.height;
                if (std::fabs(one.m.height - expectedTotal) > 1e-3f)
                    ++totalMismatch;
                if (!one.ok)
                    std::printf("  calibration failed width=%.1f\n", width);
            }
    std::printf("heights: %d cases; primary height bit mismatches=%d; secondary=%d; line count mismatches=%d; total height off=%d\n", total, mismatchP, mismatchS, lineCountMismatch, totalMismatch);
}

static void TabsExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    std::printf("incremental tab stop body=%.4f small=%.4f\n", body->GetIncrementalTabStop(), smallf->GetIncrementalTabStop());
    const std::wstring p = L"Documents", s = L"C:\\a\tb\tc\td\te";
    auto sep = MakeSep(g, body, smallf, p, s, 396.0f);
    auto one = MakeSingle(g, body, smallf, p, s, 396.0f);
    auto a = RenderMain(g, sep, 12, 8, 96);
    auto b = RenderSingle(g, one, 12, 8, 96);
    auto d = Diff(a, b, g.w);
    std::printf("tab in secondary, default tab stops: %zu px differ\n", d.diffPixels);
    CHECK(one.layout->SetIncrementalTabStop(smallf->GetIncrementalTabStop()));
    auto b2 = RenderSingle(g, one, 12, 8, 96);
    d = Diff(a, b2, g.w);
    std::printf("tab in secondary, layout tab stop set to Small's: %zu px differ\n", d.diffPixels);
}

// Does D2D capture a brush's color at draw time? Two rows, one shared brush whose color changes between them.
static void BrushExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    const std::wstring p = L"Documents", s = L"C:\\Users\\Eric\\Documents";
    auto one = MakeSingle(g, body, smallf, p, s, 396.0f);
    auto one2 = MakeSingle(g, body, smallf, p, s, 396.0f);
    g.dc->SetDpi(96, 96);
    ComPtr<ID2D1SolidColorBrush> shared, bp, bp2, b1, b2;
    CHECK(g.dc->CreateSolidColorBrush(kS, &shared));
    CHECK(g.dc->CreateSolidColorBrush(kP, &bp));
    CHECK(g.dc->CreateSolidColorBrush(kP, &bp2));
    const D2D1_COLOR_F colorA = D2D1::ColorF(0.9f, 0.2f, 0.2f, 1.0f), colorB = D2D1::ColorF(0.2f, 0.9f, 0.2f, 0.4f);
    CHECK(one.layout->SetDrawingEffect(shared.Get(), DWRITE_TEXT_RANGE{one.sStart, one.sLen}));
    CHECK(one2.layout->SetDrawingEffect(shared.Get(), DWRITE_TEXT_RANGE{one2.sStart, one2.sLen}));
    g.dc->BeginDraw();
    g.dc->Clear(kBg);
    shared->SetColor(colorA);
    g.dc->DrawTextLayout(D2D1::Point2F(12, 8), one.layout.Get(), bp.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    shared->SetColor(colorB);
    g.dc->DrawTextLayout(D2D1::Point2F(12, 108), one2.layout.Get(), bp2.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    CHECK(g.dc->EndDraw());
    auto shared_px = g.Read();

    // Reference: dedicated brushes, same colors.
    CHECK(g.dc->CreateSolidColorBrush(colorA, &b1));
    CHECK(g.dc->CreateSolidColorBrush(colorB, &b2));
    CHECK(one.layout->SetDrawingEffect(b1.Get(), DWRITE_TEXT_RANGE{one.sStart, one.sLen}));
    CHECK(one2.layout->SetDrawingEffect(b2.Get(), DWRITE_TEXT_RANGE{one2.sStart, one2.sLen}));
    g.dc->BeginDraw();
    g.dc->Clear(kBg);
    g.dc->DrawTextLayout(D2D1::Point2F(12, 8), one.layout.Get(), bp.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    g.dc->DrawTextLayout(D2D1::Point2F(12, 108), one2.layout.Get(), bp2.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    CHECK(g.dc->EndDraw());
    auto ref_px = g.Read();
    auto d = Diff(shared_px, ref_px, g.w);
    std::printf("shared mutable brush vs dedicated brushes: %zu px differ (ink %zu)\n", d.diffPixels, d.inkPixels);
}

static void EdgesExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    struct E
    {
        const char* name;
        std::wstring p, s;
    };
    const std::vector<E> es = {
        {"secondary trailing newline", L"Documents", L"Line\n"},
        {"secondary leading newline", L"Documents", L"\nLine"},
        {"secondary only newlines", L"Documents", L"\n\n"},
        {"secondary CRLF", L"Documents", L"one\r\ntwo\r\nthree"},
        {"secondary U+2028", L"Documents", L"one\u2028two\u2029three\u0085four"},
        {"primary U+2028", L"one\u2028two", L"Desc"},
        {"primary trailing CR", L"Documents\r", L"Desc"},
        {"secondary NBSP lead", L"Documents", L"\u00A0\u00A0 indented"},
        {"secondary tabs only", L"Documents", L"\t\t"},
        {"secondary zero width", L"Documents", L"\u200B\u200B"},
        {"rtl then ltr punct", L"\u05E9\u05DC\u05D5\u05DD!", L"Hello"},
        {"ltr then rtl", L"Hello!", L"\u05E9\u05DC\u05D5\u05DD"},
        {"arabic digits", L"\u0645\u0631\u062D\u0628\u0627 123", L"456 \u0645\u0631\u062D\u0628\u0627"},
        {"numbers neutrals", L"(123)", L"(456)"},
        {"unterminated RLO in label", L"abc\u202E", L"DEF123 ghi"},
        {"unterminated RLE in label", L"abc\u202B", L"DEF 123 ghi"},
        {"unterminated LRO in rtl label", L"\u05E9\u05DC\u05D5\u05DD\u202D", L"\u05E9\u05DC\u05D5\u05DD abc"},
        {"unterminated RLI in label", L"abc\u2067", L"DEF 123"},
        {"unterminated FSI in label", L"abc\u2068", L"DEF 123"},
        {"RLO in description only", L"abc", L"\u202EDEF123 ghi"},
        {"rtl label, ltr desc, trailing neutrals", L"\u05E9\u05DC\u05D5\u05DD 123 -", L"abc 456 -"},
        {"arabic label digits end", L"\u0645\u0631\u062D\u0628\u0627 2026", L"2026 abc"},
    };
    for (const auto& e : es)
    {
        auto sep = MakeSep(g, body, smallf, e.p, e.s, 396.0f);
        auto one = MakeSingle(g, body, smallf, e.p, e.s, 396.0f);
        size_t diffs = 0;
        for (float dpi : {96.0f, 144.0f})
        {
            auto a = RenderMain(g, sep, 12, 8, dpi);
            auto b = RenderSingle(g, one, 12, 8, dpi);
            diffs += Diff(a, b, g.w).diffPixels;
        }
        std::printf("%-28s sepP h=%.4f l=%u sepS h=%.4f l=%u | single hp=%.4f(%u) hs=%.4f(%u) spacerOk=%d pixels differ=%zu\n", e.name, sep.mp.height, sep.mp.lineCount, sep.ms.height,
                    sep.ms.lineCount, one.hp, one.pLines, one.hs, one.sLines, one.ok, diffs);
    }
}

// Scrollbar lane: the same layouts narrowed by 16 DIP with SetMaxWidth, recalibrated, against a fresh separate build.
static void ReflowExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    const std::wstring p = L"Sélection définitive pour impression et archivage — réunion familiale été 2026 & photographies originales";
    const std::wstring s = L"D:\\Sauvegardes\\Archives photographiques personnelles de plusieurs générations\\Exposition annuelle de la médiathèque";
    size_t total = 0, bad = 0;
    for (float w0 : {396.0f, 300.0f, 250.0f})
    {
        auto one = MakeSingle(g, body, smallf, p, s, w0);
        const float w1 = w0 - 16.0f;
        CHECK(one.layout->SetMaxWidth(w1));
        // re-measure and recalibrate
        auto lines = Lines(one.layout.Get());
        UINT32 pos = 0;
        float hp = 0, hsp = 0, hs = 0;
        for (auto& l : lines)
        {
            if (pos < one.spacerPos)
                hp += l.height;
            else if (pos == one.spacerPos)
                hsp = l.height;
            else
                hs += l.height;
            pos += l.length;
        }
        const float target = std::ceil(hp) - hp + 3.0f;
        float sz = one.spacerSize * target / hsp;
        CHECK(one.layout->SetFontSize(sz, DWRITE_TEXT_RANGE{one.spacerPos, 1}));
        auto sep = MakeSep(g, body, smallf, p, s, w1);
        auto a = RenderMain(g, sep, 12, 8, 96);
        auto b = RenderSingle(g, one, 12, 8, 96);
        ++total;
        const auto d = Diff(a, b, g.w);
        if (d.diffPixels)
            ++bad;
        std::printf("reflow %.0f->%.0f: hp=%.4f vs %.4f hs=%.4f vs %.4f pixels differ=%zu\n", w0, w1, hp, sep.mp.height, hs, sep.ms.height, d.diffPixels);
    }
    std::printf("reflow comparisons %zu bad %zu\n", total, bad);
}

static void TimingExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    const size_t rows = 48;
    std::vector<std::wstring> P(rows), S(rows);
    for (size_t i = 0; i < rows; ++i)
    {
        P[i] = L"Archives photographiques " + std::to_wstring(i) + L" de la réunion familiale";
        S[i] = L"D:\\Sauvegardes\\Collection déjà présente\\Génération " + std::to_wstring(i);
    }
    for (int mode = 0; mode < 3; ++mode)
    {
        g_useGuess = mode == 2;
        std::vector<double> runs;
        for (int rep = 0; rep < 25; ++rep)
        {
            QueryPerformanceCounter(&t0);
            for (size_t i = 0; i < rows; ++i)
            {
                if (mode == 0)
                {
                    auto r = MakeSep(g, body, smallf, P[i], S[i], 396.0f);
                    (void)r;
                }
                else
                {
                    auto r = MakeSingle(g, body, smallf, P[i], S[i], 396.0f);
                    (void)r;
                }
            }
            QueryPerformanceCounter(&t1);
            runs.push_back(1000.0 * (t1.QuadPart - t0.QuadPart) / f.QuadPart);
        }
        std::printf("%s: prepare %zu rows median %.3f ms (min %.3f)\n", mode == 0 ? "separate        " : (mode == 1 ? "single          " : "single + guess  "), rows, Median(runs), *std::min_element(runs.begin(), runs.end()));
    }
}


// Marginal cost of each step of the single-layout design, 12 distinct rows.
static int64_t RunStepMode(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf, int step, size_t rows)
{
    std::vector<std::wstring> P(rows), S(rows);
    for (size_t i = 0; i < rows; ++i)
    {
        P[i] = L"Archives photographiques " + std::to_wstring(i) + L" de la réunion familiale";
        S[i] = L"D:\\Sauvegardes\\Collection déjà présente\\Génération " + std::to_wstring(i);
    }
    ComPtr<ID2D1SolidColorBrush> bs;
    CHECK(g.dc->CreateSolidColorBrush(kS, &bs));
    const int64_t before = static_cast<int64_t>(LiveHeapBytes());
    std::vector<ComPtr<IDWriteTextLayout>> v;
    for (size_t i = 0; i < rows; ++i)
    {
        ComPtr<IDWriteTextLayout> l;
        std::wstring text;
        switch (step)
        {
            case 10: text = P[i]; break;                 // primary only
            case 11: text = S[i]; break;                 // secondary only (body format)
            default: text = P[i] + L"\n\n" + S[i]; break;
        }
        CHECK(g.dw->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), step == 11 ? smallf : body, 396.0f, 1000000.0f, &l));
        ApplyPolicy(l.Get());
        const UINT32 pLen = static_cast<UINT32>(P[i].size());
        const DWRITE_TEXT_RANGE sec{pLen + 2, static_cast<UINT32>(S[i].size())};
        if (step >= 1 && step < 10)
            CHECK(l->SetFontSize(11.0f, sec));
        if (step >= 2 && step < 10)
            CHECK(l->SetFontSize(2.8f, DWRITE_TEXT_RANGE{pLen + 1, 1}));
        if (step >= 3 && step < 10)
            CHECK(l->SetDrawingEffect(bs.Get(), sec));
        DWRITE_TEXT_METRICS m{};
        if (step >= 4 && step < 10)
        {
            auto lines = Lines(l.Get());
            (void)lines;
        }
        CHECK(l->GetMetrics(&m));
        v.push_back(l);
    }
    const int64_t prepared = static_cast<int64_t>(LiveHeapBytes());
    v.clear();
    return prepared - before;
}

static void StepExperiment(Gfx& g, IDWriteTextFormat* body, IDWriteTextFormat* smallf)
{
    const char* names[] = {"V0 one layout, P\\n\\nS, no overrides", "V1 + description size override", "V2 + spacer size", "V3 + drawing effect", "V4 + GetLineMetrics", "primary-only layout (12)", "secondary-only layout, small format (12)"};
    const int steps[] = {0, 1, 2, 3, 4, 10, 11};
    for (int i = 0; i < 7; ++i)
    {
        std::vector<double> runs;
        RunStepMode(g, body, smallf, steps[i], 12);
        for (int c = 0; c < 10; ++c)
            runs.push_back(static_cast<double>(RunStepMode(g, body, smallf, steps[i], 12)));
        std::printf("  %-44s %10.0f  per row %8.1f\n", names[i], Median(runs), Median(runs) / 12.0);
    }
}


static ComPtr<IDWriteTextFormat> MakeFormatEx(IDWriteFactory* dw, const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL)
{
    ComPtr<IDWriteTextFormat> f;
    CHECK(dw->CreateTextFormat(family, nullptr, weight, style, stretch, size, L"", &f));
    return f;
}

// Different families, weights, styles and sizes for the description than for the label: the range overrides and the spacer
// calibration are not specific to the one font pair this machine resolves.
static void FontsExperiment(Gfx& g)
{
    struct Pair
    {
        const char* name;
        const wchar_t* bodyFamily;
        float bodySize;
        DWRITE_FONT_WEIGHT bodyWeight;
        DWRITE_FONT_STYLE bodyStyle;
        const wchar_t* smallFamily;
        float smallSize;
        DWRITE_FONT_WEIGHT smallWeight;
        DWRITE_FONT_STYLE smallStyle;
    };
    const Pair pairs[] = {
        {"Segoe UI 13 / Segoe UI 11 (as DxUi resolves here)", L"Segoe UI", 13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, L"Segoe UI", 11.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL},
        {"Segoe UI 13 / Consolas 11", L"Segoe UI", 13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, L"Consolas", 11.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL},
        {"Segoe UI 13 / Cambria 11 italic", L"Segoe UI", 13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, L"Cambria", 11.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC},
        {"Segoe UI 13 / Segoe UI 11 semibold", L"Segoe UI", 13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, L"Segoe UI", 11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL},
        {"Arial 13 / Times New Roman 11", L"Arial", 13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, L"Times New Roman", 11.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL},
        {"Consolas 12 / Segoe UI 18", L"Consolas", 12.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL},
        {"Cambria 15 bold / Arial 9", L"Cambria", 15.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, L"Arial", 9.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL},
    };
    const std::vector<std::pair<std::wstring, std::wstring>> texts = {
        {L"Documents", L"C:\\Users\\Eric\\Documents"},
        {L"Sélection définitive pour impression et archivage — réunion familiale été 2026", L"D:\\Sauvegardes\\Archives photographiques personnelles de plusieurs générations\\Exposition annuelle"},
        {L"Photos \U0001F4F7 famille", L"Archive \U0001F389 été\tfin\tde ligne"},
        {L"\u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD!", L"D:\\Backup\\\u05EA\u05DE\u05D5\u05E0\u05D5\u05EA"},
        {L"写真のアーカイブ", L"D:\\バックアップ\\写真"},
    };
    int total = 0, bad = 0;
    for (const auto& pair : pairs)
    {
        auto body = MakeFormatEx(g.dw.Get(), pair.bodyFamily, pair.bodySize, pair.bodyWeight, pair.bodyStyle);
        auto smallf = MakeFormatEx(g.dw.Get(), pair.smallFamily, pair.smallSize, pair.smallWeight, pair.smallStyle);
        size_t pairBad = 0, pairTotal = 0;
        bool calibrated = true;
        for (const auto& text : texts)
            for (float width : {396.0f, 180.0f})
            {
                auto sep = MakeSep(g, body.Get(), smallf.Get(), text.first, text.second, width);
                auto one = MakeSingle(g, body.Get(), smallf.Get(), text.first, text.second, width);
                calibrated = calibrated && one.ok;
                for (float dpi : {96.0f, 144.0f})
                {
                    auto a = RenderMain(g, sep, 12, 8, dpi);
                    auto b = RenderSingle(g, one, 12, 8, dpi);
                    ++pairTotal;
                    ++total;
                    if (Diff(a, b, g.w).diffPixels)
                    {
                        ++pairBad;
                        ++bad;
                    }
                }
            }
        std::printf("%-52s calibrated=%d comparisons=%zu differing=%zu\n", pair.name, calibrated, pairTotal, pairBad);
    }
    std::printf("font pair comparisons: %d, differing: %d\n", total, bad);
}


// Live bytes of a layout against its text length (one paragraph, no overrides): is the cost per layout or per character?
static void CharsExperiment(Gfx& g, IDWriteTextFormat* body)
{
    for (size_t chars : {1u, 10u, 25u, 50u, 100u, 200u, 400u, 800u})
    {
        std::vector<double> runs;
        for (int rep = 0; rep < 6; ++rep)
        {
            const int64_t before = static_cast<int64_t>(LiveHeapBytes());
            std::vector<ComPtr<IDWriteTextLayout>> v;
            for (int i = 0; i < 12; ++i)
            {
                std::wstring text;
                for (size_t c = 0; c < chars; ++c)
                    text.push_back(static_cast<wchar_t>(L'a' + (c * 7 + i) % 26));
                // spaces every few characters so there are wrap opportunities
                for (size_t c = 5; c < text.size(); c += 6)
                    text[c] = L' ';
                ComPtr<IDWriteTextLayout> l;
                CHECK(g.dw->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), body, 396.0f, 1000000.0f, &l));
                ApplyPolicy(l.Get());
                DWRITE_TEXT_METRICS m{};
                CHECK(l->GetMetrics(&m));
                v.push_back(l);
            }
            const int64_t after = static_cast<int64_t>(LiveHeapBytes());
            if (rep > 0)
                runs.push_back(static_cast<double>(after - before) / 12.0);
        }
        std::printf("  %4zu characters: %9.1f bytes per layout\n", chars, Median(runs));
    }
}

int main(int argc, char** argv)
{
    CHECK(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    SetConsoleOutputCP(CP_UTF8);
    Gfx g;
    g.Init(520, 300);
    const bool varAvail = FamilyAvailable(g.dw.Get(), L"Segoe UI Variable Text") && FamilyAvailable(g.dw.Get(), L"Segoe UI Variable smallf");
    std::printf("Segoe UI Variable available: %d\n", varAvail);
    auto body = MakeFormat(g.dw.Get(), L"Segoe UI Variable Text", 13.0f);
    auto smallf = MakeFormat(g.dw.Get(), L"Segoe UI Variable smallf", 11.0f);

    const std::string mode = argc > 1 ? argv[1] : "pixels";
    if (mode == "pixels")
    {
        int total = 0, bad = 0;
        for (const auto& cs : Cases())
        {
            auto sep = MakeSep(g, body.Get(), smallf.Get(), cs.p, cs.s, cs.width);
            auto one = MakeSingle(g, body.Get(), smallf.Get(), cs.p, cs.s, cs.width, true);
            std::printf("%-24s sep: P h=%.4f lines=%u | S h=%.4f lines=%u || single: hp=%.4f(%u) hs=%.4f(%u) spacer=%.4f(size %.4f) ok=%d total=%.4f expected=%.4f\n",
                        cs.name, sep.mp.height, sep.mp.lineCount, sep.ms.height, sep.ms.lineCount, one.hp, one.pLines, one.hs, one.sLines, one.spacerHeight,
                        one.spacerSize, one.ok, one.m.height, std::ceil(sep.mp.height) + 3.0f + sep.ms.height);
            for (float dpi : {96.0f, 120.0f, 144.0f, 192.0f})
            {
                for (float oy : {8.0f, 8.37f})
                {
                    for (float ox : {12.0f, 12.41f})
                    {
                        auto a = RenderMain(g, sep, ox, oy, dpi);
                        auto b = RenderSingle(g, one, ox, oy, dpi);
                        auto d = Diff(a, b, g.w);
                        ++total;
                        if (d.diffPixels)
                        {
                            ++bad;
                            std::printf("    DIFF dpi=%.0f origin=(%.2f,%.2f): %zu px differ (ink %zu) maxDelta=%d bbox=(%d,%d)-(%d,%d)\n", dpi, ox, oy, d.diffPixels, d.inkPixels,
                                        d.maxDelta, d.minX, d.minY, d.maxX, d.maxY);
                        }
                    }
                }
            }
        }
        std::printf("pixel comparisons: %d, differing: %d\n", total, bad);
    }
    else if (mode == "memory")
        MemoryExperiment(g, body.Get(), smallf.Get());
    else if (mode == "heights")
        HeightsExperiment(g, body.Get(), smallf.Get());
    else if (mode == "tabs")
        TabsExperiment(g, body.Get(), smallf.Get());
    else if (mode == "brush")
        BrushExperiment(g, body.Get(), smallf.Get());
    else if (mode == "edges")
        EdgesExperiment(g, body.Get(), smallf.Get());
    else if (mode == "reflow")
        ReflowExperiment(g, body.Get(), smallf.Get());
    else if (mode == "fonts")
        FontsExperiment(g);
    else if (mode == "chars")
        CharsExperiment(g, body.Get());
    else if (mode == "steps")
        StepExperiment(g, body.Get(), smallf.Get());
    else if (mode == "timing")
        TimingExperiment(g, body.Get(), smallf.Get());
    return 0;
}








