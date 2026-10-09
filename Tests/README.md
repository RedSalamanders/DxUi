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
```powershell
./Test-Changes.ps1 -Scopes <name> -SkipBuild
./Test-Changes.ps1 -Mode Full -Force
./test.ps1 -Full
```

Repeated unchanged scopes print REUSED. Force reruns them. SkipBuild requires an attestation established by a prior Test-Changes build; stale binaries are rejected. Explicit test.ps1 suite/named-test calls retain the lower-level diagnostic interface. Its ordinary call now routes to affected iteration; Full is explicit. An unknown local comparison ref fails with Git's diagnostic rather than silently selecting no work.

PrePush lists the separate focus-taking and menu-resource suites as `INTERACTIVE_NOT_RUN`; run those only with `test.ps1 -Interactive` after the person at the desktop agrees to the time. Capability-skipped scopes remain partial evidence and do not create reusable receipts.

Verified GitHub-hosted Windows jobs run requested foreground suites through the same restoring lease with a parent
anchor and a child-specific foreground grant. Both the PowerShell launcher and native `--run-hosted` mode check the
exact hosted markers before taking that runner's desktop. Hosted receipts use `.hosted` and record lease results;
ordinary local and self-hosted runs still require the person's confirmation through `-Interactive`.
