# Raw receipt filenames

Performance receipts retain their canonical JSON. Other raw JSON uses `.receipt.txt`, following the established Measurements convention. Contents are unchanged; SHA256SUMS binds every byte. Archived drivers and analyzers refer to their original external evidence roots and original filenames.

- `baseline/binary-hashes.json` → `baseline/binary-hashes.receipt.txt`
- `baseline/inputs.json` → `baseline/inputs.receipt.txt`
- `baseline/report.json` → `baseline/report.receipt.txt`
- `candidate/binary-hashes.json` → `candidate/binary-hashes.receipt.txt`
- `candidate/inputs.json` → `candidate/inputs.receipt.txt`
- `candidate/report.json` → `candidate/report.receipt.txt`
