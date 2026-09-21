# Native menu controller exit-lifetime qualification

Source `d461095efeeabf514d0a1940d58349728e7a471c` completes
[CI 35656340075](https://github.com/RedSalamanders/DxUi/actions/runs/35656340075)
successfully in x64 and native ARM64 Debug, Release and ASan Debug.

All nineteen regular suites pass in each profile, including the new isolated
`MenuExitLifetime` process. The x64 Release resource diagnostic also passes.
All six external consumer tests and both ASan annotation-disabled variants pass.
Both intentional ASan fault probes confirm detection; their exit code 1 is
expected and is not the outcome of the menu regression test.

Nine pre-existing ARM64 Menu desktop-capability skips remain explicit per
profile. All x64 suites report zero skips. These results do not qualify consumer
native mixed-DPI interaction or assistive technology, and unpaired CI performance
does not accept any resource tradeoff.

The regression keeps a captured asynchronous menu alive during CRT thread-local
destruction. The retained baseline fails with use-after-free; the fixed controller
prevents recursive finalization and destroys the popup chain before its members.
See [the local baseline/candidate diagnosis](../exit-lifetime/README.md).

Raw suite/performance/consumer receipts and menu/ASan logs are retained with
byte-preserving attributes. `sha256.receipt.txt` binds every archived input. Complete
build logs and gallery artifacts remain under
`C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-d461095-final/`.
The subsequent v4 resource-fixture activation correction and text-layout-only
diagnostic are test changes requiring their own qualification; they are not
included in this source's CI result.
