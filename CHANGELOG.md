# Changelog

New entries start as fragments under [Changes](Changes/README.md) and are folded in here, newest first.

## Unreleased

- Resolve canonical private include paths while preserving independent public headers and shipping sources; add boundary controls and register the permanent payload ICF regression in the applicable native inventory.
- Preserve the pinned SDK API while making private includes and HRESULT boundaries explicit for /Wall consumers; use per-type data tags so folded deleters cannot admit a wrong payload type, with a Release ICF and lifetime regression in WindowHost.
- Add optional immutable prepared Tree accessibility rows with complete offscreen Unicode names and bounded snapshot capture; existing models retain their fallback behavior.
  - Coalesce native structure notifications on one reusable delivery slot so a real UIA callback can invoke an owner action without blocking publication. Canonical roots no longer retain superseded creation snapshots.
- Embedded structure notifications use a bounded coalesced delivery slot and a COM-marshalled provider
  proxy on an MTA. Public embedded providers retain their STA/site contract. A held UIA client callback no longer
  runs inline on the publishing owner or retains historical prepared row snapshots. Disconnect revokes registration
  and clears pending delivery without joining clients. API revision 3 and the public C++ interface are unchanged.

  Qualification is tracked in `Specs/Plans/WIP/PreparedTreeAccessibility_2026-10-06.md`; actual consumer callbacks,
  the complete configuration matrix and current paired measurements remain required before adoption closes.

  - Recorded the x64 Debug/Release/real ASan test matrix and three ARM64 cross-builds for the pushed structure-delivery source, including independent readback of 1,394 archived files. Current-source paired scenes and RedPrism client qualification remain open.
- Internal embedded UI Automation property-event providers preserve control identity without retaining complete
  creation snapshots, so event references kept by UIA do not pin superseded prepared Tree row epochs. Public
  retained row providers and text ranges preserve their existing source ownership and stale-control guards.
- Add an explicit embedded interaction layout boundary for caller-arranged bounds between frames, preserving deferred painting, capture cancellation and full preparation after failure, size/DPI or device changes.
- `vcpkg-install.ps1` restores with the Visual Studio installation and default MSVC toolset that MSBuild compiles DxUi
  with, not the newest toolset vcpkg finds. The development machine's VS 18 Insiders has MSVC 14.52.36725, which has no
  x64-hosted ARM64 compiler, beside the default 14.51.36231, so every fresh ARM64 restore failed configuring
  `wil:arm64-windows` (vcpkg chose
  `14.52.36725\bin\Hostx64\x64\cl.exe` for it and the compile test ended in `LNK1104` on `MSVCRTD.lib`), and a consumer's
  compiled vcpkg libraries could be built by a newer toolset than the one that links them.
  - The installation comes from the discovery `build.ps1` uses, now one function (`Get-DxUiVisualStudioInstallation` in
    `Tools/VisualStudio.psm1`, which `test-consumer.ps1` and `Test-AsanRuntime.ps1` call too), and the toolset from that
    installation's `VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt`; nothing is hard-coded. A missing or
    malformed version file fails before anything is cloned, naming the file and the repair.
  - Each platform gets an overlay triplet, `<output root>\vcpkg-triplets\<platform>\<triplet>.cmake`
    (`Tools/VcpkgTriplet.psm1`): the pinned vcpkg checkout's triplet copied unchanged in its own line endings, then
    `VCPKG_VISUAL_STUDIO_PATH` and `VCPKG_PLATFORM_TOOLSET_VERSION` (major.minor), passed to `vcpkg install` as
    `--overlay-triplets`. It is rewritten only when its bytes change, so a patch update of the default toolset leaves it
    alone. vcpkg's package ABI hash changes with the triplet, so the first restore after this change rebuilds the
    packages (WIL here, built in 5-6 s). In an ARM64 restore on an x64 host the host triplet's tool ports keep the stock
    triplet; they run during the restore and are not linked.
  - A fresh ARM64 restore and a fresh x64 restore now succeed with vcpkg's own `Compiler found:` line naming
    `14.51.36231\bin\Hostx64\arm64\cl.exe` and `...\Hostx64\x64\cl.exe`. The interface (`-Platform`, `-OutputRoot`) is
    unchanged, so a consumer that runs the pinned checkout's script (RedXe, RedPrism and RedSalamander do) needs no
    change, and the script still parses in Windows PowerShell 5.1, which RedSalamander runs it with. A consumer's own
    installer for compiled libraries needs the same pins. `Tools/tests/Test-VcpkgTriplet.ps1` covers the fixture
    installations, the overlay text, the write-only-on-change rule and the wiring; each of 26 throwaway mutants (dropping
    either pin, losing the stock text, always rewriting, a second discovery, a new installer parameter, and more) fails it
    (`Build_ToolchainAndConsumption`). A nonvisual tooling change: no gallery image or design-system preview changes.
- The Tooltip suite's timer fixtures no longer depend on wall-clock timing.
  - **The failure.** `TestTooltipLayerHideDelayExpiresAfterTimerTicks` scheduled the default 100 ms hide, pumped messages
    for 50 ms of wall time, and required the tooltip to be visible. A CI runner that stalled overran that margin and failed
    the check, on a pull request that did not touch tooltips.
  - **The cause.** The deadline is on the animation dispatcher's clock.
    - While the dispatcher is idle, that clock reads the wall clock, and its first tick restarts it from the wall clock. A
      stall between scheduling and that first tick therefore put the clock past the deadline at once. A 150 ms stall
      injected there reproduces the CI failure.
    - Once the dispatcher is running, each tick moves the clock by the time since the previous tick, clamped to 50 ms.
  - **What the hide-delay fixtures do now.** They keep the dispatcher ticking with a subscription of their own, take the
    deadline from its clock, and dispatch one message at a time. Each tick is therefore observed with the state it left.
  - **Hide delay.** With a delay of twice the clamp, at least one tick lands before the deadline. The tooltip must be visible
    after each such tick, and hidden after the first tick at or past the deadline.
  - **Pointer move.** The move-cancels-hide fixture moves the tooltip one tick into the delay, then requires it to stay
    visible once the clock passes the old deadline. Before, a stalled runner let it pass without testing the cancel.
  - **Passive tooltip lifetime.** The old check never tested the five-second display lifetime: with the old check in place, a
    6 s lifetime still passed.
    - It ran after a click that had already hidden the tooltip: mouse-up releases capture, and losing capture clears the
      tooltip.
    - Its window was never shown, and a host ticks its tooltip only while its window is visible, so the tick did nothing.
    - Even on a visible tooltip, its tick at `GetTickCount64()` plus 6 s mixed the wall clock with the dispatcher's. On a
      machine whose mouse hover time is about a second or longer, that tick lands before the lifetime ends.
  - **What the passive fixture does now.** It shows its window without activating it, and checks the lifetime before the
    click. The tooltip is shown with a tick past the longest show delay (2.5 s), and the lifetime is checked exactly from
    that tick: visible at 4,999 ms, hidden at 5,000 ms. The rest of the fixture is unchanged, apart from a message that now
    says the click hid the tooltip, not the lifetime.
  - **No new hang risk.** A ten-second limit ends a run whose ticks never come. Library behavior is unchanged.
