# Native lifecycle and payload type safety — 2026-10-28

## Scope and source

The SDK fix makes posted-payload type identity independent of the function address of its deleter. Release `/OPT:ICF` can fold identical deleter bodies for distinct payload types; using those addresses as type identity allowed a wrong `Take` or `Reclaim` to accept another type. The registry now stores a distinct writable per-type tag on each bounded entry. `Post`/`Send` publish the tag, and `Take`/`Reclaim` compare it before removing ownership. The registered deleter still destroys the payload. Window, message, token, capacity, ownership, failure and teardown-drain behavior remain in force.

A permanent Release ControlTests regression uses two same-layout payload types with identical counted destructor bodies, requires actual linker folding, and covers wrong-type `Take`/`Reclaim`, ownership preservation, valid `Take`, valid `Reclaim`, drain and exactly-once destruction. The shipped library linker policy is unchanged; this is a correctness repair, not a performance claim.

The hosted provider run [40605395948](https://github.com/RedSalamanders/DxUi/actions/runs/40605395948) tested source `086ad82a5872768db23ea83862ab4af13dff0c5b` in merge candidate `f2bad236` against base `5d983536`. All six hosted native profiles and external-consumer checks passed. The profiles include x64 and ARM64 Debug, Release and ASan Debug. Artifact evidence:

| Profile | Artifact ID | GitHub-reported artifact SHA-256 |
| --- | ---: | --- |
| x64 Debug | 12781748474 | `9f707a70b55e5a099a6a477e8f87d01475465a254e6cae2811d97075e6623115` |
| x64 Release | 12782396793 | `c2c0bd6911b4f0621de4f2dc5497fa5302cf84489c45934d07bb6ae2a57bd71a` |
| x64 ASan Debug | 12782791011 | `2760743f2564f64dff0fc3689b6b33ad1c448fa1e2879ac64c53ae1a3c4061e4` |
| ARM64 Debug | 12783132661 | `19d40ff227808a1a244b7f9bdd86ce44530530512ad91260834520a86caba36d` |
| ARM64 Release | 12784080061 | `dc141d806c8350e7e3849728154a4b64c3f4e9f4cacf5366e3d1aeff23c9f645` |
| ARM64 ASan Debug | 12783718895 | `967697d12f856938bffe6db8751b3b2828d540e16c68d525fa1e71c1ee4d0011` |

These digests identify the six provider artifacts; extracted native receipts and their manifests retain separate integrity checks. External-consumer checks are included in the hosted run result.

## Current local and cross-repository status

Current-source local x64 Debug, Release and ASan each passed all four foreground suites: Menu, NativeTextInput, MenuResources and MenuResourceScaling. All twelve suite receipts have exit zero and zero capability skips. Each authorized thirty-second automatic Start lease verified unchanged foreground, keyboard focus and cursor afterward. The ASan detector separately recorded the expected exit one with detection true; its enclosing profile passed. Required unpaired performance observations are diagnostic only.

| Profile | ControlTests executable SHA-256 |
| --- | --- |
| Debug | `00D8888F7CCA9E87347150FB83B53DE7ADD34B7FC69002EBC01150A8915D6861` |
| Release | `6A3E769A3920F39ACC710E96514BF38B97515419F821F1A33FAB9B04E21233DD` |
| ASan Debug | `32D0A100756C1D44832D4393599F3128DF5E42567A34F2B54EAD281F76E6E5E9` |

The private input summary binds all twelve receipts, three restoration records, build attestations and detector evidence (SHA-256 `22F7EF84826C717080F4E2AD0D03CE03F2F0BFC2F8F3D72E9F1D1AF420F5340E`). Its failed first logging wrapper is retained separately; the canonical Debug build completed successfully and its fresh attestation admitted the test. Later documentation changes retain producer `086ad82a` for these executions.

Paired job `122543848684` completed measurement and artifact upload successfully, then failed at verdict publication. Its verdict is inconclusive with `policy-review-required`: the actual-base policy is not independently qualified. An empty gated scenario verdict does not mean zero metric regressions. The source-bound collection uses tested merge `f2bad236`, base `5d983536`, twelve blocks and seed `513368947`. The full 194,142,030-byte provider ZIP (artifact `12786737643`) is retained with verified SHA-256 `492392a38a9688188082c91f78634bd434c3c860be0b6ea21f8309a419a44cac`; large binaries and raw host telemetry remain compressed. All study reports and both-side build metadata are retained separately for audit. This run cannot approve its own policy.

RedTilde adopts SDK `086ad82a` in frozen producer `e0908338`. Six consumer build profiles passed; three x64 native profiles each passed 67 cases and 32 capture pairs, including real ASan detection; the nine-stage input run passed. The latest RedTilde audit-only source changes do not change native code. RedTilde hosted ARM64 remains billing-blocked. These results do not close RedTilde's open performance findings or hosted ARM64 gate.

## Remaining qualification

Keep both Markdown presentation and performance qualification in WIP while policy, consumer platform and release holds remain. Native correctness and input qualification do not approve the performance policy or claim savings. The owner requests integration with those disclosed holds; the merge does not close them.
