# SDK data notices and license closure

Status: DONE — scoped DOCS-07 implementation, packaging regression and documentation closeout

- [x] Retain the original SDK ZIP counterexample: Unicode/WHATWG licenses absent and five local license links broken.
- [x] Add byte-preserved Unicode License V3 and full WHATWG notices to the repository and source notice inventory.
- [x] Stage the nine required license/notice files, including the actual restored WIL copyright, through one helper.
  Rewrite license links for offline use and repository provenance links for the exact source commit.
- [x] Verify the complete file set and bytes before ZIP creation and after extraction; bind the extracted
  manifest inventory and record `licenseClosurePassed` and each relative license path/SHA-256.
- [x] Exercise independent set/byte/link/extraction expectations and missing, modified, source-notice and
  moving-source refusals in the normal tooling roster.
- [x] Include license inputs in CI identity despite general documentation exclusions. The real pinned
  classifier includes current and future license files while ordinary documentation remains excluded.
- [x] Pass complete local tooling, the focused updated notice/classifier checks and specification/skill validation.
- [x] Build the x64 production library with zero warnings/errors and qualify the real extracted SDK-only
  consumer, header closure, license closure and deliberately rejected first-party warning control.
- [x] Merge durable requirements into [SDK packaging](../../Build/Build_UnifiedCI.md),
  [documentation](../../Core/Core_Documentation.md), the tool inventory and usage guide.

Implementation: `2649773a8b9567aab1ef4e82bce36db9dbdedcef`.
CI input closure and final package source: `08c2127bf30d9c8778a57f016c1224c70cbac498`.
The second commit changes CI classification, its independent fixture and the contract; production archive inputs
remain unchanged, verified before reusing the qualified library for the final packaging smoke.

| Original retained evidence | Identity/result |
| --- | --- |
| SDK `0.1.1546` / `ab850a40e1026cdbbf62153db9a0a55156d2775c` | ZIP SHA-256 `0c573ca05bca86d2a300de002f9f1fdb44d56d74af38f1fffE7234e47cb226fe`; two missing notices and five dead `licenses/docs/licenses/...` links. |
| Pinned CI engine `e32cf9f9b29f4e817decb09e1be44be7fc1a94de` | Previously excluded root source notices and both Unicode/WHATWG license inputs. Explicit project includes now override those exclusions. |

| Qualified x64 SDK `0.1.1552` | SHA-256 |
| --- | --- |
| ZIP, 54,976,813 bytes | `cbd0cb47c9cc81d227ad813acb5bab4a1e083d5d1ba5f74c76946fe2d7da10c4` |
| Production library | `fecfc0d936a7bac6c80cdf2b6c3a3121e11ede916f79faf7db38ce13b146c222` |
| Original package receipt | `1ee25ebf54c0ca7ccf2da4f1fa172c6735aceb83f1fdd56e5bb7b1ab65f8097b` |
| Unicode license | `e7a93b009565cfce55919a381437ac4db883e9da2126fa28b91d12732bc53d96` |
| WHATWG notice | `85dc6f5ccb57a6fe8c33d158f9fc8fc7ee5655a5d3db2cdd131c6a3d0f48a864` |

Raw archives, receipts, complete logs and the original negative inventory remain private generated evidence.
The warning control intentionally emits C4189 with `/WX` disabled; the zero-warning validator refuses it.
This is a successful negative control, separate from the warning-free library and positive consumer.

No runtime source, public header, API revision or gallery pixels change. Documentation/gallery review therefore
requires no new raster generation. This closes DOCS-07; it does not establish new native ARM64, interactive,
consumer, performance, privacy-history or release acceptance. Those remain in the
[single active plan](../WIP/RemainingWork_2026-11-04.md).
