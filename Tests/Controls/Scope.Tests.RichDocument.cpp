#include "Scope.Tests.RichDocument.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichDocument.h>
#include <random>
namespace
{
using namespace DxUi::RichText;
template <typename F> void Reject(F&& operation, const char* description)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    Require(rejected, description);
}
void TestRichDocumentPrototypeContracts()
{
    Document document(L"A\U0001f600\r\n\u03b2");
    const auto saved    = document.Snapshot();
    const auto revision = document.Revision();
    document.SetSelection({1, 3});
    Require(document.ReplaceSelection(L"Unicode"), "replace supplementary scalar");
    Require(saved.Text() == L"A\U0001f600\r\n\u03b2", "immutable snapshot");
    Require(document.Text() == L"AUnicode\r\n\u03b2", "replace retains CRLF and Greek");
    Require(document.Undo() && document.Text() == saved.Text(), "undo text");
    Require(document.GetSelection() == Selection{1, 3}, "undo selection");
    Require(document.Revision() > revision, "revision monotonic on undo");
    Require(document.Redo(), "redo");
    Require(document.GetSelection() == Selection{8, 8}, "redo selection");
    document.Undo();
    document.SetSelection({0, 0});
    document.ReplaceSelection(L"Z");
    Require(! document.CanRedo(), "new edit clears redo");
    const auto before         = document.Text();
    const auto beforeRevision = document.Revision();
    Reject([&] { document.SetSelection({3, 3}); }, "surrogate selection rejected");
    Reject([&] { document.Replace(3, 3, L"X", {0, 0}); }, "surrogate edit rejected");
    Reject([&] { document.Replace(0, 0, std::wstring(1, 0xd800), {0, 0}); }, "invalid UTF-16 insertion rejected");
    Reject([&] { document.Replace(0, 0, L"X", {999, 999}); }, "future selection validated");
    Require(document.Text() == before && document.Revision() == beforeRevision, "rejected edits atomic");
    Reject([&] { document.SetSelection({5, 5}); }, "CRLF split rejected");
    const std::string utf8 = "\xef\xbb\xbf# caf\xc3\xa9\r\n\xf0\x9f\x98\x80";
    Require(EncodeUtf8(DecodeUtf8(utf8)) == utf8, "UTF-8 BOM line endings byte identity");
    Reject([] { DecodeUtf8("\xc0\x80"); }, "overlong UTF-8 rejected");
    Reject([] { DecodeUtf8(std::string(1, '\0')); }, "embedded NUL rejected");
    Document full(std::wstring(Document::MaximumUnits, L'x'));
    Reject([&] { full.ReplaceSelection(L"x"); }, "prototype size limit");
    Require(full.Length() == Document::MaximumUnits, "limit atomic");
    Document random;
    std::wstring oracle;
    std::mt19937 generator(47);
    for (unsigned i = 0; i < 1000; ++i)
    {
        const auto first = generator() % (oracle.size() + 1), last = first + generator() % (oracle.size() - first + 1);
        std::wstring insert(generator() % 5, static_cast<wchar_t>(L'a' + generator() % 26));
        oracle.replace(first, last - first, insert);
        const auto caret = first + insert.size();
        random.Replace(first, last, insert, {caret, caret});
        Require(random.Text() == oracle, "independent randomized string oracle");
    }
    Document bounded;
    for (std::size_t i = 0; i < Document::MaximumHistory + 20; ++i)
        bounded.ReplaceSelection(L"x");
    std::size_t count = 0;
    while (bounded.Undo())
        ++count;
    Require(count == Document::MaximumHistory, "bounded history");
}
void TestRichDocumentSlicesAndJoinedBoundaries()
{
    Document document(L"A\rB\U0001f600C");
    const auto before = document.Snapshot();
    document.Replace(2, 2, L"\n", {3, 3});
    const auto joined = document.Snapshot();
    Require(joined.Text() == L"A\r\nB\U0001f600C", "piece join creates exact CRLF source");
    Require(! joined.IsBoundary(2) && ! joined.IsBoundary(5), "joined CRLF and scalar cannot be split");
    Require(joined.Slice(1, 3) == L"\r\n" && joined.Slice(4, 6) == L"\U0001f600", "slices span immutable pieces");
    Require(joined.Slice(0, 0).empty() && before.Text() == L"A\rB\U0001f600C", "empty range and earlier snapshot");
    Reject([&] { joined.Slice(0, 2); }, "slice rejects CRLF split");
    Reject([&] { joined.Slice(6, 5); }, "slice rejects reversed range");
    Reject([&] { joined.Slice(0, 999); }, "slice rejects out of range");
    Require(! joined.IsBoundary(999), "snapshot boundary out of range is false");
    const auto revision = document.Revision();
    Reject([&] { document.Replace(0, 0, L"", {2, 2}); }, "future no-op selection rejects CRLF split");
    Require(document.Revision() == revision && document.GetSelection() == Selection{3, 3}, "rejection preserves selection/revision");
    document.Undo();
    const auto undone = document.Revision();
    Require(! document.Replace(0, 1, L"A", {1, 1}), "identical replacement is no transaction");
    Require(document.Revision() == undone && document.CanRedo(), "no-op preserves redo and revision");
    document.Redo();
    document.Reset(L"reset");
    Require(document.GetSelection() == Selection{} && ! document.CanUndo() && ! document.CanRedo(), "reset clears history and selection");
    Require(joined.Text() == L"A\r\nB\U0001f600C", "snapshot survives reset and history retirement");
    Document cr(L"A\rB");
    Reject([&] { cr.Replace(2, 2, L"\n", {2, 2}); }, "future selection rejected across a new piece join");
    Require(cr.Text() == L"A\rB" && ! cr.CanUndo(), "joined-boundary failure publishes nothing");
}

