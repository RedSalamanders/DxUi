# W9.3 text history by bytes

Plan item `W9.3` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Contract: [single-line text history](../../../Specs/Core/Core_PerformanceAndResources.md#single-line-text-history).

## Scenario

The receipt-only `history` variants of FX7 (a 65,472-unit `TextField`, after the fixture's IME composition phases) and
FX2 (the 500-item editable `ComboBox` with 65,536 units of text, after the fixture's popup and typing phases) end with a
`history` phase of 128 edits (`Tests/Embedded/Embedded.Tests.TextHistoryEdits.h`): each selects its own place and makes
one edit through the host's key and character input (a 1,000-unit selection replaced by one character, Backspace,
Delete, a typed character, Ctrl+Backspace, Ctrl+Delete, a 64-unit selection replaced by 64 units, a surrogate pair), and
the last replaces the whole text. Every edit is its own undo step, so the 64-step count cap holds the history. The
counter is G13's `text-history` line, `EmbeddedStatistics::largestTextHistoryBytes` after the last phase.

Receipts written by `performance-fixtures.ps1 -Fixture FX7,FX2 -Variant history -FixtureHost Embedded` on WARP
10.0.26100.9711, Windows build 26300, MSVC 19.52.37025, 2026-11-15:

- Before, x64 Debug, the plan's base commit `714e1f77e` (full-text snapshot history) with this item's tests and fixture
  variants added uncommitted: [FX7](before-fx7-history-embedded-warp-x64-debug.json),
  [FX2](before-fx2-history-embedded-warp-x64-debug.json).
- After, x64 Debug and Release, source `7b6c96dbd` (this item rebased on integration tip `9574105be`, before the
  receipts were added; its code is the commit that adds this file): FX7 [Debug](after-fx7-history-embedded-warp-x64-debug.json),
  [Release](after-fx7-history-embedded-warp-x64-release.json); FX2 [Debug](after-fx2-history-embedded-warp-x64-debug.json),
  [Release](after-fx2-history-embedded-warp-x64-release.json).

## Result

| Fixture | Before (Debug) | After (Debug) | After (Release) | Budget |
| --- | --- | --- | --- | --- |
| FX7 `TextField` | 7,234,416 B, exceeded | 132,080 B, met | 131,696 B, met | 4,194,304 B |
| FX2 editable `ComboBox` | 7,143,776 B, exceeded | 130,336 B, met | 129,952 B, met | 4,194,304 B |

Before, each of the 64 kept steps held its own copy of the 64 Ki-unit text (about 136 KiB with the growth room each
edit's copy reserves). After, the sealed steps hold only the units their edits replaced; most of the remainder is the
newest step's open copy of the text before it (131,136 B), which the next history operation seals. The gate test
(`TestFx7AndFx2TextHistoryGate`) also reads the counter after every one of the 64 undos and 64 redos: the peak is
132,720 B (FX7) and 130,944 B (FX2) in Debug, 132,328 B and 130,552 B in Release, and every undo and redo restores the
expected text, caret and selection.

Entry costs on the history itself (`TestTextHistoryEntryCosts`, 64 Ki-unit text, x64 Debug): a sealed typed character or
paste keeps no text beyond its slot (72 bytes for a `TextField` or `ComboBox` entry in x64); the redo entry of an undone
64 Ki-unit paste holds the pasted units (131,152 B); a replace-all holds the replaced text once. Budget
(`TestTextHistoryBudgetEvictsOldestFirst`): 48 whole-text replacements of a 64 Ki-unit text keep the newest 31
(3,938,112 B of spans and storage, which with the open step's 131,136-byte text fits 4 MiB) and drop the oldest; an
open text larger than 4 MiB stays alone as the newest step.
