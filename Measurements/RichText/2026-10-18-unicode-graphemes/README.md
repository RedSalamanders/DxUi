# Shared Unicode 17 graphemes and bounded UTF-8 admission

The canonical pure UTF-16 service now implements Unicode 17.0.0 default extended
graphemes. Existing text controls, password-mask indexing, rich prepared logical
stops and UIA character commands share it. The fixed data includes 833 merged
property ranges and every one of the 766 original conformance vectors. Font
shaping and explicit source ACP geometry remain DirectWrite operations; valid
interior source selections are retained. Visual bidi/word navigation, remaining
rich transactions and the complete Markdown/extensions engine remain open.

The qualified library fingerprint is
`55DE3A71D32382F2DE7DC23D3A7E4EED6A3CDE40F93684C45FF83880F47E9C85`.
Collection used the feature checkout at HEAD 948e7d2 with recorded dirty source,
TEMP=TMP=`Z:\DxUiPerfTemp\uia-ranges`, and MSBuild node reuse disabled. The
64-Ki UTF-16 / 128-history limits and public API revision 4 are unchanged.

Before implementation, the original native corpus test failed at vector 1,
offset 0: the approximate helper joined an extender after a control, returning
2 instead of 1. Rejecting 65,537 ASCII UTF-8 bytes requested 131,127 C++ heap
bytes. Original root/native logs, the corpus failure receipt and the canonical
report/artifact binding are retained. The ordinary Embedded suite JSON pathname
was reused by a later run before archival, so that original JSON is unavailable;
its original failure log and benchmark binding survive. No replacement metadata
is represented as the original receipt.

After implementation, all **57 ordinary x64 suite runs** pass across Debug,
Release and ASan Debug, with zero native capability skips. Each profile includes
47 named rich-text scenarios, the complete Unicode corpus at every UTF-16 offset,
malformed/long-context cases, password-mask source geometry/hit/deletion and
native/embedded rich UIA navigation, selection, deletion and undo. Explicit ACP
geometry stays available inside a grapheme. The next slice tests collapsed
keyboard deletion from such an interior ACP and bulk movement costs; this packet
does not close those newly identified follow-through items.

Every profile records these deterministic C++ allocation observations:

| Operation | Observation |
| --- | ---: |
| Forward mixed-script walk | 49,152 UTF-16 units; 16,384 graphemes |
| Forward-walk allocation calls / requested bytes | 0 / 0 |
| Reject 65,537-byte ASCII input | 0 requested bytes |
| Reject 196,609-byte impossible input | 0 requested bytes |
| Warm embedded composition | 1,000 composites; 0 allocations |

Embedded passes 5,161 checks per profile. Caller-owned input is outside the
decoder rejection budget; Windows/CRT-internal allocations are not measured by
the C++ override. Exact three-byte BMP and supplementary-scalar capacity limits
also decode successfully. The specific isolated ASan probe diagnoses
heap-use-after-free, establishing that the sanitizer runtime actually executes.

All six x64/ARM64 configuration builds have valid attestations. The initial
six-profile driver stopped after five attempts in 1,811.691 seconds: ARM64
Release compiled with zero warnings/errors, but its build attestation was
correctly refused after the generator's console status wording changed during
the build. Its output, original MSBuild log and unattempted final slot remain
retained. Restoring the exact original generator bytes restores the complete
compiled-input identity to the successful native runs' identity. A separate
predeclared two-build recovery passes ARM64 Release and ASan Debug in 387.945
seconds, within 900 seconds overall and 600 per owned child. ARM64 is
cross-compilation only. Neither a refused build nor an unattempted slot is
silently counted as an original pass.

All five validators, the full tooling suite (125 profiling cases and four
Unicode generation fixture cases included) and clang-format 22.1.3 pass. The
full tooling run used the wording-only variant; the restored generator's four
fixture cases pass again. Other tested executable code is unchanged. The optional CMake readback
probe reports CMake unavailable on PATH; this is distinct from the native suites'
zero capability skips. Reviewed gallery inputs, glyph shaping, theme/style data
and populated examples are unchanged, so published gallery pixels are retained.
Consumer pins are unchanged.

Post-archive validation first refused five canonical files without an adjacent
Performance explanation; the original refusal remains in Raw/Closeout. Adding
that explanation repairs specification validation. The other four validators
pass, and a private packet copy with one changed native-log byte is rejected
by the expected SHA-256 check. The original packet remains byte intact.

The five original canonical reports cover the failing baseline, first six-scope
Release success and final three profiles. All are unpaired. No timing or memory
acceptance, baseline, threshold, judge or policy changes follow. The
[clear/readback investigation](../2026-10-18-uia-boundaries/README.md) still
locates a blocking-Map wait without scheduler/GPU causal attribution; performance
acceptance, native ARM64 execution and physical IME/AT gates remain open.

## Retention and verification

Native holds 57 exact suite/log pairs and the sanitizer probe. Performance holds
all five original canonical reports/comparisons. Build holds six actual build
and intermediate attestations and original compiler logs, including the refused
attempt. Raw retains both collection schedules/completions, original failures,
first successes, source hashes, restoration proof, validation and formatting.
Native/metadata JSON uses `.receipt.txt` aliases without changing its bytes.

Inputs, Generated, Generator and Source are immutable replay evidence. Canonical
build inputs remain under src/include/Tests/Tools/ThirdParty. Unicode input URLs,
hashes and the included license are retained; [provenance](../../../ThirdParty/Unicode/README.md)
explains regeneration. The generated corpus header is byte-identical to the
one used for the original failing native test; later test assertions also cover
nearest snapping and long context.

Run `./Verify.ps1 -Packet <this-folder>` to verify the seal, original failure
predicates, native/performance/build bindings, refused/recovery inventories,
restored input identity and resource observations. It regenerates expected data
in read-only check mode using the frozen generator and inputs. This verifies
retained evidence; it does not rerun the native executables or qualify future
source changes. Collection drivers document the original fixed schedules.
