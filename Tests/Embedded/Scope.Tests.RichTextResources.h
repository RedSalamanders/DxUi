#pragma once
#include "../Support/Scope.Tests.FlowchartLayout.h"
#include "../Support/Scope.Tests.Footnotes.h"
#include "../Support/Scope.Tests.MarkdownBlocks.h"
#include "../Support/Scope.Tests.MarkdownCapacity.h"
#include "../Support/Scope.Tests.MarkdownInlines.h"
#include "../Support/Scope.Tests.PreparedFlowchart.h"
#include "../Support/Scope.Tests.RichImage.h"
#include "../Support/Scope.Tests.RichImagePaths.h"
#include "../Support/Scope.Tests.RichImageResources.h"
#include "../Support/Scope.Tests.SemanticDiagrams.h"
#include "../Support/Scope.Tests.SemanticSourceBlock.h"
#include <DxUi/RichSemanticDocument.h>

static void TestRichImageDecoderAllocationFailures()
{
    using DxUi::RichText::RichImage;
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Check(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "image failure fixture initializes or borrows COM");
    const auto apartment = wil::scope_exit([&] noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> factory;
    Hr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())), "image failure factory");
    std::array<BYTE, 24u> source{};
    for (unsigned i = 0u; i < 6u; ++i)
    {
        source[i * 4u]      = static_cast<BYTE>((i + 1u) * 30u);
        source[i * 4u + 3u] = 255u;
    }
    const auto encoded = RichImageTests::Encode(factory.get(), GUID_ContainerFormatPng, source, 2u, 3u);
    std::shared_ptr<const RichImage> image;
    const auto sweep = [&](auto&& request)
    {
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const bool oldCounting = countAllocations;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const auto result             = request();
        countAllocations              = false;
        Hr(result, "baseline image allocation schedule");
        const auto requests = allocations;
        Check(requests > 0u && requests < 128u, "immutable pixel creation has a bounded observed allocation schedule");
        for (std::size_t point = 0u; point < requests; ++point)
        {
            const auto held = image;
            HRESULT failed{};
            {
                const auto previous = failAllocationAfter;
                const auto reset    = wil::scope_exit([=] noexcept { failAllocationAfter = previous; });
                failAllocationAfter = point;
                failed              = request();
            }
            Check(failed == E_OUTOFMEMORY && image == held, "every owned image allocation failure preserves the immutable resource");
            Hr(request(), "explicit image retry");
            Check(image->Width() == 2u && image->Height() == 3u && image->Pixels()[20u] == std::byte(180u), "image retry retains exact known pixel contents");
        }
        std::cout << "immutable image allocation points=" << requests << '\n';
    };
    sweep([&] { return RichImage::Decode(factory.get(), std::as_bytes(std::span(encoded)), image); });
    sweep([&] { return RichImage::CreatePbgra(2u, 3u, std::as_bytes(std::span(source)), image); });
}
static void TestRichImagePathAllocationFailures()
{
    using namespace DxUi;
    const auto sweep = [&](auto&& action)
    {
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const bool oldCounting = countAllocations;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        action(false);
        countAllocations    = false;
        const auto requests = allocations;
        Check(requests > 0u && requests < 512u, "image preparation has a bounded observed allocation schedule");
        for (std::size_t point = 0u; point < requests; ++point)
        {
            const auto oldFailure = failAllocationAfter;
            {
                const auto recover  = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter = point;
                action(true);
            }
            action(false);
        }
        std::cout << "image preparation allocation points=" << requests << '\n';
    };
    sweep([&](bool failure)
    {
        bool rejected{};
        try
        {
            const auto result =
                RichText::Detail::BuildMarkdownImagePath(L"C:\\notes\\chapters\\note.md", L"\\\\?\\C:\\notes\\images\\\u6771\U0001f600 [x]#%.png");
            Check(! failure && result && *result == L"![\u6771\U0001f600 \\[x\\]\\#\\%](../images/%E6%9D%B1%F0%9F%98%80%20%5Bx%5D%23%25.png)",
                  "image destination and escaped source retry exactly");
        }
        catch (const std::bad_alloc&)
        {
            rejected = true;
        }
        Check(rejected == failure, "every observed image source allocation failure propagates without termination");
    });
    constexpr wchar_t list[] = L"C:\\images\\\u6771\U0001f600.png\0D:\\images\\other.jpg\0";
    const auto bytes         = RichImagePathTests::DropBuffer(std::wstring_view(list, std::size(list)));
    TextClipboardContent content;
    sweep([&](bool failure)
    {
        const auto result = DecodeClipboardFilePaths(bytes, content);
        Check(failure ? result == E_OUTOFMEMORY : result == S_OK, "file list allocation failures return a named HRESULT");
        Check(content.filePaths.size() == 2u && content.filePaths[0] == L"C:\\images\\\u6771\U0001f600.png",
              "failed file decode leaves previously published content exact");
    });
    sweep([&](bool failure)
    {
        bool rejected{};
        try
        {
            const auto path = DxUi::RichText::Detail::ResolveLocalImagePath(L"C:\\notes\\chapters\\note.md", L"../images/%E6%9D%B1%F0%9F%98%80.png");
            Check(! failure && path && *path == L"C:\\notes\\images\\\u6771\U0001f600.png", "URI/base resolution retries exact Unicode paths");
        }
        catch (const std::bad_alloc&)
        {
            rejected = true;
        }
        Check(rejected == failure, "URI/base resolution propagates every observed allocation failure");
    });
}

static void TestFootnoteBlockAllocationFailures(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto finish = [](auto& state)
    {
        while (state.HasPendingPreparation())
            state.Advance(7u);
    };
    MarkdownParser baseline;
    baseline.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
    finish(baseline);
    const auto oldSyntax = baseline.Published();
    std::size_t total{};
    const auto sweep = [&](auto make, auto request, auto sourceOf, std::wstring_view expected)
    {
        std::size_t requests{};
        {
            auto state             = make();
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            request(*state);
            finish(*state);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 2048u, "note preparation has a finite independently observed allocation schedule");
        for (std::size_t point = 0u; point < requests; ++point)
        {
            if (trace)
                std::fprintf(stderr, "note block allocation failure=%zu/%zu\n", point, requests);
            auto state     = make();
            const auto old = state->Published();
            bool rejected{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = point;
                try
                {
                    request(*state);
                    finish(*state);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && state->Published() == old && ! state->HasPendingPreparation() && sourceOf(old) == L"previous",
                  "every note parser/semantic allocation failure preserves the held source and retires staging");
            request(*state);
            finish(*state);
            Check(sourceOf(state->Published()) == expected, "note preparation can explicitly retry after every observed allocation failure");
        }
        total += requests;
    };
    for (const auto* source : {L"[^n]: first\r\n\r\n    second\r\n",
                               L"- [^n]:\n      > - nested\n      >\n      >   body\n",
                               L"[^\u03b2\U0001f600]: **body**\n[^m]: next\n",
                               L"main[^X] and[^missing]\n\n[^x]: body[^y]\n[^X]: duplicate\n[^y]: body[^x]\n",
                               L"> - [^n]: **note**\n\nmain <a href='#x'>[^n]</a> again[^n]\n"})
    {
        sweep(
            [&]
        {
            auto value = std::make_unique<MarkdownParser>();
            value->Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
            finish(*value);
            return value;
        },
            [&](auto& value) { value.Begin(source, 2u, MarkdownProfile::RichMarkdown); },
            [](const auto& value) { return value->Source(); },
            source);
        MarkdownParser parser;
        parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
        finish(parser);
        const auto syntax = parser.Published();
        sweep(
            [&]
        {
            auto value = std::make_unique<SemanticBuilder>();
            value->Begin(oldSyntax);
            finish(*value);
            return value;
        },
            [&](auto& value) { value.Begin(syntax); },
            [](const auto& value) { return value->Map().Source(); },
            source);
    }
    std::cout << "note block/binding parser/semantic allocation scenarios=10 sweptRequests=" << total << " sourcePreserved=1 retry=1\n";
}

static void TestScopedSemanticSourceAllocationFailures(bool trace = false)
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto oldSyntax      = RichSemanticDocumentTests::Parse(L"previous", MarkdownProfile::RichMarkdown);
    const auto pendingSyntax  = RichSemanticDocumentTests::Parse(L"pending", MarkdownProfile::RichMarkdown);
    const auto finishSemantic = [](auto& builder)
    {
        while (builder.HasPendingPreparation())
            builder.Advance(7u);
    };
    std::size_t semanticTotal{}, geometryTotal{}, parserTotal{};
    const auto sweep = [&](auto make, auto request, auto finish, auto pending, auto key, auto verify, auto clear)
    {
        auto measured = make();
        std::size_t requests{};
        {
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=]() noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            request(*measured);
            finish(*measured);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 4096u, "scoped source preparation has a finite independently observed allocation schedule");
        for (std::size_t point = 0u; point < requests; ++point)
        {
            if (trace)
                std::fprintf(stderr, "scoped source failure=%zu/%zu\n", point, requests);
            auto state     = make();
            const auto old = key(*state);
            bool rejected{}, began{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = point;
                try
                {
                    request(*state);
                    began = true;
                    finish(*state);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && key(*state) == old && pending(*state) != began,
                  "scoped source failures preserve publication/generation and retain old pending work only when Begin failed");
            if (! began)
                finish(*state);
            request(*state);
            finish(*state);
            verify(*state);
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = 0u;
                clear(*state);
            }
        }
        return requests;
    };
    for (const auto source : {L"", L"~~broken\r\n[^n]: **&amp;**\r\n---\r\n", L"> ```\nmissing prefix\n> ```\n"})
    {
        const Document model(source);
        const auto finishParser = [](auto& parser)
        {
            while (parser.HasPendingPreparation())
                parser.Advance(1u);
        };
        parserTotal += sweep(
            [&]
        {
            auto parser = std::make_unique<MarkdownParser>();
            parser->Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
            finishParser(*parser);
            parser->Begin(L"pending", 2u, MarkdownProfile::RichMarkdown);
            return parser;
        },
            [&](auto& parser) { parser.BeginSourceRegion(model.Snapshot(), model.Revision(), {{0u, model.Length()}, 19u}); },
            finishParser,
            [](const auto& parser) { return parser.HasPendingPreparation(); },
            [](const auto& parser) { return std::pair{parser.Published().get(), std::uint64_t{}}; },
            [&](const auto& parser)
        {
            Check(parser.Published()->SourceRegion() == MarkdownSourceRegion{{0u, model.Length()}, 19u},
                  "opaque parser source retry restores the requested region, including empty EOF and malformed syntax");
        },
            [](auto& parser)
        {
            parser.Cancel();
            parser.Clear();
        });
    }
    for (const auto scenario : {0u, 2u, 4u, 6u, 8u, 13u})
    {
        const auto syntax = RichSemanticDocumentTests::Parse(SemanticSourceBlockTests::cases[scenario], MarkdownProfile::RichMarkdown);
        const auto node   = SemanticSourceBlockTests::SourceBlock(*syntax);
        semanticTotal += sweep(
            [&]
        {
            auto builder = std::make_unique<SemanticBuilder>();
            builder->Begin(oldSyntax);
            finishSemantic(*builder);
            builder->Begin(pendingSyntax);
            return builder;
        },
            [&](auto& builder) { builder.Begin(syntax, node); },
            finishSemantic,
            [](const auto& builder) { return builder.HasPendingPreparation(); },
            [](const auto& builder) { return std::pair{builder.Published().get(), std::uint64_t{}}; },
            [&](const auto& builder)
        {
            Check(builder.Published()->Syntax() == syntax && builder.Published()->SourceBlock() == node,
                  "explicit source-mode retry restores the requested exact scope");
        },
            [](auto& builder)
        {
            builder.Cancel();
            builder.Clear();
        });
    }
    const auto write          = PreparedFlowchartTests::WriteFactory();
    const auto drawing        = PreparedFlowchartTests::DrawingFactory();
    const auto oldDocument    = SemanticDiagramTests::Build(L"previous", Check);
    const auto finishGeometry = [](auto& geometry)
    {
        while (geometry.Pending())
            geometry.Advance({1u, 3u});
    };
    for (const auto scenario : {0u, 13u})
    {
        const auto syntax = RichSemanticDocumentTests::Parse(SemanticSourceBlockTests::cases[scenario], MarkdownProfile::RichMarkdown);
        const auto node   = SemanticSourceBlockTests::SourceBlock(*syntax);
        SemanticBuilder builder;
        builder.Begin(syntax, node);
        finishSemantic(builder);
        const auto document = builder.Published();
        LayoutOptions options;
        options.widthDip = 1024.f;
        geometryTotal += sweep(
            [&]
        {
            auto geometry       = std::make_unique<PreparedSemanticGeometry>();
            auto oldOptions     = options;
            oldOptions.widthDip = 800.f;
            geometry->Begin(write.get(), oldDocument, oldOptions, drawing.get(), {1u, 1u});
            finishGeometry(*geometry);
            oldOptions.widthDip = 900.f;
            geometry->Begin(write.get(), oldDocument, oldOptions, drawing.get(), {2u, 2u});
            return geometry;
        },
            [&](auto& geometry) { geometry.Begin(write.get(), document, options, drawing.get(), {3u, 3u}); },
            finishGeometry,
            [](const auto& geometry) { return geometry.Pending(); },
            [](const auto& geometry) { return std::pair{&geometry.Document(), geometry.Generation()}; },
            [&](const auto& geometry)
        {
            Check(geometry.Ready() && &geometry.Document() == document.get() && geometry.HasNativeObjects() == (scenario == 13u),
                  "source-mode geometry retries with the same exact raw scope and unaffected neighboring native graph");
        },
            [](auto& geometry)
        {
            geometry.Cancel();
            geometry.Invalidate();
            geometry.Clear();
        });
    }
    std::cout << "scoped source failure parserScenarios=3 sweptRequests=" << parserTotal << " semanticScenarios=6 sweptRequests=" << semanticTotal
              << " geometryScenarios=2 sweptRequests=" << geometryTotal << " preservedPublication=1 preservedPendingBegin=1 retry=1 retirementAllocations=0\n";
}

