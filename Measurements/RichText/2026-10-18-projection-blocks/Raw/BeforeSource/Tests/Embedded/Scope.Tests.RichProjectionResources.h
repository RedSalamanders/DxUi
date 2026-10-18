#pragma once
static void TestRichProjectionMappingAndUnchangedAllocationBudget()
{
    using namespace DxUi::RichText;
    std::wstring input;
    for (size_t line = 0u; line < 512u; ++line)
        input += L"# **a\u0301** \U0001f600\r\n";
    Document document(input);
    PreparedProjection prepared;
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    const auto flat        = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    bool correct = true;
    size_t calls = 0u, bytes = 0u;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        for (size_t repeat = 0u; repeat < 4u; ++repeat)
        {
            prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
            for (size_t offset = 0u; offset <= flat.source.size(); ++offset)
                correct = correct && prepared.ToDisplay(offset) == flat.ToDisplay(offset);
            for (size_t offset = 0u; offset <= flat.text.size(); ++offset)
                for (const bool trailing : {false, true})
                    correct = correct && prepared.ToSource(offset, trailing) == flat.ToSource(offset, trailing);
        }
        countAllocations = false;
        calls            = allocations;
        bytes            = allocationBytes;
    }
    std::cout << "rich projection mappings repeats=4 lines=512 CppAllocations=" << calls << " CppRequestedBytes=" << bytes << '\n';
    Check(correct && calls == 0u && bytes == 0u, "prepared projection mappings and unchanged preparation allocate no C++ storage");
}

static void TestRichProjectionPreparationAllocationFailureAtomicity()
{
    using namespace DxUi::RichText;
    std::wstring seed;
    for (size_t line = 0u; line < 32u; ++line)
        seed += L"line **bold**\r\n";
    Document document(seed);
    const auto previous         = document.Snapshot();
    const auto previousRevision = document.Revision();
    const auto position         = seed.find(L"line", 100u) + 1u;
    document.Replace(position, position + 1u, L"x\n# new\r\n", {0u, 0u});
    size_t requests = 0u;
    {
        PreparedProjection candidate;
        candidate.Prepare(previous, previousRevision, ViewMode::Rich);
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
        candidate.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 256u, "changed projection has a finite observed allocation schedule");
    for (size_t failure = 0u; failure < requests; ++failure)
    {
        PreparedProjection candidate;
        candidate.Prepare(previous, previousRevision, ViewMode::Rich);
        const auto text   = candidate.DisplayText();
        const auto blocks = candidate.Blocks();
        const auto stats  = candidate.LastPreparation();
        bool rejected     = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                candidate.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && candidate.Ready() && candidate.Revision() == previousRevision && candidate.DisplayText() == text &&
                  candidate.SourceLength() == seed.size() && candidate.Blocks().size() == blocks.size() &&
                  candidate.LastPreparation().rebuiltBlocks == stats.rebuiltBlocks && candidate.LastPreparation().reusedBlocks == stats.reusedBlocks &&
                  candidate.LastPreparation().rebuiltSourceUnits == stats.rebuiltSourceUnits,
              "failed preparation preserves the complete published projection and its identity");
        for (size_t index = 0u; index < blocks.size(); ++index)
            Check(candidate.Blocks()[index].content == blocks[index].content, "failed preparation retains every original immutable block identity");
        candidate.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
        Check(candidate.DisplayText() == Project(document.Snapshot(), document.Revision(), ViewMode::Rich).text,
              "normal preparation recovers after every injected allocation failure");
    }
    std::cout << "rich projection allocation-failure sweptRequests=" << requests << '\n';
}
