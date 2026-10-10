# Independent-assignment study and native follow-up

This DxUi-only packet preserves original evidence for PR #70 head
`d5dfefcc9f0e27d5ad100484b38b533bcfaf84d0`. CI run 38000606497 compiled and tested
merge candidate `a231a54a54a923b42bc39f27014ac538fdd787df`. No record is relabelled as a later candidate.
The archive manifest records each original length/SHA-256 and every archive hash. Every entry was reopened
and independently compared with its original bytes.

`d5-native-ci.zip` retains all six native artifacts and the actual harness-generated Release gallery.
All three x64 profiles passed all 21 suites with zero capability skips; Release's two additional menu
resource fixtures and all four relocated x64 consumer cases passed. Each consumer case checked API
revision 4, twelve EXE/DLL ownership probes and ten rejected pin/build mismatches. All three ARM64
profiles passed 19 noninteractive suites with zero skips, then failed Menu owner activation and
NativeTextInput foreground activation. Their read-only desktop probes passed. The target was active
and focused while another HWND remained foreground; the logs do not identify that window's process.
The ASan Menu abort is the test failure path, not a diagnosed memory defect. New hosted-lease execution
requires fresh native qualification. The lead checked all six job logs and all 126 main-suite receipts.
Native artifacts omit executable bytes: this audit verifies report bytes and recorded identities,
and does not claim an independent rehash of unavailable CI binaries.

`d5-paired-ci.zip` retains the three-scenario, twelve-block independent-assignment study, seed 942072014.
The lead independently checked all 144 report hashes, 1440 raw rounds, identity bindings, exact median/peak
budgets and all 78 aggregate decisions using a separate enumeration implementation. All three sets have
zero aggregate regression/improvement flags and agree with the immutable-base judge. The official result
is **inconclusive / policy-review-required**: the measured base has no independently qualified acceptance
policy. The packet cannot qualify its own proposed policy. `d5-review-verification.zip` retains those
audits, all seven job logs, the audit implementation, clean native-filesystem Linux validation 53 and
six-profile PrePush accounting. Deferred CI accounting is not a pass.

`local-d5-aa-diagnostic.zip` retains a completed local Release x64 A/A study, seed 2026101001, twelve
blocks and the same three scenarios. Both labels used one d5 worktree and one executable, independently
checked against its actual SHA-256 `BCA6C801D79060162AA3FFFF62376FDE333A2AA3CBCB0564000B0A569DDE27C6`.
The lead independently checked all 144 receipt hashes and recomputed all 78 aggregate decisions; all
three sets have zero flags. However, 70 of 72 short same-binary controls were unstable (23/24 Default,
24/24 MultilineGrid, 23/24 MultilineGridDistinct). Every timing/FPS/memory phase-metric cell had an excursion:
all 54 remain inconclusive under the A/A review guidance. All 24 exact-budget cells were stable.
No foreign compiler/linker build was observed during this study. This evidence shows no code regression,
but does not establish calibrated timing/memory acceptance. A quiet-host rerun is required before policy
seeding; do not widen bands or silently rebaseline to close this gate.

`local-d5-paired-invalid.zip` preserves the local baseline/candidate attempt that stopped after one
receipt. The baseline driver's file retained its old 6322-byte content followed by 387 NUL bytes; it did
not match the 6709-byte harness. The lead confirmed distinct file identities and the exact old prefix.
Isolated copy checks did not reproduce the corruption, so its cause remains unproven. The receipt is
invalid, supplies no performance conclusion and is not combined with a retry. The verified-overlay
repair now checks actual destination bytes before build/reuse and around measurement, checks retained
original backup hashes and attempts every restoration. Fresh studies remain separate obligations.

The developer's accepted accessibility coalescing tradeoff is unchanged: batch costs fell 95.92%/98.43%,
with fresh first-query costs of about 431.8/321.9 microseconds. Its original workload, historical inference
correction and larger/query-heavy consumer limits remain in their own retained packet and performance contract.

`local-guard54.zip` retains complete Windows validation, all tooling, clang-format 22.1.3 checking,
the fresh canonical x64 Debug build and both whole WindowHost/InteractiveLease scopes, with zero skips.
The lead independently recomputed their exact source/build/run identities and actual executable hashes.
All six native lease verification cases passed, including private-desktop confirmation/warning behavior,
no-dialog deliberate runtime failure and rejection of each unverified hosted marker. These tests took
no personal desktop focus. The associated performance report is unpaired reporting only. Initial format
54 failed and is preserved alongside the repaired successful check. This packet does not close ARM64
foreground, full final-source native profiles, performance-policy or physical consumer adoption gates.
