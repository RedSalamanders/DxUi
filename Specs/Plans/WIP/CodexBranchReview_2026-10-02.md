# Fixes kept from the codex branches

- **Status**: ACTIVE (review done; the provider-lifetime and caret fixes are in main with their tests (#55), and so is
  the focus-callback audit (#57); per-entry elements for ordinary menus and the snapshot pre-sizing are ported and
  measured, and await the developer's acceptance of their measured cost; the wider delegate audit remains)
- **Owner**: DxUi accessibility (`src/Controls/DxUi.Accessibility.cpp`), `Button` and `TextField`. RedSalamander's
  `codex/fileops-ui-i26` branch is the consumer that met these bugs.
- **Why**: four branches on GitHub were never merged and have no pull request: `codex/multiline-caret-viewport`, which
  contains the other three, `codex/fileops-ui-qualified`, `codex/menu-description-layout` and `codex/grid-line-clamp`
  (20 September to 1 October). Besides about 3,600 evidence files they change library code that main does not have.
  RedSalamander's `codex/fileops-ui-i26` pins their tip, `19ca44d`. This plan keeps what main lacks, rebuilt on main's
  own design, and records why the rest is left.

## How the review was made

Each of the 13 code commits was compared with main: how much of the code it adds is in main, then its intent against
main's current code. Where the commit carries regression tests, they were ported unchanged to main and run against
main's library, as oracles: a test that fails shows a fix main lacks.

| Commit | Change | Main |
| --- | --- | --- |
| `b5b58b8`, `c1f047b`, `356006a`, `1315b4f` | Described native menu rows, their qualification and heap probes | In main, rebuilt (#24, #33, #37): 87–90% of the added code |
| `d461095` | Reentrant async menu destruction at CRT exit | In main as the `MenuExitLifetime` suite (#23) |
| `7ada987` | Scrollbar width from each row's original text width | In main (`row.textWidthDip`) |
| `f72941b` | Share identical menu text layouts within a popup | Superseded: main gives each described row one layout for both fields (#37) |
| `826fa56` | A grid text-layout cache probe | Superseded: main has `DebugGetTextLayoutStatistics` and its own bounds tests |
| `49c963f` | Native providers bound to their control's lifetime | Mostly in main (identity in snapshots and runtime ids, identified resolution); the oracles find four gaps, below |
| `65b0257` | Pre-size the snapshot's vectors for large menus | Not in main; an unmeasured optimization |
| `15be545`, `d45c361` | Per-entry elements for ordinary menus, their scrolled bounds, native focus that selects no row | Not in main, whose contract keeps per-entry elements to described menus |
| `19ca44d` | Clip a multiline caret to its text viewport | Not in main: the oracle fails |

The oracles of `49c963f`, run against main: a queued `Select` of a stale text range is refused (passes). These fail:

- a queued `Invoke` from the element of a window whose only control was replaced runs the replacement's action, and a
  retained element of that collapsed root invokes the replacement directly: the window's root element never binds to
  the control it stands for (main's own stale-element test keeps two controls on purpose so the root does not collapse);
- `SetFocus` of an element whose focus callback rebuilds the controls reports success, and `Button::Invoke` with
  `focusSelf` goes on using the button after that callback destroyed it;
- `GetRuntimeId` of an element whose control is gone still answers, where every other call reports
  `UIA_E_ELEMENTNOTAVAILABLE`.

## Checklist

- [x] Bind the window's root element, when a single semantic control collapses into it, to that control's identity, as
  every other element is: an element or queued action of the old control reports `UIA_E_ELEMENTNOTAVAILABLE`, and the
  window gives newly acquiring clients a fresh root. A root that stands for no control keeps representing the live host.
- [x] Stop `Button::Invoke` once a focus callback has destroyed the button, and make an element's `SetFocus` and
  `Invoke` report the element gone when its control did not survive the focus change.
- [x] Answer `GetRuntimeId` of an element whose control is gone with `UIA_E_ELEMENTNOTAVAILABLE`.
- [x] Clip a multiline `TextField`'s caret to its text viewport (`19ca44d`).
- [x] Keep the oracle tests: the five native-lifetime tests in the Accessibility suite, with a sixth for a root acquired
  before a control collapsed into it, and the caret-viewport test in the Embedded suite. All but the queued `Select` fail
  against main before the port, and each of ten single-point reversions of the fixes (nine of the provider fix, one of the caret clip) fails one of them (reverting the
  `Button::Invoke` check crashes the test with an access violation on the destroyed button).
- [ ] Decide with the developer whether ordinary menus get per-entry elements (`15be545`, `d45c361`): it changes the
  contract in `UI_InputAndAccessibility.md` and the menu resource budgets, so it needs paired menu resource runs. The
  developer chose to adopt them on 2 October; `improve/ordinary-menu-accessibility` ports them with the contract change
  and three paired sets ([packet](../../../Measurements/PlainMenuUia/2026-10-02/README.md)): about 1.6 KB of live heap
  per open command row, and 0.3 ms plus 1.4 to 2.6 µs per row for each key or wheel notch that moves the rows. The
  developer's acceptance of that measured cost, or another of the packet's options, closes this item.
- [x] Measure `65b0257` on a large menu before taking it; leave it out unless a paired run shows a gain. Taken: rebuilt
  on main, a paired run at 4,096 rows saves 24% of a Down (7.9 to 6.0 ms), 28% of a wheel notch and 0.97 MB of the open
  menu, and 10% to 13% of a key at 128 rows.
- [x] Audit the other controls that focus themselves and go on using `this` (about 25 `SetFocusControl(this)` call
  sites), and the UI Automation actions that select a grid row before focusing the grid, for the same focus-callback or
  delegate rebuild, as a follow-up (#57). 28 call sites focused their own control and 25 went on using it; all now go
  through `FocusControlAndSurvive` and stop when a focus callback destroyed the control. `TabControl::SelectTab` reports
  whether its focus and selection callbacks left the control alive. UI Automation's Select, AddToSelection and grid-row
  and grid-cell focus check that their control survived the selection's delegate, and `Grid::SelectRow`, which they
  share with a press, a double click and the arrow keys, stops after a delegate that destroyed its grid, as Ctrl+A and
  RemoveFromSelection do. Each path has a replacement test that AddressSanitizer fails against the old library.
- [ ] An application delegate that destroys its control in the middle of an input handler is a wider class than the
  focus callbacks, and only partly covered. Grid's group toggles, row activation, checkbox toggles, sorting and context
  menus, and the selection changes `SetModel`, `SetSelectionMode`, `NotifyDataChanged` and `ApplyGroupLayout` report,
  still keep using the grid after its delegate (`docs/controls.md` tells applications to post a rebuild from them).
  Audit the delegate and callback call sites of the controls that keep running after them, as the focus sites were.
- [ ] Keep the four codex branches until RedSalamander's `codex/fileops-ui-i26` pins a DxUi main.

## Validation

`validate.ps1`, `format.ps1 -Check`, `test.ps1` in x64 Debug, Release and ASan Debug (without the suites that take the
desktop, which CI runs) and the three ARM64 builds; each oracle fails against main before its fix.
