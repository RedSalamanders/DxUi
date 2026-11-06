# Upgrade a DxUi consumer

**API 5 migration in progress:** the source in this worktree is unqualified and no API 5 consumer pin is published.
The application owns `Failure::ProcessHooks`, its reporter, crash storage and termination policy. Install it on the
application owner thread before constructing controls or entering DxUi. Pass its versioned POD `HostServices` to
each independently linked module and register there outside `DllMain`, before that module creates objects or starts
workers. Stop new work, join workers, destroy controls, detach every module, and uninstall the host on its installing
thread. [The example host](../Samples/Shared/FailureHost.h) illustrates ownership; consumers must qualify their own
original-context dump handling, module boundaries and native configurations. The
[migration plan](../Specs/Plans/WIP/FailurePolicy_2026-11-02.md) remains open.

Direct stack, `new` and `std::make_unique` control construction requires an enclosing host-owned `Failure::Scope`.
`Separator()` is again declared `noexcept`; this does not make member construction fallible or permit unexpected
exceptions to continue. `CreateControl` uses genuinely checked outer heap storage: check its HRESULT and retain the
existing output on failure. Nested C++/STL construction failure takes the fatal host route. Use the new checked
setters and query results for invalid input, preserve checked native HRESULTs, and handle `MarkdownParser::Advance`
returning `Failed` without replacing its prior publication. Audit custom allocations and placement construction.
Rebuild every EXE and DLL with the exact matching headers, archive, compiler, platform, configuration and CRT; API 5
changes both source contracts and object layout. Changing a lock alone neither ports nor qualifies an application.

SDK ZIP users keep the matching `DxUi.pdb` beside `DxUi.lib` when linking, and retain the resulting application/DLL
PDBs for dump analysis. The SDK manifest binds both producer files; copying the library alone can lose library
debug information. A source-pin build produces its matching compiler PDB in its isolated output automatically.

For ASan diagnostic applications, configure the existing `ASAN_SAVE_DUMPS` facility to an application-owned
location before startup and verify the actual written path; use a short name/path supported by the runtime.
MSVC's sanitizer can abort an allocation before the C++ new-handler; its saved dump is
sanitizer-owned evidence. Preserve Visual Studio's sanitizer callback rather than installing a competing one.
Ordinary Debug/Release and escaping C++ exceptions retain the failure-host contract described here.

Use failure host **protocol 2** consistently in every EXE and DLL. It adds native-call
entry/exit handling so Windows can handle its own internal exceptions while reentrant
DxUi callbacks remain protected. Prefer the provided `ProcessHooks` table; a custom
table must implement and qualify the complete protocol. Keep the reporter and module
registration alive until outstanding UI Automation peers, retained snapshots and
queued notifications are released. `Uninstall`/`Detach` returns `BUSY` while these
objects still pin the host; closing a view alone does not prove their completion.
Public input and callback-dispatch methods enter their own failure scopes even
when called directly. Keep your own construction and argument preparation scoped
where they can allocate; a callee's scope starts after its arguments are evaluated.

Keep application scopes around direct work and construction, and outside the native message pump and COM
initialization/cleanup. At final process shutdown, retire views/clients and invoke `UiaDisconnectAllProviders`
while COM and the reporter are still alive; a component DLL must not perform that process-wide operation.
Keep each STA that registered an asynchronous accessibility interface COM-initialized and pumping until its
GIT registrations have been revoked; disconnecting a view alone does not establish that an in-flight delivery
has finished. There is no per-view drain notification, so retain the registering STA through the process-owned
UIA and failure-host drain unless the application independently establishes completion.
The example's explicit `Host::UninstallWhenIdle` then retries `BUSY` with owner-message dispatch and a five-second
cooperative deadline. Stop new work first: a blocked application window procedure cannot be interrupted by that
deadline. Timeout remains a failed shutdown. Retired UIA providers keep readable immutable runtime IDs and null
windowless/non-root HWND-host answers so native cleanup can complete; their state, navigation and actions stay unavailable.
Asynchronous UIA delivery now owns a shared registration lease until its proxy is released. Closing a view
retires its cache and baselines, and a late query cannot recreate the old canonical root. Keep the failure host
installed until those leases and retained providers finish; do not discard `BUSY` or force an unowned COM release.

