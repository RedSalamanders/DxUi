# Rich semantic preparation baseline — 2026-11-01

This is an unpaired local diagnostic, not D4c or hosted performance acceptance. The
same tracked fixture and a separately linked candidate must be measured under the
owning paired policy. The process was x64 Release on Windows 10.0.26300, AMD Ryzen
9 9950X3D, using a borrowed WARP device and no window activation.

- Baseline source: `4423074dee2410acb28e62b41072e6c561a79eb8`.
- Baseline `DxUi.lib` SHA-256: `461d45af8ada65534f0decc1938c1e24996b4e5f52046386cc193cd308bdced8`.
- Fixture header SHA-256: `8c745b1a34c352bf93c114b668ff2b140977fac9040d85de0c7c96a1023694ed`.
- Fixture entry source SHA-256: `ca463251d59fb771fcd3e593520fd5c6613c043aee88f9ba4ea27f4480ce8d72`.
- Probe project SHA-256: `8ea805964204c91961e993da4e08051836e34bb029c230b0eb785be1f54938a8`.
- Linked probe SHA-256: `9226614208852c5947c2d82fd55513937edd93f871fbdc2db21a5d9e75aa04ea`.

The warning-free standalone probe links `Tests/Embedded/DxUi.RichPreparationBenchmark.vcxproj`
against the exact test-enabled Release output specified by `TargetRoot`. The recorded producer is the project as committed at `4c8d54f4`; later six-profile declarations require a fresh paired run before acceptance. Run the
resulting `DxUi.RichPreparationBenchmark.exe` once per process. It owns no HWND,
worker or timer and makes no external file/network requests. Its process-local
baseline receipt is `.build/rich-preparation-probe/baseline-final.json`.

The fixture contains 1,000 paragraphs, lists, a table, two image requests, three
formulas and a flowchart (48,056 UTF-16 source units). It prepares the document,
completes both image jobs in one burst, then inserts one early character and waits
for readiness. The run recorded 548 initial turns, two admitted/accepted images,
507 burst turns (507 layout advances), and 548 edit turns (507 layout advances,
two semantic advances). It retained 2,016 reported geometry chunks. Process
private bytes at the end were 46,088,192; total elapsed time was 106 ms. Private
bytes include the whole process and are not a measurement of retained C++ storage.
No candidate or confidence interval is available, and these values do not alter
any threshold.