static void TestSemanticDiagramAllocationFailures(bool trace = false)
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto oldSyntax      = RichSemanticDocumentTests::Parse(L"previous", MarkdownProfile::RichMarkdown);
    const auto pendingSyntax  = RichSemanticDocumentTests::Parse(L"pending", MarkdownProfile::RichMarkdown);
    const auto finishSemantic = [](SemanticBuilder& builder)
    {
        while (builder.HasPendingPreparation())
            builder.Advance(7u);
    };
    // Count the actual allocation schedule, including native parsing, captions,
    // normalized-to-raw provenance, diagnostic publication and footer ownership.
    const std::array<std::wstring_view, 4u> sources = {SemanticDiagramTests::scenarios[5u].source,
                                                       L"main[^n]\r\n\r\n[^n]: note\r\n    > ```mermaid\r\n    > graph LR;A{wide}-->|yes|B\r\n    > ```\r\n",
                                                       SemanticDiagramTests::scenarios[11u].source,
                                                       SemanticDiagramTests::scenarios[8u].source};
    const auto observe                              = [](auto&& operation)
    {
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=]() noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        operation();
        countAllocations = false;
        return allocations;
    };
    std::size_t semanticRequests{}, geometryRequests{};
    for (const auto source : sources)
    {
        const auto syntax = RichSemanticDocumentTests::Parse(source, MarkdownProfile::RichMarkdown);
        SemanticBuilder measured;
        measured.Begin(oldSyntax);
        finishSemantic(measured);
        measured.Begin(pendingSyntax);
        const auto requests = observe([&]
        {
            measured.Begin(syntax);
            finishSemantic(measured);
        });
        Check(requests > 0u && requests < 4096u, "native semantic preparation has a finite observed allocation schedule");
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "semantic diagram failure=%zu/%zu\n", failure, requests);
            SemanticBuilder builder;
            builder.Begin(oldSyntax);
            finishSemantic(builder);
            const auto old = builder.Published();
            builder.Begin(pendingSyntax);
            bool rejected{}, began{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    builder.Begin(syntax);
                    began = true;
                    finishSemantic(builder);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && builder.Published() == old && builder.HasPendingPreparation() != began && old->Map().Source() == L"previous",
                  "native semantic Begin preserves pending work; failed Advance retires only staging and preserves exact published source");
            if (! began)
            {
                finishSemantic(builder);
                Check(builder.Published()->Map().Source() == L"pending", "old pending semantic request remains usable after rejected admission");
            }
            builder.Begin(syntax);
            finishSemantic(builder);
            Check(builder.Published()->Map().Source() == source, "native semantic preparation retries after every observed C++ failure");
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = 0u;
                builder.Cancel();
                builder.Clear();
            }
        }
        semanticRequests += requests;
    }
    const auto write       = PreparedFlowchartTests::WriteFactory();
    const auto drawing     = PreparedFlowchartTests::DrawingFactory();
    const auto oldDocument = SemanticDiagramTests::Build(L"previous", Check);
    for (std::size_t scenario = 0u; scenario < 4u; ++scenario)
    {
        const auto document = SemanticDiagramTests::Build(SemanticDiagramTests::scenarios[scenario == 3u ? 0u : scenario].source, Check);
        LayoutOptions options;
        options.widthDip = scenario == 3u ? 40.0f : 1024.0f;
        LayoutOptions previousOptions;
        previousOptions.widthDip = 800.0f;
        const auto finish        = [](PreparedSemanticGeometry& geometry)
        {
            try
            {
                while (geometry.Pending())
                    geometry.Advance({1u, 3u});
                return true;
            }
            catch (const NativeObjectCapacity&)
            {
                return false;
            }
        };
        PreparedSemanticGeometry measured;
        measured.Begin(write.get(), oldDocument, previousOptions, drawing.get(), {1u, 1u});
        finish(measured);
        auto pendingOptions     = previousOptions;
        pendingOptions.widthDip = 900.0f;
        measured.Begin(write.get(), oldDocument, pendingOptions, drawing.get(), {2u, 2u});
        const auto requests = observe([&]
        {
            measured.Begin(write.get(), document, options, drawing.get(), {3u, 3u});
            finish(measured);
        });
        Check(requests > 0u && requests < 1024u, "native semantic geometry has a finite observed allocation schedule");
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "semantic diagram geometry=%zu failure=%zu/%zu\n", scenario, failure, requests);
            PreparedSemanticGeometry geometry;
            geometry.Begin(write.get(), oldDocument, previousOptions, drawing.get(), {1u, 1u});
            finish(geometry);
            const auto generation = geometry.Generation();
            geometry.Begin(write.get(), oldDocument, pendingOptions, drawing.get(), {2u, 2u});
            bool rejected{}, began{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    geometry.Begin(write.get(), document, options, drawing.get(), {3u, 3u});
                    began = true;
                    finish(geometry);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && geometry.Ready() && &geometry.Document() == oldDocument.get() && geometry.Generation() == generation &&
                      geometry.Pending() != began,
                  "native label/object/paragraph allocation failures preserve the previous immutable geometry and pending admission contract");
            if (! began)
                Check(finish(geometry), "old pending geometry remains usable after failed admission");
            geometry.Begin(write.get(), document, options, drawing.get(), {3u, 3u});
            const auto ready = finish(geometry);
            Check(scenario == 3u ? ! ready && &geometry.Document() == oldDocument.get()
                                 : ready && geometry.HasNativeObjects() && &geometry.Document() == document.get(),
                  "explicit retry distinguishes intrinsic-width capacity from transient allocation failure");
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = 0u;
                geometry.Cancel();
                geometry.Invalidate();
                geometry.Clear();
            }
        }
        geometryRequests += requests;
    }
    std::cout << "semantic diagram failure scenarios=4 sweptRequests=" << semanticRequests << " geometryScenarios=4 sweptRequests=" << geometryRequests
              << " preservedPublication=1 preservedPendingBegin=1 explicitRetry=1 retirementAllocations=0\n";
}

