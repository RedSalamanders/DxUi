# RichTextEditor UIA range commands

The retained before.json is the unchanged Release Default fixture measured before
source changes at 71059fb9. Only measurement files were dirty. It does not contain
RichTextEditor and establishes no controlled performance acceptance. The first
attempt was refused because writing its report changed sourceDirty during the
run; Raw retains its output and error, not a qualified baseline. The successful
retry wrote under .build before being copied here. Historical D4c warnings remain
open. No baseline or threshold has been replaced.

## Functional source-command qualification

[qualification.json](qualification.json) binds source fingerprint
E3437165FCDB1374DE2AA8A2AE0AA791CB4581AB9528180ACA91D90508ECFDCA
to six successful builds and 57 ordinary x64 Debug/Release/ASan suite runs with
zero capability skips. There are 41 named rich-text scenarios per x64 profile,
including 14 accessibility cases. Ten new native/embedded cases cover selection
union/subtraction/rejection, ordinal Unicode text and read-only attribute search,
scroll alignment/clamping/caret/empty/hidden/RTL behavior, remaining range units,
cloning/comparison/children/arguments, retirement and thread ownership. Existing
real-client tests now invoke add/remove/scroll through UIA and check selection
text and TextSelectionChanged delivery through the actual parent bridge.

Embedded profiles pass 5,155 checks and 1,000 warm composites with zero C++
allocations. Caret/visible query metadata stays at 432 requested bytes in
Debug/ASan and 400 in Release at both 32 and 65,536 source units, below 4 KiB.
This bounds query metadata, not snapshot/layout or Windows allocation costs.
ASan's actual heap-use-after-free probe passes. All validators/tooling and pinned
clang-format 22.1.3 checks pass; final specification checks include these receipts.
ARM64 receipts establish cross-builds only. Desktop focus-taking and actual AT,
rich structural/style attributes, semantic anchors, full Unicode grapheme/bidi,
complete Markdown/extensions and incremental/large-document work remain open.

The initial test build used the private scroll member name instead of the public
scrollOffsetDip diagnostic field and failed. The correction's first builds and
14 focused cases passed; later explicit unit-fallback coverage and its public
comment correction preceded the final matrix. Raw preserves those exploratory
records separately; none is restamped as the final source's qualification.
The gallery needs no regeneration: the fixture and rendering are unchanged,
while commands use existing scroll/selection painting and preparation.

## Performance acceptance on hold

[performance-summary.json](performance-summary.json) retains both serial Release
comparisons and their original logs. Clean frame p95 increases from about
0.430 ms to 0.452 ms (+5.02%) and 0.504 ms (+17.05%). Both comparisons return
advice-required. Other metrics do not flag regressions in those runs; this does
not make the repeated tail warning acceptable. No further repeat is used to
select a passing report.

The fixture, toolchain and environment identities match; both runs are still
unpaired diagnostics. The 83-control/1,000-row Default fixture does not instantiate
RichTextEditor. No causal attribution or controlled performance acceptance is
claimed. Functional qualification is separate; acceptance/merge and D4c are held
pending developer advice. Options are to keep the reviewed code as WIP while
deferring performance acceptance, or investigate with controlled baseline/candidate
profiling. No tradeoff budget or regression waiver is approved here. Historical
warnings, consumer pins and the original concurrent checkout are unchanged.
