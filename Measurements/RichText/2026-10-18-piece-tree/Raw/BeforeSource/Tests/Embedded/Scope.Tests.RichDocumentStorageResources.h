#pragma once

// Included after the supplied-device fixture's C++ allocation counters and Check helper.
static void TestRichDocumentFragmentedHistoryStorageBudget()
{
    const std::wstring seed(8192u, L'a');
    std::wstring expected;
    for (size_t index = 0; index < 4096u; ++index)
        expected += L"aX";
    expected.append(4096u, L'a');
    const bool previousLiveCounting = countLiveBytes;
    const auto previousLiveBytes    = liveBytes;
    ptrdiff_t retained = 0, released = 0;
    bool textMatches = false;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countLiveBytes = previousLiveCounting;
            liveBytes      = previousLiveBytes;
        });
        liveBytes          = 0;
        countLiveBytes     = true;
        {
            DxUi::RichText::Document document(seed);
            for (size_t index = 0; index < 4096u; ++index)
            {
                const auto position = index * 2u + 1u;
                document.Replace(position, position, L"X", {position + 1u, position + 1u});
            }
            textMatches = document.Text() == expected;
            retained    = liveBytes;
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    std::cout << "rich fragmented storage units=" << expected.size() << " edits=4096 retainedCppBytes=" << retained << " releasedDelta=" << released << '\n';
    Check(textMatches, "fragmented edit source matches an independently constructed flat oracle");
    Check(released == 0, "model, captured buffers and capped history release their tracked C++ storage");
    Check(retained >= 0 && retained <= 4 * 1024 * 1024, "fragmented source plus capped history retains at most four MiB of C++ storage");
}

static void TestRichDocumentSameContentAllocationBudget()
{
    DxUi::RichText::Document document(std::wstring(8192u, L'a'));
    for (size_t index = 0; index < 4096u; ++index)
    {
        const auto position = index * 2u + 1u;
        document.Replace(position, position, L"X", {position + 1u, position + 1u});
    }
    document.Replace(0u, 1u, L"q", {1u, 1u});
    document.Undo();
    const auto revision = document.Revision();
    const bool undo = document.CanUndo(), redo = document.CanRedo();
    auto same                      = document.Snapshot().Slice(4096u, 8192u);
    const bool previousCounting    = countAllocations;
    const auto previousAllocations = allocations, previousBytes = allocationBytes;
    bool changed = true;
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
        changed                       = document.Replace(4096u, 8192u, std::move(same), {8192u, 4096u});
        countAllocations              = false;
        calls                         = allocations;
        bytes                         = allocationBytes;
    }
    std::cout << "rich same-content edit CppAllocations=" << calls << " CppRequestedBytes=" << bytes << '\n';
    Check(! changed && document.Revision() == revision && document.CanUndo() == undo && document.CanRedo() == redo &&
              document.GetSelection() == DxUi::RichText::Selection{8192u, 4096u},
          "same-content edit only acknowledges exact selection and preserves revision and both histories");
    Check(calls == 0u && bytes == 0u, "same-content replacement clones no source storage or history");
}
