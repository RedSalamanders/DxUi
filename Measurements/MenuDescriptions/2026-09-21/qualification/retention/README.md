# Repeated common-scene retention investigation

Both executables run the same final driver (`BenchmarkMain.h`) and scene/phase probe
(`ComplexUiBenchmark.h`): `--benchmark-retention <output-prefix>` performs sixty full
create/render/hide/destroy cycles in one process. Raw inner reports are byte-for-byte
`*-raw.txt`; each side's JSON index binds their hashes, exact executable/library identity,
fixture inputs, completion timestamps and extracted per-cycle medians. All runs are x64 Release.
The unchanged library remains `78b3`; only its benchmark driver was changed to match.

For cycles 40–59, clean private medians are 34,385,920 bytes (original) and 34,357,248
(candidate); dirty medians are 34,385,920 and 34,328,576. The early ~1.1 MiB difference
does not persist as a fixed retained cost. Between-cycle entry memory oscillates substantially:
26,488,832–33,542,144 original, 21,815,296–33,439,744 candidate. This shows allocator/runtime
variation, not an exact allocation-stack attribution or proof of an indefinitely bounded soak.

Timing is not accepted: tail dirty FPS medians are 434.43 versus 396.11, and the last candidate
cycle falls to 228.13. The desktop was not controlled for outside activity, so neither a
timing pass nor a cause for that slowdown is claimed. Retain the earlier short-run flags;
do not substitute this investigation for the normal paired acceptance benchmark.

See the [overall findings and remaining gates](../../README.md). The separately measured
~1.15 MiB extra while a twelve-entry described menu is open is a different workload/cost.
