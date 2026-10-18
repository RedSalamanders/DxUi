# Pinned Office recalculation evidence, 2026-10-18

Collected through registered Ghidra MCP run_script_inline over an owned stdio
bridge, using the repository's MCP framing. No raw plugin HTTP call, policy
change, import, analysis run, symbol edit, save or current-program switch occurs.
The saved /office-modern/RICHED20.DLL program opens read-only from the existing
RichEditReference project. SHA-256 matches
7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41.
The original inventory records version 16.0.20430.20032. This is the pinned saved
reference, not a claim about the currently installed Office version.

The Java script checks every exact symbol/address before decompilation and records
programChanged=false. The active program remains /windows-edit/user32.dll before
and after. MCP console output explicitly reports successful script completion;
all six nonempty C pseudocode files and the inventory exist. A successful MCP
transport result alone would not have established Java script success.

Observed code:

- CDisplay::InvalidateRecalc, RVA 0x45170, marks pending state and conditionally
  clears engine contexts/layout caches. It updates stored integer bounds; their complete private layout remains uncertain.
- CDisplayML::StepBackgroundRecalc, RVA 0x40b0, uses CWriteLock, guarded state and
  a callback-driven step. Its completion path calls TxKillTimer with ID 0x1af.
  The name alone does not prove a separate thread.
- CDisplayML::WaitForRecalc, RVA 0x40810, guards a reentrant call with state at
  offset 0xd2 and delegates to the multiline or PTS engine path.
- CDisplayEngineML::WaitForRecalc, RVA 0x40990, compares requested positions with
  current progress, chooses complete or continuation recalculation, rebinds the
  first visible line and updates scrollbars. Large pending ranges may set a wait
  cursor. These calls are not a nonblocking scheduling contract.
- CDisplayEngineML::RecalcLines, RVA 0x3df50, and CDisplayML::RecalcView,
  RVA 0x41280, provide the larger line/viewport paths. Keep their raw pseudocode;
  not every field, bit, indirect call or layout structure is understood.

Owned design implications (inferences, not recovered original source): distinguish
source invalidation that can reuse prepared data from complete resource retirement;
guard reentrancy and revision/binding identity; publish compatible source/geometry
and scroll state together. Use DxUi's existing host preparation callback and shared
GraphicsDevice ownership. Do not copy the Office per-control timer or private
offset/bit ABI. Add controlled budget/cancellation/hidden/two-view tests before
claiming a shared/cooperative scheduler. This evidence informs the next component;
it does not retroactively prove implementation or close the performance hold.
