// Inactive test draft for the next slice; integrate only after its baseline.
void TestRichDocumentPersistentSplitMergeOracle()
{
    using namespace DxUi::RichText;
    Document document;
    std::wstring expected;
    std::vector<std::pair<SourceSnapshot, std::wstring>> retained;
    std::uint32_t random = 0x5a17421du;
    const auto next = [&]()
    {
        random = random * 1664525u + 1013904223u;
        return random;
    };
    const auto boundary = [](std::wstring_view text, size_t position)
    {
        if (!position || position == text.size())
            return true;
        const auto left = static_cast<unsigned>(text[position - 1u]), right = static_cast<unsigned>(text[position]);
        return !(left >= 0xd800u && left <= 0xdbffu && right >= 0xdc00u && right <= 0xdfffu) && !(left == '\r' && right == '\n');
    };
    for (size_t round = 0u; round < 32u; ++round)
    {
        document.Reset(std::wstring(8192u, L'a'));
        expected.assign(8192u, L'a');
        for (size_t index = 0u; index < 2048u; ++index)
        {
            const auto position = index * 2u + 1u;
            document.Replace(position, position, L"X", {0u, 0u});
            expected.insert(position, L"X");
        }
        retained.emplace_back(document.Snapshot(), expected);
        // Keep tiny prefixes/suffixes after crossing many internal nodes,
        // then alternate prepend, append and interior edits on the survivors.
        const auto left = static_cast<size_t>(next() % 7u), right = expected.size() - static_cast<size_t>(next() % 7u);
        document.Replace(left, right, L"\U0001f600\r\n", {0u, 0u});
        expected.replace(left, right - left, L"\U0001f600\r\n");
        for (size_t operation = 0u; operation < 256u; ++operation)
        {
            size_t first = next() % (expected.size() + 1u), last = next() % (expected.size() + 1u);
            if (first > last)
                std::swap(first, last);
            while (!boundary(expected, first))
                --first;
            while (!boundary(expected, last))
                ++last;
            const std::wstring insertion = (operation % 3u == 0u) ? L"\r\n\U0001f600" : L"\u0915\u094d\u0915";
            const auto before = expected;
            const auto changed = document.Replace(first, last, insertion, {0u, 0u});
            expected.replace(first, last - first, insertion);
            Require(document.Text() == expected, "split/merge edits agree with an independent flat Unicode oracle");
            if (changed)
            {
                Require(document.Undo() && document.Text() == before, "cross-node undo preserves exact Unicode source");
                Require(document.Redo() && document.Text() == expected, "cross-node redo restores the expected source");
            }
            const auto snapshot = document.Snapshot();
            for (size_t position = 0u; position <= expected.size(); ++position)
                Require(snapshot.IsBoundary(position) == boundary(expected, position), "tree boundaries agree at every independent scalar/CRLF edge");
        }
        if (retained.size() > 8u)
            retained.erase(retained.begin());
        for (const auto& [snapshot, original] : retained)
            Require(snapshot.Text() == original, "retained snapshots survive later splits, merges, reset and history retirement");
    }
}
