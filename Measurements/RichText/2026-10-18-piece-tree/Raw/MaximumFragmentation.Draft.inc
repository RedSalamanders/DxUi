void TestRichDocumentMaximumFragmentationAndRepeatedRootCollapse()
{
    Document document(std::wstring(32768u, L'a'));
    std::wstring expected;
    for (size_t index = 0u; index < 32768u; ++index)
    {
        const auto position = index * 2u + 1u;
        document.Replace(position, position, L"X", {0u, 0u});
        expected += L"aX";
    }
    const auto maximum = document.Snapshot();
    Require(document.Length() == Document::MaximumUnits && document.Text() == expected,
            "maximum fragmented source matches an independent alternating flat oracle");
    for (size_t position = 0u; position <= expected.size(); ++position)
        Require(maximum.IsBoundary(position), "every ASCII boundary is reachable through the maximum-depth tree");
    for (size_t probe = 0u; probe < 512u; ++probe)
    {
        const auto first = probe * 127u;
        Require(maximum.Slice(first, first + 128u) == expected.substr(first, 128u), "cross-node slices retain exact maximum-size source");
    }
    Reject([&] { document.Replace(1u, 1u, L"!", {0u, 0u}); }, "full fragmented source still rejects capacity overflow atomically");
    Require(document.Text() == expected, "capacity rejection preserves the maximum tree");
    document.Replace(1u, document.Length() - 1u, L"", {0u, 0u});
    Require(document.Text() == L"aX", "a multi-level middle deletion keeps only its two boundary pieces");
    for (size_t cycle = 0u; cycle < 128u; ++cycle)
    {
        for (size_t index = 0u; index < 256u; ++index)
            document.Replace(1u, 1u, L"y", {0u, 0u});
        Require(document.Text() == L"a" + std::wstring(256u, L'y') + L"X", "repeated prepend growth preserves the independent source");
        document.Replace(1u, document.Length() - 1u, L"", {0u, 0u});
        Require(document.Text() == L"aX", "repeated root collapse preserves both surviving ends");
    }
    Require(maximum.Text() == expected, "a maximum snapshot survives deep cuts and subsequent history retirement");
}
