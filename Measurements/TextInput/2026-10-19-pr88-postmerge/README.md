# PR #88 post-merge evidence, 19 October 2026

Owner: DxUi. Shared acceptance remains owned by
[ConsumerUiQualification](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).
This reviewed Markdown summary keeps raw reports, machine details and desktop records private.

## Source and completed checks

[PR #88](https://github.com/RedSalamanders/DxUi/pull/88) merged as
`f65e4a4b9311370b3e3070088e2a8abe29adcb27` after its documentation-only PR checks passed.
Its tree matches reviewed head `c0c7614b8b9251c37285568de426bd11a59f7333`.
The [post-merge run](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836) succeeds at that exact source:
all six native profiles, eight enabled public-consumer checks, validation, Windows tooling, formatting, native
scope and the aggregate gate pass. No failed jobs require a repair.

The artifact audit verifies 126 core suite receipts (21 per profile) plus two x64 Release characterization receipts:
128 successful receipts, all with zero capability skips. Every attached performance report identifies the clean
merge source and matching native architecture. Both ASan configurations detect the intentional heap-use-after-free
probe; those expected nonzero probe exits are separate from the 128 successful suite receipts.

| Native profile | Menu | NativeTextInput | Capability skips | Capture reason / focus restoration |
| --- | --- | --- | --- | --- |
| [x64 Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836/job/117927148751) | 113/113 | 169/169 | 0 | recorded / unchanged |
| [x64 Release](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836/job/117927148791) | 113/113 | 169/169 | 0 | recorded / unchanged |
| [x64 ASan Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836/job/117927148786) | 113/113 | 169/169 | 0 | recorded / unchanged |
| [ARM64 Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836/job/117927148808) | 113/113 | 169/169 | 0 | foreground-minimized / none |
| [ARM64 Release](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836/job/117927148707) | 113/113 | 169/169 | 0 | foreground-minimized / none |
| [ARM64 ASan Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39211934836/job/117927148669) | 113/113 | 169/169 | 0 | foreground-minimized / none |

The two additional x64 Release receipts are MenuResourceScaling and MenuTextLayoutResources. They do not
establish native ARM64 opt-in MenuResources/MenuResourceScaling execution or hardware/product acceptance.
The [PR #87 packet](../2026-10-19-pr87-postmerge/README.md) retains its own merge source, audit and limitations.

## ARM64 focus-restoration limitation and next steps

The ARM64 rows represent three shared Full leases. Their initial capture/restoration state is copied into the
Menu and NativeTextInput receipts; these are not six independent focus snapshots. In each lease,
`focusCapture.Reason=foreground-minimized`, query error is 0, focus is `none`, and foreground/pointer restoration
pass. No initial keyboard focus was saved before confirmation, so the no-saved-focus branch attempts no focus
restoration. ARM64 focus restoration remains **unverified**. This does not report a child control losing focus or
a restoration timeout. x64 captures initial focus and reports it unchanged.

Retain these exact-source captures and obtain separately approved ordinary-host and native ARM64 observations
with a visible foreground and recorded initial keyboard focus, then verify restoration. Keep missing initial
focus separate from child focus behavior. Preserve admission, capture/refusal checks, deadlines and attempt
bounds; do not add activation retries or longer waits to turn unavailable evidence into acceptance.

## Qualification remains open

The post-merge push skips paired benchmarking by design. All attached performance comparisons are labeled
unpaired; green functional CI establishes no accepted paired non-regression or qualified policy. It does not
change the earlier inconclusive paired results, timing/memory flags or MultilineGrid memory uncertainty.

All 20 qualification HOLDs remain open. The policy stays `qualified=false`; timing/FPS and memory bands remain
5% and 2%. All deadlines, including the 900 ms hover and 180 ms pointer guards, attempt bounds, exact budgets and
allocation ceilings are unchanged. Earlier failed/refused/cancelled receipts, attribution gaps and independent
retention/rehashing limits remain. Native ARM64 opt-in resources, physical input/device removal, real IME/AT/touch
and consumer pins/product acceptance need their own evidence; library consumer-fixture passes update no pins.

The private audit binds all 128 receipts and the selected foreground logs. Its SHA-256 is:

```text
96C4A287EC902FB375756FBB8AB65EAF1F00628FA2756A571AC446435DD23705
```

The hash identifies retained bytes and closes no retention HOLD. Raw machine/workspace/desktop identities are
not published. This documentation-only update changes no library/API, consumer adapter, control pixels or gallery
examples; gallery regeneration is unnecessary.
