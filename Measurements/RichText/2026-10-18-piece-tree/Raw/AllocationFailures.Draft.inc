static void TestRichDocumentAllocationFailureAtomicity()
{
    using namespace DxUi::RichText;
    Document baseline(std::wstring(1024u, L'a'));
    for (size_t index = 0u; index < 512u; ++index)
    {
        const auto position = index * 2u + 1u;
        baseline.Replace(position, position, L"X", {position + 1u, position + 1u});
    }
    baseline.Undo(); // Both histories must survive every failed operation.
    const auto expected = baseline.Text();
    const auto selection = baseline.GetSelection();
    const auto revision = baseline.Revision();
    const auto saved = baseline.Snapshot();
    const auto apply = [](Document& document, size_t operation, std::wstring text)
    {
        switch (operation)
        {
            case 0u: document.Replace(77u, 81u, std::move(text), {0u, 0u}); break;
            case 1u: document.ReplaceTransient(std::move(text), {0u, 0u}); break;
            case 2u: document.Reset(std::move(text)); break;
            case 3u: document.Undo(); break;
            case 4u: document.Redo(); break;
        }
    };
    const auto historiesMatch = [](Document left, Document right)
    {
        for (bool redo : {false, true})
            for (size_t step = 0u; step <= Document::MaximumHistory * 2u; ++step)
            {
                const bool a = redo ? left.Redo() : left.Undo();
                const bool b = redo ? right.Redo() : right.Undo();
                if (a != b || left.Text() != right.Text() || left.GetSelection() != right.GetSelection())
                    return false;
                if (!a)
                    break;
            }
        return true;
    };
    for (size_t operation = 0u; operation < 5u; ++operation)
    {
        size_t requests = 0u;
        {
            Document candidate = baseline;
            std::wstring insertion = L"prepared replacement source";
            const auto oldCount = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = oldCount;
                allocations = oldCalls;
                allocationBytes = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations = true;
            apply(candidate, operation, std::move(insertion));
            countAllocations = false;
            requests = allocations;
        }
        Check(requests > 0u && requests < 64u, "allocation-failure sweep has a finite observed operation count");
        for (size_t failure = 0u; failure < requests; ++failure)
        {
            Document candidate = baseline;
            std::wstring insertion = L"prepared replacement source";
            bool rejected = false;
            {
                const auto previous = failAllocationAfter;
                const auto restore = wil::scope_exit([=] noexcept { failAllocationAfter = previous; });
                failAllocationAfter = failure;
                try
                {
                    apply(candidate, operation, std::move(insertion));
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && candidate.Text() == expected && candidate.GetSelection() == selection && candidate.Revision() == revision &&
                      candidate.CanUndo() && candidate.CanRedo() && saved.Text() == expected,
                  "failed source/history allocation publishes no text, selection, revision or history change");
            Check(historiesMatch(candidate, baseline), "all undo/redo entries survive allocation failure in order");
        }
        std::cout << "rich source allocation-failure operation=" << operation << " sweptRequests=" << requests << '\n';
    }
}
