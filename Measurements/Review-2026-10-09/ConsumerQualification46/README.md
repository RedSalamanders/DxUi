# Relocated exact-pin x64 consumer qualification

Four canonical `test-consumer.ps1` cases passed for `ffc64c8947301dfaf2b1e295968a1cc6253bd413`: Debug, Release,
ASan Debug, and ASan Debug with STL annotations disabled. Each independently cloned the committed library into
a path with spaces, restored its own dependencies, compiled API revision 4 through public headers, rendered the
public/complex/text samples, checked twelve EXE/two-DLL ownership probes and rejected ten pin/build mismatches.
The two annotation modes have different attested build identities.

The lead independently verified all four report fields, current executable/library hashes, twelve harness PNGs
and forty negative-check logs. All three Release captures were visually reviewed, including retained Japanese
text and caret placement. These are deterministic offscreen harness captures; no desktop screenshot was used.

`verification-manifest.txt` records the identities and hashes. `consumer-qualification-46.zip` retains 57 original
records: four reports, twelve PNGs, forty rejection logs and the manifest. Every archived entry was byte-checked.
Archive SHA-256: `DC2B7F7B1B4CCA80641560DE1E3E8DD8AF86092A59337A664B1D454B86F42CAC`.
The four complete parent logs are retained beside it.

These receipts qualify their recorded `ffc64c8` pin. A later tooling-fixture correction needs its own final-source
CI accounting; these records are not relabelled. Native ARM64, physical IME/assistive-technology/touch and product
adoption remain separate. No consumer repository or dependency pin was changed by this library qualification.