For editor configuration, handle `TrySetDefaultContextMenuLabels`, `TrySetMarkdownAnalysisProfile`,
`TrySetMarkdownAnalysisExtensions`, `TrySetMarkdownRenderingEnabled` and `TrySetOnCodeCopyRequested` results.
`TrySetDecorations` returns an HRESULT: invalid ranges use `E_INVALIDARG`, capacity uses `E_BOUNDS`,
and a stale source uses `HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH)`; native query errors are returned unchanged.
Use checked document and syntax queries such as `TrySlice` and `TryLexemeValue` for externally supplied ranges.
Handle checked native preparation HRESULTs in the application, including device loss and allocation refusal;
do not rely on catching a C++ exception as the native error channel. Legacy convenience entrypoints that cannot
report invalid input use the host contract-failure route.

Keep scheduling optional editor visuals after content becomes ready. `preparationPending` reports content
prerequisites; individual syntax, code-ink or source-guide diagnostics can still be pending. Embedded hosts
continue on `NeedsPreparation()` and the existing owner-turn request while composing the current coherent
surface. Custom hosts must preserve that continuation without polling. Tokenizers now run only for visible
code blocks plus overscan, at most once per turn; do not use those callbacks for document-wide side effects.
Returned token refusal uses ordinary ink; an unexpected tokenizer exception is fatal through the host.

Audit application callbacks, model/delegate methods and custom controls as part of the port. An unexpected C++
exception escaping a callback invoked by DxUi is fatal through the host, including after an editor change was
committed. Return expected refusal through that callback's documented status or value; do not depend on an
exception being caught and execution continuing. This also applies across Win32, COM and module boundaries.

### Port checked operations

Use the checked form whenever data, ranges, identities or configuration can come from the application or a user.
Legacy convenience forms that cannot report a refusal take the fatal contract path on invalid input. The table
is a migration map, not a substitute for each declaration's bounds and ownership contract.

| Earlier operation | API 5 checked operation and result |
| --- | --- |
| Editor `SetText`, `SetSelection`, `SetDocumentFilePath`, `SetViewMode`, `SetAppearance`, `SetZoom`, search/highlight, labels and Markdown configuration | Corresponding `TrySet...` returns `RichText::SourceResult`; `TrySetDecorations` returns `HRESULT`. Check the declaration for its exact parameters. |
| `Document::Reset`, `SetSelection`, `Replace`, `ReplaceWholeText`, `ReplaceTransient`, `ReplaceSelection` | Corresponding `Try...` returns `SourceResult`; replacements also write `changed`, distinguishing a successful no-op from a changed document. |
| `DecodeUtf8`, `EncodeUtf8`, snapshot/syntax slices and lexeme queries | Use `TryDecodeUtf8`, `TryEncodeUtf8`, `TrySlice` and `TryLexeme...`, with their checked output parameters. |
| Projection, source/display mapping and source search | Use `TryProject`, `TryPrepare`, checked `TryToSource`/`TryToDisplay` and range mapping, or `TryFindInSource`; inspect `SourceResult`. |
| Markdown command planning or application and source edits | Use `TryBuildMarkdownCommand`/`TryBuildMarkdownDeletion`, `TryApplyMarkdownCommand` and `TryApplySourceEdits`. Check `SourceResult` and the returned plan or `changed`; a successful inapplicable plan is not a failure. |
| Semantic construction and export | Use `SemanticBuilder::TrySetSoftBreakPolicy`/`TryBegin`, `TryExportSemanticDocument`/`TryExportMarkdownSyntax`, and checked word/heading helpers. |
| Prepared text/document layout and queries | Use `TryPrepare`, `TryHitTest`, `TryCaret`, `TryRange`, `TryDraw` and other matching `Try...` mapping/navigation queries; inspect the exact `HRESULT`. |

