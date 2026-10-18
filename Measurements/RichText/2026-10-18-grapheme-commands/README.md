# Unicode character commands and interior ACP deletion

Bulk character movement now walks established Unicode 17 grapheme boundaries
with bounded forward traversal and overflow-safe signed counts. Collapsed
Backspace/Delete remove the containing grapheme when an explicit ACP is inside
it. Exact-boundary and explicit noncollapsed selection semantics are preserved.
TextField, multiline TextField, editable ComboBox and native/embedded rich editors
share the deletion policy. Rich queries reuse the prepared boundary index.

Qualified library fingerprint:
`6B5ED058F3E2B3E5C2EB479A7EEDE48D615CBAAB3B98DEDE229F41B355DF1B71`.
Collection used the feature checkout at HEAD 135667d with recorded dirty inputs,
TEMP=TMP=`Z:\DxUiPerfTemp\uia-ranges` and MSBuild node reuse disabled. Public API
revision 4, the 64-Ki UTF-16 source bound and 128-history bound remain unchanged.

Three predeclared before slots completed in 342.901 seconds. The unchanged
movement oracle passed, while legacy surrogate-interior Backspace produced
three units containing invalid UTF-16. Native and embedded rich fixtures failed
complete-grapheme deletion from an interior scalar ACP. Every original filtered
receipt, native/root log and canonical benchmark binding was copied before the
next invocation reused its output pathname. Their artifact hash is
`E226DE8C45CB301D31726D951E4902F983D527FC80F77FBA8561FA842B1B717A`.

After implementation, all **57 ordinary x64 suite runs** pass across Release,
Debug and ASan Debug, with zero native skips. Each profile includes 52 named
rich scenarios plus shared corpus and legacy-control cases. All 766 original
vectors exercise movement and collapsed deletion at every UTF-16 offset,
including out-of-range offsets, zero counts, signed extremes and independent
expected boundaries. Native/embedded provider tests exercise 65,536 units and
32,768 graphemes, actual moved counts, selection and history preservation.
Deletion tests retain source/interior selection through undo/redo. Explicit
noncollapsed selections can still remove only the selected combining mark.

Embedded passes 5,167 checks per profile and 1,000 warm composites with zero C++
allocations. A 49,152-unit / 16,384-element movement/deletion fixture requests
zero C++ allocation calls and bytes in each profile. The override measures C++
allocation, not Windows/CRT-internal allocation. The isolated ASan probe actually
diagnoses heap-use-after-free. Six x64/ARM64 builds have successful attestations;
ARM64 is cross-compilation, not native execution. The fixed six-slot follow-up
matrix completes all slots in 1,653.615 seconds within its 3,600-second overall
and 900-second child bounds. Full tooling, all five validators and clang-format
22.1.3 pass. Optional CMake readback is unavailable on PATH; native suites have
zero capability skips. Existing gallery inputs, shaping and styles are unchanged,
so published gallery images and consumer pins are retained.

Five fixed within-process trials use 6,144 source units / 3,072 graphemes:

| Median elapsed time | Before | After |
| --- | ---: | ---: |
| INT_MAX forward | 83.0029 ms | 0.0579 ms |
| INT_MIN backward | 82.7482 ms | 0.0553 ms |

These are descriptive observations from one fresh native process per revision.
The two measured oracle method bodies are byte-identical and independently
rechecked. No same-binary controls, quiet-host proof or paired Default acceptance
is claimed. The common single-backward-step path remains a follow-through item;
this packet establishes bulk bounds without claiming its speed improved.

The [clear/readback investigation](../2026-10-18-uia-boundaries/README.md) still
locates a blocking-Map boundary without scheduler/GPU causal attribution.
Performance acceptance, physical IME/AT and native ARM64 execution stay open.
The remaining Markdown/extensions, incremental layout and persistent storage
work remains in the active plan.

Native contains 57 exact suite/log pairs and the ASan probe. Build contains six
build/intermediate attestations and original compiler logs. Performance contains
seven original canonical reports with their comparisons: three before, first
affected Release, remaining Release, Debug and ASan Debug. Four current reports
bind the 57 successes. Raw preserves schedules, all three original failures,
first successes, fixed trial analysis, unchanged method-body hashes, validation
and formatting. Source is frozen replay evidence, not another implementation.
JSON receipt aliases retain original bytes.

Run `pwsh -NoProfile -File Verify.ps1 -Packet <this-directory>` to check the seal,
native/build/performance bindings, fixed schedules, original failures, exact
oracle spans, maximal command/resource observations and derived trial medians.
The packet does not require current build products to replay these checks.