void TestRichDocumentCrossPieceUnicodeOracle()
{
    const std::array<std::wstring, 5> tokens{L"a", L"\U0001f600", L"\r\n", L"\u03b2", L"a\u0301"};
    Document document;
    std::wstring oracle;
    std::mt19937 random(619);
    std::vector<SourceSnapshot> snapshots;
    std::vector<std::wstring> expected;
    for (unsigned round = 0; round < 1200; ++round)
    {
        std::vector<std::size_t> offsets;
        for (std::size_t i = 0; i <= oracle.size(); ++i)
            if (IsEditBoundary(oracle, i))
                offsets.push_back(i);
        auto first = offsets[random() % offsets.size()], last = offsets[random() % offsets.size()];
        if (first > last)
            std::swap(first, last);
        const auto& inserted = tokens[random() % tokens.size()];
        if (round % 31 == 0)
        {
            snapshots.push_back(document.Snapshot());
            expected.push_back(oracle);
        }
        oracle.replace(first, last - first, inserted);
        document.Replace(first, last, inserted, {first + inserted.size(), first + inserted.size()});
        Require(document.Text() == oracle, "independent Unicode/CRLF edit oracle across pieces");
        const auto snapshot = document.Snapshot();
        for (std::size_t i = 0; i <= oracle.size(); ++i)
            Require(snapshot.IsBoundary(i) == IsEditBoundary(oracle, i), "piece-aware boundary equals independent text boundary");
    }
    for (std::size_t i = 0; i < snapshots.size(); ++i)
        Require(snapshots[i].Text() == expected[i], "historical snapshots remain immutable across randomized edits");
}
void TestRichDocumentUnicodeCodecRejections()
{
    Require(EncodeUtf8(L"").empty() && DecodeUtf8("").empty(), "empty Unicode codec identity");
    for (const std::string bytes : {"\xc0\x80", "\xe2\x82", "\xed\xa0\x80", "\xf4\x90\x80\x80"})
        Reject([&] { DecodeUtf8(bytes); }, "overlong truncated surrogate and out-of-range UTF-8 rejected");
    for (const auto& text : {std::wstring(1, 0xd800), std::wstring(1, 0xdc00), std::wstring(L"a\0b", 3)})
    {
        Require(! IsValidUtf16(text), "invalid UTF-16 detected without conversion");
        Reject([&] { EncodeUtf8(text); }, "invalid UTF-16 encoding rejected");
    }
    Reject([] { EncodeUtf8(std::wstring(Document::MaximumUnits + 1, L'x')); }, "UTF-16 codec bound enforced");
    Reject([] { DecodeUtf8(std::string(Document::MaximumUnits + 1, 'x')); }, "UTF-8 decoded bound enforced");
    const std::wstring source = L"\ufeff\u03b2\U0001f600\r\nx\ry\n";
    Require(DecodeUtf8(EncodeUtf8(source)) == source, "BOM supplementary scalar and mixed-newline codec identity");
}
} // namespace

void RunRichDocumentTests()
{
    DXUI_RUN_TEST(TestRichDocumentPrototypeContracts);
    DXUI_RUN_TEST(TestRichDocumentSlicesAndJoinedBoundaries);
    DXUI_RUN_TEST(TestRichDocumentCrossPieceUnicodeOracle);
    DXUI_RUN_TEST(TestRichDocumentUnicodeCodecRejections);
}
