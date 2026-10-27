# DxUi source-bound native and paired evidence

This reviewed packet combines the measured 702 CI evidence and a distinct local Debug Menu supplement. Its native results and policy hold remain separate; it does not establish product or performance acceptance.

The implementation from PRs 113–116 is now merged at 6382b30646211be825c0275672b4986e65dc2ceb; its tree matches the tested 702 tree. This merge does not establish performance policy acceptance.

## Paired performance result

The measured PR head is `7020121dce2fc36d6c3b1219a17ea2d20e5a6983`; its Actions merge candidate is `2b57c31a915b80107cacae0228a6b64b39fdf4d8`, tree `f7f484f624550beb653d157434431ca78bee2475`. The baseline is `4a05f62c1d775724ddc1ad23dd35f3b30f6902b1`. The paired workflow concluded failure because the base policy is unqualified. All three independently recomputed scenario sets have zero regressed metrics and are within their noise budgets, while 69 of 72 same-side controls are unstable and 229 of 1,872 control metric observations drifted. MultilineGrid's producer legacy/migrated verdict agreement is false. The result remains `policy-review-required`; no performance acceptance is claimed.

The archive matched its provider digest and contained 471 entries / 699,356,246 expanded bytes. Both measured baseline and candidate EmbeddedTests executables and PDB payloads are retained. The candidate EXE hash matches measurement metadata. PDB hashes identify retained PDB bytes only; no CodeView GUID match is claimed. The schedule used 12 actual typed .NET `Random.Next(2)` draws once, expanded as the shared schedule across three scenarios (36 scenario/block assignments).

## Hosted native evidence

All six native CI profiles passed with no skips. The independent archive replay passed 8,307 checks across 732 extracted files, 505 completion-manifest streams, and 130 suite reports, including two ASan detectors. The three ARM64 profiles report focus unavailable/unverified; foreground and cursor restoration passed across the profiles. Therefore full physical-input qualification remains false. Native profile success does not override the paired policy hold.

## Local Debug Menu supplement

A separate x64 Debug Menu run on the same 702 source completed 124 individual cases with zero failures or skips. Its recorded lease state was foreground unchanged, focus unchanged, and cursor unchanged; focus capture was recorded successfully. The run's source and report hashes are in `local-debug-menu.json`. This local suite result is separate from the hosted six-profile matrix.

## Separate SDK 4077 A/A calibration

The previously published same-binary A/A study remains separate: [SDK 4077 A/A calibration](https://github.com/RedSalamanders/DxUi/blob/6382b30646211be825c0275672b4986e65dc2ceb/Measurements/RedesignCompletion_2026-10-26/aa-calibration-4077/README.md). It covers 144 receipts from source 4077, with 72/72 unstable controls, 51 drifted slots, and 631/1,872 drifted observations. Its observer is contextual only, not evidence of a quiet host. Those records are not pooled into this 702 CI cohort.

## Bound files

`packet-manifest.json` binds the LF-normalized packet files and `.gitattributes`. The full paths, logs, receipts, and private provenance are retained only in the local `.build` audit tree, not copied into this packet.
## Archive input identity

The `workloadOwner`, composite `fixture` and `benchmarkInputs` fields identify this library-owned archive projection, not a new runtime receipt. The 20 paired-harness input hashes are copied from the measured producer summary. Additional `source-702-git-blob/` entries are SHA-256 values of actual native/Menu fixture source blobs at head702; their representation is stated explicitly. The original reported fixture names remain in `nativeFixtures`. No historical receipt, numeric result, threshold or policy decision is rewritten. `metric-results.json` wraps its unchanged 78 rows in this archive metadata.
