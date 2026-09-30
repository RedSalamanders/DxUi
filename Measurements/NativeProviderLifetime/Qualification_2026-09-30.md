# Native provider lifetime qualification review

Reviewed code: `49c963f5751a6754e6b08e0362b96cfc7fc64414` against
`73ba9365726cce30299290ab9ae8afa065c4fe3d`. This review concerns the native
provider lifetime correction, not the earlier Grid/Menu feature costs or consumer I26.

The [original packet](2026-09-28/README.md) retains the stale-provider and focus-reentry
red/green witnesses, 54 local suite passes, ARM64 cross-builds and native-menu resource
samples. The [supplement](2026-09-30/README.md) adds exact-source CI, both crossover
series and original-source restoration: 475 logical files, 348 unique payloads,
verified SHA-256 and exact packet membership. Its mapped numerical audit was written
before restoration finished; the final mapped `driver-result.json` is the completion
authority. Both original headers and all four original Debug/Release binaries were
restored, with compiled-input hashes checked and prior short reports unchanged.

## Correctness and platform evidence

[CI 36358394952](https://github.com/RedSalamanders/DxUi/actions/runs/36358394952)
succeeds on the exact reviewed commit: 122 successful suite receipts, eight external
consumer receipts and two expected positive ASan detections. Each ARM64 Menu profile
records nine desktop-dependent skips. Those paths remain unexecuted; all other suite
skip lists are empty. The consumer's real stale-provider queue reorder and Skip
journeys pass on isolated pin49; this does not establish completion of its product plan.

## Resource interpretation

The original baseline and every comparator flag remain intact. The longer matched
fixture changes only the frame count and corresponding sample indices in both copies:
five rounds of 1,000 frames, Debug and Release, A-B-B-A. Of its 12 comparisons, eight
are `advice-required` and four `within-noise-budget`. This is not an all-green automated
performance result.

No adverse metric exceeds its investigation band across all four A/B pairings.
That fact alone would not exclude a smaller regression. In particular, long-run dirty
private bytes are higher for B in all pairings: +0.70–2.94% Debug and +0.69–3.54%
Release. The earlier matched short series instead gives -4.16–+0.74% Debug and
-9.19–+0.85% Release. Same-binary Release private memory varies by +2.08% in the long
B repeat, and by -5.85% / +4.56% in the short A/B repeats. These observations do not
establish a repeatable causal private-memory increase from this correction.

Long-run allocation and surface counters match exactly in all pairs: no clean or
composition C++ allocations, identical dirty allocation counts, and 3,686,400-byte
surface/replacement peaks. Release clean P95 improves in every long A/B pairing;
unchanged A alone crosses the +5% investigation band. Dirty Release FPS ranges from
511.11 to 521.02 across all four runs. These are offscreen WARP measurements; they
neither measure presented FPS nor exercise native UIA attachment.

The appropriate native-path resource comparison is the original packet's
`audit/current-vs-reference.json` and `audit/README.md`, backed by the unchanged
`dxui-plain-menu-uia-resources-v1` fixture, 476×322 at 96 DPI, and exactly 12/128
accessible children. Warm open heap means decrease by 1,819/3,314 bytes in Release
and 520/1,053 bytes in Debug. Process-private shifts do not have a consistent direction.
ASan heap-walker zeros are uninformative. The recorded one-handle residual in some
12-row samples is an aggregate process observation, not an identified provider leak;
the 128-row residual pattern matches the baseline. This is a bounded sample, not a
proof of zero lifetime cost. The change uses existing weak-token fields/storage and
adds identity validation/refcount work; it introduces no token allocation or new cache.

Review conclusion: the required paired comparison and investigation are complete;
no attributable degradation requiring a code change is established by these samples.
Keep the raw flags and these limits with the handoff. Do not silently rebaseline or
describe the comparator as passed. Final consumer resource qualification is separate.
The September 23 described-menu heap-phase experiment is a different code change and
is explicitly excluded from attribution for this correction.

## Documentation and handoff

The normative lifetime contract and control documentation are updated in the code
commit. This fix changes no control pixels, layout, theme or gallery example, so it
needs no regenerated visual gallery. Explicit consumer pin adoption, its remaining
product validation and this plan's index closeout are recorded separately in the
[execution plan](../../Specs/Plans/Done/NativeProviderLifetime_2026-09-28.md).
