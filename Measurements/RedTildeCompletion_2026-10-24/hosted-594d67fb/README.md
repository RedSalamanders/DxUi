# Hosted SDK native qualification — 25 October

The [reviewed native packet](summary.json) records all six successful hosted native jobs in workflow
`40177964080` for head `594d67fb0fcc3829541211a7c8f100df046c87e3`. The compiled merge is
`8c78b5adf821daa216bee53572b3f9aae7d40ef7`, with actual base `b06f534b` then head `594d67fb` as its parents.
Its tree `fa4193a259c9e2b087a21399ec13cde387202002` equals the head's tree. Preserve the recorded merge identity;
these are actual x64 and ARM64 native runs, separate from the [local cross-build matrix](../qualification-594d67fb/README.md).

| Profile | Full native suites | Capability skips | Verified Full payload files |
| --- | ---: | ---: | ---: |
| x64 Debug | 21/21 | 0 | 82 |
| x64 Release | 21/21 | 0 | 82 |
| x64 ASan Debug | 21/21 | 0 | 84 |
| ARM64 Debug | 21/21 | 0 | 82 |
| ARM64 Release | 21/21 | 0 | 82 |
| ARM64 ASan Debug | 21/21 | 0 | 84 |

The hosted 21-suite catalogue includes Menu and NativeTextInput under the owned runner's foreground lease.
The local noninteractive matrix has 19 suites per profile. Hosted input does not replace the current person's
separate menu/input agreement or the RedTilde product keyboard obligation.

All six provider ZIP digests match. Independent replay verifies every declared Full-run file against both its
ZIP entry and expanded bytes by exact length and SHA-256: 496 files across six passed completion inventories.
The lead repeats that verification and reconciles every projected profile, provider digest and completion
digest to the independent audit. The JSON retains source, provider, completion, independent-audit and lead-replay
digests. Original reports, ZIPs and failed verifier-authoring attempts remain private beneath `.build`.

Both ASan profiles also pass the isolated deliberate use-after-free detection probe with its expected exit 1;
this is separate from the 21 passing suite outcomes. All six relocated public consumers and galleries pass.
The STL-annotations-disabled consumer runs only in both ASan profiles; other instances are skipped. A separate
x64 Release run passes MenuResourceScaling and MenuTextLayoutResources; other optional instances are skipped.

The reports bind executable, compiler, linker and MSBuild digests, but these native ZIPs do not retain the
matching executable, compiler, source, object, library or PDB bytes. Those reported binary digests are not
independently rehashed artifacts. Optional-run receipts/catalogues were checked; their payload inventories were
not separately replayed. These limits remain part of the evidence.

The top-level workload/fixture input hashes are matching-source Complex UI metadata from SDK `594d67fb`,
independently matched to the same revision's local report and six source files. The hosted merge has that same
tree. These fields do not claim retained hosted source bytes or a replay of the complete 21-suite input closure.

The structured workflow status is the native-audit snapshot. Its paired benchmark was still running then;
this packet contains no paired metric, verdict or performance acceptance. Current foreground input,
R10/D4c policy/advice, physical/assistive-technology and product gates remain separate. RedTilde's account-blocked
hosted ARM64 gate remains open at the owner's request.
