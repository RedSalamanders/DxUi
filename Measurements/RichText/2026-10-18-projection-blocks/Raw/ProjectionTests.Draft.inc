void CheckPreparedProjection(const DxUi::RichText::Document& document, const DxUi::RichText::PreparedProjection& prepared,
                             DxUi::RichText::ViewMode mode)
{
    using namespace DxUi::RichText;
    const auto full = Project(document.Snapshot(), document.Revision(), mode);
    Require(prepared.Ready() && prepared.Revision() == document.Revision() && prepared.Mode() == mode &&
                prepared.SourceLength() == full.source.size() && prepared.DisplayLength() == full.text.size(),
            "prepared block identity and lengths match the current document");
    Require(prepared.DisplayText() == full.text, "line-block display matches the canonical whole-source projection");
    for (size_t offset = 0u; offset <= full.source.size(); ++offset)
        Require(prepared.ToDisplay(offset) == full.ToDisplay(offset), "every source ACP maps identically across projection blocks");
    for (size_t offset = 0u; offset <= full.text.size(); ++offset)
        for (const bool trailing : {false, true})
            Require(prepared.ToSource(offset, trailing) == full.ToSource(offset, trailing),
                    "every display ACP preserves both source affinities across block boundaries");
    std::wstring source;
    std::vector<StyleRun> runs;
    size_t sourceOffset = 0u, displayOffset = 0u;
    for (const auto& block : prepared.Blocks())
    {
        Require(block.sourceStart == sourceOffset && block.displayStart == displayOffset && block.content && block.content->mode == mode,
                "projection block prefixes cover both coordinate spaces without gaps");
        for (size_t offset = 0u; offset < block.content->units.size(); ++offset)
        {
            const auto unit = block.content->units[offset];
            Require(unit.start + sourceOffset == full.units[displayOffset + offset].start &&
                        unit.end + sourceOffset == full.units[displayOffset + offset].end,
                    "block display units preserve exact original source spans");
        }
        for (auto run : block.content->runs)
        {
            run.start += displayOffset;
            if (!runs.empty() && runs.back().start + runs.back().length == run.start && runs.back().flags == run.flags && runs.back().heading == run.heading)
                runs.back().length += run.length;
            else runs.push_back(run);
        }
        source += block.content->source;
        sourceOffset += block.content->source.size();
        displayOffset += block.content->text.size();
    }
    Require(source == full.source && runs.size() == full.runs.size(), "block source and normalized style count match the complete projection");
    for (size_t index = 0u; index < runs.size(); ++index)
        Require(runs[index].start == full.runs[index].start && runs[index].length == full.runs[index].length &&
                    runs[index].flags == full.runs[index].flags && runs[index].heading == full.runs[index].heading,
                "all block styles and heading levels match the complete projection");
}

void TestRichProjectionBlockFullEquivalence()
{
    using namespace DxUi::RichText;
    for (const auto mode : {ViewMode::Source, ViewMode::Rich})
        for (const auto text : {L"", L"# ", L"one\r\n# ", L"\r\n\n\r", L"# Title\r\n**bold** and *italic*\n`code` ~~strike~~\rfinal",
                               L"## \u05d0\u05d1\u05d2 \U0001f469\u200d\U0001f4bb\r\na\u0301 **\U0001f600**\n",
                               L"**open\nclose**\n# \r\n# Last", L"before\r\nafter\r\n"})
        {
            Document document(text);
            PreparedProjection prepared;
            prepared.Prepare(document.Snapshot(), document.Revision(), mode);
            CheckPreparedProjection(document, prepared, mode);
        }
}

void TestRichProjectionBlockReuseAndLineJoins()
{
    using namespace DxUi::RichText;
    std::wstring seed;
    for (size_t line = 0; line < 20u; ++line) seed += L"line " + std::to_wstring(line) + L" **bold**\r\n";
    Document document(seed);
    PreparedProjection prepared;
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    const auto old = prepared.Blocks();
    const auto position = old[10].sourceStart + 2u;
    document.Replace(position, position + 1u, L"X", {0u, 0u});
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
    const auto stats = prepared.LastPreparation();
    Require(stats.rebuiltBlocks == 3u && stats.reusedBlocks == 17u && stats.rebuiltSourceUnits < document.Length(),
            "single-line edit rebuilds its line and two neighbors rather than the complete source");
    for (size_t index = 0; index < old.size(); ++index)
        Require((prepared.Blocks()[index].content == old[index].content) == (index < 9u || index > 11u),
                "unchanged line blocks retain their exact immutable projection identity");
    document.Replace(position, position, L"\n# inserted\r\n", {0u, 0u});
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
    document.Undo();
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
    document.Redo();
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
    const auto join = document.Text().find(L"\r\n");
    document.Replace(join, join + 2u, L"", {0u, 0u});
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
}

