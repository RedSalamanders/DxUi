#pragma once
#include "../Support/Scope.Tests.RichSemanticDocument.h"

static void TestRichSemanticDocumentFailureAndStorageBudgets(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto oldSyntax = RichSemanticDocumentTests::Parse(L"previous\n");
    const auto syntax    = RichSemanticDocumentTests::Parse(L"# staged &fjlig;\r\n\r\n> - [x] **nested *styles*** and \x0060 a\r\n>   b \x0060\n\n"
                                                            L"| first | second | third |\n| :--- | :---: | ---: |\n| [link](local) | ![alt *text*](image) |\n\n"
                                                            L"3. ordered\n4. next\n\n<script>raw &amp;</script>\n\n    literal &amp;\n");
    const auto finish    = [](SemanticBuilder& builder, const MarkdownSyntax& input) { return RichSemanticDocumentTests::Complete(builder, input); };
    const auto count     = [](auto&& operation)
    {
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
        operation();
        countAllocations = false;
        return allocations;
    };
    std::size_t requests = 0u, beginRequests = 0u;
    {
        SemanticBuilder builder;
        builder.Begin(oldSyntax);
        finish(builder, *oldSyntax);
        requests = count([&]
        {
            builder.Begin(syntax);
            finish(builder, *syntax);
        });
        SemanticBuilder pending;
        pending.Begin(oldSyntax);
        pending.Advance(1u);
        beginRequests = count([&] { pending.Begin(syntax); });
    }
    Check(requests > 0u && requests < 4096u && beginRequests > 0u && beginRequests < requests,
          "semantic builder has independently observed finite complete/admission allocation schedules");
    for (std::size_t failure = 0u; failure < requests; ++failure)
    {
        if (trace)
        {
            std::fprintf(stderr, "semantic document failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
        SemanticBuilder builder;
        builder.Begin(oldSyntax);
        const auto old = finish(builder, *oldSyntax);
        bool rejected  = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                builder.Begin(syntax);
                finish(builder, *syntax);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && builder.Published() == old && ! builder.HasPendingPreparation() && old->Map().Display() == L"previous\n",
              "each semantic builder allocation failure retains the old immutable document and retires staged work");
        builder.Begin(syntax);
        Check(finish(builder, *syntax)->Map().Source() == syntax->Source(), "semantic builder explicit retry succeeds after each allocation failure");
    }
    for (std::size_t failure = 0u; failure < beginRequests; ++failure)
    {
        SemanticBuilder builder;
        builder.Begin(oldSyntax);
        builder.Advance(1u);
        bool rejected = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                builder.Begin(syntax);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && builder.HasPendingPreparation() && ! builder.Published(), "failed semantic Begin retains the prior pending request");
        Check(finish(builder, *oldSyntax)->Map().Source() == oldSyntax->Source(), "the preserved pending semantic request still completes");
    }
    SemanticBuilder ready;
    ready.Begin(syntax);
    const auto publication = finish(ready, *syntax);
    const auto idle        = count([&]
    {
        ready.Begin(syntax);
        for (std::size_t read = 0u; read < 128u; ++read)
            Check(ready.Published() == publication && ! ready.HasPendingPreparation(), "semantic read does not advance or change publication");
        ready.Clear();
    });
    Check(idle == 0u, "coalesced semantic requests, publication reads and retirement allocate nothing");
    const std::wstring maximum(Document::MaximumUnits, L'a');
    const auto maximumSyntax = RichSemanticDocumentTests::Parse(maximum);
    ptrdiff_t retained = 0, released = 0;
    {
        const auto previousCounting = countLiveBytes;
        const auto previousBytes    = liveBytes;
        const auto restore          = wil::scope_exit([=] noexcept
        {
            countLiveBytes = previousCounting;
            liveBytes      = previousBytes;
        });
        countLiveBytes              = true;
        liveBytes                   = 0;
        {
            SemanticBuilder builder;
            builder.Begin(maximumSyntax);
            finish(builder, *maximumSyntax);
            retained = liveBytes;
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    Check(retained > 0 && retained <= 8 * 1024 * 1024 && released == 0,
          "maximum plain semantic publication stays bounded and releases its tracked C++ storage");
    std::cout << "rich semantic document sweptRequests=" << requests << " pendingBeginRequests=" << beginRequests << " idleRetireAllocations=" << idle
              << " maximumPlainRetainedCppBytes=" << retained << " releasedDelta=" << released << '\n';
}