static void TestPreparedFlowchartAllocationFailures(bool trace = false)
{
    using namespace PreparedFlowchartTests;
    const auto write    = WriteFactory();
    const auto drawing  = DrawingFactory();
    const auto oldGraph = FlowchartLayoutTests::Graph(L"graph TD;Old");
    const auto rich     = FlowchartLayoutTests::Graph(L"graph LR;A{\"Unicode \u03b2 \U0001f600\"}-->|yes|B((second));B-->A;A-->A");
    const auto huge     = FlowchartLayoutTests::Graph(L"graph TD;A[\"" + std::wstring(1024u, L'W') + L"\"]");
    std::size_t total{};
    for (const bool capacity : {false, true})
    {
        const auto graph = capacity ? huge : rich;
        LayoutOptions options;
        if (capacity)
            options.fontSizeDip = 512.0f;
        std::size_t requests{};
        {
            PreparedFlowchart prepared;
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=]() noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
            Complete(prepared, Check);
            countAllocations = false;
            requests         = allocations;
            Check(requests > 0u && requests < 128u, "native diagram has a bounded observed C++ preparation allocation schedule");
        }
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "native paint capacity=%d failure=%zu/%zu\n", capacity ? 1 : 0, failure, requests);
            PreparedFlowchart prepared;
            LayoutOptions oldOptions;
            prepared.Begin(write.get(), drawing.get(), oldGraph, oldOptions, {1u, 1u});
            Complete(prepared, Check);
            const auto old        = prepared.Geometry();
            const auto generation = prepared.Generation();
            prepared.Begin(write.get(), drawing.get(), oldGraph, oldOptions, {9u, 9u});
            bool rejected{}, began{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
                    began = true;
                    Complete(prepared, Check);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && prepared.Geometry() == old && prepared.Generation() == generation && prepared.Pending() != began,
                  "failed native Begin preserves pending work; failed Advance retires only staged resources");
            if (! began)
            {
                Complete(prepared, Check);
                Check(prepared.Ready(), "old pending native resources can still finish after failed admission");
            }
            prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
            Complete(prepared, Check);
            Check(capacity ? ! prepared.Ready() && prepared.Geometry()->nodes.empty() : prepared.Ready(),
                  "explicit native retry reaches prepared or capacity outcome after every observed C++ failure");
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = 0u;
                prepared.Cancel();
                prepared.Invalidate();
                prepared.Clear();
            }
            Check(! prepared.Ready() && ! prepared.Pending(), "native resource retirement allocates no C++ storage");
        }
        total += requests;
    }
    static thread_local bool drawOldCounting{}, drawAllocated{};
    static thread_local std::size_t drawOldCalls{};
    drawAllocated = false;
    BorrowedTargets(Check,
                    [](bool before) noexcept
    {
        if (before)
        {
            drawOldCounting  = countAllocations;
            drawOldCalls     = allocations;
            countAllocations = true;
        }
        else
        {
            drawAllocated    = drawAllocated || allocations != drawOldCalls;
            countAllocations = drawOldCounting;
            allocations      = drawOldCalls;
        }
    });
    Check(! drawAllocated, "actual borrowed-target native drawing creates zero tracked C++ allocations");
    std::cout << "native paint C++ failure scenarios=2 sweptRequests=" << total << " retirementAllocations=0 drawAllocations=0\n";
}
static void TestNativeFlowchartLayoutAllocationFailureAtomicity(bool trace = false)
{
    using namespace FlowchartLayoutTests;
    std::size_t total{};
    for (const auto mode : {0u, 1u, 2u})
    {
        const auto graph = Graph(L"graph LR;A[\"Unicode \u03b2 \U0001f600\"]-->|yes|B((second));B-->A;A-->A");
        std::vector<FlowchartSize> nodes(graph->nodes.size(), {20.0, 12.0}), edges(graph->edges.size(), {20.0, 12.0});
        if (mode == 1u)
            nodes[0u].width = NativeFlowchartLayout::MaximumExtent;
        if (mode == 2u)
            for (auto& size : edges)
                size = {65500.0, 65500.0};
        std::size_t requests{};
        {
            NativeFlowchartLayout layout;
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            layout.Begin(graph, nodes, edges);
            countAllocations = false;
            requests         = allocations;
            allocations      = 0u;
            countAllocations = true;
            Complete(layout, Check, 7u);
            Check(allocations == 0u, "graph geometry Advance allocates nothing after bounded admission");
            countAllocations = false;
        }
        Check(requests > 0u && requests < 32u, "graph geometry has a bounded observed admission allocation schedule");
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "native graph layout mode=%u failure=%zu/%zu\n", mode, failure, requests);
            NativeFlowchartLayout layout;
            const auto oldGraph                           = Graph(L"graph TD;Old");
            const std::array<FlowchartSize, 1u> oldLabels = {{{16.0, 12.0}}};
            layout.Begin(oldGraph, oldLabels, {});
            const auto old = Complete(layout, Check);
            layout.Begin(oldGraph, oldLabels, {});
            layout.Advance(2u);
            bool rejected{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    layout.Begin(graph, nodes, edges);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && layout.Published() == old && layout.HasPendingPreparation(),
                  "every failed layout admission preserves pending and published geometry");
            Check(Complete(layout, Check)->nodes.size() == 1u, "previous pending graph still completes after allocation failure");
            layout.Begin(graph, nodes, edges);
            const auto retry = Complete(layout, Check, 7u);
            Check(mode == 0u ? retry->issue == FlowchartLayoutIssue::None && retry->nodes.size() == 2u
                             : retry->issue != FlowchartLayoutIssue::None && retry->nodes.empty(),
                  "explicit graph layout retry reaches the intended prepared or capacity result");
        }
        total += requests;
    }
    std::cout << "native graph layout allocation scenarios=3 sweptRequests=" << total << " AdvanceAllocations=0 preservedPending=1 retry=1\n";
}
static void TestNativeFlowchartAllocationFailureAtomicity(bool trace = false)
{
    using namespace NativeFlowchartTests;
    const std::wstring sources[] = {L"graph LR;A[\"Unicode \u03b2 \U0001f600\"]-->|yes|B((Second));B-->A;A-->A",
                                    L"graph TD;A[staged];B[another];A-->B;style A fill:red",
                                    L"graph TD;A[staged];B[another];A-->|\"incomplete label",
                                    L"graph TD;A[\"" + std::wstring(NativeFlowchartParser::MaximumLabelUnits + 1u, L'x') + L"\"]"};
    std::size_t total{};
    for (const auto& source : sources)
    {
        std::size_t requests{};
        {
            NativeFlowchartParser parser;
            parser.Begin(L"graph TD;Old", 1u);
            Complete(parser);
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            parser.Begin(source, 2u);
            Complete(parser, 7u);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 512u, "native flowchart has a bounded observed allocation schedule");
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "native flowchart failure-point=%zu/%zu\n", failure, requests);
            NativeFlowchartParser parser;
            parser.Begin(L"graph TD;Old", 1u);
            const auto old = Complete(parser);
            parser.Begin(L"graph LR;Staged", 99u);
            bool rejected{};
            bool began{};
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    parser.Begin(source, 2u);
                    began = true;
                    Complete(parser, 7u);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && parser.Published() == old && parser.HasPendingPreparation() != began && old->source == L"graph TD;Old",
                  "failed native Begin keeps pending work; failed Advance retires only staging; neither changes the old publication");
            if (! began)
                Check(Complete(parser)->source == L"graph LR;Staged", "native Begin allocation failure preserves the previous pending source");
            parser.Begin(source, 2u);
            Check(Complete(parser)->source == source, "explicit native flowchart retry succeeds after every observed allocation failure");
        }
        total += requests;
    }
    std::cout << "native flowchart allocation scenarios=4 sweptRequests=" << total << " sourcePreserved=1 retry=1\n";
}

static void TestRichFrontMatterAllocationFailureAtomicity(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto finish = [](MarkdownParser& parser)
    {
        while (parser.HasPendingPreparation())
            parser.Advance(7u);
    };
    std::size_t total         = 0u;
    std::size_t semanticTotal = 0u;
    const auto finishSemantic = [](SemanticBuilder& builder)
    {
        while (builder.HasPendingPreparation())
            builder.Advance(7u);
    };
    MarkdownParser oldParser;
    oldParser.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
    finish(oldParser);
    const auto oldSyntax = oldParser.Published();
    std::size_t activationRequests{};
    {
        DxUi::RichTextEditor editor(L"previous");
        editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        editor.SetMarkdownRenderingEnabled(true);
        countAllocations   = false;
        activationRequests = allocations;
    }
    Check(activationRequests > 0u && activationRequests < 32u, "semantic activation lazily creates a bounded liveness state");
    for (std::size_t failure = 0u; failure < activationRequests; ++failure)
    {
        DxUi::RichTextEditor editor(L"previous");
        editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        bool rejected = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                editor.SetMarkdownRenderingEnabled(true);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && ! editor.IsMarkdownRenderingEnabled() && editor.GetText() == L"previous" && ! editor.CanUndo(),
              "failed semantic activation preserves mode, exact source and history");
        editor.SetMarkdownRenderingEnabled(true);
        Check(editor.IsMarkdownRenderingEnabled() && editor.GetText() == L"previous", "semantic activation retries after each observed allocation failure");
    }
    for (const std::wstring_view source :
         {L"---\r\n# comment\r\nname: '\U0001f600'\r\n---\r\n# after\r\n", L"+++\n# comment\nname = '\u03b2'\n+++\n", L"---\nkey: [\n", L"+++\nkey = [\n"})
    {
        std::size_t requests{};
        {
            MarkdownParser parser;
            parser.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
            finish(parser);
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
            finish(parser);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 512u, "metadata parser exposes a finite observed allocation schedule");
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "front matter failure-point=%zu/%zu\n", failure, requests);
            MarkdownParser parser;
            parser.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
            finish(parser);
            const auto old = parser.Published();
            bool rejected  = false;
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
                    finish(parser);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && parser.Published() == old && ! parser.HasPendingPreparation() && old->Source() == L"previous",
                  "every observed metadata allocation failure retires staging and preserves the exact old publication");
            parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
            finish(parser);
            Check(parser.Published()->Source() == source && parser.Published()->Nodes()[1u].block.kind == MarkdownBlockKind::FrontMatter,
                  "explicit metadata retry succeeds after every observed allocation failure");
        }
        total += requests;
        MarkdownParser parsed;
        parsed.Begin(source, 2u, MarkdownProfile::RichMarkdown);
        finish(parsed);
        const auto syntax = parsed.Published();
        std::size_t semanticRequests{};
        {
            SemanticBuilder builder;
            builder.Begin(oldSyntax);
            finishSemantic(builder);
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            builder.Begin(syntax);
            finishSemantic(builder);
            countAllocations = false;
            semanticRequests = allocations;
        }
        Check(semanticRequests > 0u && semanticRequests < 512u, "metadata semantic publication has a finite independently observed allocation schedule");
        for (std::size_t failure = 0u; failure < semanticRequests; ++failure)
        {
            if (trace)
                std::fprintf(stderr, "front matter semantic failure-point=%zu/%zu\n", failure, semanticRequests);
            SemanticBuilder builder;
            builder.Begin(oldSyntax);
            finishSemantic(builder);
            const auto old = builder.Published();
            bool rejected  = false;
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    builder.Begin(syntax);
                    finishSemantic(builder);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && builder.Published() == old && ! builder.HasPendingPreparation() && old->Map().Source() == L"previous",
                  "metadata semantic/diagnostic allocation failures retain the old publication and retire staging");
            builder.Begin(syntax);
            finishSemantic(builder);
            Check(builder.Published()->Syntax() == syntax && builder.Published()->Map().Source() == source,
                  "semantic metadata and missing-fence help retry without changing source after each observed allocation failure");
        }
        semanticTotal += semanticRequests;
    }
    std::cout << "front matter allocation scenarios=4 sweptRequests=" << total << " sourcePreserved=1 retry=1\n";
    std::cout << "front matter semantic allocation scenarios=4 sweptRequests=" << semanticTotal << " sourcePreserved=1 retry=1\n";
    std::cout << "semantic accessibility activation sweptRequests=" << activationRequests << " sourcePreserved=1 retry=1\n";
}

