# Code review of the 15-day window (2026-10-16 to 2026-10-31)

Status: SPEC READY. Review only: no library, test or tooling code changed. Remediation follows the
[sequence](#sequence). The overlapping DX-* repair text stays in
[Delivery review 2026-10-31](DeliveryReview_2026-10-31.md); this plan is the full delivery sequencer.
Pin prerequisites are the Wave 0 decisions and Wave 1-2 repairs. DX-1 and the lower-priority DX items
remain Wave 5 follow-ups and do not expand that gate. DX-1 does not block the bump. Qualification and
exact-pin consumer evidence remain separately required under Wave 6.
Date: 2026-10-31.
Window: parent `9945d7fc` (2026-10-13) through `cb94aed5` / delivery tip `f2940cc2` / current `origin/main`
`93c77750` (merge of #156). `include/` and `src/` are byte-identical among those tips; later `main` commits are
CI evidence, changelog folds and this review. Line numbers below are that shared source.
Related: [the 28 October review](CodeReview_2026-10-28.md) (R28-* IDs), [the 10 October review](CodeReview_2026-10-10.md)
(CR-* IDs), [the RichText audit](RichTextAudit_2026-10-21.md) and
[its findings register](RichTextAudit_2026-10-21/findings.md). This plan does not re-list every earlier finding.
It records what a second, independent red-team confirmed, refuted, or added, and the work required before a
consumer pin of current `main` is honest.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md),
[input and accessibility](../../UI/UI_InputAndAccessibility.md),
[Win32 host](../../Rendering/Rendering_Win32Host.md),
[embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[performance](../../Core/Core_PerformanceAndResources.md),
[validation](../../Testing/Testing_Validation.md) and
[toolchain](../../Build/Build_ToolchainAndConsumption.md).

`apiRevision` stays 4. Do not publish an API revision 5 for this work.

## Verdict

**Not production-ready for a consumer pin.** The 15-day window shipped a real library (Unicode rich editor, menu
timers, TSF, Grid/Tree, embedded host) and then reviewed it. No library repair from those reviews has landed.
Current `src/` and `include/` still contain reachable document-integrity, memory-safety, menu-cascade, IME-loss
and common-path UI defects.

The 31 October delivery plan scheduled four Wave-1 repairs and left the fourteen confirmed P1s from the 28/30
October review untouched. Pinning `f2940cc2` or current `main` is not a complete menu fix and is not a complete
editor fix.

Nothing in this review was compiled or executed. Every retained finding is a static trace against current bytes.

## How the review was made

- **Pass 1.** Official Security Review and Bugbot against `src/` and `include/`, plus an independent re-read of
  every stop-ship site named by [DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md) and
  [CodeReview_2026-10-28](CodeReview_2026-10-28.md).
- **Pass 2.** A second angle on delivery scope, consumer locks, test honesty, claimed-fix status and qualification
  HOLDs. This pass asked whether a pin of this tip would actually work, not only whether a line looked wrong.
- Prior review documents were treated as untrusted hints and re-verified. `commands-01` (DeleteTableColumn OOB)
  is **fixed** in current source (`RichTextCommands.cpp` 2096-2099). Posted-payload ICF identity is typed tags,
  not deleter addresses.

## Confirmed stop-ship and P1 findings

| ID | Location | Finding | Confidence |
| --- | --- | --- | --- |
| DX-3 | [RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) 427-447 | CF_HDROP quoted fallback writes raw paths into live Markdown. Native paste 2463; embedded 1188. | High |
| R28-44 | [Grid.cpp](../../../src/Controls/DxUi.Grid.cpp) 1172-1180 | `SetModel` returns on `bad_alloc` before `_model = model`. The caller cannot see the failure and destroys the old model. | High |
| R28-41 | [WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) 4261-4269 | Device-loss HRESULT on `ResizeBuffers` only discards size-dependent resources. Present recovery is 4588-4598. | High |
| DX-4 / R28-05 | [TextStoreTarget.h](../../../src/Controls/TextStoreTarget.h) 36-38 | `NativeTextStoreTarget` does not override capacity. Import catches `logic_error` and returns false ([RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) 1843-1851). | High |
| DX-2 | [Menu.cpp](../../../src/Controls/DxUi.Menu.cpp) 3512-3526 | UIA `OnFocusChanged` writes `keyboardIndex` and does not cancel. `HandleSubmenuHoverTimer` 4446-4460 still opens `hoverTimerItemIndex`. | High |
| R28-45 | [Menu.cpp](../../../src/Controls/DxUi.Menu.cpp) 5454-5457 | Every typed or inert key, including unmatched `WM_CHAR`, cancels pointer cascade timers. | High |
| R28-01 | Grid.cpp / Tree.cpp | Neither overrides `OnBoundsChanged`. Grow-window leaves scrolled rows unreachable. | High |
| R28-02 | [Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) 2468-2473 | Any pending `PrepareContent` cancels an unrelated mouse capture. | High |
| R28-03 | [RichTextLayout.cpp](../../../src/Controls/DxUi.RichTextLayout.cpp) 321 | `lineHeightMultiplier` is skipped whenever `textOffset != 0`. | High |
| R28-04 | [RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp) 820 | RTL Subtle tables subtract `left` from `{left, width}`. | High |
| Enter EOL | [RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) 2349 | Source / Shift+Enter / refused paragraph always insert CRLF. Paste already picks the document ending at 381-387. | High |
| R28-06 | [TextStoreACP.cpp](../../../src/Controls/DxUi.TextStoreACP.cpp) 1335-1344 | `GetStatus` never reports read-only or multiline. | High |
| R28-40 | [WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) 3720-3723 | A focused read-only editor swallows printable application shortcuts. | High |
| R28-07 / R28-08 | [NativeMath.cpp](../../../src/Controls/DxUi.NativeMath.cpp) 214, 491-493 | `\\` is a literal backslash; `\|` draws a single bar. | High |
| R28-09 | [NativeFlowchart.cpp](../../../src/Controls/DxUi.NativeFlowchart.cpp) 177, 399-416 | `:::`, `accTitle` and `accDescr` become diagram content. | High |
| R28-12 | [Tree.cpp](../../../src/Controls/DxUi.Tree.cpp) 842-871; [Grid.cpp](../../../src/Controls/DxUi.Grid.cpp) 1434 | Selection and busy setters skip invalidate. | High |

After-assign `Grid::SetModel` OOM (1230-1233) is a stale-selection / empty-column hole, not a second C++ dangling
pointer. The UAF is the early-return path.

## Architecture problems

- **DeliveryReview under-scopes CodeReview.** Wave 1 of the 31 October plan (UIA timer, Enter, TSF capacity, HDROP
  quotes) leaves Grid/Tree scroll, capture cancel, RTL tables, TSF read-only, math/Mermaid, shortcuts, device loss
  and `SetModel` UAF in the tree that would be pinned.
- **Menu cancel is the wrong shape.** Keyboard cancels every inert `WM_CHAR` (R28-45). UIA cancels nothing (DX-2).
  Copying `RouteMenuKeyboardEvent` into `OnFocusChanged` would over-cancel. Need `TakeKeyboardOwnership`, UIA cancel,
  and refuse-stale-timer together.
- **Embedded re-entry vs header (Q1).** [Embedded.h](../../../include/DxUi/Embedded.h) forbids `requestPreparation`
  re-entry. Retirement tests treat it as safe. Setters that call `GetHost` after `Invalidate` are use-after-free if
  the tests are the contract.
- **Allocation-failure policy split (Q2).** `OnKeyDown` terminates on `bad_alloc`. Bare `DispatchCharacter` returns
  false. `OnChar` is `noexcept` and can swallow after publish. The input contract only mandates terminate inside
  observer callbacks.
- **`ControlKind` ordinals shifted, `apiRevision` stayed 4.** `Separator` was inserted mid-enum in
  [ControlCatalog.h](../../../include/DxUi/ControlCatalog.h). No in-tree consumer stores raw ordinals. Still a
  silent ABI hazard if any out-of-tree numeric `CreateControl` exists.
- **Qualification HOLDs vs “close to delivery”.** Paired performance is unqualified, the repaired A/A has 72/72
  unstable controls, ARM64 overlapping-popup root-switch is still unattributed, and physical-input / consumer gates
  are open. A green x64 suite is not a delivery proof.

## Second-angle findings

- A pin of current `main` is not a menu fix. Keyboard cancel `762983a0` is on `origin/main` only. UIA cancel is
  absent everywhere. RedPrism `a059e07c` (API 3) and RedXe `616104f3` have neither, and they do not merge `main`.
  RedSalamander `4cdf1ce5` and RedTilde `6ed0512c` are ancestors of `main` and still miss both cancels.
- Tests hide the bugs. Menu UIA-focus cases never deliver `WM_TIMER`. Stationary-hover accepts an already-open
  child, then synthesizes the timer. Rich `ScrollIntoView` tests call `Publish()` after the scroll. The
  zero-warning gate can pass an empty or uncompiled log (R28-51).
- Claimed fixes that do not hold: math-parser-12, MM-33/34, CR-16 (`E_ABORT` never retries), CR-25 (paint-time
  layout still present).
- The rich editor is in the RedTilde pin with a 241-finding audit that is SPEC READY and unimplemented except the
  few command-planner items already fixed.

## Refuted or downgraded

| Claim | Verdict |
| --- | --- |
| Posted-payload type identity broken by `/OPT:ICF` | Rejected. `PostedPayloadTypeTag<T>`, not deleter address. |
| `TryPrepareClipboardPaste` opens files | Rejected. Builds source only. |
| `commands-01` DeleteTableColumn OOB | Fixed. `column > 0` guard at [RichTextCommands.cpp](../../../src/Controls/DxUi.RichTextCommands.cpp) 2096. |
| DxUi loads arbitrary DLLs | Rejected. No `LoadLibrary` in `src/`. |
| Grid `SetModel` after-assign OOM is a C++ UAF | Downgraded to stale selection + empty columns. |
| Math/Mermaid misparse is remote code execution | Rejected. Source-preserving fallback. |

## Consumer locks

A fix that exists only on DxUi `main` does not ship. Locks are `Dependencies/DxUi.lock.json` in each product.
Details and port targets stay in [DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md#consumer-locks).

| Product | Lock | API | Action |
| --- | --- | --- | --- |
| RedPrism | `a059e07c` | 3 | Port both menu cancels onto `origin/ej/wall-compat-redprism-20261028`. Re-pin that branch, not `main`. |
| RedXe | `616104f3` | 4 | Same port onto `origin/ej/wall-compat-redxe-20261028`. |
| RedSalamander | `4cdf1ce5` | 4 | Pin the Wave 1+2 `main` commit. |
| RedTilde | `6ed0512c` | 4 | Same. Ships the rich editor, so Wave 2 is required before the bump. |

## Sequence

### Wave 0 — decisions that unblock remediations

Answer these before implementation starts. Recommended answers:

1. **Q1** (R28-14). `requestPreparation` must not retire the tree. Header wins. Re-entrancy guard + debug assert.
2. **Q2** (R28-15, DX-10). Decline `bad_alloc` atomically at input. Terminate only in observer callbacks.
3. **Q8** (R28-07). Reject `\\` outside rows to source fallback.
4. **Q9** (R28-09). Reject `:::`, `accTitle` and `accDescr` as `UnsupportedStatement`.
5. **Q10** (R28-13). `NotifyDataChanged` clamps only. Typeahead keeps a matching current prefix. No silent row-0 select.
6. **Q14** (DX-11). Keep `apiRevision` 4. Pin historical `ControlKind` ordinals only if asserts are added.

### Wave 1 — stop-ship on `main` and both wall-compat branches

Land as one effort. Port the menu half onto the two wall-compat branches in the same effort; those branches do
not merge `main`. Repair text for DX-2, DX-3 and DX-4 stays in
[DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md).

- [ ] **DX-3** quoted file-paste fallback.
- [ ] **DX-4 / R28-05** native TSF capacity.
- [ ] **R28-44** `Grid::SetModel` install-first.
- [ ] **R28-41** device-loss HRESULT on resize/create.
- [ ] **DX-2 + R28-45** `TakeKeyboardOwnership` on real keyboard/UIA ownership only; `OnFocusChanged` cancels the
      chain; `HandleSubmenuHoverTimer` refuses open/close against a `keyboardIndex`. Do not cancel on unmatched
      `WM_CHAR`.
- [ ] Port both menu cancels onto `origin/ej/wall-compat-redprism-20261028` and
      `origin/ej/wall-compat-redxe-20261028`.

### Wave 2 — user-visible P1s before RedTilde / RedSalamander pin

- [ ] Source / Shift+Enter / refused paragraph use the paste ending helper. Update
      [UI_InputAndAccessibility.md](../../UI/UI_InputAndAccessibility.md) 1156-1157 and
      [docs/rich-text.md](../../../docs/rich-text.md) 256 in the same change.
- [ ] **R28-01** Grid and Tree `OnBoundsChanged` clamps.
- [ ] **R28-02** cancel capture only when the captured control is in a pending-content subtree or its bounds moved.
- [ ] **R28-03** apply `lineHeightMultiplier` to positioned chunks.
- [ ] **R28-04** use `HorizontalTextBounds().second` as width; named `{left, width}` struct.
- [ ] **R28-06** `TS_SD_READONLY`, `TS_SS_MULTILINE`, `TS_E_READONLY`, `OnStatusChange`.
- [ ] **R28-40** `AcceptsTypedText()` for the printable `WM_KEYDOWN` swallow only.
- [ ] **R28-07 / R28-08** reject `\\` outside rows; map `\|` to U+2016.
- [ ] **R28-09** reserve `accTitle`, `accDescr`, `title`, `cssClass`.
- [ ] **R28-12** invalidate Tree selection setters and Grid busy/sort setters.

### Wave 3 — RedTilde editor fidelity

- [ ] **R28-10** bare autolink Link ink; `\(` `\)` `\[` `\]` as two-unit delimiters.
- [ ] **R28-11** republish after a moved UIA `ScrollIntoView`.
- [ ] **R28-23** collect every native-object rejection in one geometry pass.
- [ ] **R28-24** after Q4: Source sync uses `Invalidate()`, not `RetireLayout()` + `scrollDip = 0`.
- [ ] Remaining audit W1 command-planner defects except `commands-01` (already fixed). Do not start the wider
      EOL-scanner work as a second implementation of Enter.

### Wave 4 — pin bumps

1. Land Wave 1+2 on DxUi `main`. One `Changes/` fragment. Do not edit `CHANGELOG.md`.
2. Land menu ports on both wall-compat branches.
3. RedTilde then RedSalamander pin that `main` commit.
4. RedPrism and RedXe re-pin wall-compat, not `main`. RedPrism stays API 3 until its own revision move.
5. Do not advertise file drag-and-drop. [RichTextFileDrop_2026-10-20](RichTextFileDrop_2026-10-20.md) is SPEC READY
   and unimplemented.

### Wave 5 — remaining DeliveryReview items and resiliency

Owned in detail by [DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md). Land on `main` so they are not left
behind the pin. DX-1 does not block the bump.

- [ ] DX-1 Tree invoke after collapse.
- [ ] DX-8 inactive Tree selection text.
- [ ] DX-9 wrapped Label typography override.
- [ ] DX-10 `OnChar` `bad_alloc` after Q2.
- [ ] DX-5 `Separator()` `noexcept`.
- [ ] DX-6 `TagPicker::ClearChildren` null slots.
- [ ] DX-11 optional historical `ControlKind` ordinals.
- [ ] R28-14 remaining setter lifetime after Q1.
- [ ] R28-16 bounded `GetTextExt`.
- [ ] R28-17 content-failure HRESULT after Q5.
- [ ] R28-18 / R28-19 tree-item UIA event cap and snapshot pinning.

### Wave 6 — qualification honesty before saying delivered

- [ ] `Test-Changes.ps1 -Explain`, then x64 Debug / Release / ASan Debug for the files actually changed. PrePush
      before merge.
- [ ] New tests: UIA-then-`WM_TIMER`, unmatched `WM_CHAR` vs stationary hover, HDROP injection, `SetModel` OOM,
      RTL Subtle widths, three-chunk Source pitch, maximize-after-scroll Grid/Tree.
- [ ] Interactive Menu / NativeTextInput only with desktop agreement. ARM64 overlapping-popup root-switch needs a
      native ARM64 host and remaining attribution from [PopupReadiness_2026-10-17](PopupReadiness_2026-10-17.md).
- [ ] Do not claim paired non-regression while the policy is unqualified and 72/72 controls are unstable.
- [ ] Exact-pin consumer receipts after the bump. Library CI does not waive product scoped tests.

### Wave 7 — tooling and CI (parallel, not a pin blocker)

- [ ] R28-37 / R28-38 CODEOWNERS and `prContractFiles` cover every script CI actually runs.
- [ ] R28-46 Menu tests assert pending-open before the timer.
- [ ] R28-51 zero-warning gate requires `Build succeeded` and summary counts.
- [ ] R28-52 / R28-53 fold submission checks exact fold content; `Test-Changelog -Base` needs a revert/release path.
- [ ] Supply `EXPECTED_*` / `PAIRED_*` / `VERDICT_*` to `Test-CiGate.ps1`.

## Explicitly not a ship item

- DX-7 character-receipt `DispatchCharacter`. No consumer passes a receipt.
- File drag-and-drop.
- S3–S7 consumer UI proposals.
- API revision 5.
- The 241-finding RichText audit beyond Wave 3.

## Validation when implementation starts

Extend the existing menu, rich-text, tree, grid, layout, catalog and TagPicker tests named in
[DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md#validation-when-implementation-starts) and the Wave 6
list above. A new native test file uses `<Component>.Tests.Something.h/.cpp` and `Tests/native-test-files.json`.
Run the affected `test.ps1` configurations for the files actually changed. Wall-compat ports are verified on those
branches. Move this plan to Done only after the chosen waves are implemented, their tests pass, and the Enter
contract sentences are updated. DX-11 may stay unchecked if the ordinal assertions are not added.
