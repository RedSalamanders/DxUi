# Host/control review integration qualification

Source: `da335d360f7ccccab0e7e627366114f82162f29a`, including main
`ceffe76a429d15039ceff1f7dfae7e1d7126323e`. This is independent DxUi evidence on an x64 Windows host,
using WARP and library-owned synthetic fixtures. Documentation and gallery publication are descendants of
that source; they do not replace the original native or consumer pins. The
[active review plan](../../../Specs/Plans/WIP/CodeReview_2026-10-10.md) owns the remaining acceptance work.

## Local qualification at the frozen source

| Check | Result and boundary |
| --- | --- |
| x64 Debug, Release, ASan Debug | Three builds and all 19 ordinary suites per profile passed: 57 executions with zero skips. |
| Authorized foreground coverage | Menu, NativeTextInput, MenuResources and MenuResourceScaling passed in all three x64 profiles: 12 executions with zero skips. The standard visible 30-second automatic Start countdown and desktop lease were used. |
| ASan detection | Both separate intentional fault probes were detected, one in the ordinary run and one in the foreground run. These expected failures are separate from the 69 passing native suites. |
| ARM64 Debug, Release, ASan Debug | All three cross-builds passed. No native ARM64 execution is claimed. |
| API-4 isolated consumer fixture | All three x64 profiles passed at the exact source above, each with 10 negative and 12 native-module checks, relocated RichText rendering, and default STL annotations. Product consumer pins did not change. |
| Repository checks | All five validators, the complete PowerShell tooling roster, all 12 CMake triplet read-back cases, and pinned clang-format 22.1.3 passed. |
| Gallery | All six no-activation harness captures were regenerated from the qualified Release artifacts and visually inspected. Native diagram content is visible in all five theme sheets; broader visual qualification remains separate. |

The [curated receipt inventory](../../../Specs/Plans/WIP/CodeReview_2026-10-10/qualification-20261024.json)
binds six build profiles, six native runs, 69 zero-skip suite receipts, two ASan probes and three consumer
annotations. Selection uses each final runner's PID and creation time, the exact profile and interval, sealed
receipt/log bytes, and current attested executable hashes. It excludes earlier failed or filtered attempts
without editing them. Detailed process and desktop diagnostics, machine identities, security metadata and
filesystem paths remain private.

The final ASan foreground wrapper remains recorded as `FAILED` with exit code 1. Its in-process caller read
the intentional ASan probe's leftover `$LASTEXITCODE` after `test.ps1` had completed successfully. The sealed
native run records `passed`, all four suite exits are zero, and the lease completed with recorded restoration.
Independent in-process and fresh-process reproductions, plus throwing controls, establish the invocation error.
The curated packet retains an audited disposition and the original wrapper/run/proof hashes; it does not
rewrite the failed wrapper record or claim that every original wrapper step passed.

Full source identity: `3c384a8022d512fae3921007fbe986e5e88f71e3e3a0e9542a1dc0ac6081151b`.
Compiled source identity: `096498bfd69551173472d5db5cbbf731f6ccf41ea3bed58f7672f5f8f64f4a96`.
Every final foreground receipt records saved initial keyboard focus and a completed lease; foreground, focus and cursor outcomes are individually retained as unchanged or restored in the curated inventory.
Post-publication checks confirm that the compiled identity and all selected build artifacts remain unchanged.

## Integration failures retained and repaired

- At `081c8551`, the Release WindowHost suite failed the exactly-one translated-character check for Space.
  The printable-editor guard ran before the existing guarded Return/Space translation permission. Commit
  `5b85b4b7` restores that ordering while preserving owner, root, epoch, window and lifetime checks. The original
  failure, exact binary and diagnostic reproduction remain preserved.
- At `5b85b4b7`, all six builds and 57 ordinary suites passed, but the application TSF capacity fixture failed.
  That fixture incorrectly applied native long-document semantics to a bounded application snapshot; the shared
  query path also advertised insertions beyond the application's existing capacity. Commit `db3c717f` adds a
  private target-specific ceiling and subtraction-safe shared query/mutation checks. Boundary tests require
  refusal without partial edits, full-capacity replacements and exact-limit insertion. Native 70,000-unit
  document coverage is unchanged.
- At `db3c717f`, foreground NativeTextInput reached the stale-owner case and failed to clear the native session
  after its current owner became ineligible. The early invalid-target return skipped canonical pruning. Commit
  `da335d36` prunes an ineligible current owner while preserving unrelated rejected focus requests. Coverage
  checks disabled, hidden and ancestor-disabled owners, host/native/local focus, text preservation and reactivation.
- The same attempt's Menu suite stopped at repository-root discovery before pixel comparison. A controlled
  diagnostic reproduced divergence between PowerShell's location and the process working directory; the old
  child directory was not recorded. The final runner explicitly binds and records both directories. Full Menu
  then passed with no Menu production change. This is a verified invocation correction, with the historical
  attribution limited by the missing child-directory record.

Original failed attempts and prior binaries are retained separately. An interrupted consumer ASan bootstrap
at the earlier source established no build or runtime result. Historical
[21 October evidence](../2026-10-21/README.md) keeps its original identities and scope.

## Remaining acceptance

- Every local complex-UI FPS/memory report is unpaired. These runs establish functional qualification only;
  they establish no quiet-host result, performance non-regression, cost waiver or trusted rebaseline. The separate
  RedTilde R10 optimization/advice gate remains open.
- Final hosted CI results, native ARM64 execution, manual IME/assistive-technology and physical-device coverage,
  product adoption and the existing RichText qualification HOLDs remain separate. No product pin was updated.
- Native diagram content is populated in all five fresh theme sheets, including Dark, High Contrast and Rainbow Light. The historical blank-tile observation is not reproduced at this source. Broader RichText visual, manual and platform qualification remains open.
- The tools support expected base/candidate SHA checks, but the current workflow does not pass the event pair
  through scope, verdict and aggregate steps, which still execute from the merge-ref checkout. CR-29/Q12 retains
  the trusted-base checkout, event-SHA propagation and negative workflow tests as follow-up work. The unqualified
  benchmark policy still fails closed; this evidence does not approve it.
- The review's release-asset uploads, history rewrite and ruleset administration remain deferred. This integration
  performs none of them. The review plan remains WIP while these acceptance items are open.
