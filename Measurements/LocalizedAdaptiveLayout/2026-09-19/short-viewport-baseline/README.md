# Constrained localized scene — unchanged-library witness

The unchanged `f7fe1e4` library fails the shared fixture's requirement that measured French action
labels fit their rectangles and the viewport. The baseline adapter uses the existing horizontal
StackPanel, measured widths and a fixed 32-DIP height; the candidate uses measured wrapping and
multiline Buttons. The rest of the scene and acceptance checks are identical.

Fixture SHA-256: `25F4A6935D39F605D0BC2E6B589DF46E4AC158392951D5B81D12B1CD598C4C6F`.
The exact fixture and baseline runner sources are retained here. This full-scene witness was
collected against isolated unchanged production source after the earlier action-only baseline
and implementation; it does not claim to have preceded those changes. The earlier measured
action-layout baseline remains the pre-implementation performance witness.

Command: `test.ps1 -Configuration Release -Platform x64 -Suites Embedded` in
`C:/Users/eric/.codex/worktrees/layout-performance-baseline/DxUi`.
Log: `C:/RedSalamander.Perf/evidence/i26-ui/dxui-short-baseline.log`.
Its common benchmark is retained but unpaired, with conspicuously slower timing than the earlier
baseline processes; it is not selected as the performance comparison reference.