- Grid selection membership no longer scans a large selection, and a selection model no longer keeps the room of one it has
  dropped. `GridSelectionModel::IsSelected` was a linear search of the ordered ids and a `Grid` asks it once for every visible
  row on every paint, so after Ctrl+A on a long list each repaint searched the selection 24 times (0.9 to 1.4 us a call at
  20,000 selected rows, 47 to 94 us at 1,000,000); a pending Tree multi-select change reuses the model. The model now keeps
  its ids twice, in selection order (what `GetOrderedSelection` returns) and ascending beside it, and answers a selection of
  up to 1,024 ids by the scan it always used and a larger one by a binary search of the ascending copy, which allocates
  nothing. What a selection holds is unchanged: the order of `GetOrderedSelection`, the anchor rules of `Toggle` (the first id
  of the selection order when the anchor leaves), `SetRange` (the id it started from) and `PreserveOrdered` (the first id
  kept), an id a list gives twice (held twice), and the exception contract (`Clear`, `SetSingle`, `Toggle` and the accessors
  stay `noexcept`; `SetRange` and `PreserveOrdered` leave the selection as it was when an allocation fails).
  - The limit is 1,024, not the 16 to 64 that a first measurement suggested. That measurement asked the same 32 ids on every
    pass, so a processor learned which way each comparison of a search went and the search looked 2 to 5 ns at any size; its
    0.7 to 2 ns slowdown below 64 ids belonged to the two executables (built alike, the sorted copy is never slower than `main`
    in that order). The ids of the rows on screen, asked of a selection of hashed ids, come in no order a processor can learn,
    and a search then costs 5 to 20 ns more than the scan at 8 to 128 selected ids (a miss at 16 ids: 4.4 ns on `main`, 18.6 on
    the sorted copy alone). The two meet at about 500 to 650 ids; 1,024 is the first power of two beyond them, so that no size
    is slower than the scan it replaced. In three builds of ten runs per side (`main`, the sorted copy alone and this; x64
    Release, WARP; run as A, B, C, C, B, A), of 82 per-call comparisons at up to 1,024 ids against `main` 80 are within noise
    and 2 read slower at the 5% rate, and all 16 above 1,024 ids are faster (1,500 ids: -90% in the order a processor
    predicts, -44% to -59% in random order; 1,000,000 ids: -100%, 14 to 16 ns, or 149 ns in random order, against 49 to 94 us).
  - A Grid's paint no longer grows with its selection: Prepare takes 22% less at 200,000 selected rows and 64% less at
    1,000,000 (27% to 30% and 70% to 73% in the first set), flat at 0.5 to 0.9 ms, and what `IsSelected` costs inside a paint,
    isolated by painting alike with a small and a full selection, falls from 0.05, 0.35 and 1.6 ms at 20,000, 200,000 and
    1,000,000 rows to nothing measurable; a paint allocates as often as before, and no paint metric differs from the sorted
    copy alone.
  - A buffer with room for more than 4,096 ids (32 KiB) is given back when the ids that replace its contents need at most half
    of it: `Clear` and `SetSingle` swap it with an empty vector (no allocation in a Release build, so both stay `noexcept`),
    `SetRange` makes both copies beside the old ones and swaps them in, a `PreserveOrdered` that drops ids keeps no room it
    reserved for a selection that shrank, and `Toggle` leaves its room. After Ctrl+A over 200,000 ids a model held 3.2 MB (twice
    what the ordered ids alone held); it holds 0 bytes after `Clear`, 16 after a click on a row and 1,600 after a Shift+click 99
    rows away, and a selection of 4,096 ids keeps its room. Giving the room back costs 0.1 to 0.2 ms and getting it again
    0.3 ms at 200,000 ids (1 to 2 us at 20,000), against 0.4 to 11 ms for the Ctrl+A itself.
  - `PreserveOrdered`, which `NotifyDataChanged` calls on every data change of a Grid with a selection, asks about every row of
    the model. The sorted copy made it cheaper than the hash set it replaced when all or half of the rows are selected (0.70 ms
    instead of 1.45 for 20,000 ascending ids, 8.2 instead of 41.9 for 200,000), but dearer for a modest selection in a long list
    with hashed ids: 200,000 rows with 16 to 5,000 ids clicked took 2.4 to 11 ms where `main` took 1.2 to 2.8. It now asks a
    table of bits (16 to 32 per selected id) first and searches only the rows it lets through: 0.2 to 1.1 ms for 3 to 5,000
    ids, with 2 or 3 heap calls where `main` made 5 to 5,008 and the sorted copy alone 1 or 2.
  - The costs: 8 bytes per selected row (the model grows from 40 to 64 bytes); `SetRange` over ids that do not already ascend
    sorts the second copy (1.0 ms for 20,000 ids, 10.9 ms for 200,000; a probe puts a radix sort at about a seventh of that,
    and it is not done); `Toggle` at 1,000 scattered ids takes 0.26 us where it took 0.17; and an empty selection costs the
    scan's 4.7 ns a call where the sorted copy alone cost 2.2.
  - Tests (Grid suite): a copy of the old linear logic as the reference, and fixed-seed randomized runs over universes of 1
    to 5,200 ids (so that selections pass 1,024 and 4,096 ids) that check count, order, anchor and `IsSelected` after every
    operation and the room rule after every mutator that replaces the selection, and must reach repeated ids, fallbacks, dropped
    ids, unchanged data changes, the anchor's removal, selections on both sides of both limits, and every way of giving room
    back; focused cases (a selection grown past 1,024 ids and shrunk back, answering membership at every size around the
    limit; `Clear` keeping 4,096 ids' room and giving back 4,097's; a range over exactly half of the room giving it back and
    one id more reusing it; a click and a Shift+click after Ctrl+A at the Grid; a long list of 60,000 hashed rows kept in two
    orders; the earlier ones for repeated ids, anchors and orders); and `static_assert`s of the `noexcept` contract. The room is
    asserted through an additive diagnostics accessor, `GridSelectionModel::DebugGetBuffers`, the library's own exact count,
    like `DebugGetContextMenuResources`. Of 40 throwaway mutants, 35 are caught (the first campaign's 15, now also through
    `Toggle` and the large selections, where the scan hides the ascending copy; the scan dropping the newest or oldest id; the
    search reading the wrong copy; each limit and half-rule comparison; releasing one buffer but not the other, for `Clear`,
    `SetSingle` and `SetRange`; the filter turned off or built from the wrong ids), and the other 5 cannot fail a test because
    they give the same answers (a scan limit of 1,023 or 0 ids, the scan and the search swapped, a filter that lets every id
    through, `SetRange` testing the room of one copy only, the two always being equal): the per-call measurement sees those.
  - The opt-in `DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json> [parts]` (`Tests/Embedded/GridSelectionBenchmark.h`,
    fixture `dxui-grid-selection-v2`) measures paint, the isolated cost, `IsSelected` per call in both orders, the heap a model
    holds after each way back from Ctrl+A, the mutators and `PreserveOrdered` over a long list with synthetic data; the
    allocation hook of `EmbeddedTests.cpp` keeps an exact live byte count on request. Its receipts and README are under
    `Measurements/GridSelection/2026-10-01` (two sets of the sorted copy against `main`, and the three-build set).
    API revision stays 2 (an additive diagnostics accessor, and private members of `GridSelectionModel` and so the size of
    `Grid`, changed). The performance contract, the testing contract and `docs/performance.md` describe the budget and the
    measurement; no default or pixel changed (the six sheets of a local Release render are byte-identical to `main`'s and to
    `docs/gallery`).
