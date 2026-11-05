# Paired attestation module ownership

Status: DONE — scoped tooling failure and regression repair

- [x] Preserve [hosted run 42106904205](https://github.com/RedSalamanders/DxUi/actions/runs/42106904205), including raw logs/artifacts. It failed before measurement and wrote no summary or verdict.
- [x] Reproduce the exact `ScopedTesting ScopedTesting` command-resolution error with two visible checkout modules: 41 of 42 PerformancePolicy cases pass before the repair.
- [x] Retain the exact imported dependency module in each performance-policy instance and invoke private discovery through that reference.
- [x] Compare the complete native closure before and after importing a hostile same-name module; all 42 cases pass after repair.
- [x] Pass all five canonical validators, complete tooling tests and formatting; update normative performance and tool usage contracts.

The failure was a process-wide name lookup returning multiple module instances during a paired build.
It was not a native compiler error or performance regression. The preserved run built the original library
with zero warnings/errors before benchmark pre-build attestation refused the ambiguous lookup.

Source and raw evidence SHA-256:

| Artifact | SHA-256 |
| --- | --- |
| PerformancePolicy module | 6e86137500d7bab5c4b90c793afa1e1898c79e3c269ecae9d3ccf3cf257816a7 |
| Independent collision fixture | 1f1ba4a321a9dac11145a1ad285bcacb6c85cedf6866c0598ac2811378d1dbd9 |
| Original local counterexample log | c7afab1253f4648bbbb046f9345c1142669ba55bf30ec93965d99cffec300ad1 |
| Corrected 42-case run log | ec151676665f38c9aafc9c6a2ffcdb6e7ef537180d7b0ace2b209557d0fdb336 |
| Complete canonical validation log | 3acb629dd82ccb1cbf8519f2e053c51231e6de80453dae432501b616732d3422 |

The exact baseline `0acd56080d97d2b45fe8e92395496dcd2cf5b911` and candidate
`f765aeca5e059dbe768841b605b1be6a5f6ea942` still require a fresh paired run with the repaired harness.
Performance-policy/control qualification and acceptance remain in [D-P1](../WIP/RemainingWork_2026-11-04.md#d-p1).
