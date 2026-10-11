# Running only the tests your change affects

```powershell
./Test-Changes.ps1 -Explain
./Test-Changes.ps1 -Configuration Debug
./Test-PrePush.ps1 -Explain
# After reviewing the six-profile plan:
./Test-PrePush.ps1
./Test-Changes.ps1 -Mode Full -Configuration Debug
```

The default uses local merge-base changes plus staged, unstaged and untracked work. Read the explanation before a large run. Unknown code/build inputs widen coverage. Selected work is partial evidence. PR delegation is pending CI work, and only an enabled workflow with matching reviewed candidate bytes/profile can justify it.
An empty plan reports `NO_WORK; repository NOT_EVALUATED`; a fully delegated plan reports `NO_LOCAL_WORK; CI_PENDING`.
Capability skips are reported explicitly and prevent native-suite receipt reuse. `FULL_SCOPES_PASSED` covers the scope
manifest, while foreground, consumer, gallery and native-platform qualification remain separate obligations.

Native test files use `Scope.Tests.Something.h/.cpp`; register new active files in `native-test-files.json`. `test-scopes.json` owns standalone scope/PR metadata. Shared-library or runtime byte changes conservatively invalidate local standalone reuse. No consumer dependency pin changes are part of this workflow.

For independent resource investigations, force fresh execution and retain paired fixture/build provenance. Timing, native ARM64, real devices, IME/assistive technology and foreground input remain separate qualification requirements. These commands do not manufacture those claims.
The `Control` scope includes intrinsic measurement/Flex/form correctness, the localized form matrix and the
256-field inventory/cache boundary. `Accessibility` includes native/embedded form provider and notification
tests. `performance-forms.ps1` supplies a separate candidate-only synthetic resource diagnostic; its populated
pixel/cache preflights and lifecycle observations do not replace these correctness scopes or consumer tests.

`performance-profile.ps1 -Configuration Release -Platform x64 -Scenario Default` runs a separate, fixed startup/CPU/cache
diagnostic; repeat with `-Scenario MultilineGrid` and optional `-Heap` in a declared bounded serial pilot. Reports retain
phase query failures, bounded worker-thread inventories, raw frame samples and sampling cost. The guarded runner limits
only its owned child to 120 seconds and requires exact v2 build/source/tool/dependency/artifact identity. Its 20/100/200
warm-up checkpoints and heap-off/on observations do not change canonical performance acceptance or qualify the CI policy.
UIA client regressions fail a quiet-wait timeout and retain client/provider timelines for the unchanged Grid selection
case. A quiet interval and a passing repeat do not establish the cause of a previously delayed event.
The shared UIA client's owner waits wake for incoming COM messages with a bounded message wait. Sender caching and
the three-second notification deadline stay unchanged. The integrated form regression checks all four presentation
properties, unchanged-state silence and clearing the state; bounded owner attempts and client arrivals remain
failure diagnostics. The retained comparison isolates owner-message dispatch, without qualifying unrelated UIA delays.
The [retained UIA investigation](../Measurements/UiaSelection/2026-10-10/README.md) also supplies a fifteen-case
source-only corruption replay. It checks the archived reader without launching native tests; use a fresh output
directory and keep its synthetic cases separate from runtime evidence.
```powershell
./Test-Changes.ps1 -Scopes <name> -SkipBuild
./Test-Changes.ps1 -Mode Full -Force
./test.ps1 -Full
```

Repeated unchanged scopes print REUSED. Force reruns them. SkipBuild requires an attestation established by a prior Test-Changes build; stale binaries are rejected. Explicit test.ps1 suite/named-test calls retain the lower-level diagnostic interface. Its ordinary call now routes to affected iteration; Full is explicit. An unknown local comparison ref fails with Git's diagnostic rather than silently selecting no work.

PrePush lists the separate focus-taking and menu-resource suites as `INTERACTIVE_NOT_RUN`; run those only with `test.ps1 -Interactive` after the person at the desktop agrees to the time. Capability-skipped scopes remain partial evidence and do not create reusable receipts.

The zero-size NativeTextInput fixture prints bounded `[FOCUS-SETUP]` observations after its guarded setup sequence.
Activation terminal branch, native/logical focus, session/TSF state, host/client dimensions and existing counters
distinguish initial setup from the later resize assertion. `sequenceHeld` describes the complete guarded sequence;
stage reads are sequential observations, not message history. A missing foreground-loss message does not prove
initial acquisition. Setup failure remains a failure, and retries retain all earlier attempts and their exact bytes.

Verified GitHub-hosted Windows jobs run requested foreground suites through the same restoring lease with a parent
anchor and a child-specific foreground grant. Both the PowerShell launcher and native `--run-hosted` mode check the
exact hosted markers before taking that runner's desktop. Hosted receipts use `.hosted` and record lease results;
ordinary local and self-hosted runs still require the person's time agreement through `-Interactive`, followed by
the native Start confirmation unless automatic start was explicitly approved.
When the person has explicitly approved automatic start and the test time, add `-InteractiveAutoStart`:

```powershell
./test.ps1 -Configuration Release -Platform x64 -SkipBuild -Interactive -InteractiveAutoStart -Suites Menu,NativeTextInput,MenuResources,MenuResourceScaling
```

The visible dialog defaults to Start and starts after 30 seconds if nobody clicks. Cancel, Esc, closing the dialog
and Ctrl+C stop it. Ordinary `-Interactive` keeps its two-minute Cancel default. Both modes use the same warning,
desktop refusal and verified restoration. Automatic receipts record the native mode/countdown and distinguish
`autostarted` from a clicked `started`; missing or mismatched automatic metadata fails the parent run.
The printed summary uses all three restoration statuses. A false `runMovedSomething` flag or exit code 0 without a
result cannot establish an unchanged desktop; saved/exit pointer coordinates alone do not identify what moved it.
If a verified hosted runner refuses ordinary warning activation, the lease can briefly clip its warning to a marked
input patch and click that owned point. It rechecks authorization/targeting, balances and observes the mouse release,
rejects capture on the foreground or lease thread, uses an opaque nonlayered patch, and restores and verifies the full
warning style, layered attributes and shape before launching any child. Refused targets retain read-only geometry
and hit-window diagnostics. Failure remains a failed run. This fallback
does not run on a person's ordinary local desktop; its private-desktop tests take no personal input or focus.
