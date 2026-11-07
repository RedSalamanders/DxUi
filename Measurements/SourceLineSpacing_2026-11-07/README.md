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
dump, images and source remain separately retained. Complete current-SDK native and
platform qualification, consumer adoption, physical input and performance-policy
acceptance are separate from this focused proof.
