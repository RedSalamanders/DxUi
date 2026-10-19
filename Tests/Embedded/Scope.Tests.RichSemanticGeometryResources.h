#pragma once
#include "../Support/Scope.Tests.RichSemanticGeometry.h"

static void TestRichSemanticGeometryFailureBudgets(bool trace = false)
{
    using namespace DxUi::RichText;
    using DxUi::RichText::Detail::PreparedSemanticGeometry;
    auto factory        = RichSemanticGeometryTests::Factory();
    const auto previous = RichSemanticDocumentTests::Build(L"previous\n");
    const auto candidate =
        RichSemanticDocumentTests::Build(L"# **Heading**\n\n| left | right |\n| :--- | ---: |\n| a | b |\n\n> - [x] [label](local) and ![alt](image)\n");
    LayoutOptions options;
    options.widthDip = 240.0f;
    size_t requests  = 0u;
    {
        PreparedSemanticGeometry geometry;
        const auto oldCount = countAllocations;
        const auto oldCalls = allocations;
        const auto restore  = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCount;
            allocations      = oldCalls;
        });
        allocations         = 0u;
        countAllocations    = true;
        geometry.Begin(factory.get(), candidate, options);
        RichSemanticGeometryTests::Complete(geometry);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 4096u, "semantic geometry has an independently observed finite owned allocation schedule");
    for (size_t point = 0u; point < requests; ++point)
    {
        if (trace)
        {
            std::fprintf(stderr, "semantic geometry allocation failure point=%zu of %zu\n", point, requests);
            std::fflush(stderr);
        }
        PreparedSemanticGeometry geometry;
        geometry.Begin(factory.get(), previous, options);
        RichSemanticGeometryTests::Complete(geometry);
        const auto generation = geometry.Generation();
        bool rejected         = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = point;
            try
            {
                geometry.Begin(factory.get(), candidate, options);
                RichSemanticGeometryTests::Complete(geometry);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && geometry.Ready() && ! geometry.Pending() && geometry.Generation() == generation && &geometry.Document() == previous.get(),
              "every geometry allocation failure preserves the complete previous source/layout publication and retires staging");
        geometry.Begin(factory.get(), candidate, options);
        RichSemanticGeometryTests::Complete(geometry);
        Check(&geometry.Document() == candidate.get(), "explicit semantic geometry retry succeeds after each failed allocation");
    }
    PreparedSemanticGeometry geometry;
    geometry.Begin(factory.get(), candidate, options);
    RichSemanticGeometryTests::Complete(geometry);
    const auto oldCalls = allocations;
    countAllocations    = true;
    for (size_t repeat = 0u; repeat < 8u; ++repeat)
    {
        geometry.Begin(factory.get(), candidate, options);
        geometry.Caret(0u);
        geometry.HitTest(4.0f, 4.0f);
        geometry.SourceLines();
    }
    geometry.Clear();
    countAllocations = false;
    Check(allocations == oldCalls, "semantic geometry coalescing, scalar geometry queries and retirement allocate no owned C++ storage");
    std::cout << "rich semantic geometry completeFailures=" << requests << " idle/retireAllocations=0\n";
}
