#include "Scope.Tests.RichSourceChanges.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichDocument.h>
#include <atomic>
#include <thread>

namespace
{
using namespace DxUi::RichText;
void CheckReconstruction(const SourceSnapshot& previous, const SourceSnapshot& current, const SourceChangeRange& change)
{
    Require(change.begin <= change.previousEnd && change.previousEnd <= previous.Length() && change.begin <= change.currentEnd &&
                change.currentEnd <= current.Length(),
            "changed interval is bounded in both source coordinate spaces");
    Require(previous.IsBoundary(change.begin) && previous.IsBoundary(change.previousEnd) && current.IsBoundary(change.begin) &&
                current.IsBoundary(change.currentEnd),
            "changed interval preserves scalar and CRLF edges in both snapshots");
    if (change.kind == SourceChangeKind::Unchanged)
        Require(previous.Text() == current.Text(), "unchanged source identity carries identical content");
    Require(previous.Slice(0u, change.begin) == current.Slice(0u, change.begin), "changed interval retains its exact independent prefix");
    Require(previous.Slice(change.previousEnd, previous.Length()) == current.Slice(change.currentEnd, current.Length()),
            "changed interval retains its exact independent suffix");
    Require(previous.Slice(0u, change.begin) + current.Slice(change.begin, change.currentEnd) + previous.Slice(change.previousEnd, previous.Length()) ==
                current.Text(),
            "replacing the reported source interval reconstructs the current snapshot exactly");
}
void CheckTracked(const SourceSnapshot& previous, const SourceSnapshot& current, size_t begin, size_t previousEnd, size_t currentEnd)
{
    const auto change = current.ChangedRangeFrom(previous);
    CheckReconstruction(previous, current, change);
    Require(change.kind == SourceChangeKind::TrackedEdits, "recent source edits retain bounded change metadata");
    Require(change.begin == begin && change.previousEnd == previousEnd && change.currentEnd == currentEnd,
            "tracked source interval matches independently expected coordinates");
}
void TestRichSourceChangeSingleTransactions()
{
    Document document(L"abcdef");
    const auto previous = document.Snapshot();
    document.Replace(2u, 4u, L"XYZ", {5u, 5u});
    const auto current = document.Snapshot();
    CheckTracked(previous, current, 2u, 4u, 5u);
    const auto revision = document.Revision();
    Require(! document.Replace(2u, 5u, L"XYZ", {5u, 2u}), "same-content edit only acknowledges selection");
    const auto unchanged = document.Snapshot().ChangedRangeFrom(current);
    Require(unchanged.kind == SourceChangeKind::Unchanged && document.Revision() == revision && document.GetSelection() == Selection{5u, 2u},
            "no-op edits create neither a source change nor a revision");
    Document scalar(L"A\U0001f600BC");
    const auto oldScalar = scalar.Snapshot();
    scalar.Replace(1u, 3u, L"\U0001f44d", {3u, 3u});
    CheckTracked(oldScalar, scalar.Snapshot(), 1u, 3u, 3u);
    Document combining(L"a\u0301b");
    const auto oldCombining = combining.Snapshot();
    combining.Replace(1u, 2u, L"\u0308", {2u, 2u});
    CheckTracked(oldCombining, combining.Snapshot(), 1u, 2u, 2u);
}
void TestRichSourceChangeJoinedCrlfBounds()
{
    Document cut(L"A\rx\nB");
    const auto beforeCut = cut.Snapshot();
    cut.Replace(2u, 3u, L"", {3u, 3u});
    CheckTracked(beforeCut, cut.Snapshot(), 1u, 4u, 3u);
    Document appendLf(L"A\rB");
    const auto beforeLf = appendLf.Snapshot();
    appendLf.Replace(2u, 2u, L"\n", {3u, 3u});
    CheckTracked(beforeLf, appendLf.Snapshot(), 1u, 2u, 3u);
    Document prependCr(L"A\nB");
    const auto beforeCr = prependCr.Snapshot();
    prependCr.Replace(1u, 1u, L"\r", {3u, 3u});
    CheckTracked(beforeCr, prependCr.Snapshot(), 1u, 2u, 3u);
}
void TestRichSourceChangeUndoRedoAndBranches()
{
    Document document(L"abcd");
    const auto base = document.Snapshot();
    document.Replace(1u, 1u, L"X", {2u, 2u});
    const auto firstBranch = document.Snapshot();
    document.Undo();
    CheckTracked(firstBranch, document.Snapshot(), 1u, 2u, 1u);
    document.Redo();
    CheckTracked(base, document.Snapshot(), 1u, 1u, 2u);
    document.Undo();
    document.Replace(3u, 4u, L"Y", {4u, 4u});
    CheckTracked(firstBranch, document.Snapshot(), 1u, 5u, 4u);
    Document fork     = document;
    const auto common = document.Snapshot();
    document.Replace(0u, 1u, L"q", {0u, 0u});
    fork.Replace(2u, 3u, L"r", {0u, 0u});
    const auto acrossCopies = fork.Snapshot().ChangedRangeFrom(document.Snapshot());
    Require(acrossCopies.kind == SourceChangeKind::TrackedEdits, "copied documents retain distinct branches sharing a bounded ancestor");
    CheckReconstruction(document.Snapshot(), fork.Snapshot(), acrossCopies);
    CheckReconstruction(common, fork.Snapshot(), fork.Snapshot().ChangedRangeFrom(common));
}
void TestRichSourceChangeTransientPagesAndExpiry()
{
    Document document(L"seed");
    std::vector<SourceSnapshot> snapshots{document.Snapshot()};
    std::wstring expected = L"seed";
    for (size_t index = 0u; index < 200u; ++index)
    {
        expected += L'x';
        document.ReplaceTransient(expected, {0u, 0u});
        snapshots.push_back(document.Snapshot());
        Require(! document.CanUndo() && ! document.CanRedo(), "transient change metadata creates no undo or redo entries");
    }
    const auto current = document.Snapshot();
    for (size_t distance = 1u; distance <= 96u; ++distance)
    {
        const auto& previous = snapshots[snapshots.size() - 1u - distance];
        const auto change    = current.ChangedRangeFrom(previous);
        Require(change.kind == SourceChangeKind::TrackedEdits, "every recent 96-edit transient burst remains tracked across page rollover");
        CheckReconstruction(previous, current, change);
    }
    const auto expired = current.ChangedRangeFrom(snapshots.front());
    Require(expired.kind == SourceChangeKind::FullReplacement, "history outside the bounded metadata window falls back to complete replacement");
    CheckReconstruction(snapshots.front(), current, expired);
    document.Reset(expected);
    const auto reset = document.Snapshot().ChangedRangeFrom(current);
    Require(reset.kind == SourceChangeKind::FullReplacement, "reset starts unrelated metadata even when source bytes are identical");
    CheckReconstruction(current, document.Snapshot(), reset);
}
void TestRichSourceChangeIndependentUnicodeOracle()
{
    Document document(L"A\r\n\U0001f600z");
    std::wstring expected = document.Text();
    std::vector<std::pair<SourceSnapshot, std::wstring>> recent{{document.Snapshot(), expected}};
    std::uint32_t random = 0x712a03u;
    const auto next      = [&]()
    {
        random = random * 1664525u + 1013904223u;
        return random;
    };
    const auto boundary = [](std::wstring_view text, size_t offset)
    {
        if (! offset || offset == text.size())
            return true;
        const unsigned left = text[offset - 1u], right = text[offset];
        return ! (left >= 0xd800u && left <= 0xdbffu && right >= 0xdc00u && right <= 0xdfffu) && ! (left == '\r' && right == '\n');
    };
    for (size_t operation = 0u; operation < 1024u; ++operation)
    {
        size_t first = next() % (expected.size() + 1u), last = next() % (expected.size() + 1u);
        if (first > last)
            std::swap(first, last);
        while (! boundary(expected, first))
            --first;
        while (! boundary(expected, last))
            ++last;
        const std::wstring replacement = operation % 3u == 0u ? L"\r\n\U0001f600" : L"\u0915\u094d\u0915";
        expected.replace(first, last - first, replacement);
        document.Replace(first, last, replacement, {0u, 0u});
        const auto current = document.Snapshot();
        Require(current.Text() == expected, "source changes agree with the independent flat Unicode edit model");
        for (const auto& [previous, original] : recent)
        {
            const auto change = current.ChangedRangeFrom(previous);
            Require(change.kind != SourceChangeKind::FullReplacement, "short random edit windows remain tracked");
            CheckReconstruction(previous, current, change);
            Require(previous.Text() == original, "change queries never mutate an older Unicode source");
        }
        recent.emplace_back(current, expected);
        if (recent.size() > 8u)
            recent.erase(recent.begin());
    }
}
void TestRichSourceChangeConcurrentEmptyAndMovedValues()
{
    SourceSnapshot empty;
    Document document(L"a");
    const auto previous = document.Snapshot();
    for (size_t index = 0u; index < 64u; ++index)
        document.Replace(0u, 0u, L"x", {0u, 0u});
    const auto current = document.Snapshot();
    std::atomic<bool> valid{true};
    std::thread reader([&]
    {
        for (size_t query = 0u; query < 1024u; ++query)
        {
            const auto change = current.ChangedRangeFrom(previous);
            if (change.kind != SourceChangeKind::TrackedEdits || change.begin != 0u || change.previousEnd != 0u || change.currentEnd != 64u)
                valid.store(false, std::memory_order_relaxed);
        }
    });
    for (size_t index = 0u; index < 128u; ++index)
    {
        document.Reset(L"new");
        document.Replace(0u, 0u, L"q", {0u, 0u});
        document.Undo();
        document.Redo();
    }
    reader.join();
    Require(valid.load(std::memory_order_relaxed), "immutable change queries remain valid while the owner replaces source and histories");
    auto old         = current;
    const auto moved = std::move(old);
    CheckReconstruction(old, moved, moved.ChangedRangeFrom(old));
    CheckReconstruction(moved, old, old.ChangedRangeFrom(moved));
    Require(empty.ChangedRangeFrom(old).kind == SourceChangeKind::Unchanged, "default and moved-empty snapshots share empty content");
}
void TestRichSourceChangeLocalizedTransientPreviews()
{
    Document document(L"abcdef");
    const auto original = document.Snapshot();
    document.ReplaceTransient(L"abXYZef", {5u, 5u});
    CheckTracked(original, document.Snapshot(), 2u, 4u, 5u);
    const auto current  = document.Snapshot();
    const auto revision = document.Revision();
    Require(! document.ReplaceTransient(L"abXYZef", {5u, 2u}) && document.Revision() == revision &&
                document.Snapshot().ChangedRangeFrom(current).kind == SourceChangeKind::Unchanged,
            "same-content preview changes neither revision nor metadata");
    Document crlf(L"A\rx\nB");
    const auto beforeCrlf = crlf.Snapshot();
    crlf.ReplaceTransient(L"A\r\nB", {3u, 3u});
    CheckTracked(beforeCrlf, crlf.Snapshot(), 1u, 4u, 3u);
    for (const auto replacement : {L"A\U0001f603B", L"A\U00020200B"})
    {
        Document scalar(L"A\U0001f600B");
        const auto before = scalar.Snapshot();
        scalar.ReplaceTransient(replacement, {3u, 3u});
        CheckTracked(before, scalar.Snapshot(), 1u, 3u, 3u);
    }
    document.ReplaceTransient(L"", {0u, 0u});
    Require(document.Text().empty(), "localized preview can remove every source unit");
    const auto empty = document.Snapshot();
    document.ReplaceTransient(L"\U0001f600\r\n", {4u, 4u});
    CheckTracked(empty, document.Snapshot(), 0u, 0u, 4u);
    Require(! document.CanUndo() && ! document.CanRedo(), "localized previews never create ordinary history");
}

} // namespace
void RunRichSourceChangeTests()
{
    DXUI_RUN_TEST(TestRichSourceChangeSingleTransactions);
    DXUI_RUN_TEST(TestRichSourceChangeLocalizedTransientPreviews);
    DXUI_RUN_TEST(TestRichSourceChangeJoinedCrlfBounds);
    DXUI_RUN_TEST(TestRichSourceChangeUndoRedoAndBranches);
    DXUI_RUN_TEST(TestRichSourceChangeTransientPagesAndExpiry);
    DXUI_RUN_TEST(TestRichSourceChangeIndependentUnicodeOracle);
    DXUI_RUN_TEST(TestRichSourceChangeConcurrentEmptyAndMovedValues);
}
