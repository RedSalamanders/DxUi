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

## Output and remaining qualification

Focused controls borrow an explicit caller-owned existing fixture parent beneath the already approved
workspace and retain a unique child; they do not initialize a new drive root or run old aggregate paths
that write repository-local test data. CI adapters retain their own explicit result child. Product builds,
foreground input, hosted dispatch, publication and pin adoption require their separate authorized phases.
Keep this plan in WIP until its acceptance gates are closed; deferred gates are not completed outcomes.
