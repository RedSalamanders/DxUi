# Host/control review reconciled with current main

Source: `a424c968d2055c7c8bd4683a906d39b6eb9eb685`, including main
`fa5821c87f563a1d5b24ced7832b9c4fb9c1dcab`. Subsequent summary/gallery publication changes no native inputs.
This is independent DxUi evidence on an x64 Windows host using WARP and synthetic library-owned models.
The [active review plan](../../../Specs/Plans/WIP/CodeReview_2026-10-10.md) owns remaining decisions;
[draft PR #96](https://github.com/RedSalamanders/DxUi/pull/96) contains the reconciled implementation.

## Completed local checks

| Check | Result and boundary |
| --- | --- |
| x64 Debug, Release, ASan Debug | All 19 default noninteractive suites per profile passed: 57 executions, zero capability skips. The isolated expected ASan failure was detected. |
| ARM64 Debug, Release, ASan Debug | All three builds passed. These are cross-builds, with no local ARM64 execution claim. |
| Repository validation | All five validators and the complete PowerShell tooling roster passed. Formatting passed with the repository-discovered formatter. |
| API-4 external consumer fixture | Debug/Release at `e67a3e139ba1de018c3eb3405a4793b459611c4d`; ASan Debug at `a424c968d2055c7c8bd4683a906d39b6eb9eb685`. Each passed 10 negative checks, 12 native module checks and relocated RichText rendering. Only `Tools/tests/Test-ScopedTesting.ps1` differs between those pins. |
| Gallery | `gallery.ps1 -Configuration Release -SkipBuild -PublishDocs` generated all five theme sheets and the embedded sample from the validated Release artifacts. All six were visually inspected. The generation receipt retains its real source commit. |
| Native naming | Inventory validates 146 contextual `Scope.Tests.Something.h/.cpp` files, including the frozen API fixture. Literal `Scope` is rejected. |

The exact suite, artifact, build, toolchain, dependency and unpaired diagnostic identities are retained in the
[summary-only receipt inventory](../../../Specs/Plans/WIP/CodeReview_2026-10-10/qualification-20261021.json).
Original reports, logs, binaries and failed attempts remain in generated output and the private recovery archive;
this summary does not substitute a different source identity for an original receipt.
After the evidence/gallery edits, a read-only check against `Get-ScopedRunIdentity` and `Test-ScopedReceipt`
confirmed that all 57 successful native receipts still match their exact source/dependency/artifact/profile/environment
identities. That check executed no native tests. Specification, documentation/gallery integrity and the folded
changelog validation also passed after publication edits.

## Reconciliation checks

The initially ported preparation retry failed the existing rich-text regression. The repaired path preserves
current main's deferred finish guard: unchanged failed preparation schedules no automatic retry, while a replaced
root schedules exactly one replacement request after callbacks unwind. Embedded then passed in all three profiles.
Source-only pending RichText input retains its own revision/permission gate; unrelated dirty geometry permits
keyboard dispatch without weakening text-service snapshots. The UIA boundary preserves current semantic ranges
and maps named standard exceptions, including `std::bad_alloc` to `E_OUTOFMEMORY`.

The extracted allocation instrument is identical to main's inline block after only CRLF and trailing-whitespace
normalization: both SHA-256 values are
`A17E152AFC13678906FCFF7B6D3CF892FD329BC0DCC066A5C4CAB03C308CFF24`.
See the [block-equivalence record](../../../Specs/Plans/WIP/CodeReview_2026-10-10/benchmark-instrument-equivalence-20261021.json).

Earlier Debug build attempts were refused because inputs changed during the build; no successful attestation
was published for those attempts. Frozen-source Debug retries passed on both architectures. The enclosing matrix
wrapper retains the original failed attempt's nonzero aggregate exit; the per-profile final receipts above are
the qualification evidence. Earlier tooling/naming failures and the original native predicate failure remain
listed in the [log seal](../../../Specs/Plans/WIP/CodeReview_2026-10-10/validation-log-hashes-20261021.json).

## Remaining acceptance

- Menu, NativeTextInput, MenuResources and MenuResourceScaling still require agreement to their desktop time.
  No focus-taking suite was started in this reconciliation.
- The enabled PR workflow owns native ARM64 execution and the matched Release benchmark: 12 randomized blocks
  over Default, MultilineGrid and MultilineGridDistinct, retaining all 144 measurements and both judges. The initial
  draft validation run was cancelled before final evidence publication; final-revision CI remains pending.
  There is no duplicate local paired run and no new performance non-regression claim. Every local test run's
  complex-UI FPS/memory report is explicitly unpaired.
- The native diagram gallery tile appears empty in Dark, High Contrast and Rainbow Light. These three PNGs are
  byte-identical to main. Investigate this existing RichText/gallery readiness issue before visual qualification;
  a passing gallery integrity validator does not establish that the tile is populated.
- Product adoption, physical-device/foreground acceptance, existing RichText qualification HOLDs and the review's
  deferred release-asset/history/ruleset administration remain separate. No consumer pin or repository visibility
  changed. No history was rewritten.

The plan remains WIP and the PR remains draft while these gates are open. No historical baseline, resource budget,
trusted benchmark policy or failed receipt was replaced to obtain a passing result.
