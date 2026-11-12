# Explicit state machines and lifetime safety

Status: ACTIVE — plan only; code implementation has not started.
Priority: P1 interaction/coherence correctness, TSF/IME & animation reentrancy safety, developer ergonomics (DX) and agent safety (AX).
Owner: DxUi maintainers.
Prepared: 2026-11-10.
Planned-at commit: `8af272753cec60a68cbdb3a8b2a7bef0563116ac` (`RedSalamanders/DxUi`).

- **Owning contracts**: [repository guidance](../../../AGENTS.md), [specification authority](../../README.md), [architecture](../../Core/Core_Architecture.md), [performance and resources](../../Core/Core_PerformanceAndResources.md), [failure handling](../../Core/Core_FailureHandling.md), [documentation](../../Core/Core_Documentation.md), [changes workflow](../../Core/Core_Changes.md), [controls and layout](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md), [design system](../../UI/UI_DesignSystem.md), [embedded D3D11 rendering](../../Rendering/Rendering_EmbeddedD3D11.md), [Win32 host rendering](../../Rendering/Rendering_Win32Host.md), [toolchain and consumption](../../Build/Build_ToolchainAndConsumption.md), [unified CI](../../Build/Build_UnifiedCI.md), [validation](../../Testing/Testing_Validation.md), and [run evidence](../../Testing/Testing_RunEvidence.md).
- **Coordinated plans**: [RemainingWork_2026-11-04.md](RemainingWork_2026-11-04.md) retains ownership of formal performance-policy qualification, broader admission, and consumer adoption; [RuntimeEfficiency_2026-11-07.md](RuntimeEfficiency_2026-11-07.md) retains ownership of `P0`–`P8`, `V1`–`V8`, `B1`–`B4`, and API-revision-6 decisions (`D3`, `D5`–`D10`). This plan introduces **zero public ABI changes**, **zero new controls**, and **zero consumer pin bumps**; it refactors private state representations and `Tools/` lock/iteration state machines to eliminate state-tearing bugs.

## 1. Mandatory pre-execution drift check

Before editing any file for this plan, run these read-only checks from the `DxUi` repository root:

```powershell
git status --short
git rev-parse HEAD
git diff --name-status 8af272753cec60a68cbdb3a8b2a7bef0563116ac HEAD -- AGENTS.md include src Tools Tests Test-Changes.ps1 validate.ps1 Specs
git diff --cached --name-status
git ls-files --others --exclude-standard
git worktree list --porcelain
```

Reopen every file reported by `git diff` and verify line anchors against current `HEAD` before applying any slice.

## 2. Architectural decision and non-breaking boundaries

Across `EmbeddedHost`, `ControlHost`, `RichTextEditor`, `MenuController`/`MenuPopup`, `ComboBox`, `TooltipLayer`, `TabControl`, `NumericStepper`, `Grid`, `Tree`, `AnimationDispatcher`, `SemanticBuilder`, `PreparedSemanticGeometry`, and `Tools/`, mutually exclusive interaction modes, TSF edit locks, incremental preparation passes, and scoped-test iteration leases are currently represented across independent booleans, overloaded `std::wstring` buffers, and unprefixed/zero-wait file locks.

### Non-breaking design patterns

1. **Pattern A — Exhaustive flat `enum class Phase : uint8_t` + centralized transitions**:
   - Used for `EmbeddedHost::State` readiness/wake/prepare phases, `AnimationDispatcher::RunState`, `ClientTextStoreTarget::EditLockState`, `TooltipLayer::Mode`, `MenuController::SessionPhase`, and `SemanticBuilder::Preparation::Visit::Kind`. Switch statements omit `default:` so `/Wall` enforces exhaustiveness (`C4061`/`C4062`).
2. **Pattern B — Inline value-type `std::variant<std::monostate, ...>`**:
   - Used for `RichTextEditor::State::PointerGesture`, `TabControl::HeaderGesture`, `GridPointerGesture`, `Tree` row pointer gestures, `NativeTextInputSession` (`TsfSessionState` vs `ImmSessionState`), and `NativeImagePlacement` (`RasterPlacementState`, `SvgPlacementState`, `MathPlacementState`)—with zero heap allocations on hot paths and single-statement `.emplace<std::monostate>()` cancellation.
