# API 4 consumer background-process capture

Status: **ACTIVE**. This is a tooling-only maintenance backport, not API 5 adoption or completed qualification.

## Scope and protected source

Base: `4cdf1ce52c7785446698e346b8aec058ed6a9283` (API revision 4), isolated branch
`ej/dxui-api4-consumer-capture-20261115`. The active main/API 5 checkout is not changed.
Preserve every C++ implementation/header, capabilities/API revision, the four public
`Get-DxUiConsumerBuildIdentity` parameters, sole module export and identity/hash algorithm.
Changing the adopted producer commit intentionally changes the source identity.

Use a private PS5.1-compatible captured process boundary inside standalone `ConsumerBuild.psm1`.
Co-versioned VisualStudio discovery, dependency installer and pin validator may reuse that private
boundary; no consumer repository import, public helper API or identity reimplementation is introduced.
The existing scoped Git process adds its no-window flag. Metadata stays silent and separates streams;
background installer output remains live. Ordinary interactive builds and genuine console inheritance
are preserved. These bounded launch-site fixes do not establish which process created the historical
terminal foreground surface and do not claim all vendor descendants obey the same policy.

The lasting contract belongs to [Build toolchain and consumption](../../Build/Build_ToolchainAndConsumption.md).
The [version 6 carry-forward guide](../../Build/Build_ConsumerProcessCapture_Version6_Integration.md)
is a separate implementation/qualification checklist; it does not mark that port as implemented.

## Work and evidence gates

- [x] Verify exact base, API 4 and isolated worktree without altering active producer work.
- [x] Implement private captured queries/live dual streams and the bounded producer-owned caller closure.
- [x] Preserve primary failures and propagate direct/wrapped fatal exceptions at fallback boundaries.
- [x] Review standalone fixture adapter and retained existing-parent ownership before execution.
- [x] Execute focused safe controls under Windows PowerShell 5.1 and PowerShell 7, including literal
      arguments, separate full pipes, live delivery, exits, cleanup, batch metacharacter CWD, unchanged
      identity semantics and selected evaluation-only MSBuild metadata. Retain first failures honestly.
- [ ] Run owning validators, focused tooling suite and canonical scoped Explain; record actual selected
      scope without treating unmatched consumer-module changes as Tooling-only coverage.
- [x] Fold only this change's fragment using the canonical producer command before PrePush/final handoff.
- [ ] Complete reviewed producer qualification and maintenance branch/PR delivery with exact-source evidence.
      Tooling checks do not replace the six native producer/relocated-consumer profiles.
- [ ] Adopt the qualified exact commit in the separate RedSalamander PR and rebuild/re-attest/qualify
      its affected workflows and product. No consumer pin changes occur implicitly from this producer work.
- [x] Complete the checked-in version 6 carry-forward guide without changing that line's implementation,
      runtime/API or unified CI in this maintenance branch.

## Component checkpoint (2026-11-15)

The uncommitted API 4 maintenance snapshot based on `4cdf1ce52c7785446698e346b8aec058ed6a9283` passed
17/17 component controls on Windows PowerShell 5.1.26100.9711 and 17/17 on PowerShell 7.6.9.
Each host performed two actual x64 evaluation-only MSBuild identity observations, with STL annotations
enabled and disabled; all 58 recorded native starts used the guarded no-window/captured-pipe boundary.
The two annotation-policy fingerprints matched across hosts. Owned-child observations reported no
visible console window and PIPE stdout/stderr. This does not establish global broker absence or the
creator of the historical terminal foreground surface.

The tested component hashes are ConsumerBuild
`d6a359c62ca659fc124e84a97556a20089c6caefc6d5766d483181bf433e6b6e` and Test-ConsumerBuild
`b1e52d9cae0045f5eb4c3c1a75d8f8b53b44919368d3eaf4bb558be4129ddbeb`.
The accepted private component proof is hash-pinned as
`14cabb2afc52c896417dda1157196a2270b475b479626fe36db341b894fdff74`.
Raw paths, arguments, environment, SDK metadata and diagnostics remain private; only each host's closed
summary/manifest publication pair is eligible for publication.

Earlier failed fixture/bootstrap attempts remain retained as failures. These passing component controls
qualify neither a future producer commit nor native C++/hosted/relocated-consumer profiles or the
RedSalamander pin update. Governance, producer qualification, maintenance delivery and adoption remain
pending. The version 6 guide is authored; its implementation and qualification checklist remains open.

### Bounded local governance checkpoint

Across the local checkpoint, 34 process controls and 29 selected CiGate/changelog controls passed, along with
the owning source validators. The actual affected Explain invocation selected all 20 scopes and reported
zero INTERACTIVE_NOT_RUN entries. A separate pure-API PrePush/Full planning derivation selected 20 scopes
and four interactive obligations as NOT_RUN; no actual PrePush Explain invocation was run. These were
planning observations, not native-suite execution.
The full Tooling aggregate, native/hosted producer qualification, maintenance publication and RedSalamander
adoption remain pending. Earlier failed fixture and governance wrappers retain their original verdicts.

## First hosted candidate and follow-up (2026-11-15)

