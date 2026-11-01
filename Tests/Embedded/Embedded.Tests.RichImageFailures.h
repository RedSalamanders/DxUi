#pragma once
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Support.Tests.PreparedFlowchart.h"
#include "Support.Tests.RichImageResources.h"
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
        state->Publish();
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
    Check(measured->Complete(measuredJobs[0], picture), "stage baseline image completion");
    const auto publications = observe([&] { measured->Publish(); });
    for (std::size_t point = 0u; point < publications; ++point)
    {
        auto state = seed();
        stage(*state);
        std::vector<RichImageRequest> jobs(0u);
        state->Take(jobs);
        const auto held = state->Snapshot();
        Check(state->Complete(jobs[0], picture), "stage image completion before publication failure");
        const bool failed = inject(point, [&] { state->Publish(); });
        Check(failed && state->Snapshot() == held && jobs[0].Cancelled() && state->HasUnpublished() && state->Statistics().readySources == 1u,
              "failed publication retains the prior snapshot and committed private pixels for retry");
        state->Publish();
        Check(! state->HasUnpublished() && state->Snapshot()->placements.size() == 2u, "publication retry exposes both duplicate placements exactly once");
    }
    const auto replacement = RichImageResourceTests::Picture(2u, 2u);
    const auto stageFrame  = [&](RichImageResources& state, RichImageRequest& request)
    {
        state.Begin(previous, L"C:\\docs\\note.md", 1u, 1u);
        finish(state);
        std::vector<RichImageRequest> jobs(0u);
        state.Take(jobs);
        Check(jobs.size() == 1u && state.Complete(jobs[0], picture), "frame failure fixture completes its source");
        state.Publish();
        request = jobs[0];
        Check(request.Cancelled() && state.ReplaceFrame(request, replacement), "completed request stages an immutable replacement frame");
    };
    RichImageResources measuredFrame;
    RichImageRequest measuredFrameRequest;
    stageFrame(measuredFrame, measuredFrameRequest);
    const auto framePublications = observe([&] { measuredFrame.Publish(); });
    for (std::size_t point = 0u; point < framePublications; ++point)
    {
        RichImageResources state;
        RichImageRequest request;
        stageFrame(state, request);
        const auto held   = state.Snapshot();
        const bool failed = inject(point, [&] { state.Publish(); });
        Check(failed && state.Snapshot() == held && state.HasUnpublished() && state.Statistics().decodedBytes == replacement->Pixels().size(),
              "failed frame publication retains the prior snapshot and accepted replacement for retry");
        state.Publish();
        Check(! state.HasUnpublished() && state.Snapshot() != held && state.Snapshot()->placements[0u].image == replacement,
              "frame publication retry exposes the accepted replacement exactly once");
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
    std::cout << "image resource allocation points stage=" << stages << " take=" << takes << " publish=" << publications << " idle/retire=0\n";

    const auto remoteCandidate = RichImageResourceTests::Semantic(
        L"![local](one.png) ![remote](HTTPS://Example.test/Picture.png?rev=One) ![duplicate](https://example.test/Picture.png?rev=One) "
        L"![second](http://example.test/second.png) ![refused](https://user@example.test/no.png)");
    const auto remoteSeed = [&]
    {
        auto state = seed();
        state->SetRemoteImagesEnabled(true);
        return state;
    };
    const auto remoteStage = [&](RichImageResources& state)
    {
        state.Begin(remoteCandidate, L"C:\\docs\\note.md", 2u, 1u);
        finish(state);
    };
    auto remoteMeasured     = remoteSeed();
    const auto remoteStages = observe([&] { remoteStage(*remoteMeasured); });
    for (std::size_t point = 0u; point < remoteStages; ++point)
    {
        auto state        = remoteSeed();
        const auto held   = state->Snapshot();
        const bool failed = inject(point, [&] { remoteStage(*state); });
        Check(failed && ! state->Pending() && state->Snapshot() == held,
              "each mixed local/remote admission failure preserves ready pixels and retires incomplete staging");
        remoteStage(*state);
        std::vector<RichImageRequest> jobs(0u);
        state->Take(jobs);
        Check(jobs.size() == 3u && state->Statistics().imageElements == 4u,
              "mixed admission retry coalesces canonical remote sources and retains rejected alternatives");
        std::size_t localSources{}, remoteSources{};
        for (const auto& job : jobs)
        {
            Check(! job.Cancelled(), "mixed admission retry never exposes abandoned stage tokens");
            if (job.SourceKind() == RichImageSourceKind::LocalFile)
            {
                ++localSources;
                Check(job.Source() == L"C:\\docs\\one.png", "mixed admission retains the local source resolver");
            }
            else
            {
                ++remoteSources;
                Check(job.FilePath().empty() &&
                          (job.Source() == L"https://example.test/Picture.png?rev=One" || job.Source() == L"http://example.test/second.png"),
                      "remote resolution normalizes only scheme/authority and preserves exact path/query spelling");
            }
            Check(state->Complete(job, picture), "mixed admission retry accepts each current completion");
        }
        state->Publish();
        Check(localSources == 1u && remoteSources == 2u && state->Snapshot()->placements.size() == 4u,
              "mixed admission retry publishes both duplicate remote placements exactly once");
    }
    std::cout << "mixed local/remote image admission allocation points=" << remoteStages << '\n';
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
    resources.Publish();
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
        calls_.copyBitmap                                = &Copy;
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
    std::size_t copies{}, copiedBytes{}, widestTile{}, tallestTile{}, failCopyAt = std::numeric_limits<std::size_t>::max();

private:
    static HRESULT Create(
        ID2D1RenderTarget* target, D2D1_SIZE_U size, const void* pixels, UINT32 stride, const D2D1_BITMAP_PROPERTIES& properties, ID2D1Bitmap** output) noexcept
    {
        const auto index = active_->uploads++;
        if (index == active_->failAt)
            return active_->failure;
        return target->CreateBitmap(size, pixels, stride, properties, output);
    }
    static HRESULT Copy(ID2D1Bitmap* bitmap, const D2D1_RECT_U& rect, const void* pixels, UINT32 stride) noexcept
    {
        const auto index = active_->copies++;
        const auto width = rect.right - rect.left, height = rect.bottom - rect.top;
        active_->copiedBytes += static_cast<size_t>(width) * height * 4u;
        active_->widestTile  = std::max(active_->widestTile, static_cast<size_t>(width));
        active_->tallestTile = std::max(active_->tallestTile, static_cast<size_t>(height));
        if (index == active_->failCopyAt)
            return active_->failure;
        return bitmap->CopyFromMemory(&rect, pixels, stride);
    }
    DxUi::RichText::Detail::RichImageRasterCalls calls_;
    const DxUi::RichText::Detail::RichImageRasterCalls* previous_;
    RasterCalls* previousFixture_;
    inline static thread_local RasterCalls* active_{};
};
} // namespace RichImageFailureTests
static void TestRichImageFrameHostFailure()
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    GraphicsFixture gpu;
    gpu.width  = 480u;
    gpu.height = 240u;
    Hr(gpu.Create(), "create frame replacement WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create frame replacement graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach frame replacement host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetDocumentFilePath(L"C:\\docs\\note.md");
    editor->SetText(L"![frame](image.png)");
    Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "prepare frame source");
    std::vector<RichImageRequest> requests(0u);
    editor->TakePendingImageRequests(requests);
    Check(requests.size() == 1u && editor->CompleteImageRequest(requests[0], RichImageResourceTests::Pattern()), "complete initial frame");
    Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "prepare initial frame");
    const auto document = editor->GetAcknowledgedSemanticDocument(view.Controls());
    Check(static_cast<bool>(document), "initial frame has an acknowledged document");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose initial frame");
    std::vector<uint8_t> initial(0u);
    Hr(gpu.Read(initial), "read initial frame pixels");
    const auto source    = editor->GetText();
    const auto revision  = editor->Revision();
    const auto selection = editor->GetSelection();
    Check(editor->ReplaceImageFrame(view.Controls(), document, requests[0], RichImageResourceTests::Picture()),
          "host accepts a current completed-request frame");
    Check(! editor->GetAcknowledgedSemanticDocument(view.Controls()) && editor->GetText() == source && editor->Revision() == revision &&
              editor->GetSelection() == selection,
          "pending frame revokes geometry actions without editing source or selection");
    RichImageFailureTests::RasterCalls calls;
    calls.failAt   = 0u;
    HRESULT result = S_OK;
    for (unsigned turn = 0u; turn < 1024u && ! calls.uploads && SUCCEEDED(result); ++turn)
        result = view.Prepare(gpu.width, gpu.height);
    Check(calls.uploads && FAILED(result) && ! editor->GetAcknowledgedSemanticDocument(view.Controls()),
          "failed frame upload leaves current semantic actions unavailable");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "failed host preparation refuses to present an incoherent surface");
    calls.failAt = std::numeric_limits<size_t>::max();
    Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "retry replacement frame after upload failure");
    Check(editor->GetAcknowledgedSemanticDocument(view.Controls()) == document && editor->GetText() == source && editor->Revision() == revision &&
              editor->GetSelection() == selection,
          "successful frame retry restores acknowledged geometry without a source transaction");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose retried replacement frame");
    std::vector<uint8_t> updated(0u);
    Hr(gpu.Read(updated), "read replacement frame pixels");
    Check(updated != initial, "retried frame changes the actual embedded host surface");
}
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
    resources.Publish();
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
    {
        // Math family is irrelevant to the v0 public text path but remains
        // admitted by the semantic path before its layout can publish.
        LayoutOptions invalidMath = options;
        invalidMath.mathFontFamily.clear();
        Document plainDocument(L"ordinary text");
        PreparedLayout plain;
        plain.Prepare(write.get(), Project(plainDocument.Snapshot(), plainDocument.Revision(), ViewMode::Source), invalidMath);
        Check(plain.Ready(), "v0 text layout ignores the semantic-only math family");
        PreparedSemanticGeometry semantic;
        semantic.Begin(write.get(), RichImageResourceTests::Semantic(L"ordinary text"), invalidMath);
        bool rejected = false;
        try
        {
            while (semantic.Pending())
                semantic.Advance({1u, 32u});
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        Check(rejected && ! semantic.Ready() && ! semantic.Pending(), "semantic layout rejects an invalid math family without publication");
    }
    begin(1u);
    for (const auto invalidBudget : {LayoutWorkBudget{0u, 3u},
                                     LayoutWorkBudget{1u, 0u},
                                     LayoutWorkBudget{1u, 3u, 0u},
                                     LayoutWorkBudget{1u, 3u, 1u},
                                     LayoutWorkBudget{1u, 3u, 2u},
                                     LayoutWorkBudget{1u, 3u, 3u}})
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
    for (const auto failure : {E_OUTOFMEMORY, DXGI_ERROR_DEVICE_REMOVED, D2DERR_RECREATE_TARGET, E_FAIL})
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
    // A patterned source shared by two placements tests tile boundaries and
    // pixel parity independently of image admission and URI de-duplication.
    const auto tiledDocument = RichImageResourceTests::Semantic(L"![first](pattern.png) ![again](pattern.png)\n");
    resources.Begin(tiledDocument, L"C:\\docs\\note.md", 1u, 1u);
    while (! resources.Advance())
    {
    }
    resources.Take(jobs);
    Check(jobs.size() == 1u && resources.Complete(jobs[0], RichImageResourceTests::Pattern()), "tiled upload uses one shared patterned image");
    resources.Publish();
    const auto tiledImages = resources.Snapshot();
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.put()));
    const auto render = [&](const PreparedSemanticGeometry& value)
    {
        target->BeginDraw();
        target->Clear(D2D1::ColorF(D2D1::ColorF::White));
        value.Draw(target.get(), brush.get(), brush.get(), D2D1::Point2F(), 0.f, 240.f);
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(480u * 240u * 4u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 480u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
        return pixels;
    };
    const auto runTiled = [&](PreparedSemanticGeometry& value, size_t byteBudget)
    {
        unsigned turns{};
        size_t transferred{};
        while (value.Pending())
        {
            const auto beforeCopies = calls.copies;
            const auto beforeBytes  = calls.copiedBytes;
            const auto result       = value.Advance({1u, 32u, byteBudget});
            transferred += result.uploadedBytes;
            Check(result.uploadedBytes <= byteBudget && calls.copies - beforeCopies <= 1u && calls.copiedBytes - beforeBytes <= byteBudget &&
                      result.layoutsPrepared <= 1u && result.blockSteps <= 32u,
                  "each tiled image turn respects byte, copy, layout and event budgets");
            Check(++turns < 30000u, "tiled image preparation makes finite progress");
        }
        return transferred;
    };
    PreparedSemanticGeometry whole, tiled;
    whole.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(whole, 4u * 1024u * 1024u);
    const auto expectedPixels      = render(whole);
    options.widthDip               = 360.f;
    const auto beforeReflowUploads = calls.uploads, beforeReflowCopies = calls.copies;
    whole.Begin(write.get(), tiledDocument, options, drawing.get(), {epoch, epoch}, tiledImages, target.get());
    const auto reflowBytes = runTiled(whole, 28u);
    Check(! reflowBytes && calls.uploads == beforeReflowUploads && calls.copies == beforeReflowCopies,
          "width-only reflow reuses completed raster bitmaps without uploading or copying tiles");
    const auto reflowPixels = render(whole);
    PreparedSemanticGeometry fresh;
    fresh.Begin(write.get(), tiledDocument, options, drawing.get(), {epoch, epoch}, tiledImages, target.get());
    runTiled(fresh, 4u * 1024u * 1024u);
    Check(render(fresh) == reflowPixels, "reused bitmap reflow matches independently prepared geometry pixels");
    const auto beforeEpoch = calls.uploads;
    whole.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(whole, 4u * 1024u * 1024u);
    Check(calls.uploads == beforeEpoch + 1u && render(whole) == reflowPixels, "changed target epoch uploads a new bitmap with identical pixels");
    wil::com_ptr<IWICBitmap> replacementBitmap;
    THROW_IF_FAILED(imaging->CreateBitmap(480u, 240u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, replacementBitmap.put()));
    wil::com_ptr<ID2D1RenderTarget> replacementTarget;
    THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(replacementBitmap.get(), properties, replacementTarget.put()));
    const auto beforeTarget = calls.uploads;
    whole.Begin(write.get(), tiledDocument, options, drawing.get(), {epoch, epoch}, tiledImages, replacementTarget.get());
    runTiled(whole, 4u * 1024u * 1024u);
    Check(calls.uploads == beforeTarget + 1u, "changed render target uploads a new bitmap even at the same epoch");
    options.widthDip          = 400.f;
    const auto initialUploads = calls.uploads;
    const auto initialCopies  = calls.copies;
    calls.widestTile = calls.tallestTile = 0u;
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(tiled, 28u);
    Check(calls.uploads - initialUploads == 1u && calls.copies - initialCopies > 64u && calls.widestTile == 7u && calls.tallestTile == 1u,
          "sub-row byte budget creates one bitmap and horizontally tiled copies shared by duplicate placements");
    Check(render(tiled) == expectedPixels, "horizontal tiled uploads retain every patterned pixel");
    options.widthDip             = 360.f;
    const auto afterTilesUploads = calls.uploads, afterTilesCopies = calls.copies;
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {epoch, epoch}, tiledImages, target.get());
    const auto tiledReflowBytes = runTiled(tiled, 28u);
    Check(! tiledReflowBytes && calls.uploads == afterTilesUploads && calls.copies == afterTilesCopies && render(tiled) == reflowPixels,
          "width reflow shares a fully tiled bitmap without further copies and preserves fresh geometry pixels");
    options.widthDip      = 400.f;
    const auto rowUploads = calls.uploads;
    calls.widestTile = calls.tallestTile = 0u;
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(tiled, 96u * 4u * 3u);
    Check(calls.uploads - rowUploads == 1u && calls.widestTile == 96u && calls.tallestTile == 3u && render(tiled) == expectedPixels,
          "whole-width row tiles share one bitmap and preserve the pattern");
    for (const auto failure : {E_OUTOFMEMORY, DXGI_ERROR_DEVICE_REMOVED, D2DERR_RECREATE_TARGET, E_FAIL})
    {
        const auto generation = tiled.Generation();
        calls.failure         = failure;
        calls.failCopyAt      = calls.copies + 2u;
        bool rejected{};
        try
        {
            tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
            runTiled(tiled, 96u * 4u * 3u);
        }
        catch (const wil::ResultException& error)
        {
            rejected = error.GetErrorCode() == failure;
        }
        calls.failCopyAt = std::numeric_limits<size_t>::max();
        Check(rejected && tiled.Ready() && ! tiled.Pending() && tiled.Generation() == generation && render(tiled) == expectedPixels,
              "failed later image tile retires staging and retains the previous complete pixels");
        tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {epoch, epoch}, tiledImages, target.get());
        runTiled(tiled, 96u * 4u * 3u);
        Check(tiled.Generation() == generation + 1u && render(tiled) == expectedPixels, "failed tile retries to complete pixel parity");
    }
    const auto nextFrame = RichImageResourceTests::Picture();
    Check(jobs[0].Cancelled() && resources.ReplaceFrame(jobs[0], nextFrame), "completed image request accepts the next animation frame");
    resources.Publish();
    const auto nextFrameImages = resources.Snapshot();
    Check(nextFrameImages != tiledImages && nextFrameImages->placements.size() == 2u && nextFrameImages->placements[0u].image == nextFrame &&
              nextFrameImages->placements[1u].image == nextFrame,
          "new immutable image snapshot shares the next frame across both placements");
    const auto oldGeneration = tiled.Generation();
    calls.failure            = E_OUTOFMEMORY;
    calls.failCopyAt         = calls.copies + 1u;
    bool frameRejected{};
    try
    {
        tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, nextFrameImages, target.get());
        runTiled(tiled, 96u * 4u * 3u);
    }
    catch (const wil::ResultException& error)
    {
        frameRejected = error.GetErrorCode() == E_OUTOFMEMORY;
    }
    calls.failCopyAt = std::numeric_limits<size_t>::max();
    Check(frameRejected && tiled.Ready() && ! tiled.Pending() && tiled.Generation() == oldGeneration && render(tiled) == expectedPixels,
          "failed next-frame upload retains the previous prepared geometry and exact pixels");
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {epoch, epoch}, nextFrameImages, target.get());
    runTiled(tiled, 96u * 4u * 3u);
    Check(tiled.Generation() == oldGeneration + 1u && render(tiled) != expectedPixels, "retry publishes the accepted replacement frame after target recovery");
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(tiled, 96u * 4u * 3u);
    Check(render(tiled) == expectedPixels, "original immutable frame remains independently reusable");
    calls.failure                      = D2DERR_UNSUPPORTED_OPERATION;
    calls.failCopyAt                   = calls.copies + 1u;
    const auto copiesBeforeUnsupported = calls.copies;
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(tiled, 96u * 4u * 3u);
    calls.failCopyAt = std::numeric_limits<size_t>::max();
    Check(tiled.Ready() && calls.copies == copiesBeforeUnsupported + 2u,
          "unsupported tile retires its partial bitmap and both duplicate placements use alternatives");
    tiled.Begin(write.get(), tiledDocument, options, drawing.get(), {++epoch, epoch}, tiledImages, target.get());
    runTiled(tiled, 96u * 4u * 3u);
    Check(tiled.HasNativeObjects() && render(tiled) == expectedPixels, "new generation retries an unsupported tiled image");
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
    resources.Publish();
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
    const auto mixedSource = RichImageResourceTests::MixedCapacitySource();
    const auto mixed       = RichImageResourceTests::Semantic(mixedSource);
    resources.Begin(mixed, L"C:\\docs\\note.md", 3u, 1u);
    while (! resources.Advance())
    {
    }
    resources.Take(jobs);
    Check(jobs.size() == 1u && resources.Complete(jobs[0], RichImageResourceTests::Picture(8u, 8u)),
          "mixed native-object ceiling completes a shared image source");
    resources.Publish();
    const auto mixedImages = resources.Snapshot();
    Check(mixedImages->placements.size() == 125u, "two formulas and a diagram leave 125 admitted image placements");
    geometry.Begin(write.get(), mixed, options, drawing.get(), {++epoch, epoch}, mixedImages, target.get());
    finish();
    Check(geometry.HasNativeObjects() && mixed->Map().Source() == mixedSource,
          "mixed admitted native objects prepare without global capacity failure and preserve all source alternatives");
    std::size_t mixedImageElements{}, mixedMathElements{}, mixedDiagramElements{};
    for (std::size_t index = 0u; index < mixed->Elements().size(); ++index)
    {
        const auto role = mixed->Elements()[index].role;
        if (role == SemanticRole::Image || role == SemanticRole::Math || role == SemanticRole::Diagram)
        {
            Check(geometry.ElementBounds(index).has_value(), "every mixed native object or source alternative has geometry");
            mixedImageElements += role == SemanticRole::Image ? 1u : 0u;
            mixedMathElements += role == SemanticRole::Math ? 1u : 0u;
            mixedDiagramElements += role == SemanticRole::Diagram ? 1u : 0u;
        }
    }
    Check(mixedImageElements == 126u && mixedMathElements == 2u && mixedDiagramElements == 1u,
          "the over-limit image remains an accessible alternative alongside admitted native objects");
    options.widthDip = 260.f;
    geometry.Begin(write.get(), mixed, options, drawing.get(), {++epoch, epoch}, mixedImages, target.get());
    finish();
    Check(resources.Begin(mixed, L"C:\\docs\\note.md", 3u, 1u), "width reflow retains the current image resource generation");
    resources.Take(jobs);
    Check(jobs.empty() && resources.Snapshot() == mixedImages && geometry.Ready(),
          "width reflow publishes geometry without re-requesting completed image sources");
    options.widthDip = 400.f;
    resources.Begin(maximum, L"C:\\docs\\note.md", 2u, 1u);
    while (! resources.Advance())
    {
    }
    resources.Take(jobs);
    Check(jobs.size() == 1u && resources.Complete(jobs[0], RichImageResourceTests::Picture(8u, 8u)),
          "restore the independent maximum-image fixture before upload refusal checks");
    resources.Publish();
    const auto unsupportedBefore = calls.uploads;
    calls.failure                = D2DERR_UNSUPPORTED_OPERATION;
    calls.failAt                 = calls.uploads;
    geometry.Begin(write.get(), maximum, options, drawing.get(), {++epoch, epoch}, resources.Snapshot(), target.get());
    finish();
    Check(calls.uploads - unsupportedBefore == 1u && maximum->Map().Source() == source,
          "unsupported upload is tried once for all duplicate image pixels while exact source remains");
    const auto fallbackGeneration = geometry.Generation();
    for (std::size_t index = 0u; index < maximum->Elements().size(); ++index)
        if (maximum->Elements()[index].role == SemanticRole::Image)
            Check(geometry.ElementBounds(index).has_value(), "unsupported upload keeps every alternative reachable in native geometry");
    geometry.Begin(write.get(), maximum, options, drawing.get(), {epoch, epoch}, resources.Snapshot(), target.get());
    Check(! geometry.Pending() && geometry.Generation() == fallbackGeneration && calls.uploads == unsupportedBefore + 1u,
          "unchanged unsupported image target does not retry during idle");
    calls.failAt = std::numeric_limits<std::size_t>::max();
    geometry.Begin(write.get(), maximum, options, drawing.get(), {++epoch, epoch}, resources.Snapshot(), target.get());
    finish();
    Check(calls.uploads == unsupportedBefore + 2u, "new resource key retries and recovers the image upload");
    std::cout << "image raster driver failures=4 maximumPlacements=128 sharedUploads=1\n";
}
