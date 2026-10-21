#pragma once
#include "../../src/Controls/DxUi.RichSemanticGeometry.h"
#include "../Support/Scope.Tests.PreparedFlowchart.h"
#include "../Support/Scope.Tests.RichImageResources.h"
static void TestRichImageResourceAllocationFailures()
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto previous  = RichImageResourceTests::Semantic(L"![old](old.png)");
    const auto candidate = RichImageResourceTests::Semantic(L"![\u6771\U0001f600](one.png) ![](one.png) ![two](two.png) ![three](three.png)");
    const auto picture   = RichImageResourceTests::Picture();
    const auto finish    = [](RichImageResources& state)
    {
        while (! state.Advance())
        {
        }
    };
    const auto seed = [&]
    {
        auto state = std::make_unique<RichImageResources>();
        state->Begin(previous, L"C:\\docs\\note.md", 1u, 1u);
        finish(*state);
        std::vector<RichImageRequest> jobs(0u);
        state->Take(jobs);
        Check(jobs.size() == 1u && state->Complete(jobs[0], picture), "image failure fixture starts with ready pixels");
        return state;
    };
    const auto observe = [&](auto&& operation)
    {
        const bool oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        operation();
        countAllocations  = false;
        const auto result = allocations;
        Check(result > 0u && result < 512u, "image resource operation has an observed bounded allocation schedule");
        return result;
    };
    const auto inject = [&](std::size_t point, auto&& operation)
    {
        const auto oldFailure = failAllocationAfter;
        const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
        failAllocationAfter   = point;
        try
        {
            operation();
        }
        catch (const std::bad_alloc&)
        {
            return true;
        }
        return false;
    };
    auto measured    = seed();
    const auto stage = [&](RichImageResources& state)
    {
        state.Begin(candidate, L"C:\\docs\\note.md", 2u, 1u);
        finish(state);
    };
    const auto stages = observe([&] { stage(*measured); });
    for (std::size_t point = 0u; point < stages; ++point)
    {
        auto state        = seed();
        const auto held   = state->Snapshot();
        const bool failed = inject(point, [&] { stage(*state); });
        Check(failed && ! state->Pending() && state->Snapshot() == held, "failed image admission preserves ready pixels and retires incomplete staging");
        stage(*state);
        std::vector<RichImageRequest> jobs(0u);
        state->Take(jobs);
        Check(jobs.size() == 3u && state->Statistics().imageElements == 4u, "explicit admission retry publishes only current coalesced image jobs");
        for (const auto& job : jobs)
            Check(! job.Cancelled(), "retry never exposes abandoned stage tokens");
    }
    std::vector<RichImageRequest> measuredJobs(0u);
    const auto takes = observe([&] { measured->Take(measuredJobs); });
    for (std::size_t point = 0u; point < takes; ++point)
    {
        auto state = seed();
        stage(*state);
        std::vector<RichImageRequest> output(1u);
        const bool failed = inject(point, [&] { state->Take(output); });
        Check(failed && output.size() == 1u && ! output[0].Id() && state->Statistics().pendingSources == 3u,
              "failed image dispatch preserves caller output and every pending source");
        state->Take(output);
        Check(output.size() == 3u, "dispatch retry does not lose or duplicate a source");
    }
    const auto completes = observe([&] { Check(measured->Complete(measuredJobs[0], picture), "baseline image completion"); });
    for (std::size_t point = 0u; point < completes; ++point)
    {
        auto state = seed();
        stage(*state);
        std::vector<RichImageRequest> jobs(0u);
        state->Take(jobs);
        const auto held   = state->Snapshot();
        const bool failed = inject(point, [&] { state->Complete(jobs[0], picture); });
        Check(failed && state->Snapshot() == held && ! jobs[0].Cancelled() && state->Statistics().pendingSources == 3u,
              "failed image completion preserves the request and prior immutable publication");
        Check(state->Complete(jobs[0], picture) && jobs[0].Cancelled() && state->Snapshot()->placements.size() == 2u,
              "completion retry publishes both duplicate placements exactly once");
    }
    const auto before      = allocations;
    const auto oldCounting = countAllocations;
    countAllocations       = true;
    for (unsigned repeat = 0u; repeat < 16u; ++repeat)
    {
        measured->Begin(candidate, L"C:\\docs\\note.md", 2u, 1u);
        measured->Take(measuredJobs);
        static_cast<void>(measured->Statistics());
    }
    measured->Cancel();
    measured->Clear();
    countAllocations = oldCounting;
    Check(allocations == before, "coalesced image state, idle dispatch and retirement allocate no C++ storage");
    std::cout << "image resource allocation points stage=" << stages << " take=" << takes << " complete=" << completes << " idle/retire=0\n";
}

