# Foreground fixture and independent-assignment follow-up

This DxUi-only packet retains the lead's checked results for draft PR #70 and the focused follow-up.
The manifest records every original file's SHA-256 and length, plus archive hashes and verified entry counts.
Every sealed entry was reopened and compared with its original bytes. No receipt was relabelled.

`ed70-native-ci.zip` contains all six native artifacts from CI run 37996368753, PR head
`ed70a8388fa137f293fbb7e5f81d0c36bb6dcab8`. Actual compiled/tested source is its merge candidate
`2cb2b53b81f4c11fc9f50bce8db7bc0f66e80ec4`; the raw reports retain that provenance.
All three x64 profiles passed all 21 suites with zero capability skips. Release's two additional menu
resource fixtures and all four relocated x64 consumer profiles passed, including standard and disabled
ASan STL annotations, API revision 4, twelve EXE/DLL ownership probes and ten rejected pin/build mismatches.
All three ARM64 profiles passed 19 noninteractive suites with zero skips, then failed Menu and NativeTextInput.
The read-only desktop probe passed; the logs show a foreground mismatch. ASan's abort supplies no memory-defect
diagnosis. The test-only activation repair requires a fresh ARM64 run. The lead independently read all six job logs.

`ed70-ci-job-logs.zip` retains those six complete job logs and the paired job log.
`ed70-paired-diagnostic.zip` retains the completed three-scenario, 12-block study; all 144 report hashes
were independently checked against its retained judge-input attestations. The official result is inconclusive:
the measured base has no independently qualified policy. Its schema-2 globally balanced assignment is also
superseded by the [randomization correction](../RandomizationDesignCorrection50/README.md).
Its timing verdicts cannot establish current-design non-regression or qualify the proposed policy.

`local-guard-qualification.zip` preserves complete Windows validation 53, clang-format 22.1.3 checking,
the fresh x64 Debug build and both whole noninteractive WindowHost/InteractiveLease suites, with zero skips.
The lead independently recomputed the current build/run identities against source, dependencies, artifact
bytes and environment, and checked the executable SHA-256. Its verification record retains exact identities.
The accompanying complex-UI measurement is unpaired reporting only. This scoped result proves the activation
blocker and lease regressions; it is not full native, foreground, ARM64 or performance qualification.

The local Debug Menu/NativeTextInput lease confirmation timed out without taking the desktop. Its raw parent
log and lease result are retained separately; foreground, keyboard focus and pointer stayed unchanged. Neither
foreground suite ran, and this cancellation supplies no qualification or capability skip approval.

The candidate policy remains unqualified. The independently reviewed repair makes block assignments
independent, versions the study/judge, checks exact replay, and binds approved assignment-generator source
as well as judge identity. Fresh A/A and baseline/candidate studies and final-source native CI remain required.
