# DxUi foreground qualification at 2fed8246

The x64 Release interactive run used the retained `DxUi.ControlTests.exe` built from source `2fed82465dcee9f213753754e0e8e54f353cf38f` (tree `2c0641bc363bf713cfc709d721c3f6549dd5aef8`, build identity `986412dbc2095a5be336dcdd39b496b4aa5a680eda2f57716e34ea04c9f586ef`). The 30-second automatic-start behavior was authorized for this run. The test harness recorded an automatic start, completed its interactive lease, and restored foreground, focus, and cursor to their original state.

| Suite | Result | Skips | Lease duration |
|---|---:|---:|---:|
| Menu | Passed | 0 | 97.4 s |
| NativeTextInput | Passed | 0 | 56.4 s |
| MenuResources | Passed | 0 | 9.2 s |
| MenuResourceScaling | Passed | 0 | 15.6 s |

All four receipts bind to the same Release executable. The completion-manifest digest and executable hash are recorded in `summary.md`; the private custody packet retains the receipts and payloads.

This is local x64 Release evidence. It does not qualify native ARM64, physical IME, assistive technology, or paired performance. A later hosted ARM64 Debug Menu pass does not explain the earlier 4faf16ed Menu failure.