static void TestRichMarkdownParserFailureAndStorageBudgets(bool trace = false)
{
    using namespace DxUi::RichText;
    const std::wstring source =
        L"# staged\r\n> - **nested *styles***\n>\n>   [a\nb]: /reference 'long title'\n\n"
        L"| header | second | third |\n| --- | --- | --- |\n| `a\\|b` |\n\n- [x] ~~[a b]~~\n\nwww.example.invalid\n\n<script>raw</script>\n" +
        std::wstring(L"\n[first]: /first 'first title'\n[second]: /second 'second title'\n| - |\n") + std::wstring(512u, L'\n') + L"&NotEqualTilde; &#x1f600;";
    Document sourceDocument(source);
    const auto sourceSnapshot = sourceDocument.Snapshot();
    const auto finish         = [](MarkdownParser& parser)
    {
        while (parser.HasPendingPreparation())
            parser.Advance(17u);
    };
    size_t requests = 0u;
    {
        MarkdownParser parser;
        parser.Begin(L"previous", 1u);
        finish(parser);
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        parser.Begin(sourceSnapshot, 2u, MarkdownProfile::Gfm029);
        finish(parser);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 2048u, "finite observed schedule includes inline links/styles, tables, tasks, tagfilter and compaction");
    for (size_t failure = 0u; failure < requests; ++failure)
    {
        if (trace)
        {
            std::fprintf(stderr, "lossless Markdown failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
        MarkdownParser parser;
        parser.Begin(L"previous", 1u);
        finish(parser);
        const auto previous = parser.Published();
        bool rejected       = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                parser.Begin(sourceSnapshot, 2u, MarkdownProfile::Gfm029);
                finish(parser);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && parser.Published() == previous && previous->Source() == L"previous" && ! parser.HasPendingPreparation(),
              "failure at every Begin/Advance allocation preserves published source and retires staged work");
        parser.Begin(sourceSnapshot, 2u, MarkdownProfile::Gfm029);
        finish(parser);
        Check(parser.Published()->Source() == source, "explicit retry after allocation failure succeeds");
    }
    size_t beginRequests = 0u;
    {
        MarkdownParser parser;
        parser.Begin(L"old pending", 1u);
        parser.Advance(1u);
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        parser.Begin(sourceSnapshot, 2u, MarkdownProfile::Gfm029);
        countAllocations = false;
        beginRequests    = allocations;
    }
    Check(beginRequests > 0u && beginRequests < requests, "Begin allocation schedule is independently observed");
    for (size_t failure = 0u; failure < beginRequests; ++failure)
    {
        if (trace)
        {
            std::fprintf(stderr, "lossless Markdown pending-begin-failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
        MarkdownParser parser;
        parser.Begin(L"published", 1u);
        finish(parser);
        const auto published = parser.Published();
        parser.Begin(L"old pending", 2u);
        parser.Advance(1u);
        bool rejected = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                parser.Begin(sourceSnapshot, 3u, MarkdownProfile::Gfm029);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && parser.HasPendingPreparation() && parser.Published() == published,
              "failed Begin preserves both an existing pending request and the previous published identity");
        finish(parser);
        Check(parser.Published()->Revision() == 2u && parser.Published()->Source() == L"old pending",
              "the original pending cursor can finish after replacement admission fails");
    }
    const std::wstring maximum(Document::MaximumUnits, L'\n');
    ptrdiff_t retained = 0, released = 0, peak = 0;
    {
        const auto oldCounting = countLiveBytes;
        const auto oldLive     = liveBytes;
        auto* const oldPeak    = liveBytesPeak;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countLiveBytes = oldCounting;
            liveBytes      = oldLive;
            liveBytesPeak  = oldPeak;
        });
        liveBytes              = 0;
        countLiveBytes         = true;
        liveBytesPeak          = &peak;
        {
            MarkdownParser parser;
            parser.Begin(maximum, 1u);
            finish(parser);
            Check(parser.Published()->Blocks().size() == maximum.size() && parser.Published()->Lexemes().size() == maximum.size(),
                  "maximum physical-line fixture publishes all partitions without truncation");
            retained = liveBytes;
            for (std::uint64_t revision = 2u; revision <= 4u; ++revision)
            {
                parser.Begin(maximum, revision);
                parser.Advance(17u);
                Check(parser.Published()->Revision() == revision - 1u, "maximum replacement keeps its previous publication until complete");
                finish(parser);
                Check(parser.Published()->Revision() == revision, "maximum replacement publishes its own completed revision");
            }
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    std::cout << "lossless Markdown maximum storage retainedCppBytes=" << retained << " replacementPeakCppBytes=" << peak << " releasedDelta=" << released
              << '\n';
    Check(retained >= 0 && retained <= 24 * 1024 * 1024 && released == 0, "maximum bounded syntax storage releases completely within 24 MiB C++ budget");
    Check(peak >= retained && peak <= 32 * 1024 * 1024, "old/staged maximum syntax replacement stays within 32 MiB peak C++ storage");
    MarkdownParser parser;
    parser.Begin(sourceSnapshot, 1u, MarkdownProfile::Gfm029);
    finish(parser);
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    size_t idleCalls = 0u;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        for (size_t repeat = 0u; repeat < 100u; ++repeat)
        {
            parser.Begin(sourceSnapshot, 1u, MarkdownProfile::Gfm029);
            static_cast<void>(parser.Published());
            parser.Advance(0u);
            parser.Cancel();
        }
        parser.Clear();
        countAllocations = false;
        idleCalls        = allocations;
    }
    Check(idleCalls == 0u, "same-content preparation, completed reads and retirement allocate no C++ storage");
    std::cout << "lossless Markdown sweptRequests=" << requests << " pendingBeginRequests=" << beginRequests << " maximumRetainedCppBytes=" << retained
              << " replacementPeakCppBytes=" << peak << " releasedDelta=" << released << " unchangedAndRetireAllocations=" << idleCalls << '\n';
}

static void TestEmbeddedFootnoteBlockSourceEdits(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "footnote borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "footnote embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    for (const auto& scenario : FootnoteBlockTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded note block preparation");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax != nullptr, "embedded note source preparation completes");
        FootnoteBlockTests::CheckSyntax(*syntax, scenario, Check);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(semantic && semantic->Syntax() == syntax && editor->GetText() == scenario.source && ! editor->CanUndo() && ! view.Controls().GetHwnd(),
              "embedded note syntax shares source and history without a control HWND");
    }
    const std::wstring source = L"[^n]: first\r\n\r\n    second\r\n";
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded note edit baseline");
    const auto at = source.find(L"second");
    editor->SetSelection({at, at + 6u});
    for (const auto command : {MarkdownCommand::ToggleStyle, MarkdownCommand::SetHeading, MarkdownCommand::SetList, MarkdownCommand::InsertTable})
        Check(editor->ExecuteMarkdownCommand({command}) == MarkdownCommandStatus::Unavailable && editor->GetText() == source && ! editor->CanUndo(),
              "initial embedded literal note commands reject child prefix reinterpretation atomically");
    Check(editor->ReplaceSelection(L"\u03b2") && editor->Undo() && editor->GetText() == source && editor->Redo(),
          "raw embedded note replacement remains one reversible Unicode transaction");
}

static void TestEmbeddedFootnoteResolutionAndSourceEdits(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "footnote binding borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "footnote binding embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    FootnoteResolutionTests::ExerciseSourceEdits(*editor,
                                                 [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded note resolution preparation"); },
                                                 [&](const auto& syntax)
    {
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        if (semantic)
            FootnoteResolutionTests::CheckSemantic(*semantic, Check);
        return semantic && semantic->Syntax() == syntax && semantic->Map().Source() == syntax->Source() && ! view.Controls().GetHwnd();
    },
                                                 Check);
    FootnoteResolutionTests::ExerciseNavigation(
        *editor, view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded footnote navigation"); }, Check, MK_CONTROL, MK_SHIFT);
}

static void TestParserCapacityMemoryFailureIsRetryable(GraphicsFixture& gpu)
{
    using namespace DxUi;
    class PreparationProbe final : public RichTextEditor
    {
    public:
        using RichTextEditor::PrepareContent;
        using RichTextEditor::RichTextEditor;
    };
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "capacity memory probe borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "capacity memory probe host");
    auto owned   = std::make_unique<PreparationProbe>(L"old");
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "capacity memory previous publication");
    const auto previous = editor->GetMarkdownSyntax();
    editor->SetText(L"new **small**");
    editor->SetSelection({1u, 2u});
    HRESULT failure{};
    {
        const auto oldFailure = failAllocationAfter;
        const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
        failAllocationAfter   = 0u;
        // Call the real protected preparation boundary directly so the first
        // failed allocation is inside parser admission, not host/tree staging.
        failure = editor->PrepareContent(view.Controls());
    }
    Check(failure == E_OUTOFMEMORY && editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Pending && editor->GetText() == L"new **small**" &&
              editor->GetSelection() == RichText::Selection{1u, 2u} && ! editor->CanUndo() && previous->Source() == L"old",
          "parser memory failure remains retryable and cannot latch source capacity fallback or mutate held source/history");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "retry parser memory failure");
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetMarkdownSyntax()->Source() == L"new **small**",
          "retry succeeds for the unchanged source request after transient parser admission memory failure");
}
static void TestEmbeddedParserCapacitySourceFallback(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    MarkdownCapacityTests::ParserBounds(Check);
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "capacity fallback borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "capacity fallback embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    MarkdownCapacityTests::EditorFallback(
        *editor, view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "capacity source fallback preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "capacity fallback acquires no control HWND");
}
static void TestEmbeddedLocalizedRecordedServiceCommit(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "recorded service shared borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "recorded service embedded owner");
    auto owned   = std::make_unique<RichTextStateReplacementTests::ServiceEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    RichTextStateReplacementTests::Service(*editor, view.Controls(), Check);
    Check(! view.Controls().GetHwnd(), "recorded embedded import needs no control HWND");
}
static void TestEmbeddedImageResources(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "image resource shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "image resource embedded owner");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto prepare = [&] { Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "image resource owner preparation"); };
    RichImageResourceTests::Retained(*editor, view.Controls(), prepare, Check);
    RichImageResourceTests::ReadyPicture(*editor, prepare, Check, true);
    const auto source   = editor->GetText();
    const auto revision = editor->Revision();
    const auto capture  = [&](bool selected = false, float scale = 1.f)
    {
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "image resource actual composition");
        std::vector<std::uint8_t> pixels(0u);
        Hr(gpu.Read(pixels), "image resource actual readback");
        RichImageResourceTests::PatternPixels(*editor, view.Controls(), pixels, gpu.width, gpu.height, scale, Check, selected);
    };
    capture();
    editor->SetSelection({0u, source.size()});
    prepare();
    capture(true);
    editor->SetSelection({0u, 0u});
    prepare();
    gpu.Bind();
    const auto before = allocations;
    countAllocations  = true;
    for (unsigned frame = 0u; frame < 12u; ++frame)
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "warm image composition");
    countAllocations = false;
    Check(allocations == before, "warm image composition allocates no C++ memory or decode graph");
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    prepare();
    capture();
    Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height, 144.f), "image raster density transition");
    capture(false, 1.5f);
    prepare();
    capture();
    view.SetVisible(false);
    Check(! editor->DebugGetState().layoutReady && ! view.NeedsPreparation(), "hidden image view releases prepared pixels without work");
    view.SetVisible(true);
    prepare();
    capture();
    std::vector<DxUi::RichText::RichImageRequest> requests(0u);
    editor->TakePendingImageRequests(requests);
    Check(requests.empty(), "image show reuses immutable decoded pixels without another file request");
    GraphicsFixture replacement;
    replacement.width  = gpu.width;
    replacement.height = gpu.height;
    Hr(replacement.Create(), "image replacement WARP device");
    std::shared_ptr<GraphicsDevice> next;
    Hr(GraphicsDevice::Create(replacement.device.get(), next), "image replacement shared pool");
    Hr(view.ReplaceDevice(next), "image resource device replacement");
    prepare();
    replacement.Bind();
    Hr(view.Composite(replacement.context.get(), replacement.Viewport()), "image after replacement composition");
    std::vector<std::uint8_t> pixels(0u);
    Hr(replacement.Read(pixels), "image after replacement readback");
    RichImageResourceTests::PatternPixels(*editor, view.Controls(), pixels, replacement.width, replacement.height, 1.f, Check);
    editor->TakePendingImageRequests(requests);
    Check(requests.empty() && editor->GetText() == source && editor->Revision() == revision && ! editor->CanUndo(),
          "device replacement rebuilds GPU images from held pixels without source/history or repeated I/O");
}
static void TestEmbeddedImagePathPaste(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "image-paste shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "image-paste embedded owner");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    RichImagePathTests::Paths(Check);
    RichImagePathTests::Codec(Check);
    RichImagePathTests::Retained(*editor, [&] { Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "image paste preparation"); }, Check);
    view.Controls().SetFocusControl(editor, false);
    editor->SetText(L"ok ");
    editor->SetSelection({3u, 3u});
    Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "image paste transport preparation");
    EmbeddedTextInputSnapshot snapshot;
    Hr(view.ReadTextInput(snapshot), "image paste reads expected revision");
    TextClipboardContent content;
    content.text             = L"C:\\docs\\a.png";
    std::wstring replacement = L"sentinel";
    Check(view.TransformClipboardPaste(snapshot.revision, snapshot.focusId, content, replacement) == S_OK && replacement == L"![a](a.png)",
          "embedded clipboard preparation uses source snapshot and document location");
    editor->SetDocumentFilePath(L"D:\\note.md");
    Check(view.TransformClipboardPaste(snapshot.revision, snapshot.focusId, content, replacement) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
          "document-location change rejects stale embedded clipboard preparation");
    Check(editor->GetText() == L"ok " && ! editor->CanUndo() && ! view.Controls().GetHwnd(), "embedded transform creates no HWND or source transaction");
}
static void TestEmbeddedRetainedObjectSourceEditing(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "source editing borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "source editing embedded owner");
    auto owned   = std::make_unique<SemanticSourceBlockTests::SourceEditingEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    SemanticSourceBlockTests::Retained(
        *editor, view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "source editing owner preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "retained source editing creates no embedded control HWND");
}
static void TestEmbeddedDiagramSelectionRaster(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "native selection borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "native selection embedded host");
    view.Controls().SetTheme(SemanticDiagramTests::SelectionPalette());
    auto owned   = std::make_unique<RichTextEditor>(std::wstring(SemanticDiagramTests::SelectionSource));
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "native selection embedded preparation");
    editor->SetSelection({0u, editor->GetText().size()});
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "native selection embedded selected preparation");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "native selection embedded composite");
    std::vector<uint8_t> pixels;
    Hr(gpu.Read(pixels), "native selection embedded pixels");
    SemanticDiagramTests::SelectionPixels(*editor, view.Controls(), pixels, gpu.width, gpu.height, 1.f, Check);
}
static void TestEmbeddedFlowchartRenderingAndFallbacks(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    NativeFlowchartTests::CheckOriginalCorpus(Check);
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "diagram borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "diagram embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    for (const auto& scenario : NativeFlowchartTests::scenarios)
    {
        const auto source = NativeFlowchartTests::FencedSource(scenario);
        editor->SetText(source);
        Hr(PrepareRichEmbeddedContent(view, 1024u, 240u), "embedded diagram definition preparation");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax != nullptr, "embedded diagram source preparation completes");
        NativeFlowchartTests::CheckFencedSource(*syntax, scenario, Check);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(semantic && semantic->Syntax() == syntax && semantic->Map().Source() == source && editor->GetText() == source && ! editor->CanUndo() &&
                  ! view.Controls().GetHwnd(),
              "embedded graph rendering/fallback preserves source and history without a control HWND");
        Check(scenario.outcome == RichText::Detail::FlowchartOutcome::Prepared ? ! RichText::Detail::SemanticExtensionAccess::Diagrams(*semantic).empty()
                                                                               : semantic->Map().Display() == NativeFlowchartTests::LiteralDisplay(scenario),
              "embedded admitted graphs render atomically and rejected diagrams keep literal code");
        editor->SetSelection({13u, 13u});
        Check(editor->ReplaceSelection(L"\u03b2") && editor->Undo() && editor->GetText() == source,
              "every embedded diagram definition remains a reversible Unicode source edit");
    }
}

