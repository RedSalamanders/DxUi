# Configured icon fallback reading direction

Status: complete for F-source functional qualification. Shared paired-performance policy and consumer adoption remain separate.

## Completed criteria

- Configured icon roles preserve requested reading direction when the Small fallback is selected.
- Deterministic cache-seeded coverage and the typography/hosting contract retain this behavior.
- F passed all six build profiles, 57/57 ordinary x64 suite executions with zero skips, the expected ASan detector probe, and the 12-suite local foreground matrix. Hosted PR #131 passed all six native profiles.
- The all-controls-unstable paired result is inconclusive and does not claim performance acceptance.

Source-bound evidence is in [the qualification packet](../../../Measurements/QualificationReliability_2026-10-28/README.md). Earlier source attempts and failures remain in the machine-readable history; they are not relabeled as F.
