- Fixed `performance-timing.ps1 -Purpose LoadSensitivity`, which rejected every session because it never took the GPU
  engine snapshots the admission rule needs ("the other-process GPU load was not recorded"). A load-sensitivity
  session now reads the `GPU Engine` counters before and after its four runs like every other purpose, so its quiet
  runs are judged on a recorded other-process GPU load (the study's own run and load children are left out by the
  per-instance deltas); its loaded runs stay judged on nothing. The tooling test simulates a load-sensitivity study
  that is admitted with a recorded figure and one that a busy GPU rejects. Validation: `Tools/tests/Test-FrameTiming.ps1`.
