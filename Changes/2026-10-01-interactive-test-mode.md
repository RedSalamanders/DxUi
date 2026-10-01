- `test.ps1 -Interactive` runs the control suites that need the person's real desktop under a desktop lease of DxUi's own, with a
  warning before it takes anything and the foreground window, keyboard focus and pointer put back afterwards. More than twenty
  control tests (22 in the Menu suite, a few in NativeTextInput) record a capability skip in a run that cannot get focus, and the
  last run of them went through an ad-hoc wrapper rebuilt from a RedSalamander header recovered from an old commit. Nothing about
  the library changed: this is the test harness.
  - Selection: without `-Suites` the run is `Menu` and `NativeTextInput`; `MenuResources` and `MenuResourceScaling`, which also
    reject `--no-activate`, run when named. Any other suite is refused by name before anything is built. The list is
    `Tools/InteractiveRun.psm1`'s, and `Test-InteractiveMode.ps1` keeps it equal to `DxUi.ControlTests.exe`'s own `suiteCanActivate`.
    A run without `-Interactive` is unchanged, including for these four suites, and never reaches the lease.
  - Refusal: before anything is built in a CI job or a process with no interactive window station; after the build the lease checks
    the session natively (`DxUi.InteractiveLease.exe --check`: visible window station, not session 0, active session, an input
    desktop it can open and that is its own, no screen saver, a readable pointer) and checks again when it is about to ask, so a
    locked screen or a disconnected session refuses with its reason. Nothing is shown or taken by a check.
  - The warning is a dialog (`Tests/InteractiveLease`) that says what will happen and for about how long. Its default button is
    Cancel, so a key pressed by accident as it appears cannot start a takeover, and if nobody answers within two minutes the answer
    is Cancel (exit code 21, nothing taken). During the run a banner at the top of the primary monitor, above every window and
    click-through, tells the person to keep hands off the keyboard and mouse.
  - The lease records the foreground window, its keyboard focus and the physical pointer before the dialog appears, takes a
    session-wide lease (a second run refuses instead of queueing), keeps the system awake, runs each suite as a child of its own in
    a kill-on-close job (the foreground is granted to that child alone; only a process the lease started is ever ended) and
    restores the foreground, then the focus, then the pointer, verifying each by reading the desktop and never by a call's result.
    It does so on every exit path: passing suites, a failing suite, the watchdog's exit code 124, a hung child, a child that cannot
    start, a cancelled or unanswered dialog, a warning that cannot be shown, Ctrl+C. The pointer is restored last because activating
    a window can move it. A pointer the fixtures did not put back is reported with where the run left it, and never counted as the
    fixtures' own restoration.
  - An interactive run passes only when every suite exits 0, none records a capability skip (it exists to run what other runs
    skip) and no part of the desktop failed to come back. Logs and receipts take the suffix `.interactive` and a receipt records
    the lease, so they never replace those of the run that records the suite's skips.
  - Tests, none of which takes or asks for a desktop: the new `InteractiveLease` control suite (36 tests, in every `test.ps1` run,
    `--no-activate`) restores a desktop built of fake windows, a fake pointer and a fake foreground and runs the whole lease against
    fake services on every path, requiring the restoration and the release of the lease and the display each time; 28 throwaway
    mutants of the restoration and the orchestration (restoring in the wrong order, trusting a call that reports success, skipping the
    restoration after Cancel or a failed child, never releasing the lease, a timeout taken for a Start, and the rest) each fail it.
    `DxUi.InteractiveLease.exe --self-test` proves the Windows services on a private desktop that is never the input desktop:
    children with their logs, exit codes, bound and stop, the session's lease, and the dialog (Start, Cancel, no answer, a stopped
    run) and the warning. `Test-InteractiveMode.ps1` covers the selection, the refusals (two bounded runs of `test.ps1` under a CI
    environment end before anything is built), the plan and result files and that `test.ps1` reaches the lease only through
    `-Interactive`; 19 throwaway mutants of the PowerShell side and the dialog's source each fail one of the two scripts.
  - Docs: the contract is in `Testing_Validation` (Interactive tests), the command in `docs/performance.md` and `Tools/README.md`,
    and `AGENTS.md` and the build and input skills say to run it only with the person's agreement. A nonvisual tooling change: no
    gallery image or design-system preview changes.
