# Qualification runner reliability

Status: complete for F-source tooling and functional qualification. The paired performance/policy gate and consumer/platform acceptance remain separate.

## Completed criteria

- The external-timeout fixture uses a closed body-readiness marker published before parent reads. A still-live child retains its fixture until closure; the original missing-progress-path cause remains unassigned.
- Journal updates use `File.Replace` with explicit CLR-null backup. Retry is restricted to codes 32 and 1175, at most three calls and two 25 ms waits, under a 250 ms retry-admission window checked again after file identity, length and SHA-256 verification. Changed state and unknown errors fail closed, and the phase body is never retried. The focused fixture controls and final static validation passed.
- F passed the six-profile build matrix, final format/validation/Explain/PrePush/changelog static sequence, 57 ordinary x64 executions with zero skips, the actual ASan detector probe and 12 local foreground outcomes with exact unchanged restoration.
- Hosted PR #131 passed all six native profiles and format, scope, tooling and validation. Its paired benchmark failed at `Publish-BenchmarkVerdict.ps1`; all 72 controls were unstable and the result is inconclusive. No performance acceptance is claimed.

Durable validation and tooling contracts are in [Testing Validation](../../../Specs/Testing/Testing_Validation.md) and [Tools README](../../../Tools/README.md). Earlier source failures, including the 9d File.Replace failure with unrecorded OS code, remain source-bound in [the qualification packet](../../../Measurements/QualificationReliability_2026-10-28/README.md).
