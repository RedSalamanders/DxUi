# Raw receipt filenames

Performance receipts retain their canonical JSON. Other raw JSON uses `.receipt.txt`, following the established Measurements convention. Contents are unchanged; SHA256SUMS binds every byte. Archived drivers and analyzers refer to their original external evidence roots and original filenames.

- `corrected/Accessibility-receipt.json` → `corrected/Accessibility-receipt.receipt.txt`
- `corrected/inputs.json` → `corrected/inputs.receipt.txt`
- `corrected/library-hash.json` → `corrected/library-hash.receipt.txt`
- `first/Accessibility-receipt.json` → `first/Accessibility-receipt.receipt.txt`
- `first/Embedded-receipt.json` → `first/Embedded-receipt.receipt.txt`
- `first/Grid-receipt.json` → `first/Grid-receipt.receipt.txt`
- `first/inputs.json` → `first/inputs.receipt.txt`
- `first/library-hash.json` → `first/library-hash.receipt.txt`
- `first/Rendering-receipt.json` → `first/Rendering-receipt.receipt.txt`

On 1 October 2026 the Performance receipts kept the first 8 of their 32 run-id digits, within DxUi's 150-character path budget. Contents are unchanged:

- `corrected/Performance-x64-Release-465181ac63314106b69885d6df75dc61.json` → `corrected/Performance-x64-Release-465181ac.json`, and its `.comparison.json`
- `first/Performance-x64-Release-ae2f58a9a77f4ffd9f4daa7ecf5b263a.json` → `first/Performance-x64-Release-ae2f58a9.json`, and its `.comparison.json`
