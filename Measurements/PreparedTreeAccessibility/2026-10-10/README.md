# Prepared Tree accessibility finish - 2026-10-10

Candidate source: `8e446386e06d10eaf3035cdd703206e75eaf7ab6`; source fingerprint
`F8466F65BBF445108CBF32AC26019ED8949A31BB78A07DFB38B3D6CD6BE098BE`.
The PR merge ref `60db6a0c328ecc210559da2c8efc0770c9230a50` has the same complete tree
`422bb22c2a311b18cfe910731152647ee0ae9a26`.
The current-main baseline is `60e1cbff8f7a75757e9447073965d09101d9833c`, fingerprint
`E6DB772710D1C2050CC2A7B6F3D38AC07F1A21EB84EAB046A209AEFB70591570`.

The candidate preserves current main's borrowed-model guards, Grid assignment identity and independent Tree
focus/selection. It fixes the transient embedded sender's null creation-snapshot runtime-ID access, restores
native structure delivery, and moves embedded control-property delivery to the same event-driven work item as
structure invalidation. Properly marshalled STA senders retain weak control identity and owned scalar/BSTR
event values. Pending properties coalesce per live control/property key and retire on control replacement.

Local x64 Debug and real ASan Debug each pass Embedded, Accessibility, Tree, Grid and WindowHost with zero
capability skips. Embedded reports 4,787 checks, including interaction-only preparation, real client owner reentry,
held structure/property bursts and retirement of all 31 intermediate sources. Accessibility runs the four new native
cases, including invalid-count fallback and a capture that replaces its control. Release builds successfully.
All six repository validation steps and format checking pass. The ASan detection probe independently catches its
deliberate error. These results do not imply personal-desktop, real screen-reader, hardware or consumer-product qualification.

The current Release snapshot benchmark runs 12 rounds per mode in the same executable: 10,000 rows and 1,024
UTF-16 units per name. Fallback performs 10,000 model item reads, 20,016 C++ allocations and 41,924,587 allocation
bytes per snapshot; prepared rows perform zero model item reads, 15 allocations and 4,548 bytes. Both modes perform
21 model count reads. Both verify the full literal offscreen last-row name and safe provider disconnection. This
comparison demonstrates the changed capture work; its timings are diagnostic and do not qualify the common scenes.

The original failing Debug/ASan source, binaries and PDBs remain under the local retained directory
`C:/Users/eric/AppData/Local/Temp/dxui-p1-red-20261010-r4`. The frozen red input archive verifies all 161 source
copies against their original SHA-256 manifest. The ASan report shows `GetRuntimeId` accessing the absent snapshot
at `0x128`. The separately retained 20-second synchronous property failure is diagnostic: its exact binary was
overwritten before preservation and no exact-product qualification is attributed to it.

Historical 6 October receipts retain their original sources and allocation design. They do not qualify this candidate
or the current independent-order performance policy. No consumer dependency pin changes; RedPrism production
prepared-row adoption, charged-source admission/retirement and actual large-row host qualification remain HOLD in
the API-4 consumer handoff register.

Hosted [PR #72 run 38052470501](https://github.com/RedSalamanders/DxUi/actions/runs/38052470501) was independently
read back from six native artifacts. The audit verifies exact merge/source identity, suite logs and results, all
24 native prepared-Tree cases and 24 embedded client cases, the ten preserved Grid UIA cases and first-presentation
case in each profile, consumer provenance, sanitizer detection and completed hosted foreground restoration.

| Profile | Passed suites | Foreground not executed | Passed consumer fixtures |
| --- | ---: | ---: | ---: |
| x64 Debug | 21 | 0 | 1 |
| x64 Release | 23 | 0 | 1 |
| x64 ASan Debug | 21 | 0 | 2 |
| ARM64 Debug | 19 | 2 | 1 |
| ARM64 Release | 19 | 2 | 1 |
| ARM64 ASan Debug | 19 | 2 | 2 |

All eight consumer fixtures verify 12 native module checks, ten negative checks and API revision 4; ASan profiles
include annotations enabled/disabled. Both sanitizer probes detect their deliberate heap-use-after-free. ARM64
Menu and NativeTextInput exit 24 with an explicit foreground-lease refusal before execution in every profile.
These six suites are unexecuted, not passed or discarded. Gallery generation completes on x64; ARM64's gallery
step is skipped after the foreground failures. The changed library behavior is exercised natively in all six profiles.

Both fresh x64 Release studies use current main, an identical harness on each side and 12 independently drawn
ABBA/BAAB blocks in Default, MultilineGrid and MultilineGridDistinct. Each has 144 raw receipts, 1,440 rounds and
7,200 timing fields. Independent audit checks receipt SHA-256 before/after judgment, recomputes all timing summaries
from 40 raw samples per round and recomputes both algorithms' metric decisions. All 24 exact resource slots retain
equal medians and raw maxima. No regression flags occur under either algorithm in either study. Hosted legacy
judgment labels two sub-microsecond clean preparation differences as improvements; the migrated method does not.
The local repeat has no improvement/regression flags in either method. No performance improvement is claimed.

The hosted study has 68/72 unstable same-binary controls; the local repeat has 72/72. Both formal verdicts are
**inconclusive**, because the unchanged base acceptance policy explicitly remains **policy-review-required**.
The pull-request paired check and ARM64 native jobs remain failed. This evidence is not an all-green CI or a
qualified performance pass. Formal Q22 acceptance and ARM64 foreground qualification remain open in the existing
production-review plan. The user's requested integration is supported by the scoped library behavior and exact
resource evidence; it closes no production, consumer, real IME, physical-touch or screen-reader gate.

The retained `evidence.zip` and `evidence.manifest.txt` preserve every source/product identity, raw receipt, failure
log, CI log, native audit and paired audit. The archive's entries are read back and hashed before committing.
Documentation closeout changes no compiled source, test, harness, dependency or build input; the validated functional
commit above remains the immutable qualification source. The final merge is not represented as a newly executed CI run.

Archive SHA-256: 

BCF0EBCB677A144937CC4EF46C6A947C3537EFE26ED9C4CF15F1BE2540387F9D

Manifest SHA-256: 

6D1FB578CED2A0C7F6ABFA20CF0FF427CB1C8510280B9BDC8C1EF252600EB800