The first committed candidate is `78c685830e569f70521d696eda9d198fbf4c23fd` (H), published in
[draft maintenance PR 256](https://github.com/RedSalamanders/DxUi/pull/256) against the exact API 4 base.
Its [manual exact-H cohort](https://github.com/RedSalamanders/DxUi/actions/runs/43716379570) is separate
from the [PR cohort](https://github.com/RedSalamanders/DxUi/actions/runs/43716357670), whose checkout is
synthetic merge `4ec5a04b9fe730a24046b9651a849f800a302be8` (T). An API run's reported head H is not
evidence that a pull-request job checked out H rather than T.

Both Ubuntu validation jobs failed. Their public consumer-control summaries recorded zero cases and
zero starts with a control-failure error digest; the available hosted evidence does not retain the
actual exception, so its cause is unknown. Zero recorded rows do not establish zero process creation.
The aggregates also exposed a
missing ConsumerBuild measured-input/Full harness-identity dependency and an outdated native-scope
guard that rejected the exact public failure-evidence upload. Both Windows tooling jobs failed these
two guards. Their ordinary PowerShell 7.6.9 aggregate consumer controls passed 16/16 on H and T, with
25 recorded starts per run and no visible owned console/PIPE stdout and stderr. The later explicit
selected-MSBuild 17-case controls for PowerShell 7 and 5.1, and the ASan smoke step, were skipped after
the earlier failure. These aggregate observations do not replace the earlier standalone controls or
establish current hosted acceptance.

At 13:16 UTC, the manual cohort's x64 Debug job had reached its relocated-consumer step, x64 Release
and ASan Debug were in native qualification, and the ARM64 profiles were still building. These are
progress observations, not terminal native or consumer passes. H is unqualified and must not be adopted.

The follow-up restores the measured and harness dependency closures, narrows the upload exception,
and records process starts before fallible platform metadata. Its public setup diagnostics expose
only closed phase/type/HResult fields; detailed failures remain private. The post-start recording
change addresses an observability gap, not a proved cause of the historical Unix exception. At that
checkpoint, fresh follow-up component, tooling, native, hosted and consumer-adoption qualification was pending;
the earlier 34 process and 29 governance controls remain historical evidence for their tested bytes.

The first focused follow-up attempt passed all four selected source cases, then Windows PowerShell
5.1 passed 16 of 17 controls with two actual identity observations and 29 actual starts. The factory
control's first pure fake used a throwing ScriptProperty; that host suppressed its intended getter
failure and a null-method error surfaced instead. PowerShell 7 was not launched. The failed raw run
and fixture remain preserved; neither the 16 passing controls nor the four source passes relabel its
failed verdict. The fixture correction uses exact held-Process getter MethodInfos and explicit reader
seams, with strict null/live/Windows/unrelated-error refusals and fatal propagation. It changes neither
the production capture core nor the 17-case plan and does not prove the historical Linux failure's
cause. The four unchanged source cases retain their prior passing evidence and were not repeated.

### Corrected getter component checkpoint

The reviewed getter correction changed five files within the follow-up scope: the control fixture,
owning build spec, this plan, the fragment and the version 6 guide. The uncommitted snapshot based on H
then passed 17/17 controls on Windows PowerShell 5.1.26100.9711 and 17/17 on PowerShell 7.6.9: 34 current
passing cases, separate from the four earlier source-case passes. Each host recorded two actual
evaluation-only MSBuild identities and 29 guarded native starts. The annotation-policy identity
fingerprints matched across hosts; strict metadata counterfactuals, owned-console/PIPE facts and the
closed public/private diagnostic bindings passed. All 58 recorded native instances and six contained
driver children closed, and the 30 frozen inputs plus executable/Utility bindings had zero drift.

The sealed component proof is
`591329327958ce563d555d6d9b4a8993045705c134dbbac7420c08a366f471a7`.
The four earlier source cases remain bound separately by
`7162a82b65400f366bd43bfe9670dadc534aac094823590a34acb0a9f822876e`;
their original run's WinPS 16/17 failure remains failed and preserved. These are component checks of
the recorded snapshot, not qualification of a future commit. Fresh successor tooling/hosted/native,
trusted-base policy review, paired acceptance and RedSalamander adoption remain pending.

The PR paired job also failed strict Full harness preflight before any measurement slot. H's common
BUILD overlay omitted the changed installer and VisualStudio discovery helpers that its Full identity
included; H also omitted ConsumerBuild from that identity. The follow-up adds ConsumerBuild to the
Full identity and overlays exactly those three producer tooling files on both revisions, preserving
each revision's C++/headers/library project and the
separate benchmark-workload selector and hash algorithm. It does not relax equality or fabricate a
paired result. There are zero measurement slots and no regression finding. The retained historical
artifact lacks the raw per-file hashes needed to replay the complete Full hash independently; the
observed preflight mismatch and source-level overlay omission remain distinct evidence.

## Output and remaining qualification

Focused controls borrow an explicit caller-owned existing fixture parent beneath the already approved
workspace and retain a unique child; they do not initialize a new drive root or run old aggregate paths
that write repository-local test data. CI adapters retain their own explicit result child. Product builds,
foreground input, hosted dispatch, publication and pin adoption require their separate authorized phases.
Keep this plan in WIP until its acceptance gates are closed; deferred gates are not completed outcomes.
