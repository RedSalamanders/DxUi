# Hosted qualification and raw timing verification

PR run [38010688230](https://github.com/RedSalamanders/DxUi/actions/runs/38010688230) measured head
`7f2e9595a26f8a84cd11f9b769abb16e70d9716f` as actual merge
`0ebf4820dce236f41e6916fa893fac29adbfcccb`. The lead checked its two parents and equal head/merge tree through
the GitHub commit API. This packet retains native artifacts and independent review; the paired study has its own packet.

| Profile | Build and noninteractive suites | Foreground suites | Consumer fixtures |
| --- | --- | --- | --- |
| x64 Debug | Pass | Menu and NativeTextInput pass | Pass |
| x64 Release | Pass | Menu and NativeTextInput pass | Pass |
| x64 ASan Debug | Pass; sanitizer detection probe passes | Menu and NativeTextInput pass | Both annotation variants pass |
| ARM64 Debug | Pass; 19 suites | Refused before input | Not run after the failed native step |
| ARM64 Release | Pass; 19 suites | Refused before input | Not run after the failed native step |
| ARM64 ASan Debug | Pass; 19 suites and sanitizer detection probe | Refused before input | Not run after the failed native step |

The three x64 profiles pass all 21 suites with zero skips. The lead checks 126 report/log pairs, complete native
architecture/source/build/performance identities, copied raw rounds, skip correspondence and lease restoration.
Four consumer cases retain API revision 4, twelve native-module checks and ten negative checks each. Release menu
resource scaling passes under its restoring lease; the offscreen menu text-layout resource fixture passes without a lease.
CI executable bytes are not supplied by these artifacts; recorded binary identities are checked without claiming an
independent hash of unavailable binary bytes.

All three ARM64 warning refusals reproduce the same geometry: point `(859,82)`, warning rectangle
`[132,8,892,104]`, client rectangle `[0,0,758,94]`, and simple region `[703,61,751,87]`. The warning is visible,
enabled, and has extended style `0x88`, with layered/transparent styles cleared. `WindowFromPoint` returns the
foreground `Windows.UI.Core.CoreWindow` belonging to `WWAHost.exe`. The point relative to the warning is
`(727,74)`, inside the reported region. The source derives it from the client patch and translates the region to
the window origin. Both the lead and a read-only reviewer found no demonstrated coordinate defect.

The exact reason the other window wins hit testing remains unproven: these logs do not include that window's
bounds/z-order or a direct warning `WM_NCHITTEST` result. Refusal happens before pointer movement, click insertion
or child launch; foreground, keyboard focus and cursor remain unchanged. Preserve this capability gate and qualify
the same suites on a provisioned ARM64 desktop with verified warning/input ownership. Neither another z-order
workaround nor a private input-desktop switch is an established repair. Do not turn this refusal into a pass/skip,
kill the foreground application, or alter a foreign window to obtain a passing result.

The separate timing auditor imports no production receipt validator. It checks seven complete raw performance
reports, reproduces 350 FPS/percentile fields from 8,400 ordered samples, and checks allocation/hidden-work budgets.
All recorded QPC frequencies are 10 MHz (100 ns counter ticks), distinct from the nominal C++ clock period of 1 ns.
For example, the main x64 Release clean-prepare P95 is 500 ns in every round; its 5% investigation band is 25 ns,
smaller than a counter tick. This quantization limitation does not establish the cause of all short-control variation
or justify changing bands. The unpaired receipts supply no paired non-regression verdict.

Clean committed `9ec25d955f40da439acd9612d14d993b642d2022`, containing the separate timing-aggregate validation
repair, passes all five validators and complete tooling on native-filesystem Ubuntu 24.04.4 with PowerShell 7.6.6.
That log is retained here as tooling portability evidence, distinct from the frozen native candidate.

Every ZIP entry was reopened and checked against original length and SHA-256; `archive-manifest.txt` binds entries
and archives. No executable/library build products are retained. Native ARM64 foreground/consumer completion,
controlled quiet-host performance calibration, trusted base-policy seeding, physical IME/assistive-technology/touch
and consumer product adoption remain separate open obligations. This packet establishes no production-wide approval.
