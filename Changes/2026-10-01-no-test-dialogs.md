- A failed runtime check in a Debug test run no longer opens a dialog on the desktop.
  - The STL range check a mutant tripped opened the CRT's modal Abort/Retry/Ignore box. Such a box holds an unattended run
    until its watchdog ends it, and interrupts whoever is at the desktop.
  - Every native test executable now first calls `Tests/Support/FailureReports.h`. The report goes to stderr and the run
    ends with exit code 3, without Windows Error Reporting's dialog. The Embedded executable calls it before `main`, so
    the fingerprinted `BenchmarkMain.h` is unchanged.
  - `Test-TestWatchdog.ps1` runs the hidden `--failure-report-self-test` and requires it to end within its bound with
    exit code 3 and the report (`Testing_Validation`).