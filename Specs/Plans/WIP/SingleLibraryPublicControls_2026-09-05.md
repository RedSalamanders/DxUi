# Single library, public controls and supplied-device consumption

Status: ACTIVE
Date: 2026-09-05

The user approved one DxUi.lib, a complete public control API, embedded rendering with a caller-supplied D3D11
device, a standalone toggle/slider consumer, all meaningful inherited control tests, and an all-control gallery.
This plan owns library delivery; RedXe AV device backends and the later RedSalamander application port remain separate.
Owning contracts: [architecture](../../Core/Core_Architecture.md), [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[controls](../../UI/UI_ControlsAndLayout.md), [validation](../../Testing/Testing_Validation.md),
[build](../../Build/Build_ToolchainAndConsumption.md), [performance](../../Core/Core_PerformanceAndResources.md).

- [x] Consolidate Foundation and controls into one DxUi.lib and expose independent public headers.
- [x] Remove application dependencies; preserve shared controls, native text, accessibility and window-host behavior.
- [x] Supply externally owned graphics/resources and input through an embedded host with separate prepare/composite phases.
- [x] Add a standalone supplied-device toggle/slider consumer using only public headers and the single library.
- [x] Provide a catalog/factory and gallery covering every concrete public control, with generated visual artifacts.
- [x] Classify every inherited test, port reusable behavior tests, remove application/source-text-only cases with reasons.
- [ ] Validate WARP rendering/input/recovery, public consumption, idle/clean costs and the four native configurations.
- [ ] Reconcile all specs/skills/build contracts and AV dependency references; publish tested main and close this plan.

## Resume handoff — user paused work on 2026-09-05

The user explicitly requested saving everything to continue later. Stop implementation at this handoff; do not
mark delivery complete or merge to main until the remaining validation is resolved.

### Repository and implemented work

- Canonical checkout: `Z:/src/DxUI`; private GitHub `RedSalamanders/DxUI`; default branch `main`.
- Work branch: `codex/single-library-controls`. Latest implementation commit before this handoff:
  `26ac85f766192ef85441a578744ad91386480546`, already pushed. Main remains at
  `f28f55d0b4648bdec131f506c017dabc797131af`; do not force-push or reset either checkout.
- One `src/DxUi.vcxproj` -> DxUi.lib, API revision 2 and lock target `["DxUi"]`. Foundation, Controls,
  Embedded and Win32 are internal implementation areas, not separate libraries.
- Public DxUi.h, Embedded.h, ControlCatalog.h, ThemeColors.h, Diagnostics.h, FrameRuntime.h and Configuration.h.
  ControlHost is neutral; WindowHost is a compatibility alias. Slider exposes Preview/Commit/Cancel.
- GraphicsDevice retains a caller-created BGRA D3D11 device; EmbeddedHost separates dirty Prepare from allocation-free
  Composite. Shared device pool, independent views, logical capture, DPI, hidden/zero-size suspension, device
  replacement and coherent prepared input are implemented. No embedded HWND, swap chain, worker or timer.
- All 26 concrete controls have catalog/factory entries and populated five-theme gallery tiles. Live/headless
  supplied-device toggle/slider example is under Samples/EmbeddedControls and uses public headers only.
- 941 inherited named cases inventoried: 853 retained runtime cases; 88 individually reasoned exclusions.
  Eight original PNG baselines unchanged. Eighteen suites run in test.ps1 with per-suite receipts and explicit skips.
- Relocated exact-pin external consumer restores isolated dependencies, renders the sample and rejects five invalid
  pin/API/target/dirty-source cases. WIL/vcpkg pinned; standalone builds require neither application checkout.
- RedXe RFCs updated locally only: Specs/Plans/WIP/RFC_Core_DxUiSharedProject.md and RFC_Plugins_AVControl.md.
  Do not stage or reset unrelated RedXe edits. RedXe still needs its real preparation/input/text/UIA bridge,
  release pin and AV backend. Human touch/IME/screen-reader checks remain adoption gates; RedSalamander is on HOLD.

### Validation and current open issue

- Local x64 Debug and Release EmbeddedTests: 1,448 checks each; 1,000 warm composites with zero C++ allocations,
  no extra preparation or surface allocations. Timing measures CPU submission, not GPU completion.
- Local complete suites passed before the last menu fixture changes. The latest menu readiness change separately
  passed Menu in Debug and Release with zero skips. Formatting, nine skills, specs/dependencies, test-port validation
  and 18 Python tooling regressions have passed; rerun relevant checks after any new change.
- Latest full CI: [run 33965482623](https://github.com/RedSalamanders/DxUI/actions/runs/33965482623),
  exact implementation commit `26ac85f766192ef85441a578744ad91386480546`.
- At pause: Linux validation succeeded; x64 Debug FAILED; x64 Release and both ARM64 jobs were still running.
  The failure cause is not yet retrieved. GitHub refused the completed-job log while the overall run remained active.
  **First resume action: retrieve this run's final status and failed logs; do not assume the menu fix made CI green.**
- x64 Debug job ID: 101304739553. A log retrieval attempt wrote an empty `.build/ci-26ac-debug.log`; it is not evidence.
- A local `gh run watch` helper may still finish and write `.build/final-ci-status.json` and `.build/final-ci-watch.log`.
  Do not rely on the process/session surviving. Explicitly refresh status from the run ID before using these files.
- Prior run 33964742224 at 775f010: validation, x64 Debug and both ARM64 passed; x64 Release failed the inherited
  owner-window-message-flood hover test. Earlier runs exposed fixture timing/motion and optional SDK-layer issues.
  Do not combine successful configurations from different source commits to claim a green final matrix.

### Recent fixes and diagnosis context

- 7971712: motion-dependent tests explicitly select an animated theme instead of inheriting Windows reduced-motion
  settings; production still honors that setting. No OS preference changes and no baseline changes.
- b37d39f: cancel captured drafts before preparing controls made hidden/disabled. Zero-size suspension also fixed.
- defcfbf: baseline failure PNGs write to .build/test-artifacts instead of the source Baselines tree.
- a5aed751: native Debug D3D creation retries once without DEBUG only on DXGI_ERROR_SDK_COMPONENT_MISSING.
  ARM64 reported 0x887A002D for its unavailable optional SDK layer. Functional tests then passed; do not claim the
  absent debug layer was exercised. A WindowHost regression prints the raw probe and requires usable text/graphics.
- 775f010: bitmap capture waits for a visible final surface (five-second cold-start allowance), avoiding a
  DPI-virtualized driver-thread client-rectangle comparison. This fixed the capture fixture.
- 26ac85f: shared menu popup lookup now requires IsWindowVisible, because native creation publishes a hidden 1x1
  measurement HWND and item geometry before backdrop capture, final positioning and the initial frame. Drivers
  previously could send input to that temporary HWND. Setup wait is five seconds; flood response remains 800 ms.
- Relevant code: Tests/Controls/DxUiTests.Menu.cpp popup Find/Wait helpers around lines 653-780 and
  TestSplitButtonContextMenuOwnerMessageFloodDoesNotStarvePointerInput around line 1200.
  Native CreateMenuPopupWindow in src/Controls/DxUi.Menu.cpp creates the temporary HWND around line 3060,
  sets fields before backdrop capture, then SetWindowPos/RenderInitialFrameForShow/ShowWindow around line 3200.
  Modal loop already prioritizes input and debug-state probes before owner messages.
- Do not weaken latency assertions, alter pixel baselines, exclude failed runtime tests or add retry-to-green loops.
  Inspect the new failure before deciding whether further fixture or production changes are needed.

### Resume and closeout sequence

1. Read AGENTS.md, relevant local skills and this plan; inspect Git status in both repos. DxUI writes/build/git require
   an escalated shell from a RedXe-rooted task (outside its original writable root); the user already authorized them.
   Use `py -3 -X utf8` in that shell. Do not rerun earlier one-shot mutation scripts under RedXe/.build.
2. Fetch the latest run's results and failed logs. Resolve actual failures, run affected local tests and the required
   full native x64/ARM64 Debug/Release matrix. Cross-building is not native validation. Preserve explicit skips.
3. After ONE final run has all five jobs successful, download its four native artifacts into
   `.build/ci-final-<runId>/native-<arch>-<config>`; require 18 successful native receipts per configuration and the
   x64 Debug ExternalConsumer receipt with five negative checks and matching tested commit. Keep runs separate.
4. A guarded, NOT YET EXECUTED closeout helper is saved locally at `.build/resume/close_delivery.py` (original:
   Z:/src/RedXe/.build/dxui_close_delivery.py). Review/update it before use: it requires completed successful
   `.build/final-ci-status.json`, all 72 exact-run native receipts and the matching consumer receipt. It marks/moves
   this plan to Done, updates indexes, fixes stale bootstrap-only normative paragraphs, and appends evidence to
   both RedXe RFCs. Counts and wording must reflect any further source/test changes. Do not run it on this failed run.
5. Reconcile remaining stale docs: UI_ControlsAndLayout and UI_ThemeAndTypography still contain old bootstrap-only
   paragraphs; Build_ToolchainAndConsumption has an earlier selected-targets sentence; normative introductions
   should refer to pending integrations instead of split targets. The saved helper prepares these corrections.
   Keep BootstrapAndRedXeAdoption ACTIVE and RedSalamander HOLD: application adoption is not delivered here.
6. Validate format/specs/skills/dependencies/test-port and tooling after final changes. Commit closeout, fetch origin
   main, fast-forward main to the tested branch without force, and push only after required evidence exists.
   The helper's "published on main" statement is only true after this step succeeds.
7. Verify private/default main, clean DxUI source and matching local/remote main. Leave unrelated RedXe work intact.

Useful commands from Z:/src/DxUI:

```powershell
gh run view 33965482623 --repo RedSalamanders/DxUI --json databaseId,headSha,url,jobs,status,conclusion
gh run view 33965482623 --repo RedSalamanders/DxUI --log-failed
./test.ps1 -Configuration Debug -Platform x64 -Suites Menu
./test.ps1 -Configuration Release -Platform x64 -Suites Menu
./gallery.ps1 -SkipBuild
./.build/x64/Debug/DxUi.EmbeddedControls.exe
```

Local gallery: `.build/gallery/x64/Debug/index.html`; sample PNG: embedded-controls.png beside it.
Local backup directory `.build/resume` preserves the closeout helper, both complete RedXe RFC files and their Git
patch. Build products, CI artifacts and logs remain under .build and are intentionally untracked.
This handoff is a documentation-only checkpoint; it does not claim implementation validation or delivery is complete.