3. **Pattern C — Reentrant callback & lifetime guards**:
   - Preserves `TreeModelQueryGuard`, `Failure::Scope` ([Core_FailureHandling.md](../../Core/Core_FailureHandling.md)), and WIL RAII across all Win32/COM/TSF callbacks; defers `AnimationDispatcher::Shutdown()` subscription clearing until any active `OnTimerTick()` loop finishes.
4. **Pattern D — Discriminated PowerShell verdicts, `Global\` mutexes & atomic staging**:
   - Aligns [Test-Changes.ps1](../../../Test-Changes.ps1), [ScopedTesting.psm1](../../../Tools/ScopedTesting.psm1), [SdkLibraryInputs.psm1](../../../Tools/SdkLibraryInputs.psm1), and [TestRunEvidence.psm1](../../../Tools/TestRunEvidence.psm1) with closed verdict objects, `Global\` profile mutexes, bounded `ERROR_SHARING_VIOLATION` lock retries, and atomic `.tmp` / staging swaps.

### Hard invariants (zero breaking changes)

- **Public API & ABI stability**: Public signatures, class layouts used across consumer pins, and `DXUI_API_REVISION` remain unchanged unless explicitly coordinated with `D3` in [RuntimeEfficiency_2026-11-07.md](RuntimeEfficiency_2026-11-07.md). Slices `S0`–`S2` touch only `.cpp`-private `State`/`Impl` structs, `src/Support/AnimationDispatcher.h`, and `Tools/*.psm1`.
- **Zero consumer coupling**: A library change never implicitly edits `RedXe`, `RedPrism`, `RedTilde`, `RedSalamander`, or `RedMonitor`.
- **Zero interactive desktop takeover without authorization**: Focus-taking suites (`Menu`, `NativeTextInput`, and the two menu resource fixtures) run only via `test.ps1 -Interactive` with explicit user time agreement.

## 3. Source-verified defect ledger and state-machine catalog

### 3.1 Concrete verified bugs (`P0` / `P1` / `P2`)

| ID | Location & symbol | Verified defect & symptom | Non-breaking fix |
| --- | --- | --- | --- |
| `DXUI-SM-01` | `RichTextEditor::State` pointer gestures in [DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) (`L614`–`L649`, `L6921`–`L6934`, `L7620`–`L7648`) | Pressing left mouse inside an existing selection sets `dragging = true` and `sourceMovePress = true`. Pressing `VK_ESCAPE` before moving past `kDragThresholdDip` clears `dragging`, `codePressGesture`, and `taskPressPlan`, **but forgets `sourceMovePress = false`**; the next uncaptured `OnMouseMove` checks `if (_state->sourceMovePress)` before `if (!_state->dragging)` and fires `onSourceDragRequested` with no button pressed. | Replace the 18 gesture fields in `RichTextEditor::State` with `PointerGesture` `std::variant` reset via `.emplace<std::monostate>()` in `VK_ESCAPE`, `OnCaptureLost`, and `OnMouseUp`. |
| `DXUI-SM-02` | `EmbeddedHost::InputIsCoherent` & `KeyboardInputIsAvailable` in [Embedded.cpp](../../../src/Rendering/Embedded.cpp) (`L140`–`L155`, `L826`–`L889`) | When `_state->preparedInteractionRevision != _host._interactionRevision`, `InputIsCoherent()` sets `coherent = false` and cancels capture **without clearing `interactionLayoutAvailable`**, allowing `KeyboardInputIsAvailable()` (`!contentPending && interactionLayoutAvailable`) to dispatch keyboard input against stale geometry while pointer input is refused. | Model readiness via `SurfaceReadiness` and centralize `State::InvalidateCoherence()` so pointer and keyboard coherence invariants never diverge. |
| `DXUI-SM-03` | `AnimationDispatcher` in [AnimationDispatcher.h](../../../src/Support/AnimationDispatcher.h) (`L78`–`L92`, `L288`–`L294`, `L461`–`L497`) | (1) `WM_NCDESTROY` sets `_timerRunning = false` without resetting `_hasTickTimestamp` and clock epoch fields, causing the next `StartTimer()` to compute a huge stale delta; (2) reentrant `Shutdown()` inside `OnTimerTick()` calls `_subscriptions.clear()` while iterating `_subscriptions`. | Group clock fields into `std::optional<TickClockEpoch>` cleared in both `StopTimer()` and `WM_NCDESTROY`; use `RunState` (`Stopped`, `Armed`, `Ticking`, `TickingShutdownRequested`) so `Shutdown()` defers clearing until `OnTimerTick()` exits. |
| `DXUI-SM-04` | `ComboBox::_typeaheadBuffer` in [DxUi.ComboBox.cpp](../../../src/Controls/DxUi.ComboBox.cpp) (`L937`, `L1765`–`L1828`) | Non-editable mode uses `_typeaheadBuffer` as a timed search prefix, whereas editable mode repurposes `_typeaheadBuffer` to buffer a UTF-16 lead surrogate (`_typeaheadBuffer.assign(1u, ch)`); `OnMouseDown` clears it unconditionally and `SetEditable(true)` retains stale prefix characters before `IS_HIGH_SURROGATE(_typeaheadBuffer[0])`. | Store editable lead surrogates in a dedicated `wchar_t` (matching `TextField`), keeping `_typeaheadBuffer` strictly for non-editable typeahead search. |
| `DXUI-SM-05` | `ClientTextStoreTarget::_failed` in [TextInputServices.cpp](../../../src/Controls/TextInputServices.cpp) (`L218`–`L240`, `L354`–`L487`) | `BeginEdit()` sets `_failed = false` and `EndEdit()` checks `if (!success \|\| _failed)`, **but `_failed` is never assigned `true` anywhere** when nested composition steps fail inside an outer edit lock. | Replace `_editing`, `_writable`, `_dirty`, and `_failed` with `EditLockState` (`Unlocked`, `ReadOnly`, `WritableClean`, `WritableDirty`, `WritableAborted`) and mark failed inner mutations `WritableAborted`. |
| `DXUI-SM-06` | `RichTextEditor::State` rejection caches in [DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) (`L570`–`L574`, `L783`–`L785`) | `SynchronizeReadOnlySourceSnapshot()` resets `markdownCapacityRejected`, `nativeGeometryRejected`, and `semanticRejected`, but omits `preparationRejected` and `nativeObjectRejections`. | Group all 5 rejection caches into `PreparationRejections` with single-call `ResetForDocumentChange()` and `ResetForLayoutChange()` helpers. |
| `DXUI-SM-07` | `TabControl` drag reorder & `Tree` single-click capture in [DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) (`L9686`–`L9687`, `L10031`) and [DxUi.Tree.cpp](../../../src/Controls/DxUi.Tree.cpp) (`L584`–`L600`, `L2180`–`L2202`) | `TabControl::UpdateDragReorder` updates `_draggingTabIndex` without clearing `_pressedTabIndex`, causing `OnMouseUp` to check the pre-drag slot; `Tree::SetDelegate` and `SetInvokeOnSingleClick(false)` reset `_singleClickInvokeItemId` without clearing `_singleClickCaptureArmed` or releasing capture. | Unify `TabControl` header gestures and `Tree` row gestures into single gesture state helpers with centralized `CancelPointerGesture()`. |
| `DXUI-SM-08` | Scoped-testing iteration & `TestRunEvidence` locks (`ISM-14`, `ISM-37`) in [Test-Changes.ps1](../../../Test-Changes.ps1) (`L88`–`L122`), [ScopedTesting.psm1](../../../Tools/ScopedTesting.psm1) (`L255`), [SdkLibraryInputs.psm1](../../../Tools/SdkLibraryInputs.psm1) (`L144`–`L236`), [TestRunEvidence.psm1](../../../Tools/TestRunEvidence.psm1) (`L64`–`L71`) | (1) Prose-seeded `iteration.json` (`nativeContext == ''`) skips `$iterationFull` escalation when `vcpkg_installed`/toolchain changes before a leaf C++ edit, `NO_WORK` early-exit skips `Remove-ScopedIteration`, and `DxUi.ScopedProfile` omits `Global\`; (2) `TestRunEvidence.psm1` `Native::Lock` has zero retry on `ERROR_SHARING_VIOLATION`; (3) `SdkLibraryInputs.psm1` can leak `sdk-library-*` staging dirs on failure. | Evaluate native dependency context on transition from prose `iteration.json`, clean `iteration.json` before `NO_WORK`, prefix `Global\`, add bounded retry in `Native::Lock`, and clean `$staging` in `finally`. |

### 3.2 Complete 19-subsystem state-machine catalog

| # | Subsystem & location | Current implicit encoding | Target non-breaking refactor |
| --- | --- | --- | --- |
| `1.1` | `EmbeddedHost::State` readiness, reentrancy & wakes ([Embedded.cpp](../../../src/Rendering/Embedded.cpp)) | 13 booleans (`DXUI-SM-02`) | **Pattern A:** `PrepareCallPhase` + `SurfaceReadiness` + `WakeSchedule`. |
| `1.2` | `ControlHost` visibility & animation suspension ([DxUi.h](../../../include/DxUi/DxUi.h), [DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp)) | Dual `_nativePresentationResumeAnimation` & `_animationSuspendedWhileHidden` booleans | **Pattern A:** `AnimationTickMode` + suspend-reason bitmask (`WindowHiddenOrIconic \| NativePresentationHidden`). |
| `1.3` | `ControlHost` UIA focus-gain turn handshake ([DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) `L5409`–`L5477`) | `_gainingWindowFocus`, `_focusGainTurnStartedMs`, `_focusGainTurnAnnounced` | **Pattern A:** Encapsulated `FocusGainTurnState` helper methods. |
| `1.4` | `ControlHost` 4-stage preparation pipeline ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) `L2365`–`L2713`) | Asymmetric per-control booleans vs `ContentPreparationState` | **Pattern A:** Symmetric `IntrinsicLayoutState` and `VisualPreparationState` transitions. |
| `1.5` | `ControlHost` TSF / IMM32 backend session ([DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) `L1424`–`L2160`) | 24 flat fields across TSF, IMM32, and keystroke translation | **Pattern B:** Internal `TsfSessionState` / `ImmSessionState` encapsulation. |
| `1.6` | `NativeTextStoreTarget` TSF edit transaction ([DxUi.TextStoreACP.cpp](../../../src/Controls/DxUi.TextStoreACP.cpp) `L1002`–`L1029`) | `_editBase`, `_stagedState`, `_editActive`, `_compositionEndPending` | **Pattern B:** `std::optional<ActiveEditTransaction>` + `std::optional<ActiveCompositionState>`. |
| `1.7` | `ClientTextStoreTarget` edit lock ([TextInputServices.cpp](../../../src/Controls/TextInputServices.cpp) `L218`–`L240`, `L481`–`L487`) | `_editing`, `_writable`, `_dirty`, unwired `_failed` (`DXUI-SM-05`) | **Pattern A:** `EditLockState` (`Unlocked`, `ReadOnly`, `WritableClean`, `WritableDirty`, `WritableAborted`). |
| `1.8` | `TextField` password reveal & concealed-mask bucket ([DxUi.TextInput.cpp](../../../src/Controls/DxUi.TextInput.cpp)) | 3 button booleans reset in 7 places + 5 mutable mask cache fields | **Pattern B:** `PasswordRevealSubcontrolState::ResetInteraction()` + `ConcealedMaskBucketCache`. |
| `1.9` | `RichTextEditor::State` preparation & rejection caches ([DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) `L557`–`L613`) | 5 independent rejection caches (`DXUI-SM-06`) + `codeInk*` | **Pattern B:** `PreparationRejections` + `CodeInkPreparationState`. |
| `1.10` | `RichTextEditor::State` pointer gestures ([DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) `L614`–`L649`) | 18 gesture fields; `VK_ESCAPE` leaks `sourceMovePress` (`DXUI-SM-01`) | **Pattern B:** Inline `PointerGesture` `std::variant`. |
| `1.11` | `MenuController` & `MenuPopup` cascade/timers ([DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp) `L1115`–`L1200`, `L1461`–`L1505`) | 15+ session/input booleans, 3 `hasLast*` pairs, 4 hover-timer fields, 7 slider fields | **Pattern A + B:** `SessionPhase`, `std::optional<ArmedSubmenuTimer>`, `std::optional<SliderStopAnimation>`, and `std::optional<POINT>`/`DWORD`. |
| `1.12` | `ComboBox` typeahead vs UTF-16 surrogate ([DxUi.ComboBox.cpp](../../../src/Controls/DxUi.ComboBox.cpp) `L1765`–`L1828`) | Dual-purpose `_typeaheadBuffer` (`DXUI-SM-04`) | Dedicated surrogate scalar decoupled from `_typeaheadBuffer`. |
| `1.13` | `TooltipLayer` show/hide/inspect/dismiss ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) `L11884`–`L12202`) | 5 booleans including `Clear()` + re-armed `_dismissGestureActive` | **Pattern A:** `TooltipLayer::Mode` (`Hidden`, `ShowScheduled`, `VisiblePassive`, `VisibleHideScheduled`, `Inspecting`, `InspectingDrag`, `AwaitingDismissUp`). |
| `1.14` | `TabControl` header press & drag reorder ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) `L9625`–`L10040`) | Stale `_pressedTabIndex` after `UpdateDragReorder` (`DXUI-SM-07`) | **Pattern B:** `HeaderGesture` clearing press slot on entry to `TabReorderDrag`. |
| `1.15` | `NumericStepper` & `ColorPicker` live preview ([DxUi.EditorControls.cpp](../../../src/Controls/DxUi.EditorControls.cpp) `L786`–`L1600`) | Programmatic `SetValue()` during `_editing` leaves stale `_editStart` | Reconcile `_editStart` / `_editPreviewed` when `SetValue()` runs outside `_syncing`. |
| `1.16` | `Grid` header resize/reorder & thumb drag ([DxUi.Grid.cpp](../../../src/Controls/DxUi.Grid.cpp) `L5007`–`L5563`) | Simultaneous `_pressedHeaderColumn` + `_dragReorderColumn` + 2 cell-anim bools | **Pattern B:** `GridPointerGesture` + `std::optional<bool>` animated-cell cache. |
| `1.17` | `Tree` reorder drag & single-click invoke ([DxUi.Tree.cpp](../../../src/Controls/DxUi.Tree.cpp) `L584`–`L600`, `L1879`–`L2203`) | Dual capture latches (`DXUI-SM-07`) | **Pattern A:** Unified `RowPointerGestureKind` + `CancelPointerGesture()`. |
| `1.18` | `AnimationDispatcher` & semantic builders ([AnimationDispatcher.h](../../../src/Support/AnimationDispatcher.h), [DxUi.RichSemanticDocument.cpp](../../../src/Controls/DxUi.RichSemanticDocument.cpp), [DxUi.RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp)) | `AnimationDispatcher` epoch/reentrancy (`DXUI-SM-03`), 7-bool `Visit`, 18-field `NativeImagePlacement`, magic `paintPhase` | **Pattern A + B:** `TickClockEpoch` + `RunState`, `Visit::Kind`, `NativeImagePlacement` `std::variant`, `PaintIndexSubphase`. |
| `1.19` | `Test-Changes.ps1`, `ScopedTesting.psm1`, `SdkLibraryInputs.psm1`, `TestRunEvidence.psm1` | Iteration escalation/cleanup gap, unprefixed mutex, zero-wait `Native::Lock`, leaked staging dir (`DXUI-SM-08`) | **Pattern D:** Discriminated `ScopedRunPlan`, `Global\` mutex, retryable `Native::Lock`, `finally` staging cleanup. |

## 4. Ordered implementation slices

1. **Slice S0 — Drift check & `Tools/` PowerShell state-machine hardening (`1.19`)**:
   - Run Section 1 drift checks.
   - Fix `DXUI-SM-08` in [Test-Changes.ps1](../../../Test-Changes.ps1), [ScopedTesting.psm1](../../../Tools/ScopedTesting.psm1), [SdkLibraryInputs.psm1](../../../Tools/SdkLibraryInputs.psm1), and [TestRunEvidence.psm1](../../../Tools/TestRunEvidence.psm1), with unit tests in `Tools/tests/Invoke-ToolingTests.ps1`.
2. **Slice S1 — Private `.cpp`-only P0/P1 bug fixes (zero public header edits: `1.1`, `1.6`, `1.7`, `1.9`–`1.11`, `1.18`)**:
   - Fix `RichTextEditor::State` `PointerGesture` (`DXUI-SM-01`) and `PreparationRejections` (`DXUI-SM-06`) in [DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp), `EmbeddedHost::State` coherence (`DXUI-SM-02`) in [Embedded.cpp](../../../src/Rendering/Embedded.cpp), `AnimationDispatcher` (`DXUI-SM-03`) in [AnimationDispatcher.h](../../../src/Support/AnimationDispatcher.h), `ClientTextStoreTarget` (`DXUI-SM-05`) in [TextInputServices.cpp](../../../src/Controls/TextInputServices.cpp), `NativeTextStoreTarget` in [DxUi.TextStoreACP.cpp](../../../src/Controls/DxUi.TextStoreACP.cpp), `MenuController`/`MenuPopup` in [DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp), and `Visit::Kind` / `NativeImagePlacement` in [DxUi.RichSemanticDocument.cpp](../../../src/Controls/DxUi.RichSemanticDocument.cpp) and [DxUi.RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp), with headless tests in `Tests/Controls` and `Tests/Embedded`.
3. **Slice S2 — Control interaction state fixes (`1.2`–`1.5`, `1.8`, `1.12`–`1.17`)**:
   - Fix `ComboBox` UTF-16 surrogate handling (`DXUI-SM-04`), `TabControl` drag-reorder release (`DXUI-SM-07`), `Tree` single-click capture reset (`DXUI-SM-07`), `NumericStepper::SetValue` during edit, `TooltipLayer::Mode`, and `ControlHost` animation suspend bitmask. Keep any `include/DxUi/DxUi.h` private-member layout touch strictly coordinated with `DXUI_API_REVISION` rules in [Build_ToolchainAndConsumption.md](../../Build/Build_ToolchainAndConsumption.md) (or keep helper state inside existing `.cpp` translation units where no header layout change is needed).
4. **Slice S3 — Baseline/candidate performance verification, docs review & spec closeout**:
   - Capture paired complex-UI FPS/memory baseline vs candidate receipts under [Core_PerformanceAndResources.md](../../Core/Core_PerformanceAndResources.md), review `docs/` under [Core_Documentation.md](../../Core/Core_Documentation.md), and update [Rendering_EmbeddedD3D11.md](../../Rendering/Rendering_EmbeddedD3D11.md), [UI_InputAndAccessibility.md](../../UI/UI_InputAndAccessibility.md), and [Testing_Validation.md](../../Testing/Testing_Validation.md).

## 5. Validation commands and expected outcomes

```powershell
pwsh -NoProfile -File ./validate.ps1
pwsh -NoProfile -File ./Tools/Test-Changelog.ps1 -Base origin/main
pwsh -NoProfile -File ./format.ps1 -Check
pwsh -NoProfile -File ./Test-Changes.ps1 -Explain
pwsh -NoProfile -File ./Test-Changes.ps1
pwsh -NoProfile -File ./test.ps1 -Platform x64 -Configuration Debug
pwsh -NoProfile -File ./test.ps1 -Platform x64 -Configuration Release
pwsh -NoProfile -File ./test.ps1 -Platform x64 -Configuration "ASan Debug"
pwsh -NoProfile -File ./build.ps1 -Platform ARM64 -Configuration Debug
pwsh -NoProfile -File ./build.ps1 -Platform ARM64 -Configuration Release
pwsh -NoProfile -File ./build.ps1 -Platform ARM64 -Configuration "ASan Debug"
```

- **Static & formatting checks**: `validate.ps1` (all five `validate-*.ps1` checks plus `Tools/tests/Invoke-ToolingTests.ps1`), `Test-Changelog.ps1 -Base origin/main`, and `format.ps1 -Check` pass with zero findings.
- **Native builds & non-interactive suites**: `x64` `Debug`, `Release`, and `ASan Debug` test runs (reporting paired complex-UI FPS/memory) and all three `ARM64` builds pass with zero warnings; renderer touches in `Embedded.cpp` / `DxUi.WindowHost.cpp` include WARP, device-loss, and clean/dirty/hidden resource checks.
- **Interactive gate**: Any touch to `DxUi.Menu.cpp` or native focus/IME suites additionally requires an explicitly agreed `test.ps1 -Interactive` pass before Done closeout.

## 6. Out-of-scope boundaries, rollback and stop conditions

- **Out of scope**: Editing consumer repositories, bumping `DXUI_API_REVISION` without explicit coordination with [RuntimeEfficiency_2026-11-07.md](RuntimeEfficiency_2026-11-07.md), adding new catalog controls, or running `test.ps1 -Interactive` without the desktop owner's explicit agreement.
- **Stop conditions**:
  - Stop immediately if a slice alters `include/DxUi/*` public signatures or breaks consumer compilation without an explicitly authorized API-revision bump.
  - Stop and request developer direction with measured options if paired complex-UI FPS/memory regresses against the retained pre-change baseline.
- **Rollback**: Slices `S0` (`Tools/`), `S1` (`.cpp`-private state), and `S2` (controls) are independent and revert cleanly per commit.

## 7. Done criteria and closeout

1. Slices `S0`–`S3` pass `validate.ps1`, `format.ps1 -Check`, all three `x64` test configurations, all three `ARM64` builds, paired performance comparison, and any applicable authorized `-Interactive` gate.
2. Durable invariants are merged into their owning `Specs/` domain contracts.
3. Move this file to `Specs/Plans/Done/ExplicitStateMachines_2026-11-10.md`, remove its entry from `Specs/Plans/WIP/README.md`, and record the archive link in [Done/README.md](../Done/README.md).