- DxUi's private window messages are registered by name instead of being `WM_APP` offsets, so an application's own
  messages on the window it shares with DxUi can no longer collide with them: RedSalamander's test message at
  `WM_APP + 0x6A` was DxUi's accessibility action. All sixteen (`src/Support/WindowMessages.h`) now come from
  `RegisterWindowMessageW` as `RedSalamanders.DxUi.<Component>.<Purpose>.v1`, with values in 0xC000–0xFFFF that no
  application message can take: the five that reach the application's window (the accessibility action and provider
  creation, the process-exit detach, the end of a focus gain's turn and a modal menu's menu-bar hover), the ten that
  menu popups receive, and `TextInputServices`' deferred lock, which was registered already and takes the new name.
  Each is registered once, on first use, and every sender and receiver compares that cached value. A failed
  registration is 0 (`WM_NULL`): no sender posts or sends it, each one behaving as when its post fails, and no
  receiver matches it, since a registered message does not compare with a message value and only `Matches` recognizes
  one. `TextInputServices::Attach` reports such a failure as `E_FAIL` rather than a stale `GetLastError`, which could
  read as `S_OK`. A window procedure forwards every message, registered ones included, to `HandleMessage`; DxUi
  reserves no `WM_APP` value, so consumers can drop their reservations of `WM_APP + 0x6A` to `0x6D` and `0x539`. An
  application that drove a modal menu's menu-bar hover by posting `WM_APP + 0x539` itself, as RedSalamander's main
  menu does, calls the new `ContextMenu::PostMenuBarHover` instead. The WindowHost suite checks that every message is
  registered, distinct and never `WM_NULL`, and sends and posts application messages at each former value to show that
  they reach the window procedure while `HandleMessage` neither consumes nor acts on them; the Menu suite shows the
  same for `WM_APP + 0x539` during a modal menu. API revision stays 2 (additive `ContextMenu::PostMenuBarHover`).
  Nothing visual changed, so the gallery is unchanged.
- A failed runtime check in a Debug test run no longer opens a dialog on the desktop.
  - The STL range check a mutant tripped opened the CRT's modal Abort/Retry/Ignore box. Such a box holds an unattended run
    until its watchdog ends it, and interrupts whoever is at the desktop.
  - Every native test executable now first calls `Tests/Support/FailureReports.h`. The report goes to stderr and the run
    ends with exit code 3, without Windows Error Reporting's dialog. The Embedded executable calls it before `main`, so
    the fingerprinted `BenchmarkMain.h` is unchanged.
  - `Test-TestWatchdog.ps1` runs the hidden `--failure-report-self-test` and requires it to end within its bound with
    exit code 3 and the report (`Testing_Validation`).
- The native menu's modal loop no longer reads a freed popup when its chain changes during a peek.
  - `PeekMessageW` first runs the handlers of messages other threads sent to the menu's thread, and such a handler can
    close or open a submenu. The loops that drain the popups' own paints and state probes walked `controller.popups` with
    a range-for, so a submenu closed during a peek left the loop reading past the shortened chain.
  - An x64 ASan Debug Menu run on main failed once in four with a container-overflow in `PeekMenuDebugStateMessage`.
  - One helper, `PeekMenuPopupMessage`, now indexes the chain afresh for every peek and holds no popup across the call,
    without allocating. The priority order is unchanged: input, then the popups' paints, then the probes.
  - `TestMenuModalLoopPeeksOnlyLivePopupsWhenAPeekClosesASubmenu` closes the open submenu from the loop's peek of the
    root popup's probes, through the new diagnostics hook `DebugSetContextMenuModalLoopPeekHookForTest`. It requires the
    loop to peek only live popups afterwards. It fails on the previous loop, and an index loop that caches the chain's
    size fails it with an STL range check.
- Changes no longer conflict over the changelog or the plan index. A change records its changelog entry as one file under
  `Changes/` (`<yyyy-mm-dd>-<topic>.md`, one bullet; `Changes/README.md`), and `Tools/Fold-Changelog.ps1` folds the
  fragments into `CHANGELOG.md`, newest first. `validate-specs.ps1` checks each fragment's name and shape, and
  `Test-Docs.ps1` covers the fold. The WIP plan index lists one plan per entry, separated by blank lines, so updates to
  different plans merge cleanly. On 30 September five branches each prepended an entry or edited a neighbouring index
  row, so they merged one at a time and ran CI again after each re-merge.
