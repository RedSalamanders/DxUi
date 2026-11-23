- Add the performance fixture framework (performance and motion plan W0.4, part 1). Each fixture `FX1`-`FX14` has its
  own header with one `Definition` (hosts, size and DPI variants, phases of deterministic steps, budgets, timed phase,
  sources) and one `Scenario`; a catalog lists all fourteen, `FX5` (ComplexUi) built and the others declared from the
  plan. One definition serves every consumer: an embedded runner (every work counter per step and phase, the C++
  allocation sink and the W0.3 memory fields) and a WindowHost runner, the hardware timing harness
  (`performance-timing.ps1 -Fixture FX<N>[@variant]`), a determinism gate that runs every built fixture twice on WARP
  per host and names the phase, step and counter of any difference, step indexing for the incremental-versus-fresh
  oracle, and gate tests. The W0.2 timing fixture and the W0.3 memory receipt now run the FX5 definition (`--memory-receipt`
  writes the framework receipt), and an Embedded test requires the framework's FX5 counts to equal the W0.1 receipt's.
  `performance-fixtures.ps1` writes a fixture's receipt under `Measurements/PerformanceAndMotion_2026-11-13/<FX>/`
  with recorded provenance and a generated README; timing stays
  "not measured" outside a hardware calibration. FX5 counts are unchanged: a dirty frame paints 83 controls and makes
  54 C++ allocations in Release; a clean frame allocates and creates nothing. The other thirteen fixtures, their
  receipts and every hardware timing field remain open.
