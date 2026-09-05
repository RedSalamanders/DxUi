# DxUi complex-UI measurement, 2026-09-05

This is independent library evidence for `dxui-complex-ui-v2`: 83 controls, 1,000 synthetic Grid/Tree records,
1280x720 at 96 DPI, reduced motion, 20 warm-up frames and five 40-frame rounds per clean/dirty scenario.
The [runnable sample](../../../Samples/ComplexUi/README.md) and benchmark share the same scene and model.
No RedXe or RedSalamander code, settings, plugin or device service is loaded.

## Captures and comparison

Both serial runs used x64 Release, Ryzen 9 9950X3D, Windows 10.0.26200, Balanced power and WARP 10.0.26100.9278.
The original baseline, every round and the repeat are retained. Values below are five-round medians.

| Run | Scenario | Completed offscreen FPS | Frame p95 | Process private bytes | Surface bytes |
| --- | --- | ---: | ---: | ---: | ---: |
| Baseline | Clean | 2,264.80 | 0.5256 ms | 26,750,976 | 3,686,400 |
| Baseline | Dirty | 501.85 | 2.2968 ms | 28,635,136 | 3,686,400 |
| Repeat | Clean | 2,266.15 | 0.5377 ms | 26,484,736 | 3,686,400 |
| Repeat | Dirty | 496.22 | 2.2937 ms | 28,065,792 | 3,686,400 |

- [Baseline raw rounds](baseline.json) and its explicit [unpaired status](baseline.json.comparison.json).
- [Repeat raw rounds](repeat.json) and the [complete comparison](repeat.json.comparison.json).
- [Rendered workload](scene.png), captured outside timing after warm-up.

The comparison is `within-noise-budget`: dirty FPS varied by -1.12%, within the 5% investigation band. Composition
allocations are zero and hidden preparation/composition counters are zero in both runs. Surface storage is unchanged.
This does not authorize future regressions; retain the original baseline and investigate repeatable degradation.

## Identity and limits

The source and executable fingerprints match between runs. The library-input fingerprint is
`C3B4B532105441856AF99CF185F592D2350B49CC90C14543CE26BCBC36E149C1`; the combined benchmark/scene/graphics-helper
fingerprint is `033604B4AF8EE132A5F0B29273C11FFA247A64986E79BB823A8127A830D37773`. Per-input hashes, compiler,
executable hash, base commit and dirty-working-tree status are recorded in each receipt. Both runs rebuilt through
the normal entrypoint; neither used `-SkipBuild`. The working tree includes the independent sample and naming changes.

This is a new fixture baseline plus a repeat on the same code, not a before/after implementation speedup. Do not
compare these numbers to the old `complex-ui-v1` fixture or use them as RedXe AV acceptance. FPS includes target clear
and blocking one-pixel GPU readback; there is no swap-chain presentation, vsync or hardware-GPU claim. Hardware
presentation, input latency and long-run retention need their own evidence.

Reproduce from the repository root with `performance.ps1 -Configuration Release -Platform x64 -OutputPath <new-file>`;
add `-Baseline <retained-baseline>` for the subsequent matched run. See [the protocol](../../../docs/performance.md).
