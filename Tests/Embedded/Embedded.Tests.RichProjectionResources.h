#pragma once
static void TestRichSnapshotSliceAllocationFailureAtomicity(bool trace = false)
{
    using namespace DxUi::RichText;
    const std::wstring expected(1024u, L'a');
    Document document(L"A" + expected + L"\U0001f600\r\nZ");
    const auto snapshot = document.Snapshot();
    const auto original = document.Text();
    const auto revision = document.Revision();
    size_t requests     = 0u;
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
        const auto slice              = snapshot.Slice(1u, 1025u);
        countAllocations              = false;
        requests                      = allocations;
        Check(slice == expected, "snapshot slice matches the independent long Unicode boundary oracle");
    }
    Check(requests > 0u && requests < 16u, "snapshot slice has a finite observed allocation schedule");
    for (size_t failure = 0u; failure < requests; ++failure)
    {
        if (trace)
        {
            std::fprintf(stderr, "rich snapshot slice failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
        bool rejected = false;
        {
            const auto previous = failAllocationAfter;
            const auto restore  = wil::scope_exit([=] noexcept { failAllocationAfter = previous; });
            failAllocationAfter = failure;
            try
            {
                static_cast<void>(snapshot.Slice(1u, 1025u));
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && snapshot.Text() == original && document.Text() == original && document.Revision() == revision && ! document.CanUndo() &&
                  ! document.CanRedo(),
              "slice allocation failure propagates without terminating or mutating source/history");
    }
    std::cout << "rich snapshot slice allocation-failure sweptRequests=" << requests << '\n';
}

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

static void TestRichProjectionPreparationAllocationFailureAtomicity(bool trace = false)
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
        if (trace)
        {
            std::fprintf(stderr, "rich projection failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
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

static void TestRichProjectionRetainedStorageBudget()
{
    using namespace DxUi::RichText;
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line " + std::to_wstring(line) + L" **bold** and `code`\r\n";
    const auto position    = seed.find(L"line 512") + 2u;
    const auto oldCounting = countLiveBytes;
    const auto oldBytes    = liveBytes;
    ptrdiff_t retained = 0, released = 0;
    bool correct = false;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countLiveBytes = oldCounting;
            liveBytes      = oldBytes;
        });
        liveBytes          = 0;
        countLiveBytes     = true;
        {
            Document document(seed);
            PreparedProjection prepared;
            prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
            for (size_t edit = 0u; edit < 512u; ++edit)
            {
                document.Replace(position, position + 1u, edit % 2u ? L"a" : L"b", {0u, 0u});
                prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
            }
            correct  = prepared.DisplayText() == Project(document.Snapshot(), document.Revision(), ViewMode::Rich).text && prepared.Blocks().size() == 1024u &&
                       prepared.LastPreparation().rebuiltBlocks == 3u;
            retained = liveBytes;
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    std::cout << "rich projection retained lines=1024 edits=512 retainedCppBytes=" << retained << " releasedDelta=" << released << '\n';
    Check(correct && retained >= 0 && retained <= 4 * 1024 * 1024 && released == 0,
          "projection plus source and capped history retains at most four MiB and releases all tracked bytes");
}

static void TestRichProjectionMaximumSourceStorageBudget()
{
    using namespace DxUi::RichText;
    for (size_t kind = 0u; kind < 2u; ++kind)
    {
        std::wstring seed;
        if (kind == 0u)
            seed.assign(Document::MaximumUnits, L'\n');
        else
        {
            seed.reserve(Document::MaximumUnits);
            for (size_t line = 0u; line < Document::MaximumUnits / 2u; ++line)
            {
                seed.push_back(static_cast<wchar_t>(0x1000u + line));
                seed.push_back(L'\n');
            }
        }
        const auto oldCounting = countLiveBytes;
        const auto oldBytes    = liveBytes;
        ptrdiff_t retained = 0, released = 0;
        bool correct = false, shared = true;
        {
            const auto restore = wil::scope_exit([=] noexcept
            {
                countLiveBytes = oldCounting;
                liveBytes      = oldBytes;
            });
            liveBytes          = 0;
            countLiveBytes     = true;
            {
                Document document(seed);
                PreparedProjection prepared;
                prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
                correct = prepared.DisplayText() == seed && prepared.Blocks().size() == Document::MaximumUnits / (kind ? 2u : 1u);
                if (kind == 0u)
                    for (const auto& block : prepared.Blocks())
                        shared = shared && block.content == prepared.Blocks().front().content;
                retained = liveBytes;
            }
            released       = liveBytes;
            countLiveBytes = false;
        }
        std::cout << "rich projection maximum kind=" << kind << " sourceUnits=65536 retainedCppBytes=" << retained << " releasedDelta=" << released << '\n';
        Check(correct && shared && retained >= 0 && retained <= 16 * 1024 * 1024 && released == 0,
              "maximum newline/unique-line projection is bounded to sixteen MiB, shares adjacent equal lines and releases all bytes");
    }
}

static bool RichProjectionValuesEqual(const DxUi::RichText::Projection& left, const DxUi::RichText::Projection& right) noexcept
{
    return left.source == right.source && left.text == right.text && left.sourceToDisplay == right.sourceToDisplay && left.revision == right.revision &&
           left.mode == right.mode && left.units.size() == right.units.size() &&
           std::equal(
               left.units.begin(), left.units.end(), right.units.begin(), [](const auto& a, const auto& b) { return a.start == b.start && a.end == b.end; }) &&
           left.runs.size() == right.runs.size() && std::equal(left.runs.begin(), left.runs.end(), right.runs.begin(), [](const auto& a, const auto& b) {
        return a.start == b.start && a.length == b.length && a.flags == b.flags && a.heading == b.heading;
    });
}
static void TestRichFlatProjectionFailureAtomicity(bool trace = false)
{
    using namespace DxUi::RichText;
    const std::wstring seed = L"# Heading\r\n**bold** a\u0301 \U0001f600 `code`\ntrailing text";
    Document document(seed);
    const auto snapshot = document.Snapshot();
    const auto revision = document.Revision();
    const auto expected = Project(snapshot, revision, ViewMode::Rich);
    size_t requests     = 0u;
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
        const auto observed           = Project(snapshot, revision, ViewMode::Rich);
        countAllocations              = false;
        requests                      = allocations;
        Check(RichProjectionValuesEqual(observed, expected), "measured flat projection preserves all owned fields");
    }
    Check(requests > 0u && requests < 4096u, "flat projection has a finite observed C++ allocation schedule");
    for (size_t failure = 0u; failure < requests; ++failure)
    {
        if (trace)
        {
            std::fprintf(stderr, "rich flat projection failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
        bool rejected = false, unexpectedlyCompleted = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                const auto observed   = Project(snapshot, revision, ViewMode::Rich);
                unexpectedlyCompleted = RichProjectionValuesEqual(observed, expected);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && ! unexpectedlyCompleted && document.Revision() == revision && snapshot.Text() == seed && document.Text() == seed,
              "flat projection allocation failure preserves its immutable source and document");
        Check(RichProjectionValuesEqual(Project(snapshot, revision, ViewMode::Rich), expected), "flat projection retry reproduces the complete owned value");
    }
    std::cout << "rich flat projection allocation-failure sweptRequests=" << requests << '\n';
}
