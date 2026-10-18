void TestRichProjectionBlockBranchesAndRetirement()
{
    using namespace DxUi::RichText;
    std::wstring seed;
    for (size_t line = 0u; line < 20u; ++line) seed += L"line **bold**\r\n";
    Document base(seed), left = base, right = base;
    const auto lineLength = std::wstring_view(L"line **bold**\r\n").size();
    left.Replace(4u * lineLength + 1u, 4u * lineLength + 2u, L"A", {0u, 0u});
    right.Replace(15u * lineLength + 1u, 15u * lineLength + 2u, L"B", {0u, 0u});
    Require(left.Revision() == right.Revision(), "sibling documents can have the same external revision with different source identity");
    PreparedProjection prepared;
    prepared.Prepare(left.Snapshot(), left.Revision(), ViewMode::Rich);
    const auto old = prepared.Blocks();
    prepared.Prepare(right.Snapshot(), right.Revision(), ViewMode::Rich);
    CheckPreparedProjection(right, prepared, ViewMode::Rich);
    Require(prepared.LastPreparation().reusedBlocks > 0u && prepared.LastPreparation().rebuiltBlocks > 0u,
            "sibling source branches reuse only unchanged outer context even with equal external revisions");
    Require(prepared.Blocks().front().content == old.front().content && prepared.Blocks().back().content == old.back().content,
            "sibling source changes retain exact immutable outer block identity");
    prepared.Clear();
    const auto rejectsRetired = [&](const auto& operation)
    {
        bool rejected = false;
        try { operation(); }
        catch (const std::logic_error&) { rejected = true; }
        Require(rejected, "retired/default projection rejects content and mapping queries");
    };
    rejectsRetired([&] { static_cast<void>(prepared.Blocks()); });
    rejectsRetired([&] { static_cast<void>(prepared.DisplayText()); });
    rejectsRetired([&] { static_cast<void>(prepared.ToDisplay(0u)); });
    rejectsRetired([&] { static_cast<void>(prepared.ToSource(0u)); });
    Document empty;
    prepared.Prepare(empty.Snapshot(), empty.Revision(), ViewMode::Rich);
    CheckPreparedProjection(empty, prepared, ViewMode::Rich);
    empty.Reset(L"");
    prepared.Prepare(empty.Snapshot(), empty.Revision(), ViewMode::Rich);
    CheckPreparedProjection(empty, prepared, ViewMode::Rich);
    Require(prepared.LastPreparation().rebuiltBlocks == 0u, "two empty sources may remain semantically unchanged across reset identities");
}
