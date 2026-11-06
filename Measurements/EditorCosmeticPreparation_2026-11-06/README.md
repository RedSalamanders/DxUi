# Independent editor cosmetic preparation

Implementation: `ba94f4eb0cd8bbdeecb0b96d5e21fb325fe2d9c9`, API 4.
The [verified qualification](qualification.json) binds six zero-warning builds, all nineteen noninteractive
suites in each x64 configuration, real ASan detection and original native/run archive identities. All 594
original compiled inputs match source identity `c827c50d2a0ae5226c63fed01b81c09f76517156bf4a9f69dea49e3ce9ae66db`.
ARM64 entries are cross-builds; this packet does not establish native ARM64 execution.

The same cosmetic implementation now passes all seven hosted main Release jobs at `14386f36` in
[run 42193587525](https://github.com/RedSalamanders/DxUi/actions/runs/42193587525).
The [verified hosted packet](hosted-qualification.json) checks all fifteen original wrapper digests, canonical
receipt members in both evidence copies, 23 native x64/21 native ARM64 suites and both assertion probes,
and the inner production SDK ZIP hashes for version `0.1.1635`. It closes this source's Release platform
gate; it does not qualify later clipboard changes, Debug/ASan ARM64, physical input/AT or paired performance.

Source/code colours, line numbers and whitespace now use the existing optional visual preparation traversal.
They never withhold otherwise usable content, cancel selection or make input wait. Source analysis can finish
after literal Source text becomes ready. Embedded hosts keep `NeedsPreparation()` true while another optional
turn is required, and their already coherent frame remains compositable. Native hosts request the next paint.
Complete prior decorations remain usable where source/geometry coordinates still match; unknown coordinates
are omitted until a complete replacement is published.

Guide work covers the viewport plus overscan: at most 256 source/number units and four DirectWrite number
layouts per owner turn, with a stable five-digit gutter. Immutable source locations use the document's newline
index and tree summaries rather than rescanning the full prefix. Code tokenization visits the visible window
plus overscan and invokes at most one consumer callback per turn. Source colour publication uses that window;
offscreen metadata visits share its bounded work allowance. Idle, unchanged windows request no further work.

The frozen baseline independently showed ready geometry with `contentPending=1` while optional guide work
continued. Final native/embedded tests instead prove a usable/compositable frame, continuing optional work,
exact source/selection/history, eventual publication and no idle wake. A flat coordinate oracle checks every
boundary, edits and older snapshots; an independently clipped full-source colour oracle checks distant windows
in 6,000 fenced blocks. Maximum-newline, RTL numbering and allocation sweeps retain atomic publication.
Native and embedded UIA tests read exact source before waiting separately for pixel/AST expectations.

The [retained attempts](retained-attempts.json) distinguish the mutable working experiment, isolated checks,
fixture failures, missing-PDB link warnings and whole-library failures. One disk/tool failure printed a clean
MSBuild footer despite structured `C0000006`/`C000007F` failure. It is rejected, and build diagnostics now refuse
such exception dumps before attestation or green output. Another independently reproduced host-continuation
failure was fixed. Final scheduling fixtures consume initial visual wakes before arming a distinct retirement
wake; colour assertions explicitly await colour without changing the earlier content/UIA boundary.

The first static closeout also retained two stale capability-table rows, a missing checkout of the exact
pinned CI engine and a timeout-fixture failure during concurrent builds. Both table rows were corrected,
the ignored engine checkout was restored at its declared pin, and the unchanged timeout, native-scope and
scoped-testing suites passed in isolated retries. These setup and documentation corrections change no
compiled input or native result.
The final complete validation aggregate and formatting check then passed with that exact pinned engine.

All six [gallery sheets](../../docs/gallery/README.md) were regenerated from clean implementation source and
visually reviewed. Their pixels match the previous gallery; generation provenance records this exact source.
The durable contracts are [performance/resources](../../Specs/Core/Core_PerformanceAndResources.md),
[controls/layout](../../Specs/UI/UI_ControlsAndLayout.md),
[embedded hosting](../../Specs/Rendering/Rendering_EmbeddedD3D11.md) and
[build admission](../../Specs/Build/Build_ConsoleAndPaths.md). Usage is in
[rich text](../../docs/rich-text.md) and [hosting](../../docs/hosting.md).

These are scoped functional and bounded-work results. Unpaired timing diagnostics do not establish paired
performance acceptance. Consumer adoption, physical input/AT and remaining rendering/API/platform gates retain
their owners in the [single active plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