static void TestEmbeddedDiagramViewCapacityRecovery(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "native diagram recovery resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "native diagram recovery embedding");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    SemanticDiagramTests::Editor(
        *editor, view.Controls(), [&](UINT width) { Hr(PrepareRichEmbeddedContent(view, width, 240u), "embedded diagram recovery preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "diagram recovery uses borrowed hosting services without a child HWND");
}
static void TestEmbeddedFrontMatterFencesAndSourceEdits(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "metadata borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "metadata embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    for (const auto& scenario : FrontMatterTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata preparation");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax != nullptr, "embedded metadata source preparation completes");
        FrontMatterTests::CheckSyntax(*syntax, scenario, Check);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(semantic && semantic->Syntax() == syntax && editor->GetText() == scenario.source && ! editor->CanUndo() && ! view.Controls().GetHwnd(),
              "embedded metadata shares acknowledged syntax and exact source/history without a control HWND");
    }
    const std::wstring source = L"---\r\n# keep comment\r\nname: '\u03b2'\r\n---\r\n# Body\r\n";
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata edit baseline");
    const auto held = editor->GetAcknowledgedSemanticDocument(view.Controls());
    const auto at   = source.find(L'\u03b2');
    editor->SetSelection({at, at + 1u});
    for (const auto command : {MarkdownCommand::ToggleStyle, MarkdownCommand::SetHeading, MarkdownCommand::InsertTable})
        Check(editor->ExecuteMarkdownCommand({command}) == MarkdownCommandStatus::Unavailable && editor->GetText() == source && ! editor->CanUndo(),
              "embedded structural commands cannot rewrite opaque metadata");
    Check(editor->ReplaceSelection(L"\u03a9") && ! editor->GetAcknowledgedSemanticDocument(view.Controls()),
          "embedded metadata edit immediately rejects stale semantic acknowledgement");
    auto expected = source;
    expected.replace(at, 1u, L"\u03a9");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata source edit");
    Check(editor->GetText() == expected && held->Syntax()->Source() == source, "embedded metadata source edits preserve untouched text and held revisions");
    Check(editor->Undo() && editor->GetText() == source && ! editor->CanUndo() && editor->Redo() && editor->GetText() == expected,
          "embedded metadata edit is exactly one undo/redo transaction");
    const auto selection = editor->GetSelection();
    editor->SetViewMode(ViewMode::Source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata source view");
    Check(editor->GetSelection() == selection && editor->GetText() == expected, "embedded source view retains metadata spelling and selection");
}
static void TestEmbeddedLosslessMarkdownPreparation(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "Markdown shared WARP pool");
    EmbeddedHost view, other;
    size_t requests = 0u;
    Hr(view.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "Markdown embedded view");
    Hr(other.Attach(graphics), "Markdown independent embedded view");
    const std::wstring source = L"# Unicode \U0001f600\r\n\n~~~lang\r\n# kept &NotEqualTilde;\n~~~\n[^unknown]: $x$\n";
    auto owned                = std::make_unique<RichTextEditor>(source);
    auto* editor              = owned.get();
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor, false);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare unchanged v0 pixels before optional analysis");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose unchanged v0 pixels before analysis");
    std::vector<uint8_t> originalPixels;
    Hr(gpu.Read(originalPixels), "read unchanged v0 pixel oracle");
    auto independent        = std::make_unique<RichTextEditor>(source);
    auto* independentEditor = independent.get();
    independentEditor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    other.Controls().SetRoot(std::move(independent));
    Hr(PrepareRichEmbeddedContent(other, 320u, 160u, 144.0f), "prepare independent Markdown view");
    const auto independentSyntax = independentEditor->GetMarkdownSyntax();
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        editor->SetMarkdownAnalysisProfile(profile);
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded lossless Markdown");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax && syntax->Source() == source && syntax->Profile() == profile && syntax->Blocks()[2].kind == MarkdownBlockKind::FencedCode &&
                  independentEditor->GetMarkdownSyntax() == independentSyntax && view.Controls().GetHwnd() == nullptr,
              "each supplied-device view owns independent keyed syntax without a renderer HWND");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose analysis-enabled unchanged preview");
        std::vector<uint8_t> pixels;
        Hr(gpu.Read(pixels), "read analysis-enabled preview pixels");
        Check(pixels == originalPixels, "lossless analysis does not silently replace or change the v0 formatted renderer");
    }
    const auto held = editor->GetMarkdownSyntax();
    for (const auto& scenario : MarkdownBlockTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded independent block scenario");
        const auto parsed = editor->GetMarkdownSyntax();
        Check(parsed && MarkdownBlockTests::BlockSignature(*parsed) == scenario.signature, "embedded block grammar matches independent expected structure");
        Check(editor->GetText() == scenario.source && ! editor->CanUndo() && independentEditor->GetMarkdownSyntax() == independentSyntax,
              "embedded block grammar retains exact source/history and isolates its second view");
    }
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        editor->SetMarkdownAnalysisProfile(profile);
        for (const auto& scenario : MarkdownInlineTests::scenarios)
        {
            editor->SetText(std::wstring(scenario.source));
            Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded inline/extension scenario");
            const auto parsed = editor->GetMarkdownSyntax();
            Check(parsed && MarkdownHtmlTests::Html(*parsed) == MarkdownInlineTests::Expected(scenario, profile),
                  "embedded inline/extension semantics match independent expectations");
            Check(editor->GetText() == scenario.source && ! editor->CanUndo() && independentEditor->GetMarkdownSyntax() == independentSyntax,
                  "embedded inline analysis preserves source/history and isolates the other view");
        }
    }
    editor->SetText(std::wstring(MarkdownInlineTests::paddedTable));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table preparation");
    const auto table = editor->GetMarkdownSyntax();
    const auto cells = MarkdownInlineTests::TableCellIdentities(*table);
    Check(cells.size() == 6u, "embedded table contains both missing body cells");
    editor->SetSelection({editor->GetText().size(), editor->GetText().size()});
    Check(editor->ReplaceSelection(L"outside\n"), "embedded edit outside padded table");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table edit preparation");
    Check(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
              MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells,
          "embedded unrelated edit retains unique source-backed and padded cell identities");
    Check(editor->Undo(), "embedded padded table unrelated edit undo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table undo preparation");
    Check(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
              MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells,
          "embedded undo retains distinct padded cell identities");
    Check(editor->Redo(), "embedded padded table unrelated edit redo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table redo preparation");
    Check(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
              MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells && MarkdownInlineTests::UniqueNodeIdentities(*table) &&
              independentEditor->GetMarkdownSyntax() == independentSyntax,
          "embedded redo, held snapshots and second view preserve independent identities");
    editor->SetText(L"[reader]\n\n[reader]: /first\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded initial reference semantics");
    const auto oldReference      = editor->GetMarkdownSyntax();
    const auto referenceIdentity = oldReference->Nodes()[oldReference->Nodes()[0u].firstChild].identity;
    const auto referenceAt       = editor->GetText().find(L"/first");
    editor->SetSelection({referenceAt, referenceAt + 6u});
    Check(editor->ReplaceSelection(L"/second"), "embedded global reference edit");
    Check(! editor->GetMarkdownSyntax(), "embedded reference edit hides stale inline semantics");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded changed reference semantics");
    const auto newReference = editor->GetMarkdownSyntax();
    Check(MarkdownHtmlTests::Html(*newReference) == L"<p><a href=\"/second\">reader</a></p>\n" &&
              newReference->Nodes()[newReference->Nodes()[0u].firstChild].identity == referenceIdentity,
          "embedded stable block identity cannot retain old reference semantics");
    Check(editor->Undo(), "embedded reference undo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reference undo preparation");
    Check(MarkdownHtmlTests::Html(*editor->GetMarkdownSyntax()) == MarkdownHtmlTests::Html(*oldReference), "embedded undo re-resolves references");
    Check(editor->Redo(), "embedded reference redo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reference redo preparation");
    Check(MarkdownHtmlTests::Html(*editor->GetMarkdownSyntax()) == MarkdownHtmlTests::Html(*newReference) &&
              MarkdownHtmlTests::Html(*oldReference) == L"<p><a href=\"/first\">reader</a></p>\n",
          "embedded redo and immutable held reference tree");
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::CommonMark0312);
    editor->SetText(L"# heading\n\n> alpha\n\ntail\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded identity initial tree");
    const auto oldTree      = editor->GetMarkdownSyntax();
    const auto originalText = editor->GetText();
    const auto editAt       = originalText.find(L"alpha");
    editor->SetSelection({editAt, editAt});
    Check(editor->ReplaceSelection(L"\U0001f600 "), "embedded Unicode block edit");
    Check(! editor->GetMarkdownSyntax(), "embedded edit hides stale current syntax");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded identity changed tree");
    const auto changedTree = editor->GetMarkdownSyntax();
    Check(changedTree && changedTree->DocumentIdentity() == oldTree->DocumentIdentity() && changedTree->Nodes().size() == oldTree->Nodes().size(),
          "embedded owner publication retains identity scope");
    for (std::size_t i = 0; i < oldTree->Nodes().size(); ++i)
        Check(changedTree->Nodes()[i].identity == oldTree->Nodes()[i].identity, "embedded unchanged/enclosing node reconciliation");
    Check(editor->Undo(), "embedded block edit undo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded undo tree");
    Check(editor->GetText() == originalText && oldTree->Source() == originalText, "embedded held tree remains immutable through undo");
    editor->SetText(L"same\n\nsame\n\nsame\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded duplicate tree");
    const auto duplicates = editor->GetMarkdownSyntax();
    const auto first      = duplicates->Nodes()[0u].firstChild;
    const auto second     = duplicates->Nodes()[first].nextSibling;
    editor->SetSelection({0u, 6u});
    Check(editor->ReplaceSelection(L""), "embedded delete first duplicate");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reconciled duplicate tree");
    const auto remaining = editor->GetMarkdownSyntax();
    Check(remaining->Nodes()[remaining->Nodes()[0u].firstChild].identity == duplicates->Nodes()[second].identity,
          "embedded tracked ancestry preserves the surviving duplicate's identity");
    std::wstring nested;
    for (unsigned level = 0u; level < 256u; ++level)
        nested += L"- ";
    nested += L"alpha\n";
    nested.append(512u, L' ');
    nested += L"beta\n";
    editor->SetText(nested);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded deep prefix preparation");
    const auto bounded = editor->GetMarkdownSyntax();
    Check(bounded && bounded->Nodes().size() == 514u && bounded->BlockStatistics().prefixInspectionUnits <= nested.size() * 40u,
          "embedded owner preparation keeps deep containers and bounded prefix inspection");
    for (const auto& edge : MarkdownBlockTests::identityEdges)
    {
        editor->SetText(std::wstring(edge.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded paragraph-edge source");
        const auto before = editor->GetMarkdownSyntax();
        editor->SetSelection(edge.selection);
        Check(editor->ReplaceSelection(L""), "embedded Unicode paragraph-edge deletion");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded paragraph-edge reconciliation");
        const auto after = editor->GetMarkdownSyntax();
        Check(after && after->Nodes()[after->Nodes()[0u].firstChild].identity == before->Nodes()[before->Nodes()[0u].firstChild].identity,
              "embedded first/last scalar deletion preserves its compatible paragraph identity");
    }
    editor->SetText(std::wstring(32000u, L'&'));
    Check(view.Prepare(480u, 240u) == S_FALSE && editor->DebugGetState().markdownPreparationPending && ! editor->GetMarkdownSyntax(),
          "embedded parser yields before geometry and never presents stale syntax as current");
    const auto advances    = editor->DebugGetState().markdownPreparationAdvances;
    const auto outstanding = requests;
    EmbeddedTextInputSnapshot input;
    Hr(view.ReadTextInput(input), "pending Markdown source text store");
    Check(input.state.text == editor->GetText() && ! input.caretBoundsDip, "parser pending source ACPs and unavailable geometry remain coherent");
    for (size_t key = 0u; key < 16u; ++key)
        Check(view.DispatchCharacter(L'\u03b2'), "pending Markdown Unicode edit");
    Check(requests == outstanding && editor->DebugGetState().markdownPreparationAdvances == advances && held->Source() == source,
          "input coalesces callbacks and never advances cancelled parse work or mutates held syntax");
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "publish latest embedded Markdown");
    auto syntax = editor->GetMarkdownSyntax();
    Hr(PrepareRichEmbeddedContent(view, 360u, 180u, 144.0f), "reshape embedded Markdown without reparsing pure source");
    Check(editor->GetMarkdownSyntax() == syntax, "width and DPI do not invalidate completed source-only analysis");
    editor->SetText(std::wstring(32000u, L'x'));
    Check(view.Prepare(480u, 240u) == S_FALSE && editor->DebugGetState().markdownPreparationPending, "pending parser visibility fixture");
    view.SetVisible(false);
    const auto hiddenRequests = requests;
    Check(! editor->DebugGetState().markdownPreparationPending && view.Prepare(480u, 240u) == S_FALSE && requests == hiddenRequests,
          "hidden embedded view cancels staged parsing without periodic work");
    view.SetVisible(true);
    Check(view.Prepare(480u, 240u) == S_FALSE && editor->DebugGetState().markdownPreparationPending, "parser resumes after showing");
    Check(view.Prepare(480u, 0u) == S_FALSE && ! editor->DebugGetState().markdownPreparationPending, "zero-height extent cancels parser staging");
    Check(view.Prepare(480u, 240u) == S_FALSE, "parser resumes after positive extent");
    Hr(view.ReplaceDevice(graphics), "retire pending parser on embedded resource replacement");
    Check(! editor->DebugGetState().markdownPreparationPending, "resource replacement retires the pending owner job");
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "resume visible source-only analysis");
    syntax = editor->GetMarkdownSyntax();
    editor->SetSelection({2u, 2u});
    Check(editor->ReplaceSelection(L"x") && editor->Undo() && editor->GetText() == source, "embedded analysis preserves source history");
    editor->SetMarkdownAnalysisProfile(std::nullopt);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "disable optional parser");
    Check(! editor->GetMarkdownSyntax() && editor->CanRedo() && syntax->Source() == source, "opt-out preserves held source and redo");
    view.Detach();
    Check(held->Source() == source && independentEditor->GetMarkdownSyntax() == independentSyntax,
          "held syntax survives detach without affecting another view");
}