- API revision 3, with a rule for when the revision changes and a validated list of what consumers may call.
  - **Why.** Three changes since revision 2 could break a pinned consumer, yet the revision stayed at 2. A consumer moving its
    pin to this commit or a later one sets `apiRevision` to 3 in `Dependencies/DxUi.lock.json` and makes these changes:
    - **Renamed interfaces (#29).** `IDxGridModel`, `IDxGridDelegate`, `IDxTreeModel` and `IDxTreeDelegate` are now
      `IGridModel`, `IGridDelegate`, `ITreeModel` and `ITreeDelegate`.
    - **No Python tools (#30).** A consumer that ran the Python build-matrix validator runs
      `validate-build-matrix.ps1 -Root <consumer root>` instead.
    - **Registered window messages (#41).** DxUi's private window messages are registered by name, so a consumer:
      - drops any DxUi `WM_APP` value it copied;
      - forwards every message to `HandleMessage`, including registered ones (0xC000–0xFFFF);
      - posts the menu-bar hover with `ContextMenu::PostMenuBarHover`.
    - **New output fingerprint.** The consumer output fingerprint includes the revision, so the first build after the
      move uses a fresh dependency output directory.
  - **The rule** (`Specs/Build/Build_ToolchainAndConsumption.md`).
    - The revision increments when a consumer built against the previous one could stop compiling, linking or behaving as
      before without changing its own code. Additions do not count.
    - Instead of incrementing, a change may keep the old form working for one revision. The alias has no `[[deprecated]]`
      attribute, because consumers compile with warnings as errors; the changelog names it instead.
  - **The consumer interface.** `capabilities.json` now lists what a consumer may call or import:
    - three scripts, with the parameters consumers pass;
    - five module functions, with their parameters;
    - the four MSBuild entry files;
    - the public header root.
    `validate-dependencies.ps1` fails when a listed entry disappears, a listed function is no longer exported, or a listed
    parameter is renamed. It reads scripts and modules from their parse trees, and new parameters pass.
  - **Single source for the revision.** `Get-DxUiConsumerBuildIdentity` and `test-consumer.ps1` read the revision from
    `capabilities.json` instead of hard-coding 2. The docs, the consumer-integration skill and the README say revision 3.
- The described-menu memory waiver is removed (plan `MenuDescriptionMemory_2026-09-27`). It accepted up to 1,175,552
  clean-round private bytes (+4.6%) that the September 21 pairs measured for described native menus. A local paired
  set of six runs per side (`e47c836` against `6f769ab`, the hosted set's harness) now finds +0.36% (p = 0.70), and dirty
  -0.89% (p = 0.56), within the bands, as the hosted set did (-0.50%, -0.70%). Described menus carry no memory envelope
  anymore; the receipts are under `Measurements/MenuDescriptions/2026-09-30/paired-local`.
- `TestMenuSurvivesAWindowClosingItFromSetCursor` runs instead of skipping everywhere. Since it was added (PR 29) it
  skipped in every run, local and x64 CI alike, as "another window covers the menu-closing window". On a developer
  desktop the cover was the desktop application's own window, which takes the foreground back while the suite runs and
  rises above the test's non-topmost window. The window is now topmost, and the skip names the covering window's
  class and process. The full Menu suite runs with no skip on an interactive desktop.
- Consumers can restore current pins again. #22 added measurement receipts with tracked paths of up to 176 characters.
  A consumer restores its pin under `<root>\.build\dependencies\DxUi\source\<commit>\`, and Git without long paths
  fails there once the full path passes 259 characters. RedXe's `main` CI has failed at restore since it pinned
  `6b34456`, and RedSalamander could not restore `a0b4934` from `D:\RedSalamander`.
  - Every tracked path is now at most 150 characters, so any consumer root of up to 35 characters restores.
    `validate-dependencies.ps1` rejects a longer tracked or untracked-but-unignored path
    (`Build_ToolchainAndConsumption`).
  - 767 evidence files were renamed, with their bytes unchanged: CI artifact directories dropped `native-`,
    `Performance-*` receipts keep 8 of their 32 run-id digits, and `associative-cache-rejected` became
    `assoc-cache-rejected`. Hash lists and manifests name the new paths, and each packet's README records the rename.
- The Menu suite's real-client focus fixtures (the click that activates a window UI Automation has or has not seen, and
  the reactivation of a seen window) no longer fail when another application takes the foreground (plan
  `SeenWindowReactivation_2026-09-30`, item 3). One failed a local Debug run while the desktop application and the Start
  menu took it. They now play each attempt with windows of their own through `RunUntilForegroundHeld`
  (`Tests/Controls/DxUiTestHelpers.h`), the multi-window form of `RunWhileForegroundHeld`, which is now written on top of
  it. A failed expectation records the first failure, with the names the client heard, instead of ending the test, and
  an attempt that another application took the foreground from, from any of its windows, is played again, five times at
  most. The attempt that kept the foreground decides the test, and one that never did records a skip naming the thief.
  Under `--foreground-thief` all three record that skip after five attempts, and never fail. Item 7's first version,
  announcing every move of a gain's turn, and removing the reactivation announcement each still fail their test.
- Activating a window UI Automation has seen announces its focused control (plan `SeenWindowReactivation_2026-09-30`).
  UI Automation answers the first focus event of a window with a call of the fragment root's `GetFocus`, and its later
  ones from whether the window's root element has the keyboard focus, which it does not while the focus is inside a
  control. The host announced nothing for what a gain focuses or restores, so switching back to such a window (Alt+Tab)
  was reported by nothing, and a screen reader did not say which control had the focus. The host now reads, as a gain
  begins and before it publishes anything, how many `GetFocus` calls have begun on the window. When any had, it
  announces the focused element once the gain's message-loop turn ends: the restored control, the one the activation
  focused, or the window when no control has focus. It does not when it already announced a move of that turn (the
  click that activates the window) or when the root element stands for its single focused control, which UI
  Automation reports itself. A window's first activation stays the answer of its first `GetFocus` call, with no
  duplicate. The message the host posts at a gain now names its turn, so the message of an earlier gain, which the
  window lost before the loop turned, no longer ends a later turn. The WindowHost suite tests the decision without a
  desktop, including a stale turn message and a single-control root. The Menu suite reactivates a seen window with an
  in-process UI Automation client, once restoring a control and once focusing the first, and requires the client to
  hear that control once. Five mutants each fail a test. API revision stays 2 (additive diagnostics accessor
  `ControlHost::DebugGetReactivationAnnouncementCount`; private members changed).
- Window-host UI Automation providers resolve their control without scanning the tree (plan
  `ReliabilityAndFollowUps_2026-09-30`, item 6): every provider call searched the published records for its control's
  path (several times per call, so a client walking every element of a window paid for the whole tree on each one), and
  every event searched the live tree for its control. Each snapshot now carries lookup tables built as it is published
  (records by path, hit rectangles by kind, path and item, and control addresses), so a provider call examines a few
  table slots and an event a search of the sorted addresses (about log2 of the records) and a walk down its path. In a
  window of 1,960 buttons a `Navigate` call examined 4,901 records on average (9,796 at most) and now 5.8 (21), a
  property read 2,942 (5,880) and now 3.5 (15), `get_BoundingRectangle` 2,942 (3,921) and now 3.4 (11), a pattern
  query 2,942 (5,880) and now 3.5 (15), and an event's control lookup 1,002 tree nodes (2,001) and now 17 (17); a tree
  of 245 buttons costs the same per call as one of 1,960. What a provider or event reaches is unchanged: the tables
  describe the snapshot's own tree, a control they hold is confirmed against the live tree before it is used, one they
  do not hold (added since the publish, hidden, no element) is searched for there as before, and stale elements still
  answer `UIA_E_ELEMENTNOTAVAILABLE`. What lives inside one control (a tree's items by id, a grid's rows and cells) and
  hit testing a point still scan. Test-only diagnostics count what resolutions examine, resolve an event's path and
  check the tables against a scan of the records.
- A click that activates a window announces the clicked control once (plan `ReliabilityAndFollowUps_2026-09-30`, item
  7): the click sets its control after the window's `WM_SETFOCUS`, in the same turn of its message loop and before UI
  Automation has acted on the system's activation focus event, and the host announced that move besides the event, so a
  client could hear the clicked control twice. What the event reports depends on what UI Automation knows of the window:
  the first focus event of a window it has not reported before is answered with a call of the fragment root's
  `GetFocus`, on whatever thread, which reads the published snapshot when it runs, and its later ones without
  `GetFocus`, from whether the window's root element has the keyboard focus, which reports nothing while the focus is
  inside a control (observed on Windows 11 build 26200). A window that gained focus now leaves a focus move of that turn
  to the event only while no `GetFocus` call has ever begun on it, and announces the move itself otherwise (at worst as
  a duplicate, never not at all): each call counts itself in the window's provider target before it loads the snapshot,
  and the host reads the count after it stores the snapshot of the move, all four operations sequentially consistent, so
  with the count at zero every call that follows loads the snapshot after the store and reports the clicked control. The
  host posts itself a message at the gain whose dispatch ends the turn (as does losing focus, or 500 ms without it,
  should a window procedure never hand it to the host); a click in the window that is already active, or any move in a
  later turn, is announced by the host as before. The WindowHost suite tests the decision without a desktop (a first
  gain's move left to the system and announced once a call has begun, a window asked before, the turn's three ends, and
  a call that a test gate holds on a thread of its own before it counts itself), and the Menu suite plays a click that
  activates a window with an in-process UI Automation client in both cases: a window UI Automation has asked before (the
  client hears the clicked control once, and last) and a first activation whose `GetFocus` call the gate holds until
  after the click (it hears the clicked control as often as it hears the control of a first activation, and never the
  control the activation focused on the way). A window procedure must pass the private message (`WM_APP + 0x06D`) to
  `HandleMessage`, as it does the accessibility ones. API revision stays 2 (additive diagnostics accessors
  `ControlHost::DebugIsInFocusGainTurn` and `DebugGetFocusMovesLeftToSystemCount`; private members changed).
- A hung control test ends the run with its name instead of holding a CI job (plan `ReliabilityAndFollowUps_2026-09-30`,
  item 12). On the pull-request run of PR 30 the x64 ASan Debug job printed nothing for the 35 minutes between starting the
  Menu suite and the job's 40-minute limit, and its log named no test. Every control test a suite runner starts through
  `DXUI_RUN_TEST`, and every fixture suite without named tests, now runs under a watchdog: one thread waits for the deadline
  on a condition variable (it never polls) and, when a test outlives it, writes `TIMEOUT: <TestName> after <N> s` and
  terminates the process with exit code 124, since a stuck test cannot be unwound. The deadline is 300 s, forty times the
  slowest of the 816 non-foreground tests under AddressSanitizer (7.5 s); `DxUi.ControlTests.exe --test-timeout=<seconds>` and
  `test.ps1 -TestTimeout <seconds>` set it and 0 turns it off. Every run prints its deadline, every `[DONE]` marker carries the
  test's duration, and `test.ps1` reports a failing suite's exit code and `TIMEOUT:` line beside the last lines of its log
  (`Tools/SuiteFailure.psm1`) and prints every capability skip under its suite as `skipped <Test>: <reason>`, so a CI log
  shows which tests a missing desktop or foreground left unrun without the uploaded suite log. The three resource fixtures report each sample, so their deadline bounds a cycle. A hidden
  `--watchdog-self-test` switch runs a test that never returns, and `Tools/tests/Test-TestWatchdog.ps1` asserts the exit code,
  the line and the time, and that with the watchdog off the same test still hangs. The audit of the Menu, NativeTextInput and
  resource suites found no unbounded polling loop, `INFINITE` wait or UI Automation wait without a deadline, but two ways a
  test could still hang: a driver thread that gave up before it found its popup left the owner thread in `ContextMenu::Show`'s
  modal loop for good (every driver now starts with `DismissMenusIfDriverFails`, which a new test with a driver that fails
  at once requires to close the menu, and a source scan requires in every driver), and a UI Automation client thread was
  joined after a pumped wait of 3 s although its teardown may need the pumping thread (the Menu and Accessibility tests
  that join one now wait as long as its setup was allowed, 20 s, and fail the test). Tests and tooling only: no library code
  changed, and the five gallery sheets are byte-identical, so `docs/gallery` needs no update.
- A `Grid` that stops painting returns its retained text layouts (plan `ReliabilityAndFollowUps_2026-09-30`, item 9):
  hidden itself or under a hidden ancestor (an unselected tab page, a collapsed panel or page host), in a hidden
  embedded view, detached from its host or given another model, it releases its layouts, their key strings, its tables
  and its ellipsis sign at once, and showing it lays out only the cells it then shows, once. Until now such a grid kept
  its last paint's layouts until a next paint that might never come. A painting grid holds one layout per cell it drew
  last, however far it has scrolled (28 in 32 entries for a 6x4 multiline test grid, 66 in 128 for 10x6, 44 in 64 for
  ten single-line rows) with 6 to 12 KB of key strings and 5 to 14 KB of tables; the layouts are the bulk, about 20 KB
  of process heap each (570 KB for the 6x4 grid, 934 KB for the single-line grid, measured in Release as what a hide
  returns). Painting, the layouts' lifetime while painting and its allocations are unchanged. The hook is a protected
  `Control::OnHidden`, which panels and page hosts forward; a hidden or minimized native window keeps its layouts.
  With this attribution the developer accepted the retention the review follow-ups introduced for their frame rate
  (`Default` dirty rate +11.3%, p = 0.029, over both 2026-09-29 sets): the performance contract's accepted Grid layout
  retention bounds it to one layout per cell of the last paint, and none while the grid is not painting.
- Publishing `docs/gallery` after a merge is no longer a manual copy (plan `ReliabilityAndFollowUps_2026-09-30`, item
  11): the manual `Publish docs gallery` workflow (`.github/workflows/gallery.yml`) regenerates the gallery from a native
  x64 Release build with `gallery.ps1 -PublishDocs`, validates the specifications and commits it to the branch it runs
  on. Like the formatting workflow's apply mode it runs only on `workflow_dispatch` with an explicit boolean input, holds
  `contents: write` for that one job, pushes normally and never forces, and never runs on `pull_request_target`. It
  commits only when a sheet, the HTML index or the README changed (`generation.json` records the source commit and so
  differs after every commit); `Tools/Commit-Gallery.ps1` makes that decision and is tested against fixture repositories.
  `docs/gallery` itself was last rendered on a hosted runner from `ed1dea9`, before PR 29's visual changes; it is
  regenerated here on the developer machine from the final sources (`main` and this tree render byte-identical sheets
  there), and the design system is republished with those sheets and the guidelines PR 29 changed.
- One validation entry point (plan `ReliabilityAndFollowUps_2026-09-30`, item 10): `validate.ps1` runs the five
  validators and the tooling tests, each in its own process, reports every failure before it fails, and is what CI's
  validation job runs; `test.ps1` now also runs the tooling tests. The validators' file scans no longer enter a nested
  git checkout (a directory other than the scanned root holding a `.git` file or directory, such as an agent's worktree
  under `.claude/worktrees`): validating a main checkout counted every worktree's copy of the Markdown (1,069 files
  instead of about 214), so a half-edited worktree could fail it. Every tooling test script now sets strict mode itself,
  which a tooling test requires: `test.ps1` is strict and the scripts it calls inherit it, so the new watchdog test passed
  11 of 11 cases alone and failed 6 under `test.ps1` (`.Count` on a function's output unrolled to one line or none).
- Paired sets can establish a result on a noisy machine (plan `ReliabilityAndFollowUps_2026-09-30`, item 4):
  `performance-paired.ps1 -Repetitions N` (default 3, at most 10) repeats the interleaved A, B, B, A pass, so each side
  has 2N runs, and judges every phase and metric on all runs at once: an exact two-sided Mann-Whitney U test (counted
  over the observed ranks, ties included, no approximation) of the baseline run medians against the candidate's, with
  the median shift and the metric's band (5% timing and FPS, 2% process memory). A metric is `regressed` or `improved`
  only when p < 0.05 and the shift exceeds the band, and any rise in an exact budget (surface bytes, replacement peak,
  allocations) in any candidate run is `regressed`; a set with a regressed metric is `advice-required`. Same-binary
  spread is reported, not a veto. Six runs against six reach p = 0.0022 when completely separated; two against two
  cannot reach 0.05. The first pass keeps its receipt and comparison names, and `Compare-PerformanceSet` in
  `Tools/PerformanceComparison.psm1` is tested on synthetic receipts. The performance contract and guide state the rule.
- `performance-paired.ps1` compares two trees (plan `ReliabilityAndFollowUps_2026-09-30`, item 5): `-BaselinePath` and
  `-CandidatePath` measure existing DxUi working trees as they are, uncommitted work included, with this checkout's
  harness written into them for the run and their files put back afterwards. A pair with a named tree is refused when
  the two library source fingerprints are identical instead of when the commits are; revisions keep the commit rule.
  `summary.json` records each side's revision or path, commit and fingerprint. The receipt's source fingerprint and
  benchmark input list moved unchanged into `Tools/PerformanceComparison.psm1`, and `Tools/PairedRun.psm1` with its
  tests owns tree selection, the refusal rules, named-tree validation and the harness overlay.
- A moved control is told what it now inherits (plan `ReliabilityAndFollowUps_2026-09-30`, item 8). A control's flow
  direction and density are inherited through its parents and a parent's own change is announced to its children, but
  a move (`ControlHost::SetRoot`, `PageHost::SetPage`, `Panel::AddChild`) announced nothing, so a control that keeps an
  arrangement for them kept the one of its old place: a `ColorPicker` (its steppers, hex field and buttons), a
  `NumericStepper`, a `TabControl` (its header, unless its host changed too) or a horizontal `StackPanel` moved out of
  a right-to-left parent stayed mirrored, and a `Tree` or `Grid` moved between densities kept its row metrics.
  `Control::Reparent` (which `SetParent` calls, and `SetRoot` and `SetPage` use in place of `SetParent` plus
  `PropagateHost`) compares what the control inherits before and after and announces `OnFlowDirectionChanged` /
  `OnDensityChanged` once, in its final place, only when a value differs: adding a child that inherits what its parent
  has, replacing a root and clearing a panel announce nothing, while a child added under a right-to-left or compact
  parent, and a root arriving in a compact host, now hear what they inherit. `ColorPicker` also releases its gradient
  brushes and their Direct2D device reference when its host changes instead of holding the old host's device until its
  next paint, and `Panel::ClearChildren` skips the empty slot a child moved out through `GetChildren()` leaves (it
  dereferenced it). The menu bar and the text field key their layouts on these inputs and were never stale. API
  revision stays 2 (additive diagnostics accessor `ColorPicker::DebugHasCachedBrushes` and protected
  `Control::Reparent`).
- The described-menu memory check is deterministic. `DebugGetContextMenuResources` (a test-only diagnostics hook) counts
  the live menu popups, the text layouts their described rows hold and the accessibility records of menu-popup
  snapshots, exactly and whatever the renderer and the allocator keep; the fixture asserts that all three return to
  their value before the menu opened after it closes while a client holds eight row elements. It no longer bounds the
  process heap, which a software renderer's surfaces and caches (WARP, or the Basic Render Driver of a GPU-less
  runner) swing by up to about 3 MB, and it now runs under AddressSanitizer too. A window-host provider that pins a
  snapshot, a target that keeps its last snapshot after the window closes and a popup that is never freed each fail it.
- NativeTextInput's focus fixtures survive another application taking the foreground. Windows then deactivates the
  window and the host releases its native text session, TSF document included, as designed; the desktop application
  hosting a session did so 30-95 ms after each test window activated and failed the TSF document fixture in three of
  six local runs. The four fixtures that pump after taking focus repeat their sequence (`RunWhileForegroundHeld`, five
  runs at most) until no application takes the foreground and then make exactly their former assertions; when one takes
  it every time they record a capability skip naming its executable, and a regression with the foreground held still
  fails. Two deterministic fixtures deliver the takeover as Windows sends it, and `DxUi.ControlTests.exe
  --foreground-thief[=<minMs>,<maxMs>]` reproduces the desktop application on demand.
- The control-test runner runs single tests: `DxUi.ControlTests.exe --suite=<Suite> --test=<Name>[,<Name>...]` and
  `test.ps1 -Suites <Suite> -Tests <Name>[,<Name>]` run only the named test functions of the suite. Every suite runner
  registers its tests as `DXUI_RUN_TEST(TestName);`, which checks the filter and prints the test's `[START]`/`[DONE]`
  markers. A name no selected suite registers, a malformed list or a fixture suite without named tests fails the run
  (exit code 2) instead of passing with nothing run, and a filtered `test.ps1` run keeps its own log and receipt
  (`*.filtered`), so it never replaces the receipt of the whole suite.
- Repository tooling is PowerShell only (plan `PowerShellTooling_2026-09-30`): the spec, skill, dependency,
  inherited-test and build-matrix validators, the performance comparator and their tests are PowerShell modules and
  scripts, so `test.ps1`, the validators and CI need no Python, pip or PyYAML. The comparator reproduces every stored
  paired comparison under `Measurements` exactly. New root entry points `validate-test-port.ps1` and
  `validate-build-matrix.ps1` join the other validators, `Tools/tests/Invoke-ToolingTests.ps1` runs the tooling tests,
  and `Tools/Install-ClangFormat.ps1` installs the checksum-pinned formatter that `format.ps1` now finds first.
- Follow up the review's reported items as decided (plan `ReviewFollowUps_2026-09-29`):
  - Multiline `Grid` cells keep their shaped layouts in 32-way set-associative tables keyed by a library-owned mixed
    hash of the value and layout box, growing (to 16,384) while the values of the current and the previous paint crowd
    a set: a repaint of unchanged cells shapes nothing and a one-row scroll either way shapes only the entering row
    (scrolling up no longer evicts the rows still in view), for any number of distinct visible values. Values whose
    visible text is the same share its omitted-tail layout through a keyed table instead of a scan of every entry. Key
    and display strings reuse power-of-two storage, so dirty frames allocate no more than the direct-mapped cache did.
    A value shapes only what it can show, wrapped or on one line: a 100,000-unit cell lays out a few hundred units
    (243 at `SetLineClamp(1)`, where the old prefix never stopped; 971 at a clamp of 1,000 in a short row, sized by the
    lines that fit), and it is retained by that prefix, so its repaint and hover shape nothing and another value
    starting the same way shares it. Keys round layout boxes to 1/64 DIP, so rows at heights such as compact density's no
    longer miss after a scroll. The omission marker takes the direction of the text it ends, read by character (an
    emoji ending Arabic text no longer sends it right), and trailing lines of other spaces or default-ignorable
    characters add nothing. A `MultilineGridDistinct` benchmark scene gives every cell its own text.
  - Single-line `Grid` cells keep their caption layouts in the same table instead of laying the caption out on every
    paint and every hover, with pixels identical to the previous centred-text draw; a long leading-aligned caption
    shapes only the prefix its cell can show (363 units for a 100,000-unit caption over three paints).
  - Window hosts raise UI Automation focus changes as focus moves inside their focused window, naming the focused
    tree item or grid row as GetFocus does, and the window itself once no control has focus; a window that lost the
    foreground announces nothing, and the window's own activation is left to the system's focus event. An element's
    SetFocus moves logical focus before Win32 focus, and UI Automation's SetFocus no longer holds the accessibility
    mutex while the host runs callbacks and raises events. A window-host element (tree items and grid rows too) whose
    control was removed or replaced at the same path reports `UIA_E_ELEMENTNOTAVAILABLE`, with runtime ids from a
    per-process serial that no later control reuses, and such a republish, a whole-tree `SetRoot` included, raises
    StructureChanged. A child of a collapsed status root reports its own events. A focus callback that removes its
    control leaves no focus behind (a rootless host keeps a live one); a control disabled or removed while focused
    loses focus, published, at the host's next message; a republish never touches a focused control its panel removed.
  - While a menu is open its popups show the arrow cursor and a window of the menu's thread outside them chooses its
    own; closing lets that window choose again (except when the owner's destruction closes it). Closing a described
    menu returns all its memory, even while a client holds its row elements.
  - A disclosure's chevron rotation starts at its first animation tick (so a change made while hidden rotates once
    shown) and a gap in the ticks pauses it; unsupplied alert colors keep distinct, readable default tones outside high
    contrast, and a derived partner is black or white by contrast measured as the pair paints (4.5:1 for any supplied
    fill, translucent ones over the window background included);
    right-to-left tab titles, `NumericStepper` labels and units and the `ColorPicker` hex caption read right to left
    from their start; `NumericStepper::SetStepButtonNames` (default "Increase" / "Decrease") and `ColorPicker::Labels`
    step-button names, caption and swatch widths (bounded to 4,096 DIPs) serve translated captions.
  - Source-breaking renames drop the repeated namespace: `IGridModel`, `IGridDelegate`, `ITreeModel`, `ITreeDelegate`,
    `RunModalLoop`, `IsRenderStageActiveForDebug`, `EmitRenderMutationBlockedForDebug`, and in `DxUi::Typography`
    `Spec`, `GetSpec`, `PerfEmitter`, `GetPerfEmitter`, `SetPerfEmitter`, `EmitPerfCounter`, `FontFamilyCacheEntry`,
    `TextFormatCacheEntry`, `GetMeasurementCacheMutex`, `GetFontFamilyCache`, `GetTextFormatCache`,
    `kFamilyCacheMissMetric` and `kTextFormatCacheMissMetric`. No aliases remain; consumers rename when they move
    their pin. API revision stays 2, as for the earlier modal-loop rename.
- Fix defects found by reviewing the last fifteen days of changes (plan `ReviewFixes_2026-09-29`):
  - Captured drags: the embedded rule resolves the capture through the live tree before touching it (a destroyed
    captured control was read after free) and requires every ancestor to stay visible and enabled. WindowHost now
    cancels a drag whose control or ancestor is disabled or hidden (`OnCaptureLost`) instead of dropping the capture
    silently and leaving the control mid-drag.
  - `Splitter`, `NumericStepper`, `ColorPicker` and `TextField`'s focus bar and button hovers skip a draw when no
    solid brush is available, as the shared helpers do. `Splitter` keeps its separator inside an extent smaller
    than both pane minimums and is exposed to UI Automation as a Thumb with RangeValue.
  - `ColorPicker` keys its gradients by a retained `ID2D1Device` (a recreated device context could reuse the cached
    address and fail every preparation with D2DERR_WRONG_RESOURCE_DOMAIN). The gradients are hue-independent and
    placed by transform, so hue drags and moves create no resources. The field being typed in keeps its text, caret
    and undo history; a canceled component edit is followed; Escape in the hex field cancels; runtime flow changes
    re-arrange the fields; right-to-left flow mirrors the saturation axis the keys already mirrored.
  - `NumericStepper` ends every previewed edit with Commit or Cancel (an unparsable edit reverts with Cancel),
    commits text changed without focus (UI Automation), re-rounds on `SetDecimals`, never sticks on a step finer
    than its decimals, reads full-width and Arabic-Indic digits, and rewrites its field only when the text changed.
  - `Tree` row drags never target the dragged row's own subtree, follow model changes and wheel scrolls, decide the
    drop at the release point, cancel on a second button, `SetModel` or `SetReorderEnabled(false)`, treat id 0 as an
    ordinary row, paint without a per-frame id scan and repaint only when the drop changes. Row `iconText` again
    uses the UI font unless it is a private-use glyph (letter icons had rendered as missing-glyph boxes).
  - Multiline `Grid` cells no longer draw with `D2D1_DRAW_TEXT_OPTIONS_CLIP` on their fractional layout box (it cut
    the last line's descenders and accounted for most of the multiline memory growth: dirty-round private bytes fall
    about 5.5 MB on the MultilineGrid fixture); trailing NEL/VT/FF and blank tail lines add no ellipsis; any
    trimming offers the tooltip; and a scrolled single-line caption offers its full value.
  - Described menus paint Info-row descriptions, render color glyphs, raise UI Automation focus changes as the
    keyboard moves through rows, and a UIA SetFocus on a popup root no longer dismisses the menu. Window-host
    UIA event providers no longer pin a full accessibility snapshot each while a client listens.
  - Right-to-left `Checkbox` and `RadioButton` captions sit beside their indicator. `ProgressBar` rejects non-finite
    values and an empty indeterminate bar requests no frames. `Slider` no longer drops a change as small as its
    smallest step.
  - `performance-paired.ps1` judges same-source controls two-sided (`unstable-control`), fixes `-SkipBuild` and
    annotates findings in CI; `performance.ps1` never re-stamps a stale receipt and keeps one default receipt per
    scenario. API revision stays 2 (additive; private members and one Tree debug field changed).
- Honor reduced motion in `ProgressBar`: an indeterminate bar rests its segment centered, requests no animation
  ticks and paints identically every frame; restoring motion resumes the 2 s sweep. `Tick` re-seeds when the tick
  clock moves backwards and drops whole loops from long gaps, so no elapsed time can corrupt the sweep phase.
  API revision stays 2 (additive diagnostics accessor only).
- Never paint status invisibly: `MakeThemePalette` still copies supplied alert colors, but a zero-alpha alert
  color, including the `ThemeColors` zero default, falls back to `windowBackground` for fills and `text` for
  text. High-contrast themes without alert colors now show toned grid rows, tone badges and throughput limits.
- Add the editor consumer controls `Splitter`, `NumericStepper` and `ColorPicker`: preview/commit/cancel notifications,
  keyboard operation, right-to-left mirroring, disabled and focus-visible states, and the public `HsvFromArgb` /
  `ArgbFromHsv` / `ParseHexColor` / `FormatHexColor` helpers. `WindowHostCursorKind::VerticalResize` maps to the
  vertical resize cursor. Catalog/factory count is 30. API revision stays 2 (additive).
- Paint `Slider` like the Windows volume flyout: a 6 DIP capsule track (fill and remainder the same thickness), a fixed
  20 DIP gray chrome disc, and an accent inner thumb that matches the track at rest and grows to 16 DIP on hover
  (12 DIP pressed). Pointer hit testing stays an unpainted 48 DIP band with a 24 DIP grab radius. Embedded
  `DispatchPointer` synthesizes control double-click (word selection) using the system interval and a 16 DIP slop.

- Release the embedded surface on `SetVisible(false)` and zero-extent `Prepare`: `surfaceBytes` reports 0 while
  hidden or zero-sized, and the next visible sized preparation reallocates exactly one surface with identical pixels.
  `AdvanceAnimation` no longer marks the view dirty unconditionally; every `Tick` that changes visual state
  invalidates (caret blink flips, button/page transitions, tooltip show/hide, grid/tree/menu animation), and
  `ControlHost::RequestAnimation` wakes an embedded view only when animation becomes requested, so paint-time
  requests (indeterminate progress, busy grids) no longer re-dirty the view inside every preparation. Bound the
  per-host solid-brush (256) and configured-text-format (96) caches with a trim at preparation/paint start, report
  `cachedBrushes`/`cachedTextFormats` in `EmbeddedStatistics`, and gate the complex benchmark at zero clean-round and
  64 (Release) / 320 (Debug) dirty per-frame C++ allocations. API revision stays 2 (additive).
- Add `PageIndicator`: a bottom strip of dots for paged surfaces. Fewer than two pages paint nothing and are not
  hittable. Click, Left/Right/Home/End and `SetSelectedIndex` share one selected index; only user input fires
  `SetOnSelected`. Catalog/factory count is 27.
- Deliver API revision 2 through one DxUi.lib: public controls, a 26-control catalog/factory, neutral themes and
  diagnostics, and native plus supplied-device embedded hosting. Foundation is part of the same archive.
- Separate dirty preparation from allocation-free D3D11 composition; support logical capture, DPI, visibility,
  device replacement, shared device pools, and slider preview/commit/cancel notifications.
- Add a public toggle/slider consumer, relocated exact-pin consumption tests, five-theme all-control gallery,
  supplied-device WARP regressions and all 853 reusable inherited runtime cases. Record the 88 exclusions.
- Add revision-checked embedded text snapshots, application-side TSF/clipboard services, and lazy embedded UIA
  attach with synthetic tests. Close library extraction and the first RedXe pin/synthetic adapters (`redxe-adapter`).
  Matched performance and real IME/AT remain RedXe AV gates.

- Make this repository the canonical home of DxUi: editable src/Controls and Tests/Controls, neutral namespace,
  one frame runtime, historical provenance metadata and explicit pending dependencies; remove the duplicate tree.

- Bootstrap independent private repository, guidance, skills, design contracts, validators and build/CI entrypoints.
- Preserve source/test provenance from RedSalamander and extract application-independent frame-runtime foundation.
- Plan RedXe-first control/embedded-host integration; retain RedSalamander migration as a separate HOLD plan.