static void TestRichImageGeometryAllocationFailures()
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Check(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "image geometry initializes or borrows COM");
    const auto apartment = wil::scope_exit([&] noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    const auto write   = PreparedFlowchartTests::WriteFactory();
    const auto drawing = PreparedFlowchartTests::DrawingFactory();
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(480u, 240u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    const auto previous  = RichImageResourceTests::Semantic(L"previous\n");
    const auto candidate = RichImageResourceTests::Semantic(L"before ![\u6771\U0001f600](one.png) after\n\n![](one.png)\n");
    RichImageResources resources;
    resources.Begin(candidate, L"C:\\docs\\note.md", 1u, 1u);
    while (! resources.Advance())
    {
    }
    std::vector<RichImageRequest> jobs(0u);
    resources.Take(jobs);
    Check(jobs.size() == 1u && resources.Complete(jobs[0], RichImageResourceTests::Pattern()), "image geometry has one shared immutable decoded source");
    const auto images = resources.Snapshot();
    LayoutOptions options;
    options.widthDip  = 400.f;
    const auto finish = [](PreparedSemanticGeometry& geometry)
    {
        unsigned turns{};
        while (geometry.Pending())
        {
            geometry.Advance({1u, 3u});
            if (++turns > 10000u)
                throw std::runtime_error("image geometry did not settle");
        }
    };
    const auto seed = [&](PreparedSemanticGeometry& geometry)
    {
        geometry.Begin(write.get(), previous, options);
        finish(geometry);
    };
    const auto stage = [&](PreparedSemanticGeometry& geometry)
    {
        geometry.Begin(write.get(), candidate, options, drawing.get(), {1u, 1u}, images, target.get());
        finish(geometry);
    };
    std::size_t points{};
    {
        PreparedSemanticGeometry measured;
        seed(measured);
        const bool oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        stage(measured);
        countAllocations = false;
        points           = allocations;
    }
    Check(points > 0u && points < 4096u, "loaded Unicode image geometry has a bounded observed owned allocation schedule");
    for (std::size_t point = 0u; point < points; ++point)
    {
        PreparedSemanticGeometry geometry;
        seed(geometry);
        const auto generation = geometry.Generation();
        bool failed{};
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = point;
            try
            {
                stage(geometry);
            }
            catch (const std::bad_alloc&)
            {
                failed = true;
            }
        }
        Check(failed && geometry.Ready() && ! geometry.Pending() && geometry.Generation() == generation && &geometry.Document() == previous.get(),
              "each image geometry allocation failure preserves the previous complete layout and retires staging");
        stage(geometry);
        Check(geometry.Ready() && geometry.HasNativeObjects() && &geometry.Document() == candidate.get(),
              "image geometry explicitly retries after each failed allocation without changing source");
    }
    std::cout << "loaded image geometry allocation points=" << points << '\n';
}