static void TestRichTextCooperativeSchedulingAndTextInput(GraphicsFixture& gpu)
{
    using namespace DxUi;
    struct Requests
    {
        EmbeddedHost* view{};
        size_t count{};
        bool replaceRoot{};
        bool tryReenter{};
        HRESULT reentrantResult = S_OK;
    } requests;
    EmbeddedHost view, other;
    requests.view = &view;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "cooperative shared graphics");
    Hr(view.Attach(graphics,
                   {&requests,
                    [](void* context) noexcept
    {
        auto& request = *static_cast<Requests*>(context);
        ++request.count;
        if (std::exchange(request.tryReenter, false))
            request.reentrantResult = request.view->Prepare(480, 240);
        if (std::exchange(request.replaceRoot, false))
        {
            try
            {
                request.view->Controls().SetRoot(std::make_unique<Label>(L"replacement"));
            }
            catch (const std::exception&)
            {
                Check(false, "replacement scheduling callback completes");
            }
        }
    }}),
       "cooperative embedded host");
    Hr(other.Attach(graphics), "cooperative independent view");
    auto independent  = std::make_unique<RichTextEditor>(L"independent **source**");
    auto* otherEditor = independent.get();
    other.Controls().SetRoot(std::move(independent));
    Hr(PrepareRichEmbeddedContent(other, 320, 160, 144), "prepare independent density");
    const auto independentGeneration = otherEditor->DebugGetState().layoutGeneration;
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    auto owned   = std::make_unique<RichTextEditor>(seed);
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor, false);
    const auto beforeTurn = requests.count;
    requests.tryReenter   = true;
    Check(view.Prepare(480, 240) == S_FALSE && view.NeedsPreparation() && requests.count == beforeTurn + 1u,
          "one bounded embedded turn queues one continuation while pixels remain dirty");
    Check(requests.reentrantResult == E_UNEXPECTED, "a continuation callback cannot recursively drain preparation");
    const auto pending = editor->DebugGetState();
    Check(pending.preparationPending && pending.lastAdvanceLayouts <= 4u && pending.lastAdvanceBlockSteps <= 4096u,
          "embedded preparation obeys layout and source-block budgets");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! editor->TryGetTextInputCaretRect(view.Controls(), 0u) &&
              ! editor->GetVisibleSourceRange(view.Controls()),
          "pending pixels and geometry are unavailable");
    EmbeddedTextInputSnapshot stale;
    Hr(view.ReadTextInput(stale), "read exact source while embedded geometry is pending");
    Check(stale.state.text == seed && ! stale.caretBoundsDip, "pending source snapshot has no fabricated caret bounds");
    const auto outstanding = requests.count;
    for (size_t key = 0u; key < 48u; ++key)
        Check(view.DispatchCharacter(L'x'), "pending embedded owner accepts rapid Unicode source input");
    const auto latest = editor->GetText();
    Check(requests.count == outstanding && editor->DebugGetState().preparationAdvances == pending.preparationAdvances,
          "typing coalesces wakeups and never drains or resumes obsolete glyph work");
    Check(view.ApplyTextInput(stale.revision, stale.state, EmbeddedTextInputAction::Commit) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) &&
              editor->GetText() == latest,
          "stale service result cannot overwrite newer pending source");
    EmbeddedTextInputSnapshot current;
    Hr(view.ReadTextInput(current), "read pending composition entry");
    auto preview = current.state;
    preview.text += L"\u03b2";
    preview.caretIndex = preview.text.size();
    preview.selectionAnchorIndex.reset();
    preview.compositionStartIndex = latest.size();
    preview.compositionEndIndex   = preview.text.size();
    Hr(view.ApplyTextInput(current.revision, preview, EmbeddedTextInputAction::Preview), "IME preview remains available during reflow");
    Hr(view.ReadTextInput(current), "read pending composition revision");
    Hr(view.ApplyTextInput(current.revision, {}, EmbeddedTextInputAction::Cancel), "cancel pending composition transaction");
    Check(editor->GetText() == latest && otherEditor->DebugGetState().layoutGeneration == independentGeneration,
          "composition cancellation preserves source/history and the independent view");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "publish latest coalesced embedded source");
    Hr(view.ReadTextInput(current), "read current prepared source");
    Check(current.state.text == latest && current.caretBoundsDip && ! view.NeedsPreparation(), "only final source/geometry becomes coherent");
    editor->SetText(seed);
    view.Controls().CaptureMouse(editor); // Embedded logical capture owns no OS capture.
    Check(view.Controls().GetCapturedControl() == editor, "pending source fixture begins with logical capture");
    Check(view.Prepare(480, 240) == S_FALSE && editor->DebugGetState().preparationPending, "enter pending zero-size fixture");
    Check(view.Controls().GetCapturedControl() == nullptr, "pending geometry cancels capture before an unavailable Up can be lost");
    const auto zeroRequests = requests.count;
    Check(view.Prepare(0, 0) == S_FALSE && ! editor->DebugGetState().preparationPending && view.GetStatistics().surfaceBytes == 0u &&
              requests.count == zeroRequests,
          "zero extent cancels staging and adds no periodic work");
    Check(view.Prepare(480, 240) == S_FALSE, "resumed extent starts fresh bounded work");
    const auto oldResources = view.Controls().GetResourceGeneration();
    Hr(view.ReplaceDevice(graphics), "replace resources with the same graphics pointer");
    Check(view.Controls().GetResourceGeneration() != oldResources && ! editor->DebugGetState().preparationPending &&
              editor->DebugGetState().retainedGeometryChunks == 0u && editor->GetText() == seed,
          "resource epoch cancels old work even when device/pool addresses repeat");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare after resource replacement");
    editor->SetText(seed + L"x");
    Check(view.Prepare(480, 240) == S_FALSE, "enter pending density and extent fixture");
    const auto cancellations = editor->DebugGetState().cancelledPreparations;
    Check(view.Prepare(360, 180, 144) == S_FALSE && editor->DebugGetState().cancelledPreparations > cancellations && editor->GetText() == seed + L"x",
          "changed DPI and width replace pending work without publishing the old key");
    editor->SetDensity(Density::Compact);
    Check(! editor->DebugGetState().preparationPending, "embedded density change retires the pending viewport request");
    Hr(PrepareRichEmbeddedContent(view, 360, 180, 144), "publish only the new embedded typography and width");
    editor->SetText(seed);
    Check(view.Prepare(480, 240) == S_FALSE, "enter pending hidden fixture");
    view.SetVisible(false);
    const auto hiddenRequests = requests.count;
    Check(! editor->DebugGetState().preparationPending && view.Prepare(480, 240) == S_FALSE && requests.count == hiddenRequests,
          "hidden view retires preparation without wakeups");
    view.SetVisible(true);
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "resume visible cooperative view");
    editor->SetText(seed);
    const auto lifetime  = GetControlLifetimeToken(*editor);
    requests.replaceRoot = true;
    Check(view.Prepare(480, 240) == S_FALSE && lifetime.expired(), "continuation callback may retire its editor safely");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare replacement root after callback retirement");
    Check(dynamic_cast<Label*>(view.Controls().GetRoot()) != nullptr && ! view.NeedsPreparation(), "retired editor work never replaces the current tree");
}

