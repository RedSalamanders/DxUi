# RedSalamander source import archive

Archived from the root `provenance` folder on 2026-09-05. DxUi is now the canonical library, so its origin does not
need a top-level working directory. This archive keeps attribution and test coverage auditable without retaining
a second implementation. Current behavior belongs in the domain specs; current code lives in `src` and `include`.

| Record | Why retained |
| --- | --- |
| [source-origin.json](source-origin.json) | Exact original commit, original hashes and current ownership mappings; required for source attribution and dependency validation. Hashes describe the imported revision, never today's edited code. |
| [test-port.json](test-port.json) | Disposition of all 941 inherited cases (853 retained, 88 excluded with reasons); the test-port validator still checks retained entrypoints. |
| [pending-dependencies.json](pending-dependencies.json) | Empty final dependency inventory; validation requires standalone code to remain free of application includes. |
| [migration-ledger.md](migration-ledger.md) | Historical sequence and consumer boundaries; the final extraction entry supersedes earlier pending status. |

Validators read these records at their archived location. When an owned file or retained test is deliberately
renamed or retired, update its mapping/disposition with a reason while preserving the original identity and hashes.
Do not add ongoing feature plans here or synchronize source back to the original application automatically.
