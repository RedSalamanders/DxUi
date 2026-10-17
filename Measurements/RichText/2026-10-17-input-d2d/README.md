# D2d rich-text input and accessibility

The before.json Release x64 Default receipt was captured at clean 0c939579
before D2d implementation. The unchanged DxUi synthetic fixture does not
instantiate RichTextEditor. This is a retained unpaired baseline, not controlled
non-regression acceptance. Historical D2b/D2c warnings remain retained.

D2d adds shared native/embedded source text adapters, silent composition preview
and rollback with single-transaction commits, mapped geometry and shared UIA
Document/Text/Text2/TextEdit providers. The source-adapter qualification is bound
to final library fingerprint
FC6F53842E58EA9EB63181CC005E51913617C80DA6ACD194CE96649B7F6F6941.
The [qualification receipt](qualification.json) records artifact hashes and raw
logs for six successful builds, 57 ordinary x64 suite runs with zero skips,
31 named rich-text control scenarios per x64 profile, all five validators/tooling
(with the failed full command and affected retry retained), pinned clang-format
22.1.3 and the actual ASan heap-use-after-free diagnosis.
All ARM64 profiles are cross-build-only. Real IME/assistive technology and
physical/focus-taking tests remain separate gates.

Each x64 embedded profile passes 5,155 checks, including 1,000 warm composites
with zero C++ allocations and 12 rich-text warm composites without new layout,
raster or surface work. Eight warm caret/visible-range query pairs request
432 C++ bytes (Debug/ASan) or 400 bytes (Release) for both 32 and 65,536 source
units, below the 4,096-byte bound. This bounds query metadata, not the published
document/layout memory. Composition and all five themed gallery sheets plus the
supplied-device example were inspected. Gallery pixels match the D2c captures.

Native IMM/ACP protocol, embedded focusId/revision ownership, single committed
history transactions, preview rollback, stale services, rebinding and callback
retirement have behavioral tests. Actual in-process UIA clients exercise native
and application-bridged providers, exact source text, navigation and text events.
Mapped ranges preserve Unicode/CRLF boundaries and hidden-marker empty bounds;
read-only, disabled, backward caret and explicit role overrides are covered.
Bounded live ranges read the current source at clamped offsets; semantic anchor
tracking, rich styles/structure, annotations and complete shared UIA scrolling/
selection-add/remove commands remain engine/AT acceptance work. This does not
qualify complete RichEdit/Markdown behavior or the D2a incremental model/layout.

## Performance diagnostics remain open

[The summary](performance-summary.json) retains both serial Release comparisons,
their raw logs and the unchanged original baseline. Both return advice-required.
The first flags private memory about 4–5% higher; its single quiet repeat flags
dirty private bytes +7.6%, peak +7.7% and working set/peak +2.5%. Clean memory
does not repeat its first warning. No further repeat or rebaseline is used to
select a passing report.

The Default fixture contains 83 controls and 1,000 rows, without RichTextEditor.
The pre-change C: compiler temporary directory differs from the final Z:
temporary directory in the attested toolchain identity. The reports are legacy
unpaired diagnostics: neither source causality nor controlled performance
acceptance is established. D4c stays open; no warning is accepted or silently
closed. Controlled matching-identity baseline/candidate and dedicated rich-editor
stress/fault qualification remain required before production acceptance.

## Exploratory failures

The final full validate.ps1 run found a README link to qualification.json before
the collector had created that file. The premature link was removed; the affected
validate-specs.ps1 retry passes. The full run's five other steps, including the
complete tooling suite, and the failed specification result are retained together
with the successful retry rather than restamping the failed command as exit 0.

The first build found a test calling the client's private request helper. A typed
public DocumentText helper now exercises the real transport. The retry failed
with MSVC C1088 while flushing a compiler intermediate on C: (no space left).
Subsequent task-owned compiler TEMP/TMP use .build/compiler-temp/d2d on Z:;
no unrelated file or process was removed. Failed logs are retained separately.

The first executable pass found missing COM initialization for the WIC capture
fixture. Focused cases then exposed the native ACP adapter's concrete-only
TextField/ComboBox apply/rollback dispatch and the shared UIA MoveEndpointByRange
stub. D2d adds RichTextEditor dispatch, actual endpoint movement and coherent
current-source range reads. Embedded mapped geometry/rebinding and real native/
application-bridged UIA client navigation/text events passed before those fixes;
those exploratory focused runs do not qualify the final source or full suites.

The first guarded Debug native launch was refused before ordinary suites because
its distinct runtime TEMP/TMP produced another resolved toolchain hash. A read-only
three-environment identity reproduction kept source, resolved build inputs and
artifacts identical; only the exact compiler-temp environment reproduced the
attested identity (attestation-temp-diagnostic.json). Native/candidate measurement
processes now use that environment; no receipt was overridden or restamped.
The original pre-change TEMP differs and strict paired acceptance stays D4c.

Debug and Release first-matrix runs passed all 19 ordinary suites each, including
all 31 rich-text control scenarios. Final review then removed unnecessary full
source copies from rich caret/visible/composition/conversion ranges and from
endpoint movement. A new supplied-device fixture bounds warm caret plus visible
range C++ allocation requests across 32 and 65,536 source units. The entire matrix
is rerun for that final source; first-matrix passes are not reused as its proof.
The temporary-directory identity reproduction is explicitly historical at source
7DA054400FD81867938E32F74A89860FA5935B1405B40A045E1F1551844DFEDC.

The first range-budget fixture dereferenced a missing provider after accepting
S_FALSE on a dirty focused frame. The retained ASan trace identifies that fixture
line directly. The fixture now prepares focus before attachment, requires S_OK
and a non-null provider, and verifies the actual published source length before
measuring. It measured 432 requested C++ bytes for caret plus visible ranges at
both 32 and 65,536 units. The next exploratory run exposed that this probe left
its counters in the suite's later absolute warm-composition check. The probe now
saves/restores the surrounding counters. Both failed fixture logs are retained;
neither is a qualified suite pass. Final review also preserves explicit base
accessibility-role overrides before RichTextEditor's default Document role.