namespace RichImageFailureTests
{
class RasterCalls final
{
public:
    RasterCalls() noexcept : previous_(DxUi::RichText::Detail::testRichImageRasterCalls), previousFixture_(active_)
    {
        calls_.createBitmap                              = &Create;
        active_                                          = this;
        DxUi::RichText::Detail::testRichImageRasterCalls = &calls_;
    }
    ~RasterCalls()
    {
        DxUi::RichText::Detail::testRichImageRasterCalls = previous_;
        active_                                          = previousFixture_;
    }
    RasterCalls(const RasterCalls&)            = delete;
    RasterCalls& operator=(const RasterCalls&) = delete;
    std::size_t uploads{}, failAt = std::numeric_limits<std::size_t>::max();
    HRESULT failure = E_OUTOFMEMORY;

private:
    static HRESULT Create(
        ID2D1RenderTarget* target, D2D1_SIZE_U size, const void* pixels, UINT32 stride, const D2D1_BITMAP_PROPERTIES& properties, ID2D1Bitmap** output) noexcept
    {
        const auto index = active_->uploads++;
        if (index == active_->failAt)
            return active_->failure;
        return target->CreateBitmap(size, pixels, stride, properties, output);
    }
    DxUi::RichText::Detail::RichImageRasterCalls calls_;
    const DxUi::RichText::Detail::RichImageRasterCalls* previous_;
    RasterCalls* previousFixture_;
    inline static thread_local RasterCalls* active_{};
};
} // namespace RichImageFailureTests
static void TestRichImageRasterFailureAndBudgets()
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Check(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "raster failure fixture initializes or borrows COM");
    const auto apartment = wil::scope_exit([&] noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    const auto write   = PreparedFlowchartTests::WriteFactory();
    const auto drawing = PreparedFlowchartTests::DrawingFactory();
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(480u, 240u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> target;
    const auto properties =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
    const auto document = RichImageResourceTests::Semantic(L"![first](one.png) ![second](two.png)\n\n![duplicate](one.png)\n");
    RichImageResources resources;
    resources.Begin(document, L"C:\\docs\\note.md", 1u, 1u);
    while (! resources.Advance())
    {
    }
    std::vector<RichImageRequest> jobs(0u);
    resources.Take(jobs);
    Check(jobs.size() == 2u && resources.Complete(jobs[0], RichImageResourceTests::Picture()) &&
              resources.Complete(jobs[1], RichImageResourceTests::Picture(48u, 32u)),
          "raster fixture has two independent decoded sources");
    const auto images = resources.Snapshot();
    RichImageFailureTests::RasterCalls calls;
    PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip  = 400.f;
    const auto begin  = [&](std::uint64_t epoch) { geometry.Begin(write.get(), document, options, drawing.get(), {epoch, epoch}, images, target.get()); };
    const auto finish = [&]
    {
        unsigned turns{};
        while (geometry.Pending())
        {
            const auto before  = calls.uploads;
            const auto advance = geometry.Advance({1u, 3u});
            if (calls.uploads - before > 1u || advance.layoutsPrepared > 1u || advance.blockSteps > 3u)
                std::cerr << "image owner budget: uploads=" << calls.uploads - before << " layouts=" << advance.layoutsPrepared
                          << " events=" << advance.blockSteps << '\n';
            Check(calls.uploads - before <= 1u && advance.layoutsPrepared <= 1u && advance.blockSteps <= 3u,
                  "each owner turn performs at most one bounded image upload and respects layout/event budgets");
            Check(++turns < 20000u, "native image raster preparation makes finite progress");
        }
        Check(geometry.Ready(), "native raster publishes complete geometry");
    };
    begin(1u);
    for (const auto invalidBudget : {LayoutWorkBudget{0u, 3u}, LayoutWorkBudget{1u, 0u}})
    {
        bool rejected{};
        try
        {
            geometry.Advance(invalidBudget);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        Check(rejected && geometry.Pending() && ! calls.uploads, "invalid semantic preparation budgets preserve staging without a bitmap upload");
    }
    finish();
    Check(calls.uploads == 2u && geometry.HasNativeObjects(), "duplicate placements share exactly two real native bitmap uploads");
    begin(1u);
    Check(! geometry.Pending() && calls.uploads == 2u, "identical image/target/epoch preparation coalesces without uploads");
    std::uint64_t epoch = 1u;
    for (const auto failure : {E_OUTOFMEMORY, DXGI_ERROR_DEVICE_REMOVED})
        for (const auto failureIndex : {0u, 1u})
        {
            const auto generation = geometry.Generation();
            calls.failure         = failure;
            calls.failAt          = calls.uploads + failureIndex;
            bool rejected{};
            try
            {
                begin(++epoch);
                finish();
            }
            catch (const wil::ResultException& error)
            {
                rejected = error.GetErrorCode() == failure;
            }
            calls.failAt = std::numeric_limits<std::size_t>::max();
            Check(rejected && geometry.Ready() && ! geometry.Pending() && geometry.Generation() == generation && &geometry.Document() == document.get(),
                  "first/later bitmap memory/device HRESULT failures preserve the prior complete publication");
            const auto before = calls.uploads;
            begin(epoch);
            finish();
            Check(calls.uploads - before == 2u && geometry.Generation() == generation + 1u,
                  "explicit raster retry recreates current bitmaps after each driver failure");
        }
    std::wstring source(0u, L' ');
    for (unsigned index = 0u; index < 129u; ++index)
        source.append(L"![small](same.png)\n\n");
    const auto maximum = RichImageResourceTests::Semantic(source);
    resources.Begin(maximum, L"C:\\docs\\note.md", 2u, 1u);
    while (! resources.Advance())
    {
    }
    resources.Take(jobs);
    Check(jobs.size() == 1u && resources.Complete(jobs[0], RichImageResourceTests::Picture(8u, 8u)), "maximum placement fixture shares one small image");
    const auto before = calls.uploads;
    geometry.Begin(write.get(), maximum, options, drawing.get(), {++epoch, epoch}, resources.Snapshot(), target.get());
    finish();
    Check(calls.uploads - before == 1u && resources.Statistics().imageElements == 128u,
          "maximum native placements upload one shared bitmap and retain the excess alternative");
    unsigned measured{};
    for (std::size_t index = 0u; index < maximum->Elements().size(); ++index)
        if (maximum->Elements()[index].role == SemanticRole::Image)
        {
            const auto bounds = geometry.ElementBounds(index);
            Check(bounds.has_value(), "maximum image and fallback alternatives retain reachable bounds");
            if (measured++ < 128u)
                Check(std::abs(bounds->right - bounds->left - 8.f) < .02f && std::abs(bounds->bottom - bounds->top - 8.f) < .02f,
                      "every admitted maximum placement preserves the actual image size");
        }
    Check(measured == 129u, "native placement capacity never removes source images");
    std::cout << "image raster driver failures=4 maximumPlacements=128 sharedUploads=1\n";
}
