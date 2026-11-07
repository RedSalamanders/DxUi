# Incremental Source line spacing

The [original comparison](counterfactual.json) uses the same compiled fixture with
the retained SDK and two candidate translation units. In a 1,000-line, 59,000-unit
document, one character changed at line spacing 1.6 formerly reshaped all 59,000
units. The candidate prepares 1,062 units and reuses 55 chunks, matching the bounded
work of native spacing. Cooperative turn counts and raw timing samples are retained
for both settings; timings observed during other native work are not formal paired
performance acceptance.

The native geometry oracle checks wrapping, both reading directions, CRLF, terminal
newlines and spacing multipliers 1, 1.6 and 4 against a complete DirectWrite layout.
All 162 WARP pixel pairs match exactly across natural and uniform Source profiles,
fractional fonts, large origins, DPI and transforms. The embedded editor retains
its geometry when unchanged and restores exact source through one Undo with the
expected source notifications. Mixed-style Rich v0 retains its compatibility path.

Original executable/PDB pairs, the shared compiled fixture, source, raw logs and all
324 capture files are archived and independently byte-verified. The first candidate
fixture confused source ACPs with CRLF-normalized display offsets; its failure record,
dump, images and source remain separately retained.

The [final correction](final-correction.json) retains the subsequent strict 1.65
pitch failure and its original source/images. Sequential floating-point line
accumulation, line-coordinate mapping and baseline mapping before pixel snapping
preserve whole-layout height, caret, hit affinity, selection and exact raster
results, including skew. The intermediate null-target and skew failures are also
retained; their unsuccessful results are not credited as validation.

At `21f63d32`, [complete local qualification](qualification.json) passes all six
zero-warning builds, 66 ordinary native x64 suite executions without skips and the
real ASan detector. Every original source/native/run/failure-image member is
independently byte-verified. [Gallery provenance](gallery.json) records six reviewed
harness captures from that clean source. ARM64 is cross-build coverage; native
ARM64, consumer adoption, physical input and performance-policy acceptance remain
separate.
