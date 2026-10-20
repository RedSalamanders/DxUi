# PR #87 post-merge evidence, 19 October 2026

Owner: DxUi. Shared acceptance remains owned by
[ConsumerUiQualification](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).
This is a reviewed Markdown summary; raw reports, machine details and desktop records remain private.

## Source and completed checks

[PR #87](https://github.com/RedSalamanders/DxUi/pull/87) merged as
`5da68bcec49c0df2c9158ede168779ebf9737a52` under the user's explicit implementation-only exception.
Its tree matches the reviewed head `b941b2dd9e633b2079549dab46f35e9a3b28a9a3`.
The [post-merge validation run](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924)
and [separate formatting run](https://github.com/RedSalamanders/DxUi/actions/runs/39187090855)
complete successfully at that exact merge commit.

All six native jobs, eight enabled public-consumer checks, repository validation, Windows tooling,
formatting, native scope and the aggregate gate pass. The artifact audit verifies 126 core suite receipts
(21 per profile), plus two x64 Release characterization receipts: 128 successful receipts with zero skips.
Every receipt's attached performance report identifies the merge commit; those unpaired reports do not
establish non-regression.

| Native profile | Menu | NativeTextInput | Capability skips | Capture reason / focus restoration |
| --- | --- | --- | --- | --- |
| [x64 Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924/job/117851744049) | 113/113 | 169/169 | 0 | recorded / unchanged |
| [x64 Release](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924/job/117851744044) | 113/113 | 169/169 | 0 | recorded / unchanged |
| [x64 ASan Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924/job/117851744086) | 113/113 | 169/169 | 0 | recorded / unchanged |
| [ARM64 Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924/job/117851744048) | 113/113 | 169/169 | 0 | foreground-minimized / none |
| [ARM64 Release](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924/job/117851744073) | 113/113 | 169/169 | 0 | foreground-minimized / none |
| [ARM64 ASan Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924/job/117851744110) | 113/113 | 169/169 | 0 | foreground-minimized / none |

The extra x64 Release receipts are `MenuResourceScaling` and `MenuTextLayoutResources`. They are separate
characterization evidence, not native ARM64 opt-in MenuResources/MenuResourceScaling execution.

## Reconciliation and focus observations

The retained NativeTextInput traces reproduce the repaired boundary in every profile: reconciliation records
text `0/1` and layout `0/1`; following queued geometry keeps text `1/1` and adds layout `1/2`.
The whole-queue counterfactual records text `0/1` and layout `0/2`; duplicate dispatch is inert.
Observed cookies and activation counts stay unchanged. This verifies the corrected test on native ARM64,
without reconstructing the missing historical counts in the original ARM64 Release failure.

The ARM64 rows represent **three shared leases**, one per Full configuration. Menu and NativeTextInput copy
the same pre-confirmation capture/restoration state into two child receipts; they are not six independent
focus snapshots. In all three leases, `focusCapture.Reason=foreground-minimized`, query error is 0 and
focus restoration is `none`. Focus restoration remains **unverified**.

That reason describes the initial desktop snapshot before the warning and either child suite. With no saved
focus, restoration returns `none` without attempting to focus a saved window. It does not report a child
control losing focus or a restoration timeout. Successful foreground functional tests do not close this limit.

Remaining focus work is to retain these source-specific records and obtain a separately approved ordinary-host
and native ARM64 observation with a visible foreground and recorded initial keyboard focus, then verify its
restoration. Keep missing-initial-focus evidence separate from child focus behavior. Preserve the existing
capture/refusal checks, deadlines and attempt bounds; add no activation retries to manufacture qualification.

## Performance and qualification remain open

The post-merge push skips the paired job by design. The preceding
[PR paired run](https://github.com/RedSalamanders/DxUi/actions/runs/39169413518/job/117799162776)
finished inconclusive with `policy-review-required`: the measured base's policy is `qualified=false`.
Its aggregate failed on that result; the implementation-only merge did not qualify the policy or performance.
Raw paired reports and all per-run flags remain retained.

All 20 qualification HOLDs stay open. Timing/FPS and memory bands remain 5% and 2%; all deadlines, including
the 900 ms hover and 180 ms pointer guards, attempt bounds, exact budgets and allocation ceilings are unchanged.
Earlier timing/memory flags, MultilineGrid memory uncertainty, failed/refused/cancelled receipts and retention
limits remain. Historical activation refusals, creation timeout, the UIA cache-factory failure, original ARM64
notification ordering and the failed probe's cursor-restoration cause remain unattributed.

Native ARM64 opt-in resources, physical input/device removal, real IME/AT/touch, consumer pins/product acceptance
and trusted paired non-regression require their own evidence. Public-consumer fixture passes do not update
application pins or establish product acceptance.

## Retained audit identity

The private reviewed audit binds all 128 source-specific receipts and the selected foreground logs. Its SHA-256 is:

```text
48F9E506E8631B29EB563F66342BA3E17B3F0B287004F375BC705520FE425E03
```

The two additional characterization report SHA-256 values are:

```text
MenuResourceScaling-x64-Release.hosted.json
5C0B48408C9A41DDB08CCB58F89C741DF0EAE63DF47C4C7AA6E0CE951FB7E836
MenuTextLayoutResources-x64-Release.json
FF851F0B68AFB2D42E709B8BA1475E86791948CF7AA5AF264E23718896889FEE
```

Hashes identify retained bytes; they do not close the independent retention/rehashing HOLD. Raw logs and desktop
identities are not published here. This update changes documentation only, with no library/API, consumer adapter,
control pixels or gallery examples changed; gallery regeneration is unnecessary.
