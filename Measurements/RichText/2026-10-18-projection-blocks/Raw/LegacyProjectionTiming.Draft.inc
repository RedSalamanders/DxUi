void TestRichProjectionWholeSourceFixedTrials()
{
    using namespace DxUi::RichText;
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line " + std::to_wstring(line) + L" **bold** and `code`\r\n";
    const auto position = seed.find(L"line 512") + 2u;
    for (size_t trial = 0u; trial < 5u; ++trial)
    {
        Document document(seed);
        const auto initialStart = std::chrono::steady_clock::now();
        const auto initial = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
        const auto initialEnd = std::chrono::steady_clock::now();
        const auto expectedChecksum = 128u * (initial.text.size() + initial.runs.size());
        size_t checksum = 0u;
        const auto editStart = std::chrono::steady_clock::now();
        for (size_t edit = 0u; edit < 128u; ++edit)
        {
            document.Replace(position, position + 1u, edit % 2u ? L"a" : L"b", {0u, 0u});
            const auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
            checksum += projection.text.size() + projection.runs.size();
        }
        const auto editEnd = std::chrono::steady_clock::now();
        Require(checksum == expectedChecksum, "whole-source operation trials preserve independently expected display and style sizes");
        const auto final = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
        Require(final.source == document.Text() && final.text.size() == initial.text.size(), "whole-source operation trials preserve exact current source");
        std::cout << "RichProjection whole-initial trial=" << trial << " sourceUnits=" << seed.size()
                  << " elapsedMs=" << std::chrono::duration<double, std::milli>(initialEnd - initialStart).count() << '\n';
        std::cout << "RichProjection whole-edits trial=" << trial << " edits=128 checksum=" << checksum
                  << " elapsedMs=" << std::chrono::duration<double, std::milli>(editEnd - editStart).count() << '\n';
    }
}
