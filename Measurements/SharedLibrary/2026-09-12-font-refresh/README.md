# Font availability refresh, September 12

This change preserves the font-selection behavior added to RedSalamander master in the canonical
library. A cache miss requests an updated DirectWrite system font collection. The new
`Typography::InvalidateFontFamilyAvailability` clears answers for one factory, or every factory
when passed null. It does not recreate retained text formats or trigger layout. Hosts serialize
invalidation with their font queries and keep it out of per-frame rendering.

Foundation coverage uses two isolated DirectWrite factories, seeds stale negative cache answers,
and verifies selective refresh and all-factory refresh. No font is installed or removed by the test.
The source hashes and executable identities in the [validation receipt](../../../Specs/Plans/Done/SharedLibraryReadiness/font-refresh-local-validation.json) identify the local
candidate based on e67bd1b. The implementation is committed at `13788e9`; RedSalamander's rebased
local candidate pins it, while RedXe retains the previously qualified ecad707.
Reviewed text copies use UTF-8/LF and omit trailing whitespace. The receipt records both copy and
original hashes; original byte streams remain under `Z:\RedSalamander.Perf\evidence\I19-20260909\font-20260912\raw`.

| Local check | Result |
| --- | --- |
| x64 Debug | All 18 suites pass; nine Menu desktop capability skips retained. |
| x64 Release | Initial run: 17 suites pass, Menu fails. Separate Menu retry and five additional consecutive Menu attempts pass. |
| x64 ASan Debug | All 18 suites and deliberate defect detection pass; nine Menu capability skips retained. |
| ARM64 Debug/Release/ASan Debug | All three cross-builds pass with zero warnings/errors; no native execution claim. |
| Skills, specifications, dependencies, formatting | Pass. |
| Gallery | All six images regenerated and reviewed. |

The Release failure is **“Right arrow on B1 focuses B11 in the submenu.”** The retained d192e47
Release Menu executable and a separate candidate retry both pass. This does not establish its cause
or justify erasing the first failure. Logs retain both attempts; the Menu assertion was not changed.
The five further local attempts use the same executable hash and execute the originally failing
case successfully. Their logs and original/reviewed hashes are retained in the validation receipt.
The cause remains unclassified; no assertion or baseline was relaxed. Native ARM64 qualification
of the font-refresh change remains open.

Gallery comparison against the prior reviewed images finds no typography/layout change. The light
sheet and embedded example are identical; four other sheets differ only in the indeterminate
progress animation at image rows 508–513. Test baselines remain unchanged.

Automatic complex-UI reports are unpaired and are retained without a non-regression claim. They do
not close the existing [paired resource acceptance](../2026-09-09/README.md) gate. RedSalamander is
rebased onto master `0dd0bd6d`; its local candidate `ba72f601` restores the exact clean library commit
through a process-scoped local Git transport mapping. Public publication is not yet claimed.
Consumer regression and packaging qualification continue locally at the user's request; no hosted
CI is used for this pass. Cross-builds do not replace native ARM64 execution.
