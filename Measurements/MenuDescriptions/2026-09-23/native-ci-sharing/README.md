# Native qualification of popup-local text sharing

Source `f72941b83ba8269e35da5b01f7bcd3133ba7515b` completes
[CI 35831275327](https://github.com/RedSalamanders/DxUi/actions/runs/35831275327)
on native x64 and ARM64 in Debug, Release and ASan Debug. No job retry was needed.

All nineteen regular suites return success in each profile. The nine existing ARM64
Menu desktop-capability skips remain explicit in each ARM64 receipt; x64 has zero
skips. Both opt-in x64 Release diagnostics also pass. All six external-consumer
profiles and both annotation-disabled ASan variants pass. The two intentional ASan
fault probes return exit 1 with detection confirmed, their expected result.

[Summary](summary.receipt.txt) enumerates the profiles. Byte-preserved receipts, workflow
identity, performance data and selected directed-input/sanitizer logs are in `raw`,
bound by `sha256.receipt.txt`. Complete downloaded artifacts remain at
`C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-f72941b/`.

These functional/native configuration results do not resolve the local paired
common-scene timing flags documented in the [sharing packet](../popup-text-sharing/README.md),
approve a resource tradeoff, prove physical mixed-monitor consumer presentation,
close assistive-technology acceptance, or change a consumer pin. The separate grid
branch remains independently owned and unadopted.
