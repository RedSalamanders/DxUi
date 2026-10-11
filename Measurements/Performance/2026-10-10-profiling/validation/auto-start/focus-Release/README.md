# x64 Release foreground execution

Clean automatic-start candidate `02961a88321c75ccd39090873f48977206d6dfdb` runs four complete suites through
`test.ps1 -Interactive -InteractiveAutoStart -SkipBuild -SkipTooling`. The visible dialog offers its approved
30-second countdown. Start is clicked in this run; the actual native receipt records `started`,
`autoStart=true` and `confirmationSeconds=30`, rather than labelling it an automatic expiry. Private native
regressions separately exercise the no-click automatic expiry, cancellation, early Start and deadline races.

| Complete suite | Native seconds | Exit | Skips |
| --- | ---: | ---: | ---: |
| Menu | 61.0 | 0 | 0 |
| NativeTextInput | 35.4 | 0 | 0 |
| MenuResources | 5.6 | 0 | 0 |
| MenuResourceScaling | 13.4 | 0 | 0 |

The lease verifies foreground/focus unchanged and pointer restored. The copied canonical plan contains exactly
the four ordered unfiltered commands and expected executable/log paths. Native and lease executable hashes,
actual/requested automatic policy, positive full-suite log markers and the complete actual 26-field post-focus
state are independently checked before root acceptance. Every report has the canonical v2 build/tool/SDK/
dependency/artifact guard; no filtered or historical receipt substitutes for this execution.



`copy.receipt.txt` binds the thirteen parent/plan/result/suite/benchmark files to their generated originals.
`archive-copy.receipt.txt` binds all immutable external files to this leaf. Later runs can replace generated
paths without changing the archived copies. The parent performance report is an unpaired observation;
performance, native ARM64, physical-input and consumer qualification remain separate.
