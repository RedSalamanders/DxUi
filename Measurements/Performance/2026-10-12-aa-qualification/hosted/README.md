# Hosted same-binary A/A calibration

[Workflow run 38288233925](https://github.com/RedSalamanders/DxUi/actions/runs/38288233925) executed the manual A/A diagnostic on a GitHub-hosted x64 Release/WARP runner. The paired benchmark job succeeded as a diagnostic. The overall workflow also completed all six native profiles, but this A/A result cannot pass the required policy rule. Its retained [verdict.receipt.txt](verdict.receipt.txt) has `Conclusion: inconclusive`, `StrictControls: false` and `PolicyStatus: policy-review-required`.

The baseline, candidate and clean harness all declare commit `0bd0e99c482ccd230dce949c65686791df682769`. The driver chose seed `165785271` and 12 independent ABBA/BAAB blocks. [summary.receipt.txt](summary.receipt.txt) has SHA256 `2B89E03F74FFE92CDB0AF305197859E103705C68B5B285BED83B46DD7CEF8207`. The 435 original uploaded report bytes are preserved here; the summary and verdict have `.receipt.txt` names to distinguish captured reports from authored measurements. [SHA256SUMS](SHA256SUMS) covers them and the analytical companions.

The [reports-only audit](audit.receipt.txt) replays all 144 raw receipt hashes, both complete judges and all 72 controls from the uploaded bytes. Both judges return `within-noise-budget` with zero aggregate regression slots in all three scenes and agree on their verdicts. Nevertheless 71/72 same-binary controls are unstable and 39/78 timing/memory cells drift at least once: Default 24 unstable controls and 15 drifting cells, MultilineGrid 24 and 18, MultilineGridDistinct 23 and 6. Strict and required-policy replay both remain inconclusive. These observations do not certify a quiet host, attribute a regression or qualify a trusted policy.

The hosted upload contains the reports but no executable, library, symbols, build receipt or toolchain/dependency inventory for an independent byte rehash. The audit checks the declared full-v2 identities and their consistency across raw receipts; its `artifacts.verified: false` deliberately does not claim actual hosted binary rehashing. To replay the report-level evidence from a checkout with the frozen harness and judge bytes:

```powershell
./Measurements/Performance/2026-10-12-aa-qualification/Audit.ps1 -ReportsDirectory ./Measurements/Performance/2026-10-12-aa-qualification/hosted -SummaryFileName summary.receipt.txt -ExpectedSummarySha256 2B89E03F74FFE92CDB0AF305197859E103705C68B5B285BED83B46DD7CEF8207 -ExpectedSeed 165785271 -ReportsOnly
```