`SourceResult` distinguishes success from invalid source, capacity, invalid range, stale identity and invalid
argument. Checked refusals preserve the documented output or transaction state; replacement `changed` is false
on refusal. Do not read an output as a new result until its status succeeds. Unexpected C++ allocation failures
remain fatal rather than becoming a `SourceResult`; a returned native allocation or device HRESULT remains an
explicit error channel.

For incremental work, inspect `MarkdownParser::Advance` and `SemanticBuilder::Advance` for
`MarkdownPreparationStatus::Failed` and their `MarkdownAdvance::result`; do not treat it as `Pending`.
An advance capacity failure discards staged work and preserves the last publication. Admission refusal preserves
existing pending/published work; the semantic builder's zero-budget rejection also preserves staging.
`PreparedDocumentLayout::BeginPreparation` can return `LayoutPreparationStatus::Failed` and exposes `Result()`;
`AdvancePreparation` can return `Failed` with its exact `LayoutAdvance::result`, discarding staging while retaining
the published layout. Handle the returned status and native HRESULT before deciding whether to retry or fall back;
do not spin on a permanent failure.

**Historical failure-policy review, 2 November 2026:** the [target allocation/exception policy](../Specs/Core/Core_FailureHandling.md)
requires checked nonthrowing refusal or host fatal diagnostics with useful originating context and dump. Published
source `5fc4d4c383bfa4891ee57a138924107bbbf11a2e` still uses exception recovery and is not qualified against that
new requirement. Follow the [open migration plan](../Specs/Plans/WIP/FailurePolicy_2026-11-02.md); this guide's
description of current constructor/API behavior is not an endorsement of exception propagation as normal error handling.

