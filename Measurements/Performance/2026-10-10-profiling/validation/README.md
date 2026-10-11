# Profiling and test follow-up validation

This packet records validation of the profiling/reporting changes and separate bounded focus setup diagnostics,
with fresh foreground validation still open.
It supplies no performance acceptance, consumer qualification or native ARM64 runtime result.

- `features/`: the four predeclared native profiling cases, Debug and ASan Default/MultilineGrid, exact native
  bytes and stamped reports, full provenance, companion hashes and independently reviewed harness captures.
- `native/`: nineteen noninteractive suite/log/scoped-receipt sets in each x64 profile, all 57 passing with zero
  skips. Each parent invocation uses affected selection. The retained output explicitly says repository
  `NOT_EVALUATED`; full production readiness is not inferred. Three canonical performance reports are unpaired.
- `builds/`: exact current six-profile v2 states, build and incremental attestations, three ARM64 build logs.
  All six executable/closure attestations pass the lead's independent check. ARM64 results are cross-compilation.
- `checks/`: original root validator/format/selection checks, the three import regressions, original failed-lease
  summary replay and the deliberate isolated sanitizer detection probe. Final closeout checks run again after
  the evidence/docs changes; logs remain in `.build/logs` and do not change the archived native source identity.
- `replay-guards/`: source-only checks reject a missing packet manifest and stale imported-helper binding.
  The complete sealed performance analysis also reproduces identical bytes. No native executable is launched.
- `focus-Debug-attempt1/`: the fresh native Start confirmation times out before any focus child launches.
  Foreground, keyboard focus and pointer are recorded unchanged, with lease exit 21 and four suite exits 24.
- `focus/`: four separately retained confirmed attempts on `5763d57e`. The Debug retry passes four full suites
  with zero skips and verified restoration; Release has a Menu popup-discovery failure, and Debug/ASan share an
  initial NativeTextInput setup failure followed by grant/restoration failures. Three direct full post-focus states
  match their pre-focus state; the earlier Debug failure's recorded identity was compared with the later Debug pass.
  The corrected setup-diagnostic binaries separately pass all six builds and 57 noninteractive executions with
  zero skips; `focus/setup-runs/` preserves those runs, two canceled ASan confirmations and the third ASan attempt's
  four complete focus-suite passes, zero skips, with verified restoration. The fresh Release confirmation cancels
  before any child executes. Actual before/after provenance and complete current scope digests pass independent
  checks. Fresh Release/Debug focus validation remains open; no failed or canceled attempt is a pass.

The lead independently checks every native suite receipt against current executable bytes, all four profiling
schemas/native-stamped fields and companion hashes, and all six exact v2 build attestations. Independent review
agrees with those checks and the canceled attempt. Earlier `Measurements/Layout/2026-10-10` focus passes remain
evidence for their own frozen binaries. No unavailable or canceled obligation is reused as a pass.

Archive-copy receipts map original paths, lengths and SHA-256 values to the retained bytes. JSON metadata that is
not a benchmark receipt is renamed to `.receipt.txt`; its content is unchanged. The main packet hash inventory
also covers these files. Documentation/gallery and design-system review found no visible control change, so no
published pixels require regeneration.
