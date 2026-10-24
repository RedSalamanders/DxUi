# Native verification projection

Reviewed Markdown projection of original private records; no native or benchmark receipt is restamped.

Executed code: 903643b4a2bc2f6d75250ff5e6834513c96432c4

Compiled-input identity: d9cbdeec221d1079b81eb77c96aa73d970c368df4121aa1ad3c888148f35eb52

| Platform | Configuration | Build | Native execution | Library SHA-256 |
| --- | --- | --- | --- | --- |
| x64 | Debug | PASSED | True | 2737853E62BD11B45E4F4B3D53527A6D91B9D5CA8E4890005076CD2527A449A6 |
| x64 | Release | PASSED | True | B024C5640207D4B56D8BAA727968F9ACB0DC6C57D950C01DA62D965ADCB8C175 |
| x64 | ASan Debug | PASSED | True | 03B18C6031C0A1032E1C4C16F8A72DA70F145AC436E946AB0F36372F5B615756 |
| ARM64 | Debug | PASSED | False | 58D20A21DE7A29722E7FECA8FAA8B453F06B6CBDBF330A915B7B8F3E27E1D178 |
| ARM64 | Release | PASSED | False | BF45A097AE63EB683F9ADA534CD2A8F44452EE4D64C2783E49F7F4C50924DEAC |
| ARM64 | ASan Debug | PASSED | False | 69EB93A87311CA590C04595B13A17D4AE55D4629B01170502A2FE3A5EF74BBF9 |

| Profile | Suites passed | Capability skips | ASan detection | Private run |
| --- | ---: | ---: | --- | --- |
| Debug | 19 | 0 | False | 20261023T200713278Z-14772-dc03efc0cd8b410cb7f19a831bb7f2b4 |
| Release | 19 | 0 | False | 20261023T195241292Z-14772-d41b392a04a34b7a8282a366d4312851 |
| ASan Debug | 19 | 0 | True | 20261023T201926117Z-14772-fb0815f793c24cd1af2ac8964897b5fa |

The 19 scopes per profile are Foundation, Embedded, Grid, Theme, Control, MenuExitLifetime, NewControls, EditorControls, TextField, MultilineText, ReadOnly, ComboBox, Tree, Tooltip, Rendering, Animation, Accessibility, WindowHost and InteractiveLease. Foreground suites are separate and were not run.

## Original record inventory

The original summary JSON publication was rejected by the measurement schema. These exact files are preserved privately; they are summary/status projections, not native benchmark receipts with invented fixture metadata.

| Private path | Bytes | SHA-256 |
| --- | ---: | --- |
| .build/visual-fidelity-publication/verification.json | 29221 | 07332900CEC6829286543B2E42544B9CFA827BFA129EB93674A3F05BF7A01978 |
| .build/visual-fidelity-publication/performance.json | 83702 | 1732ADD021863239F6B7A1E2909BBFB47AB7DAEAD23E6503A572AD6B27B3E763 |
| .build/visual-fidelity-publication/performance-first-comparison.json | 5777 | BE270F0BE00BB969D682B6A20AFE0B06C4EFFDA8408F1556B39904CAA0898D3F |
| .build/visual-fidelity-publication/performance-repeat-comparison.json | 5775 | 3F7F01F35389396B1C36984C58ADB041DA8150D41CD079A172B24A4897D5C2E2 |
| .build/visual-fidelity-publication/documentation-continuation.json | 1886 | C44D5CDB11FA7A197AB0105CC9AF8AFC1EA3795EEE3DB858BCABA9F51F28D33D |

The original native receipts, logs and sealed payloads remain under .build/evidence/test-runs. This projection supplies no physical-input, native-ARM64, CI, paired-performance or release acceptance.