This guide covers the **2 November 2026 delivery**, merged in
[PR #159](https://github.com/RedSalamanders/DxUi/pull/159), and the application work needed to adopt it.
Its original runtime delivery commit is:

```text
6df17007875ae731a3808b475b10bd056328bbdf
```

The SHA above is the historical API 4 implementation/evidence cohort, not an API 5 pin. Select a reviewed,
qualified API 5 commit only after its implementation and acceptance gates close; record its complete SHA in the
consumer lock and review intervening changes.

The historical delivery declared **API revision 4** and one static target, **DxUi**. C++ object layouts have changed:
rebuild every module that uses DxUi with matching headers, archive, compiler, configuration and CRT.
An unchanged API revision does not establish binary compatibility.

## Choose the right migration

| Current consumer | Work to do |
| --- | --- |
| Already on API 4 | Implement the API 5 host, checked-result and rebuild work above; adopt a qualified exact API 5 pin when available. |
| Still on API 3 | Also port the historical revision-4 Grid/Tree focus and selection, child/tab transfer and menu contracts below. Update the consumer's expected-revision guard together with its adapters and lock. |
| API 2 or older | First apply the earlier interface/script renames in the [compatibility contract](../Specs/Build/Build_ToolchainAndConsumption.md#compatibility-and-the-consumer-interface), then the revision-4 and API 5 migrations. |

Read the actual `Dependencies/DxUi.lock.json` in the application branch being upgraded. Historical pins in plans
or another application's successful build do not identify or qualify this application's starting point.

## Upgrade in the consumer repository

1. Create a consumer change and retain its preceding lock and adapter commit for rollback. Record the target
   platform/configuration and selected toolchain. Prerequisites and the supported MSBuild imports are in
   [getting started](getting-started.md#consume-the-library) and the
   [consumption contract](../Specs/Build/Build_ToolchainAndConsumption.md).
2. Use the consumer's existing update workflow, or edit its exact lock in that reviewed change. Preserve any
   additional consumer-owned fields. The following is a **historical API 4 lock**, provided to identify the old
   cohort; it cannot be used as an API 5 lock:

   ```json
   {
     "repository": "https://github.com/RedSalamanders/DxUi",
     "commit": "6df17007875ae731a3808b475b10bd056328bbdf",
     "apiRevision": 4,
     "targets": ["DxUi"]
   }
   ```

   For a future qualified API 5 adoption, set `apiRevision` to 5 and `commit` to that exact qualified 40-character
   commit. A consumer that explicitly expects API 3 or 4 must port its adapters and advance that guard in the same
   change. Changing the lock alone will correctly fail restore. Pin the complete commit, rather than `main`.
3. Run the consumer's restore entrypoint for each target architecture. It resolves a clean, isolated source
   checkout and fingerprinted outputs; it must leave any developer sibling checkout alone. Its resolved
   `DxUiRoot`, lock and output root must agree. The public
   [pin validator](../Tools/validate_consumer.ps1) checks the canonical repository, exact clean checkout,
   API revision and single target. A pin validation pass does not build or test the application.
4. Restore dependencies using the same selected toolchain and output identity that the build will use. Keep
   MSVC 14.52.x/v145 and the exact selected version consistent across all LTCG objects. Use `/MDd` for Debug and
   `/MD` for Release, with matching architecture/configuration and ASan annotation policy. Existing
   [Consumer props/targets](../Build/DxUi.Consumer.targets) forward the producer settings. Retain one
   `src/DxUi.vcxproj` reference; newly owned parser/rendering sources are built by DxUi, without adding them
   individually to application projects.
5. Rebuild the complete application dependency closure, including EXEs, DLLs, adapters and custom controls that
   include DxUi headers. Use the consumer's supported clean/rebuild operation and the newly fingerprinted output
   root. A new `DxUi.lib` linked with old consumer objects is not a valid migration. Keep the build's zero-warning,
   zero-error result and source/dependency/toolchain identity.
6. Run the consumer checks below, record outcomes and remaining gates, then publish/merge the consumer change
   through its own workflow. Library PR #159 does not change an application lock or complete product adoption.

### Existing consumer entrypoints

The application owns these commands and their help. Check its current checkout before running them; these are
entrypoint examples, not a guarantee that every consumer branch has already been ported to API 4.

| Consumer | Update and restore workflow |
| --- | --- |
| RedSalamander | `Get-Help .\Tools\Update-DxUi.ps1 -Full`; preview with `.\Tools\Update-DxUi.ps1 -WhatIf`. Normal execution selects the current successfully validated main and then runs the product Full gate. Restore uses `.\Tools\Restore-DxUi.ps1 -Platform x64 -Configuration Debug`. |
| RedXe | `Get-Help .\Update-DxUi.ps1 -Full`; preview with `.\Update-DxUi.ps1 -WhatIf`. Normal execution selects the current successfully validated main and then runs `test.ps1 -Full`. Restore uses `.\restore-dxui.ps1 -Platform x64`. |
| RedPrism / RedTilde | Adopt the exact lock through the application's reviewed pin/adapter change, then use `.\restore-dxui.ps1 -Platform x64`; repeat for ARM64 when required. An old revision guard is part of the application port. |

The automatic updaters select **current main**, not an arbitrary historical commit, and can refuse while their
required CI result is pending. Verify the selected lock still names the candidate you intended to adopt.
`-UpdateOnly` skips product validation; it does not establish that the product passes. RedXe's
`-AllowApiRevisionChange` records another API revision for a deliberate port; it does not implement the adapter
or restore-guard changes. A successful coordinator/PR check also does not replace the separate
[main native qualification](../Specs/Build/Build_UnifiedCI.md).

## What changed in this delivery

This comparison is against pre-delivery main `4423074dee2410acb28e62b41072e6c561a79eb8`. A consumer pinned further
back must review the intervening changes too. See the [source-bound delivery report](../Measurements/WipDelivery_2026-11-02/README.md)
for implementation dispositions and executed evidence.

| Area | Change and application action |
| --- | --- |
| C++ compatibility | Public retained/host/document/clipboard types gained state. Recompile the full closure. Review aggregate initialization and exhaustive enum switches against the new headers; avoid positional field assumptions or persisted enum ordinals. |
| Rich editor appearance | `RichTextEditorAppearance` inserts fields before `maximumTextWidthDip`, including source presentation and the default-off `directSourceInkPreview`. Replace positional initialization with member assignment or designated members; keep the application's intended defaults. |
| Custom code | The historical API 4 delivery removed `Separator()`'s `noexcept`; API 5 restores it under the fatal host contract above. Review construction traits and ensure direct construction is scoped. New `PointerAction`, `WindowHostCursorKind`, Markdown diagnostic and semantic entries need appropriate handling in application switches. |
| Embedded rendering | `Prepare` can change PS shader-resource slot 0 and output-merger targets. Rebind application graphics state after preparation. Handle failed preparation without a tight retry loop; a superseded-frame `E_ABORT` requests another owner turn. See [hosting](hosting.md#embedded-view). |
| Native visibility | Optional `ControlHost::SetNativePresentationVisible(false)` suppresses native drawing and animation for application-known occlusion. Restore `true` when presentation is needed. It defaults to visible, requires the attached owner thread, and does not discover covering windows. Embedded hosting continues to use `SetVisible`. |
| Exact source and metadata | Counted RichText source preserves BOM, embedded NUL and mixed CR/LF/CRLF. Keep length/revision-aware mirrors and UTF-16 offsets. RichMarkdown closed front matter now has bounded YAML/TOML validation; handle syntax, duplicate-key, alias and capacity diagnostics separately. Application schemas and structured property editing remain application-owned. |
| File and clipboard transport | Optional STA OLE adapters support guarded drop-at-caret and source moves. Additive clipboard extensions cover structured file/HTML/bitmap and rendered-copy routes. Existing plain-text adapters need not implement those interfaces. See the integration notes below. |
| Rich presentation and images | Expanded bounded math/Mermaid, metadata, footnotes, tables, source-region editing, image and export mechanisms remain explicit profiles/opt-ins. Image authorization, workers, completion delivery and any network transport remain application-owned. Unsupported notation retains source fallback. See [current capabilities](rich-text.md#current-capabilities). |
| Input, accessibility and recovery | Lifetime/re-entry, prepared geometry, focus, image cancellation and resource failure handling were repaired and tested in library fixtures. Recheck the application's actual keyboard, pointer, TSF/UIA, teardown and device-recovery paths; physical AT/IME/touch acceptance is separate. |
| Build and distribution | Strict diagnostic acceptance requires complete successful zero-warning/error evidence. The production SDK is a separate compiler/CRT/source-bound artifact with an extracted-consumer smoke; a source-pin migration does not select an old SDK archive. Carry the updated [third-party notices](../THIRD-PARTY-NOTICES.md) through the application's packaging workflow. |

For example, preserve the appearance defaults and name only the application override:

```cpp
DxUi::RichTextEditorAppearance appearance{};
appearance.maximumTextWidthDip = 960.0f;
```

### Optional host features

For native visibility, check the returned `HRESULT`; `S_FALSE` means the value was unchanged, while unattached/
embedded and wrong-thread calls fail. Resume explicitly after a false signal. It preserves logical input/tree/
accessibility state and does not activate the window. The
[native host contract](../Specs/Rendering/Rendering_Win32Host.md) defines the exact behavior.

For OLE drop, use [`DxUi/FileDrop.h`](../include/DxUi/FileDrop.h) and the
[compiled application example](../Samples/EmbeddedControls/Main.cpp). Initialize OLE on the receiving UI thread
(for example, `OleInitialize` with `OleUninitialize` owned by `wil::scope_exit`, as in the sample); COM STA initialization alone is insufficient for
`RegisterDragDrop`. The application owns the HWND/router and OLE registration, retains a receiver, attaches the revocable adapter, and maps screen
coordinates to the target view's local DIPs. Route preview/commit through the native or embedded host after
coherent preparation. Revalidate the target and `RichFileDropGuard` after external `IDataObject` calls; a receiver
reports success only after one source transaction commits. Revoke registration before HWND/adapter destruction;
`Detach` makes retained target references inert. Source MOVE uses the captured source move/revision checks,
rather than deleting text just because another target advertised a move. Dropping a reference does not copy an
asset or start an image worker. The bounded drop profile is 16 files and a 256-KiB medium.

For rich clipboard transport, optional interfaces in
[`TextInputServices.h`](../include/DxUi/TextInputServices.h) extend discovery without replacing the existing
clipboard vtables. Review the concrete application adapter if HTML/bitmap/file paste, semantic cut or rendered
copy is wanted. Keep ordinary source copy and original EOL spelling intact. Image display still requires the
application's request/worker/completion service. Remote/data/SVG admission and direct source-ink rendering are
independent opt-ins; pinning this delivery alone does not enable them.

## Additional changes for API 3 consumers

These are the earlier revision-4 contract changes, not a new API bump in this delivery:

- Check the result of `Panel::AddChild` / `TabControl::AddTab` before dereferencing: callback-driven retirement can
  make insertion return null.
- Check `TakeChild` / `TakeTab` before transferring ownership. `Panel::TakeChild` leaves an empty slot;
  `TabControl::TakeChild` / `TakeTab` remove a tab. A focus/capture callback may prevent extraction. Passing an
  empty result to `SetRoot` or `PageHost::SetPage` must not accidentally destroy the still-owning tree.
- Treat Tree/Grid focus separately from selection membership. Wire `OnTreeFocusedItemChanged` and
  `OnGridFocusedRowChanged` where the application needs focus notifications, and review quoted TSV extraction.
- A described menu's accessible name is its label/explicit name, with secondary text in HelpText. Remove any
  application accessible-name duplication of that secondary text.
- Initialize `ContextMenuSessionCallbacks` by member: `flowDirection` precedes `minRootWidthDip`.

Use the [full compatibility contract](../Specs/Build/Build_ToolchainAndConsumption.md#compatibility-and-the-consumer-interface)
and [revision-4 compile fixture](../Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp) when porting models/delegates.
The fixture is library source-contract evidence; application behavior still needs product tests.

## Validate and roll back

Use the consumer's affected-testing explanation to identify the relevant product coverage. Cover x64 Debug,
Release and ASan as required by that product, and its ARM64 build/runtime obligations. Native ARM64 execution
requires an ARM64 host. Retain missing native-platform or physical-input results as **NOT RUN**, rather than
counting a cross-build or a library suite as a product pass.

Exercise the application paths it actually uses: keyboard/Tab/Enter/Escape and focus return; Grid/Tree selection;
exact source, paste/cut and undo/redo; drop/image transport if enabled; metadata diagnostics; DPI/resize;
hidden/show and device replacement; cancellation and closing while callbacks or image completions are pending.
Verify every selected profile's zero-warning build, matching source/artifact identity and actual product execution.
Use the existing harness/desktop lease for foreground assertions and deterministic synthetic data for captures.

Record the old/new full pins, API revision, adapter changes, selected toolchain/CRT/configurations, build/test
receipts, remaining gates and rollback in the **consumer's** reviewed change. Its normal Full/pre-push and release
requirements continue to apply. The [shared qualification ledger](../Specs/Plans/Done/ConsumerUiQualification_2026-10-11.md)
remains open for the separately owned platform, physical-input, adoption and performance obligations.

The delivery's paired performance study remains unqualified because its controls were unstable. The same-toolset
library archive grew 24.42%, and the EmbeddedControls sample grew 77.54%; these are library/sample observations,
not an application size or speed prediction. Measure the consumer's relevant workloads and packaged output.

For rollback, restore the preceding consumer commit containing its lock **and** related adapter/toolchain changes,
run its normal exact-pin restore, and rebuild the full dependent closure against that pin. Retain the failed new-pin
receipts for diagnosis. Do not mix the old archive with new-pin headers or objects.