static void TestRichTextCooperativeFailureDoesNotRetryAutomatically(GraphicsFixture& gpu)
{
    using namespace DxUi;
    class FailingPreparation final : public RichTextEditor
    {
    public:
        FailingPreparation() : RichTextEditor(L"source survives")
        {
        }
        bool fail = true;
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            return fail ? E_OUTOFMEMORY : RichTextEditor::PrepareContent(host);
        }
    };
    EmbeddedHost view;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "failing cooperative graphics");
    size_t requests = 0u;
    Hr(view.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "failing cooperative host");
    auto owned   = std::make_unique<FailingPreparation>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto beforeFailure = requests;
    Check(FAILED(view.Prepare(480, 240)) && view.NeedsPreparation() && requests == beforeFailure,
          "preparation failure suppresses publication and schedules no automatic retry");
    editor->fail = false;
    editor->SetText(L"new source permits retry");
    Check(requests == beforeFailure + 1u, "a new model request wakes a failed dirty view exactly once");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "retry new source after explicit change");
    Check(editor->GetText() == L"new source permits retry" && editor->DebugGetState().layoutReady, "explicit retry publishes retained source");
}

// Included by the supplied-device fixture after its Check/Hr and allocation counter.
static void TestUnicodeGraphemeForwardWalkAllocationBudget()
{
    // Four independent elements per pattern: Hangul, Indic, pictographic ZWJ,
    // ASCII. The caller prepares the source before the measured forward walk.
    std::wstring source;
    constexpr size_t repetitions = 4096u;
    for (size_t index = 0; index < repetitions; ++index)
        source += L"\u1100\u1161\u11a8\u0915\u094d\u0915\U0001f469\u200d\U0001f4bbx";
    const bool previousCounting    = countAllocations;
    const auto previousAllocations = allocations, previousBytes = allocationBytes;
    size_t elements = 0u, cursor = 0u, calls = 0u, bytes = 0u;
    bool progress = true;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = previousCounting;
            allocations      = previousAllocations;
            allocationBytes  = previousBytes;
        });
        allocationBytes = allocations = 0u;
        countAllocations              = true;
        while (cursor < source.size() && elements <= source.size())
        {
            const auto next = DxUi::FindNextTextElementBoundary(source, cursor);
            if (next <= cursor || next > source.size())
            {
                progress = false;
                break;
            }
            cursor = next;
            ++elements;
        }
        countAllocations = false;
        calls            = allocations;
        bytes            = allocationBytes;
    }
    std::cout << "Unicode17 forward walk units=" << source.size() << " elements=" << elements << " CppAllocations=" << calls << " CppRequestedBytes=" << bytes
              << '\n';
    Check(progress && cursor == source.size() && elements == repetitions * 4u,
          "forward Unicode traversal makes bounded progress through independent mixed-script elements");
    Check(calls == 0u && bytes == 0u, "forward grapheme traversal owns no C++ heap storage");
}

static void TestUnicodeBulkMovementAndDeletionAllocationBudget()
{
    std::wstring source;
    std::vector<size_t> boundaries{0u};
    constexpr int repetitions = 4096, expectedElements = repetitions * 4;
    for (int index = 0; index < repetitions; ++index)
    {
        const auto first = source.size();
        source += L"\u1100\u1161\u11a8\u0915\u094d\u0915\U0001f469\u200d\U0001f4bbx";
        for (const auto end : {3u, 6u, 11u, 12u})
            boundaries.push_back(first + end);
    }
    const bool previousCounting    = countAllocations;
    const auto previousAllocations = allocations, previousBytes = allocationBytes;
    DxUi::TextRangeUnitMoveResult forward, backward, singleBackward;
    std::array<DxUi::TextElementRange, 4> deletions;
    size_t calls = 0u, bytes = 0u;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = previousCounting;
            allocations      = previousAllocations;
            allocationBytes  = previousBytes;
        });
        allocationBytes = allocations = 0u;
        countAllocations              = true;
        forward                       = DxUi::MoveAccessibilityTextPositionByUnit(source, 0u, TextUnit_Character, INT_MAX);
        backward                      = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, INT_MIN);
        singleBackward                = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, -1);
        deletions[0]                  = DxUi::GetTextElementDeletionRange(source, 1u, false);
        deletions[1]                  = DxUi::GetTextElementDeletionRange(source, 1u, true);
        deletions[2]                  = DxUi::GetTextElementDeletionRange(source, 1u, false, boundaries);
        deletions[3]                  = DxUi::GetTextElementDeletionRange(source, 1u, true, boundaries);
        countAllocations              = false;
        calls                         = allocations;
        bytes                         = allocationBytes;
    }
    std::cout << "Unicode17 bulk commands units=" << source.size() << " elements=" << expectedElements << " CppAllocations=" << calls
              << " CppRequestedBytes=" << bytes << '\n';
    Check(forward.position == source.size() && forward.moved == expectedElements && backward.position == 0u && backward.moved == -expectedElements,
          "extreme bulk commands return independent endpoints and exact signed counts");
    for (const auto range : deletions)
        Check(range.first == 0u && range.last == 3u, "indexed and streaming deletion enclose the complete first Hangul element");
    Check(singleBackward.position == source.size() - 1u && singleBackward.moved == -1, "single backward command keeps the independent final ASCII boundary");
    Check(calls == 0u && bytes == 0u, "bulk character movement and collapsed deletion request no C++ heap storage");
}

static void TestRichTextUtf8RejectionAllocationBudget()
{
    for (const auto inputBytes : {DxUi::RichText::Document::MaximumUnits + 1u, DxUi::RichText::Document::MaximumUnits * 3u + 1u})
    {
        const std::string input(inputBytes, 'a'); // Caller-owned input is outside the decoder's allocation budget.
        const bool previousCounting    = countAllocations;
        const auto previousAllocations = allocations, previousBytes = allocationBytes;
        bool lengthRejected   = false;
        size_t requestedBytes = 0u;
        {
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = previousCounting;
                allocations      = previousAllocations;
                allocationBytes  = previousBytes;
            });
            allocationBytes = allocations = 0u;
            countAllocations              = true;
            try
            {
                static_cast<void>(DxUi::RichText::DecodeUtf8(input));
            }
            catch (const std::length_error&)
            {
                lengthRejected = true;
            }
            countAllocations = false;
            requestedBytes   = allocationBytes;
        }
        std::cout << "rich UTF8 rejection inputBytes=" << inputBytes << " decoderCppRequestedBytes=" << requestedBytes << '\n';
        Check(lengthRejected, "oversized UTF8 source returns a length error");
        Check(requestedBytes <= 4096u, "oversized UTF8 rejection never materializes the oversized UTF16 result");
    }
}

static void TestRetainedRichTextUiaRangeAllocationBudget(GraphicsFixture& gpu)
{
    using namespace DxUi;
    const bool previousCounting      = countAllocations;
    const size_t previousAllocations = allocations, previousBytes = allocationBytes;
    // This metadata probe must not contaminate the suite's separate absolute
    // warm-composition counter, which starts at zero and is checked later.
    const auto restoreCounters = wil::scope_exit([=] noexcept
    {
        countAllocations = previousCounting;
        allocations      = previousAllocations;
        allocationBytes  = previousBytes;
    });
    countAllocations           = false;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "rich UIA allocation pool");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "rich UIA allocation view");
    auto root    = std::make_unique<RichTextEditor>(L"initial");
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare rich allocation source");
    view.Controls().SetFocusControl(editor, false);
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare focus before publishing rich allocation semantics");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    const EmbeddedAccessibilityPlacement placement{{0, 0, 480, 240}, true};
    Check(view.AttachAccessibility(site, 0x5249434842554447ull, placement) == S_OK, "attach coherent rich allocation semantics");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Check(view.GetAccessibilityProvider(provider.put()) == S_OK && provider, "coherent rich allocation root is present");
    site->root        = provider.get();
    const auto detach = wil::scope_exit([&]
    {
        site->root = nullptr;
        view.Detach();
    });
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    Hr(provider->ElementProviderFromPoint(16, 16, fragment.put()), "rich allocation document element");
    Check(bool(fragment), "rich source is reachable in the allocation fixture");
    wil::com_ptr_nothrow<ITextProvider2> text;
    Hr(fragment.query_to(text.put()), "rich allocation TextPattern2");
    for (const size_t units : {size_t(32), RichText::Document::MaximumUnits})
    {
        editor->SetText(std::wstring(units, L'a'));
        Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare small/maximum rich source outside range-query budget");
        Check(view.UpdateAccessibility(placement) == S_OK, "publish small/maximum rich source");
        wil::com_ptr_nothrow<ITextRangeProvider> document;
        Hr(text->get_DocumentRange(document.put()), "read published source outside metadata budget");
        Check(bool(document), "published source range exists");
        wil::unique_bstr source;
        Hr(document->GetText(-1, source.put()), "read actual published source length");
        Check(SysStringLen(source.get()) == units, "allocation fixture really publishes the requested small/maximum source");
        BOOL active = FALSE;
        wil::com_ptr_nothrow<ITextRangeProvider> warm;
        Hr(text->GetCaretRange(&active, warm.put()), "warm rich caret metadata");
        warm.reset();
        size_t largestRequest = 0;
        for (size_t iteration = 0; iteration < 8; ++iteration)
        {
            wil::com_ptr_nothrow<ITextRangeProvider> caret;
            SAFEARRAY* visible = nullptr;
            allocationBytes = allocations = 0;
            countAllocations              = true;
            const HRESULT caretResult     = text->GetCaretRange(&active, caret.put());
            const HRESULT visibleResult   = text->GetVisibleRanges(&visible);
            countAllocations              = false;
            largestRequest                = std::max(largestRequest, allocationBytes);
            const auto destroy            = wil::scope_exit([&]
            {
                if (visible)
                    SafeArrayDestroy(visible);
            });
            Hr(caretResult, "bounded caret metadata query");
            Hr(visibleResult, "bounded visible-range metadata query");
            Check(active && caret && visible, "queries retain current range identities");
        }
        // Range metadata may allocate provider objects. It must not duplicate
        // a 128 KiB maximum document for each caret/visible-range request.
        Check(largestRequest <= 4096, "rich range C++ allocation budget is independent of source size");
        std::cout << "rich UIA ranges sourceUnits=" << units << " largestMetadataRequestBytes=" << largestRequest << '\n';
    }
}

