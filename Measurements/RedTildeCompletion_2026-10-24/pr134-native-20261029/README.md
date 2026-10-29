# Hosted native qualification — 29 October 2026

**Status:** reviewed functional native evidence. This native matrix is separate from the paired performance gate and does not establish an overall green result.

## Source and artifact bindings

- Hosted run: `40869356681`; compiled merge: `b7d43757bd7c1c2464c5d349ff1b1e0993da69e9`; published source head: `c1c207a8e1038789241ca4bc975e72581b2d9616`.
- Sealed native audit SHA-256: `E575C1D7009F387D8623E6D21E733F35CD0DF9C401D0D0830755B3B820A79F24`.
- Full-input receipts report the merge source as clean and the build identity as verifiable with full scope. The sealed native audit omits the Git tree; a separate independent Git-object replay verifies that the compiled merge and published head share tree `06261ed45b87e5c849d4cf847a2898a6aff9c060`. This is a Git object ID, not a SHA-256 payload digest.
- Compiler evidence recorded in eight consumer receipts: MSVC v145 / 14.52.37025, Windows SDK 10.0.26100.0, MD runtime, x86 preferred tool architecture.

## Hosted native jobs

All six native jobs report success and verified merge checkout. Each has a full 21-suite context, with 21/21 suite results exiting 0 and no recorded skips (126 full-suite results total). A separate x64 Release resource run adds `MenuResourceScaling` and `MenuTextLayoutResources`, both exit 0 with no skips. The audit records 505 completed payload-file hash checks across the six full jobs and that separate run.

| Profile | Artifact ID | Job ID | Full suites | Full payload hash checks | Server ZIP SHA-256 |
|---|---:|---:|---:|---:|---|
| native-ARM64-Release | 12899607413 | 123432279695 | 21/21 pass, 0 skips | 82 | `36387216711b5ed91d3e9fd415df03f097d719c9c74a32fc7457cee203a7f1ec` |
| native-ARM64-ASan Debug | 12899222845 | 123432279876 | 21/21 pass, 0 skips | 84 | `61b40344e3e3e48509971e519fa71403c754efffb8ea8e8965370b504b9f0438` |
| native-ARM64-Debug | 12898452578 | 123432279674 | 21/21 pass, 0 skips | 82 | `156fb2b0f3549f384bd09b9f6cd7abec9e8866ddd6a9330b9fab0081b501880b` |
| native-x64-Release | 12898041159 | 123432279676 | 21/21 pass, 0 skips | 82 | `d93cdef79659ff4d653ba430e7926ee9ad4f2c0d127c3e0e5d25b30642d9df16` |
| native-x64-ASan Debug | 12897268626 | 123432279930 | 21/21 pass, 0 skips | 84 | `886614f746f35f75ea919a584fbd5a1314b46969c92079c7fd86e861ff4aa329` |
| native-x64-Debug | 12896976783 | 123432279659 | 21/21 pass, 0 skips | 82 | `301aae5d2d9fcda28217b2631b437c097ee38a9ca197ee6533b2ea02a9a81a7f` |

The separate x64 Release resource run records 9 verified payload files and 2/2 resource suites passing; it is not counted as another full 21-suite run.

## AddressSanitizer and consumer receipts

Both ASan profiles report the expected detector result: detected, exit code 1, with the digest matching the AddressSanitizer result record. This expected detector exit is separate from the passing full 21-suite results. All eight consumer receipts identify the compiled merge source and exit 0.

The native ZIPs do not contain raw executables. Executable digests are recorded build identities, not custody of raw binaries.

## Hosted interaction receipt limits

The hosted leases for `Menu` and `NativeTextInput` completed with exit 0. The projection copies the literal focus/foreground/cursor receipt states per profile into the projection. Receipt labels such as `none`, `unchanged`, or `restored` do not prove physical IME or assistive-technology availability, nor do they establish a general focus-none guarantee.

## Gate boundary

These hosted native results do not resolve the separate paired performance gate. No overall-green, merge, or performance-acceptance claim is made.

## Published encoding

The `*.json.template` files contain literal reviewed JSON projections and their byte manifest. This follows the existing qualification-packet convention and distinguishes these derived summaries from native runner measurement receipts; there are no placeholders to fill. The raw reports, receipts and sealed audits remain unchanged.
