# Acrylic menu pointer fixture validation

## Source and scope

PR [#104](https://github.com/RedSalamanders/DxUi/pull/104) merged as 619dc88b776f6e545542e0242e5943bbdddc5b3c; the tested PR head was 30fccffe13b2b07a5369f384943a35c23d836761 (tree 68399a81486f69cb42c396cb2f0e2a2e44d1b112). The change repairs the test fixture that places the physical cursor on an acrylic menu item, proves hover, parks outside the item rows, and captures only after hover clears. It does not modify product menu behavior.

An earlier ARM64 Release Light visual-baseline failure combined geometry and unhovered state in one predicate, so the report cannot identify which subcondition failed. The original failure archive remains preserved separately. This passing rerun does not establish that failure root cause.

## Hosted native qualification

Run [40242796721](https://github.com/RedSalamanders/DxUi/actions/runs/40242796721) completed successfully for all six native profiles. Each archived profile contains all 21 declared Full suite reports with exit code 0 and no suite-level skips; each Menu log records 123 test cases started and 123 completed, plus one suite start and one suite completion banner, with zero test failures. Both Light and Dark acrylic pointer cases ran in every profile.

| Profile | ZIP bytes | ZIP SHA-256 | Full suites | Menu cases | ControlTests SHA-256 prefix |
|---|---:|---|---:|---:|---|
| ARM64-ASan Debug | 2950257 | 8B39F8154830F1E085E6F5D7994B02FF5BAD629CD03FBF470E3530C01EC7A294 | 21/21 | 123/123 | 20243464995E9CE9 |
| ARM64-Debug | 2936334 | 2C58D1D0F6CFF9B0DD3913B71775844F8628B4A15DBFE85CD32B7388323E5CB8 | 21/21 | 123/123 | 8967AA64BF1C07A2 |
| ARM64-Release | 2928750 | 9195852891D4A6AFCB170525DB29F52E923DCBB24E9BCAE4E5927DC9C93DAE2C | 21/21 | 123/123 | 8E4C30E04F025187 |
| x64-ASan Debug | 2949619 | C426E9683AD80F3DECE04CFD41E4BBDA50E5B19DE4B5E9024B72CE374C7FEFD2 | 21/21 | 123/123 | B1947DFB56018739 |
| x64-Debug | 2928075 | B8CD0502038568B74C95D1983486D8BB59FC699DF154BF6340E0EC7D4BA3F336 | 21/21 | 123/123 | A5ED38712920CF6A |
| x64-Release | 3063263 | BCFCA47060DE37B717BEF29F86F3A03DC2F0725905995E96D70998ED89BD7B48 | 21/21 | 123/123 | 087EC6DEBBB3886F |

The hosted ARM64 Debug, Release, and ASan Debug Menu leases report focus none and focus-capture reason foreground-minimized. Foreground/cursor lease status does not verify initial keyboard focus; keep hosted ARM64 focus-restoration qualification open.

Every ZIP size and SHA-256 matches provider metadata. Hosted ZIPs retain receipts and reports, not .exe, .dll, .pdb, or .lib binaries; report hashes bind the executed ControlTests binary.

Both x64 ASan Debug and ARM64 ASan Debug reports show the expected AddressSanitizer probe was detected (detected=true, probe exit code 1); both surrounding native profile jobs passed. The relocated consumer validation passed in all profiles. The disabled-STL-annotations consumer was run in both ASan profiles and passed; it was skipped in the other four. The optional capture-free Menu resource-scaling step passed only in x64 Release and was skipped in the other five profiles.

## Separate local x64 Release evidence

The clean 30fccffe local Release build has 19 Full suite receipts passed with zero capability skips and 95/95 payload files verified. A separate interactive Menu run passed with zero capability skips and 8/8 payload files verified. Both local replays bind to ControlTests SHA-256 3D62AA80906997C990BBFF4B2B8183BFCF5F2165D0AE7E81CF925BDA934F3F56. Its lease reports foreground, focus, and cursor unchanged, with exact cursor restoration. The earlier Full SkipBuild admission attempt failed without retaining the underlying error; a fresh-shell read-only admission probe passed without changing the build receipt, and the later Full run passed. The admission failure cause remains unknown.

## Limits

The paired-benchmark job concluded success while its dependency installation, paired collection, and verdict steps were skipped. No paired performance result is established; the local unpaired timing is not performance qualification. This packet does not close shared API or policy review, repository-wide performance qualification, or RedTilde ARM64 runtime qualification. See [the test contract](../../Specs/Testing/Testing_Validation.md) and [the completed fixture plan](../../Specs/Plans/Done/MenuAcrylicVisualCapturePointer_2026-10-25.md) for durable requirements.

Structured, raw-free evidence is in [summary.json](summary.json).