static void TestRetainedRichTextDefaultFontChunkReuse(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "positioned rich-text shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "positioned rich-text embedded view");
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    auto control = std::make_unique<RichTextEditor>(seed);
    auto* editor = control.get();
    view.Controls().SetRoot(std::move(control));
    view.Controls().SetFocusControl(editor, false);
    for (const float dpi : {96.0f, 144.0f, 192.0f})
    {
        Hr(PrepareRichEmbeddedContent(view, 480, 240, dpi), "positioned rich-text embedded initial preparation");
        editor->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
        Check(editor->ReplaceSelection(dpi == 144.0f ? L"y" : L"x"), "positioned embedded fixture changes display text");
        Hr(PrepareRichEmbeddedContent(view, 480, 240, dpi), "positioned rich-text embedded edited preparation");
        const auto state = editor->DebugGetState();
        auto* format     = view.Controls().GetTextFormat(FontRole::Body);
        Check(format && format->GetFontSize() == 13.0f && state.layoutReady && state.retainedGeometryChunks > 1u && state.retainedGeometryChunks <= 128u &&
                  state.reusedGeometryChunks > 0u && state.preparedSourceUnits <= 2048u && ! state.wholeGeometryFallback && ! state.wholeRasterFallback &&
                  state.wholeLayoutsPrepared == 0u && state.wholeLayoutSourceUnits == 0u,
              "actual default 13-DIP embedded editor reuses positioned chunks at each DPI without hidden whole-layout work");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "positioned embedded cached composition");
        EmbeddedTextInputSnapshot snapshot;
        Hr(view.ReadTextInput(snapshot), "positioned embedded source text input snapshot");
        Check(snapshot.state.text == editor->GetText() && snapshot.state.text.size() == seed.size(),
              "private layout-origin units never enter the embedded text store");
        std::cout << "rich embedded positioned dpi=" << dpi << " bodyFontSize=" << format->GetFontSize() << " chunks=" << state.retainedGeometryChunks
                  << " reusedChunks=" << state.reusedGeometryChunks << " preparedSourceUnits=" << state.preparedSourceUnits << '\n';
    }
}
static void TestRetainedRichTextSharedViewsAndResources(GraphicsFixture& gpu, bool semanticRendering = false)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "rich-text shared WARP pool");
    EmbeddedHost first, second;
    size_t requests = 0;
    Hr(first.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "rich-text first view");
    Hr(second.Attach(graphics), "rich-text second view");
    std::wstring source = L"# Owned Unicode\r\n**bold** \u03b2 \U0001f600 a\u0301\n\u05d0\u05d1\u05d2\nlast";
    if (semanticRendering)
        source = L"9. [link](local)\n10. <b>native</b>\n\n<p><a href='#x'>HTML</a><br/>&fjlig;</p>\n\n" + source +
                 L"\n\n>\r\n- \r\n\n| a | b |\n| - | - |\n| value |\n";
    auto a       = std::make_unique<RichTextEditor>(source);
    auto b       = std::make_unique<RichTextEditor>(source);
    auto* editor = a.get();
    auto* other  = b.get();
    if (semanticRendering)
    {
        a->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
        b->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
        a->SetMarkdownRenderingEnabled(true);
        b->SetMarkdownRenderingEnabled(true);
    }
    first.Controls().SetRoot(std::move(a));
    second.Controls().SetRoot(std::move(b));
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text first preparation");
    Hr(PrepareRichEmbeddedContent(second, 320, 160, 144), "rich-text independent density preparation");
    Check(first.Controls().GetHwnd() == nullptr && second.Controls().GetHwnd() == nullptr, "retained rich text creates no embedded HWND");
    const auto prepared        = editor->DebugGetState();
    const auto otherGeneration = other->DebugGetState().layoutGeneration;
    Check(prepared.layoutReady && other->DebugGetState().layoutReady, "both layouts prepared through host DWrite");
    Check(! semanticRendering || (editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic &&
                                  other->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic),
          "semantic resource fixture uses the opt-in geometry engine in both independent views");
    Check(first.GetStatistics().surfaceBytes != second.GetStatistics().surfaceBytes, "two views retain separate bounded surfaces");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "rich-text initial composition");
    std::vector<uint8_t> before;
    Hr(gpu.Read(before), "rich-text initial pixels");
    Check(PrepareRichEmbeddedContent(first, 480, 240) == S_FALSE, "clean rich text does no preparation");
    const auto statistics          = first.GetStatistics();
    const size_t allocationsBefore = allocations;
    countAllocations               = true;
    for (int frame = 0; frame != 12; ++frame)
        Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "warm rich-text composition");
    countAllocations = false;
    Check(allocations == allocationsBefore, "warm rich-text composition allocates no C++ heap memory");
    Check(first.GetStatistics().preparations == statistics.preparations && first.GetStatistics().surfaceAllocations == statistics.surfaceAllocations &&
              editor->DebugGetState().layoutGeneration == prepared.layoutGeneration,
          "composition does no layout rasterization or surface allocation");

    // Control visibility differs from suspending the complete view. In both
    // cases the retired rich layout must stay retired until the control is shown.
    editor->SetVisible(false);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "hidden concrete rich-text root preparation");
    Check(! editor->DebugGetState().layoutReady, "hidden root painting does not recreate retired layout");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "hidden rich-text root composition");
    std::vector<uint8_t> hidden;
    Hr(gpu.Read(hidden), "hidden rich-text root pixels");
    Check(hidden != before, "hidden root does not expose its text");
    editor->SetVisible(true);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "shown concrete rich-text root preparation");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "shown rich-text root composition");
    std::vector<uint8_t> restored;
    Hr(gpu.Read(restored), "shown rich-text root pixels");
    Check(restored == before && first.GetStatistics().surfaceAllocations == statistics.surfaceAllocations,
          "show restores identical rich pixels without replacing the view surface");
    first.Controls().SetFocusControl(editor, false);
    editor->SetSelection({source.size(), source.size()});
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text logical focus");
    const auto revision = editor->Revision();
    Check(first.DispatchCharacter(wchar_t(0xd83d)) && editor->Revision() == revision, "pending embedded scalar remains unpublished");
    Check(first.DispatchCharacter(wchar_t(0xde00)) && editor->Revision() == revision + 1, "embedded scalar publishes one revision");
    Check(other->GetText() == source && other->DebugGetState().layoutGeneration == otherGeneration, "editing one view leaves the other independent");
    Check(requests > 0 && first.NeedsPreparation(), "editing schedules host preparation");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text changed preparation");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "rich-text changed composition");
    std::vector<uint8_t> after;
    Hr(gpu.Read(after), "rich-text changed pixels");
    Check(before != after, "retained source edit changes actual WARP pixels");
    EmbeddedTextInputSnapshot unsupported;
    Check(first.ReadTextInput(unsupported) == S_OK && unsupported.state.multiline, "D2d exposes source text through the shared embedded adapter");
    Check(first.DispatchPointer({PointerAction::Down, 12, 16}), "rich-text embedded pointer selection");
    Check(first.Controls().GetCapturedControl() == editor && editor->DebugGetState().dragging, "selection uses host capture");
    Check(first.DispatchPointer({PointerAction::Move, 110, 42}), "rich-text captured selection extends");
    Check(editor->GetSelection().Start() != editor->GetSelection().End(), "drag updates source selection");
    Check(first.DispatchPointer({PointerAction::Cancel}), "rich-text pointer cancellation");
    Check(first.Controls().GetCapturedControl() == nullptr && ! editor->DebugGetState().dragging, "capture loss retires drag");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text after capture cancellation");
    Check(first.DispatchPointer({PointerAction::Down, 12, 16}) && first.DispatchPointer({PointerAction::Up, 12, 16}), "ordinary pointer release ends drag");
    Check(first.Controls().GetCapturedControl() == nullptr && ! editor->DebugGetState().dragging, "mouse up releases host capture");
    Check(editor->OnMouseDoubleClick(first.Controls(), D2D1::Point2F(12, 16), false, 0) && editor->GetSelection().Start() != editor->GetSelection().End(),
          "double click selects a displayed word using source offsets");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text word selection");
    // Move outside double-click slop: this scenario begins a new drag.
    Check(first.DispatchPointer({PointerAction::Down, 200, 80}), "selection before Escape");
    Check(first.DispatchKey(VK_ESCAPE, true) && first.Controls().GetCapturedControl() == nullptr, "Escape cancels selection capture");
    editor->SetEnabled(false);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "disabled rich-text preparation");
    Check(! first.DispatchCharacter(L'x'), "disabled rich-text receives no character input");
    editor->SetEnabled(true);
    editor->SetSelection({0, 0});
    Hr(PrepareRichEmbeddedContent(first, 480, 240, 192), "rich-text DPI update");
    Check(editor->DebugGetState().layoutReady && editor->GetText().ends_with(L"\U0001f600"), "DPI update preserves source");
    const auto retained          = editor->Snapshot();
    const auto retainedSelection = editor->GetSelection();
    const bool canUndo           = editor->CanUndo();
    first.SetVisible(false);
    Check(first.GetStatistics().surfaceBytes == 0 && ! editor->DebugGetState().layoutReady && ! first.NeedsPreparation() && ! first.NeedsAnimation(),
          "hidden view releases pixels and layout without periodic work");
    Check(! first.DispatchCharacter(L'x') && PrepareRichEmbeddedContent(first, 480, 240) == S_FALSE, "hidden view rejects input and preparation");
    first.SetVisible(true);
    Hr(PrepareRichEmbeddedContent(first, 480, 240, 192), "rich-text show prepares again");
    Check(editor->GetText() == retained.Text(), "show preserves model");
    Hr(PrepareRichEmbeddedContent(first, 0, 0), "zero-size rich text suspends");
    Check(first.GetStatistics().surfaceBytes == 0 && ! first.DispatchCharacter(L'x'), "zero size releases surface and blocks input");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "restore rich-text extent");
    GraphicsFixture replacement;
    Hr(replacement.Create(), "rich-text replacement WARP device");
    std::shared_ptr<GraphicsDevice> nextGraphics;
    Hr(GraphicsDevice::Create(replacement.device.get(), nextGraphics), "rich-text replacement host pool");
    Hr(first.ReplaceDevice(nextGraphics), "rich-text device generation replacement");
    Check(! first.DispatchCharacter(L'x'), "replacement rejects input before coherent preparation");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text replacement preparation");
    replacement.Bind();
    Hr(first.Composite(replacement.context.get(), replacement.Viewport()), "rich-text replacement composition");
    Check(editor->GetText() == retained.Text() && editor->CanUndo() == canUndo && editor->GetSelection() == retainedSelection,
          "device replacement preserves document selection and history");
    Check(editor->Undo() && editor->GetText() == source, "history remains usable after resource recovery");
}

static void TestRetainedRichTextSchedulingCanRetireOwner(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "retiring rich-text pool");
    struct CallbackState
    {
        EmbeddedHost* view = nullptr;
        bool retire        = false;
    } callback;
    EmbeddedHost view;
    callback.view = &view;
    Hr(view.Attach(graphics,
                   {&callback,
                    [](void* context) noexcept
    {
        auto& state = *static_cast<CallbackState*>(context);
        if (std::exchange(state.retire, false))
            state.view->Controls().SetRoot(nullptr);
    }}),
       "retiring rich-text view");
    auto control   = std::make_unique<RichTextEditor>();
    auto* editor   = control.get();
    size_t notices = 0;
    editor->SetOnTextChanged([&] { ++notices; });
    view.Controls().SetRoot(std::move(control));
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "retiring rich-text preparation");
    callback.retire = true;
    Check(editor->ReplaceSelection(L"committed before retirement"), "host scheduling can retire editing owner");
    Check(view.Controls().GetRoot() == nullptr && notices == 0, "retired callback batch stops before text notification");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare replacement empty root after retirement");
}

static void TestRetainedRichTextFailedPreparation(GraphicsFixture& gpu)
{
    class FailingEditor final : public DxUi::RichTextEditor
    {
    public:
        FailingEditor() : RichTextEditor(L"# Retained source\n**kept**")
        {
        }
        mutable bool fail = true;
        void Paint(DxUi::ControlHost& host) const override
        {
            RichTextEditor::Paint(host);
            if (fail)
                throw std::runtime_error("injected post-raster preparation failure");
        }
    };
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "failed rich-text preparation pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(graphics), "failed rich-text preparation host");
    auto owned   = std::make_unique<FailingEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto source = editor->Snapshot();
    Check(FAILED(PrepareRichEmbeddedContent(view, 480, 240)), "rich-text raster exception fails preparation");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! view.DispatchCharacter(L'x'),
          "failed rich-text frame publishes neither pixels nor input");
    editor->fail = false;
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "explicit rich-text preparation retry");
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "recovered rich-text surface composes");
    Check(editor->GetText() == source.Text(), "failed preparation never changes logical document");
}
