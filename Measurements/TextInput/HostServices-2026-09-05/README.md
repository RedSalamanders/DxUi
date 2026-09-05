# Application-side text services: validation and open performance gate

The component adds shared TSF/clipboard services, focus identity, prepared geometry, native clipboard bounds,
private-clipboard control tests and a public supplied-device text sample. The source and executable fingerprints
in each raw receipt identify the tested uncommitted component before its final evidence-only archive commit.

All 18 local x64 suites passed in each configuration; validation.md records their current executable SHA-256,
completion and skips. Menu had nine interactive capability skips in each configuration. Both ARM64 configurations
cross-built without warnings. The public --text-input --output sample rendered and was visually reviewed;
Embedded passed 1,588 checks with zero C++ allocations in 1,000 warm composites. Exact-commit native CI and relocated
consumer results must follow the component commit. This does not establish real IME, touch, UIA or presented hardware acceptance.

Performance acceptance remains OPEN. Before measurement the RedXe integration plan declared A1/B1/B2/A2 in Debug
then Release: A is unchanged ad88833, B is the current component, both the same complex-ui-v2 fixture. Only the
measurement process and its children used affinity 0xFFFF; the prior affinity was restored. No other process or
power setting was modified. Measurements began after the build matrix completed with no compiler/linker active.
Both nearest pairs were required to pass; all raw runs and comparisons are retained. Thresholds are unchanged.

All four nearest-pair comparisons flagged investigation bands: Debug B1 dirty CPU composition +16.9%; Debug B2
clean frame p50 +5.5% and preparation p95 +11.1%; Release B1 clean CPU composition +5.75%, private memory +4.02%
and working set +2.28%; Release B2 dirty CPU composition +5.26%. See raw JSON for exact units and all other metrics.
The flags differ between repetitions, so this experiment does not establish one repeatable causal regression.

Control analyses compare each source against itself, using the already retained runs rather than rerunning until
green. Both unchanged-baseline comparisons and the Debug candidate comparison also fail investigation bands.
In particular unchanged Release baseline private memory rises from 24,023,040 to 25,550,848 bytes (+6.36%), more
than the candidate's first-pair memory difference; baseline clean frame p95 rises from 0.569 to 0.672 ms. Release
candidate repeat is within bands. These controls show fixture variability; they do not waive the acceptance gate.
Earlier native-store and original input-component receipts remain intact in adjacent evidence directories.

No performance regression was accepted, no baseline was replaced, and no RedXe dependency adoption is authorized
by these reports. Continue integration validation in an isolated consumer checkout while investigating the gate.
