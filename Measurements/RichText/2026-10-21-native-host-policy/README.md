# Explicit native WARP policy: local validation

Runtime source: `dd619ef0c5855307eb57758072658f2e3b777fd8`, on `ej/redtilde-s5-host-policy`, stacked
above the native math/diagram feature branch. Subsequent evidence/documentation changes do not move the
consumer's immutable runtime pin. `SourceManifest.json` records the compiled input fingerprint, actual fixture
hashes, six build attestations and binary hashes.

All 19 default noninteractive suites pass in x64 Debug, Release and ASan Debug: **57 passes, zero capability
skips**. All six x64/ARM64 builds pass; this machine is x64 and does not establish ARM64 runtime support.
The real ASan expected-failure probe detects heap use after free. Aggregate validation and pinned formatting
pass. No focus-taking suite was authorized or run.

The isolated native policy case proves first-attachment WARP selection, actual software-adapter flags,
inheritance by default peers, matching idempotence, conflict refusal with unchanged attachment accounting,
device-replacement generation changes and final host retirement. The complete WindowHost, Embedded and
control suites cover the surrounding default behavior. Geometry, tokens and default gallery content are
unchanged, so this option requires no new visual baseline.

Suite/build JSON retains its original receipt hash and adds library ownership, fixture hashes and the runtime
binding. Original private reports remain under `.build`; reviewed logs retain both the initial WIL compile
failure and the safe build refusal while another test process was being inspected. The corrected build and
all full suites pass separately. No process was killed to bypass the running-output guard.

`Performance-Before-Unpaired.json` and `Performance-After-Unpaired.json` retain independent complex-UI WARP
rounds, resource phases and provenance. They were not alternated and other work was active. They are
diagnostics only: no performance non-regression, new baseline or D4c acceptance is claimed. The application
completed-frame A/A, A/B and lifecycle evidence belongs in RedTilde's repository, not this library packet.

Physical device removal/occlusion, kernel/GPU lifetime qualification, native ARM64 execution, physical IME/AT,
time-authorized focus work and D4c remain open. Pending PR CI is not counted as a local pass. The overall
rich-editor WIP remains active.
