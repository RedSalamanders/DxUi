void TestRichProjectionBlockFixedTrials()
{
    using namespace DxUi::RichText;
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line) seed += L"line " + std::to_wstring(line) + L" **bold** and `code`\r\n";
    const auto position = seed.find(L"line 512") + 2u;
    for (size_t trial = 0u; trial < 5u; ++trial)
    {
        Document document(seed);
        PreparedProjection prepared;
        const auto initialStart = std::chrono::steady_clock::now();
        prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
        const auto initialEnd = std::chrono::steady_clock::now();
        const auto initialMs = std::chrono::duration<double, std::milli>(initialEnd - initialStart).count();
        size_t rebuilt = 0u, reused = 0u;
        const auto editStart = std::chrono::steady_clock::now();
        for (size_t edit = 0u; edit < 128u; ++edit)
        {
            document.Replace(position, position + 1u, edit % 2u ? L"a" : L"b", {0u, 0u});
            prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
            rebuilt += prepared.LastPreparation().rebuiltSourceUnits;
            reused += prepared.LastPreparation().reusedBlocks;
        }
        const auto editEnd = std::chrono::steady_clock::now();
        const auto editMs = std::chrono::duration<double, std::milli>(editEnd - editStart).count();
        CheckPreparedProjection(document, prepared, ViewMode::Rich);
        std::cout << "RichProjection initial trial=" << trial << " sourceUnits=" << seed.size() << " blocks=1024 elapsedMs=" << initialMs << '\n';
        std::cout << "RichProjection edits trial=" << trial << " edits=128 rebuiltSourceUnits=" << rebuilt << " reusedBlocks=" << reused << " elapsedMs=" << editMs << '\n';
    }
}