void TestRichProjectionBlockUnicodeEditOracle()
{
    using namespace DxUi::RichText;
    for (const auto mode : {ViewMode::Source, ViewMode::Rich})
    {
        Document document(L"# first\r\n**a\u0301**\n\u05d0\u05d1\u05d2 \U0001f600\r\nlast\n");
        PreparedProjection prepared;
        prepared.Prepare(document.Snapshot(), document.Revision(), mode);
        std::uint32_t random = 0x632be59bu;
        const auto next = [&] { random = random * 1664525u + 1013904223u; return random; };
        const std::wstring_view tokens[] = {L"", L"x", L"\r", L"\n", L"\r\n", L"**b**", L"# H\n", L"a\u0301", L"\U0001f469\u200d\U0001f4bb", L"\u05d0\u05d1"};
        for (size_t trial = 0u; trial < 512u; ++trial)
        {
            const auto oldText = document.Text();
            std::vector<size_t> boundaries;
            for (size_t offset = 0u; offset <= oldText.size(); ++offset)
                if (IsEditBoundary(oldText, offset)) boundaries.push_back(offset);
            auto first = boundaries[next() % boundaries.size()], last = boundaries[next() % boundaries.size()];
            if (first > last) std::swap(first, last);
            const auto insertion = tokens[next() % std::size(tokens)];
            document.Replace(first, last, insertion, {0u, 0u});
            Require(document.Text() == oldText.substr(0u, first) + std::wstring(insertion) + oldText.substr(last), "flat edit oracle matches source transaction");
            prepared.Prepare(document.Snapshot(), document.Revision(), mode);
            CheckPreparedProjection(document, prepared, mode);
            if (trial % 31u == 0u && document.CanUndo())
            {
                document.Undo();
                prepared.Prepare(document.Snapshot(), document.Revision(), mode);
                CheckPreparedProjection(document, prepared, mode);
                document.Redo();
                prepared.Prepare(document.Snapshot(), document.Revision(), mode);
                CheckPreparedProjection(document, prepared, mode);
            }
        }
    }
}

void TestRichProjectionBlockFallbackModesAndLifetime()
{
    using namespace DxUi::RichText;
    Document document(L"# one\r\n**two**\nthree\n");
    PreparedProjection prepared;
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    const auto held = prepared.Blocks();
    prepared.Prepare(document.Snapshot(), document.Revision() + 1u, ViewMode::Rich);
    Require(prepared.LastPreparation().rebuiltBlocks == 0u && prepared.LastPreparation().reusedBlocks == held.size(),
            "unchanged source acknowledges a new external revision without rebuilding blocks");
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Source);
    CheckPreparedProjection(document, prepared, ViewMode::Source);
    Require(prepared.LastPreparation().reusedBlocks == 0u, "view mode changes rebuild all projections");
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    for (size_t edit = 0; edit < 160u; ++edit)
        document.Replace(0u, 0u, L"x", {0u, 0u});
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
    Require(prepared.LastPreparation().reusedBlocks == 0u, "expired source ancestry safely rebuilds all blocks");
    document.Reset(L"# reset\nnew\r\n");
    prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, prepared, ViewMode::Rich);
    Require(prepared.LastPreparation().reusedBlocks == 0u, "unrelated reset does not reuse coincidentally positioned blocks");
    Require(held.front().content->source == L"# one\r\n" && held.front().content->text == L"one\n",
            "externally held immutable blocks survive source reset and preparation replacement");
    PreparedProjection moved(std::move(prepared));
    Require(!prepared.Ready() && prepared.SourceLength() == 0u && prepared.DisplayLength() == 0u,
            "moved projection service is safely unprepared");
    CheckPreparedProjection(document, moved, ViewMode::Rich);
    moved.Clear();
    Require(!moved.Ready() && moved.Revision() == 0u && moved.LastPreparation().reusedBlocks == 0u,
            "retirement releases the complete prepared state and resets its public identity");
    moved.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
    CheckPreparedProjection(document, moved, ViewMode::Rich);
    bool rejected = false;
    try { moved.Prepare(document.Snapshot(), 999u, static_cast<ViewMode>(99)); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && moved.Revision() == document.Revision(), "invalid preparation cannot replace an existing published revision");
    CheckPreparedProjection(document, moved, ViewMode::Rich);
    rejected = false;
    try { static_cast<void>(moved.ToDisplay(document.Length() + 1u)); }
    catch (const std::out_of_range&) { rejected = true; }
    Require(rejected, "invalid source ACP is rejected");
    rejected = false;
    try { static_cast<void>(moved.ToSource(moved.DisplayLength() + 1u)); }
    catch (const std::out_of_range&) { rejected = true; }
    Require(rejected, "invalid display ACP is rejected");
}

void TestRichProjectionBlockMaximumSource()
{
    using namespace DxUi::RichText;
    for (const auto character : {L'x', L'\n'})
    {
        Document document(std::wstring(Document::MaximumUnits, character));
        PreparedProjection prepared;
        prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
        CheckPreparedProjection(document, prepared, ViewMode::Rich);
        Require(prepared.Blocks().size() == (character == L'\n' ? Document::MaximumUnits : 1u),
                "maximum source and maximum physical-line count are explicitly bounded");
        document.Replace(Document::MaximumUnits / 2u, Document::MaximumUnits / 2u + 1u, L"z", {0u, 0u});
        prepared.Prepare(document.Snapshot(), document.Revision(), ViewMode::Rich);
        CheckPreparedProjection(document, prepared, ViewMode::Rich);
    }
}
