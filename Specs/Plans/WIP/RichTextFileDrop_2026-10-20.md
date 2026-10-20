# RichTextEditor file drag-and-drop

Status: SPECIFICATION READY; implementation and runtime tests NOT STARTED.
Owner decision (2026-10-20): prepare this spec and test plan; submit the existing
RichTextEditor work as a PR. Do not implement drag-and-drop in that PR.

This is an independently sequenced addition to
[the rich-text plan](RichTextEditor_2026-10-17.md), following qualified M5g.1b
file-reference paste and M5g.2 native image display. It does not close the parent
plan, D4c performance HOLD, M5g.3 resources, native ARM64, physical IME/AT or
consumer adoption. Proposed names below are design guidance, not available APIs.

## Checklist and execution order

- [x] D0: Specify source behavior, ownership, limits, cancellation and tests;
  link the pending capability from domain specs, usage and design-system docs.
- [ ] D1: Extract the shared file-reference preparation policy without changing
  paste behavior. Add source-caret-aware preparation and atomic insertion.
- [ ] D2: Implement bounded Unicode OLE data extraction and a revocable,
  application-owned drop-target adapter; test COM/media lifetime and failures.
- [ ] D3: Route native drops through prepared ControlHost hit testing and
  implement one guarded edit at the resolved source caret.
- [ ] D4: Route embedded drops through explicit application coordinate conversion,
  prepared geometry and the same edit policy. Preserve the borrowed-host model.
- [ ] D5: Wire the sample, add the native/embedded scenarios below and collect
  deterministic harness captures plus resource/performance evidence.
- [ ] D6: Complete applicable builds, tests, validation, documentation and PR
  checks. Record pending physical/platform gates honestly before moving this
  independently qualified implementation record to Done.

## Required user behavior

An external file list dropped into an available RichTextEditor inserts references
at the caret obtained from the drop point. Existing selected text is not replaced:
the drop is a collapsed insertion, even when a backward selection already exists.
The whole ordered list creates exactly one source edit, one change notification
and one undo entry. Undo restores the original source and directional selection;
redo restores the insertion and its resulting caret. Preview never changes either.

For a document located at `C:\notes\note.md`, dropping these two paths in order:

```text
C:\notes\images\photo one.png
C:\notes\attachments\report.pdf
```

produces this source in an ordinary prepared Rich-mode context:

```markdown
![photo one](images/photo%20one.png)

[report\.pdf](attachments/report.pdf)
```

Recognized image suffixes use the existing image-reference policy, filename-stem
description and application-serviced image request pipeline. Other file suffixes,
extensionless names and dotfiles use full-filename text links. Descriptions and
destinations use the exact escaping and lexical path policy of paste. Supported
decoders initially remain PNG/JPEG/BMP/GIF/TIFF, first static frame; a recognized
but undecodable image keeps its editable alternative. Classification alone does
not prove that an entry exists, is a regular file or can be decoded.

Paths are relative to the explicit document's parent, never process CWD. Unsaved
documents and different drive/UNC roots use absolute file URIs. Preserve directory,
share and filename case, Unicode and original list order. Use the first source
EOL spelling, or CRLF for a document without an EOL, matching paste. Separate
Markdown references with blank paragraphs without synthesizing unrelated wrappers.

Source mode, active scoped source editing, code, HTML, metadata, unresolved literal
footnotes and partial atomic inline contexts retain the same quoted literal-list
fallback as paste. Evaluate the context at the proposed drop caret, not the old
selection. A valid but unrepresentable path makes the complete list literal; an
invalid Unicode list or capacity failure rejects the entire drop. Never insert a
representable prefix or silently discard an unsupported tail. Stale/unprepared
syntax or geometry rejects the drop until owner-driven preparation completes;
the drag loop must not parse, shape or force a synchronous repaint.

The operation copies references into source. It does not copy, move, delete,
download, activate or open the referenced files. Links retain their inert underlined
presentation. The existing caller-owned loader decides whether to service local
image requests and whether UNC access is permitted. No file/network I/O, image
decode or bitmap upload happens in drag callbacks or paint/composition.

## Initial supported transport and feedback

Accept external `IDataObject` file lists only through `CF_HDROP`, `DVASPECT_CONTENT`,
index -1 and `TYMED_HGLOBAL`, with wide `DROPFILES::fWide` data. Reuse the bounded
decoder used by paste. Preserve the limits of 16 entries, 32767 UTF-16 units per
path and 65536 total path units, plus the existing document/edit capacity. Reject
ANSI, malformed UTF-16, embedded NULs, missing double terminators, invalid offsets,
wrong media and over-limit input without publishing partial content. Bound the
global memory admitted before locking/decoding (initial transport ceiling 256 KiB,
including the header/padding); reject a larger advertised allocation without a
copy. Preserve the existing lexical fallback for otherwise valid path spellings.

Only `DROPEFFECT_COPY` may be returned, and only if the source allows it. No
modifier or Ctrl permits reference insertion. Shift, Alt, Ctrl+Shift and right-button
drags are refused rather than acknowledging a move, shell link or context menu.
MOVE-only and LINK-only sources are refused. Returning COPY describes the source
transaction; it never authorizes copying file contents. The adapter returns NONE
until it can accept the current target, and reports COPY from Drop only after the
edit committed. A refused/failed/cancelled operation reports NONE.

DragEnter queries the offered format without fetching file data. DragOver uses
only cached format eligibility plus current geometry/availability. Drop obtains
and validates data once, then releases its storage medium on every success or
failure path using the medium's ownership contract, including `pUnkForRelease`.
Do not open the clipboard or reuse clipboard ownership calls for this transport.
Do not retain borrowed HGLOBAL pointers, path spans or data-object pointers after
the callback. Initial support rejects virtual files (`FILEDESCRIPTOR`/`FILECONTENTS`),
bitmap pixels, plain text/URLs, HTML, internal selection drags and drag-source
move/reordering. These can be separately specified later.

Preview uses the ordinary OLE allowed/refused cursor, with no document mutation,
focus transfer, selection movement, capture, hover timer or auto-scroll. Initially
only currently visible prepared positions are droppable; edge auto-scroll is out
of scope. A later visual insertion marker must use prepared geometry, invalidate
only changed bounds and clear on leave/cancel; it is not a D1-D4 dependency.

## Ownership and integration

Use one application-owned OLE adapter per receiving application HWND. The UI
thread initializes OLE with `OleInitialize`, pumps messages and balances successful
initialization at application shutdown. Register explicitly after the host/view is
ready; revoke before HWND destruction, adapter destruction or module unload. An
existing registration is not replaced. Failed registration leaves paste/editor
state usable and exposes a failure to the application. The sample currently calls
`CoInitializeEx`; its future drop-enabled startup must use the required OLE startup
instead, with tested balancing. Controls do not silently initialize OLE or register
an application's HWND. Never retain an editor/control raw pointer across OLE calls.

The application may already own an IDropTarget for other regions. Provide an
explicit router/adapter seam; consumers can forward the relevant callbacks without
a second registration. A revoked/detached adapter retained by OLE or another client
must become inert and return NONE without touching its old host. Reference counting
and callbacks must not extend borrowed host/window/module lifetimes implicitly.
Foreign-thread use fails without mutation; exceptions cannot cross COM callbacks.

Native mode uses ControlHost's existing tree ownership, availability, viewport and
text geometry. Convert screen pixels to host DIPs, hit-test the topmost eligible
control and use its existing source/display caret map. Do not send synthetic mouse
clicks, use diagnostic hit-test APIs in production, create an Edit HWND or mutate
selection to obtain a caret. Embedded mode receives an application-converted
view-local DIP point and validates prepared view extent/DPI/interaction generation.
Support an offset view, non-96 DPI, multiple views and view clipping on one HWND.
The application retains HWND routing, OLE registration and text-service ownership;
EmbeddedHost gains no HWND, device, swap chain, worker, timer or independent loop.

Use descriptive internal names such as `RichTextFileDropSession`,
`PrepareFileReferenceInsertion` and `CommitFileReferenceInsertion` if they fit the
existing contracts. Keep one shared policy instead of adding a second path parser,
Markdown escaper or image loader. Existing clipboard interfaces and public vtables
must retain their contracts. A common owned file-reference input may be extracted
from TextClipboardContent; keep an explicit compatibility path for current callers.
Document source-index units, borrowed lifetimes and reentrancy guards in comments.

## Guarded preparation and commit

1. At each preview, verify attachment, live root/control ownership, visible/enabled
   ancestors, editable policy, no capture/modal conflict and no active composition.
   Hit-test only coherent, currently prepared geometry within the editor viewport.
   Reject object interiors without an unambiguous editable source caret. Existing
   source caret sites must handle empty, trailing, container-only and padded-cell
   positions without inventing raw text or splitting a scalar, grapheme or CRLF.
2. At Drop, clear the effect to NONE before any external call. Snapshot an owner
   liveness token, attachment/root/control identity, interaction/layout generation,
   document and document-location revision, exact old directional selection,
   presentation/source-scope state and proposed source caret. Never carry a drag-
   enter caret or source reference forward as a commit capability.
3. Fetch/decode into bounded owned data. IDataObject callbacks may pump messages,
   edit source, move focus, change the base, reparent/destroy controls or detach a
   host. Recheck all identities after each external call and before dereferencing
   the target. A nested Drop is refused while a commit is in progress; every path
   clears that guard. Owner destruction itself during its member callback remains
   outside existing whole-host lifetime support: the adapter must use an external
   liveness state and stop before entering a retired host.
4. Prepare one source insertion for the resolved caret without mutating the
   document or temporarily calling SetSelection. Validate both final source size
   and the applicable embedded 65536-unit edit/snapshot ceiling. Allocation,
   validation and stale-state failure preserve source, revision, selection, history
   and output. Revalidate policy/composition after any application callback.
5. Apply once through the normal document transaction, preserving the old
   selection in undo. Synchronize the already-active text service and publish the
   usual accessibility changes; do not manufacture extra clipboard/key events.
   Refresh layout asynchronously. A notification exception after a committed edit
   must not turn it into a failed drop or cause a duplicate retry. Report COPY
   according to the committed result, even if the callback retires the target.

No focus is taken during preview. The initial Drop path also preserves OS/logical
focus; inserting source must not acquire the desktop. Existing focused services
receive fresh revision/selection state. A drag while that editor is composing is
refused without committing or cancelling the composition. Tests must verify a
drop into an unfocused editor as well as an already-focused editor.

## Native and embedded test plan

These are required future tests, NOT tests added or run by this specification.
Keep shared vectors in `Tests/Support/Scope.Tests.RichFileDrop.h`, named native
cases in `Tests/Controls/Scope.Tests.RichFileDrop.cpp` (and a matching header if
needed), and retained embedded cases in
`Tests/Embedded/Scope.Tests.RichFileDrop.h`. Register every new native file in
`Tests/native-test-files.json` and every native case with `DXUI_RUN_TEST`.
Use real HGLOBAL/media ownership with a private IDataObject; never access or
overwrite the desktop clipboard and never invoke a real drag loop in ordinary CI.

| Required behavior | Native case | Embedded case / shared proof |
| --- | --- | --- |
| PNG plus PDF, same base, exact source/order/roles, only image requests decode | `TestRichFileDropNativeMixedReferences` | `TestRichFileDropEmbeddedMixedReferences` |
| Other files, extensionless/dotfiles, Unicode/punctuation, relative/file-URI roots | `TestRichFileDropNativePathPolicy` | `TestRichFileDropEmbeddedPathPolicy`; reuse independent paste goldens |
| One edit/notice, exact undo/redo and restoration of prior backward selection | `TestRichFileDropNativeHistory` | `TestRichFileDropEmbeddedHistory` |
| Point chooses insertion rather than replacing an unrelated selected range | `TestRichFileDropNativeCaretInsertion` | `TestRichFileDropEmbeddedCaretInsertion` |
| Code/raw/HTML/metadata/source scopes; mixed list all-literal fallback | `TestRichFileDropNativeLiteralContexts` | `TestRichFileDropEmbeddedLiteralContexts` |
| Empty/trailing/container-only lines, tables/padded cells, graphemes/CRLF | `TestRichFileDropNativeCaretSites` | `TestRichFileDropEmbeddedCaretSites` |
| Disabled/read-only/hidden ancestors, clipped/outside/occluded targets | `TestRichFileDropNativeAvailability` | `TestRichFileDropEmbeddedAvailability` |
| Active composition unchanged; focused/unfocused drop does not steal focus | `TestRichFileDropNativeCompositionAndFocus` | `TestRichFileDropEmbeddedCompositionAndFocus`; application text-service bridge |
| Native screen pixels and non-96 DPI; offset/scaled/two embedded views | `TestRichFileDropNativeCoordinates` | `TestRichFileDropEmbeddedCoordinates` |
| Resize, scroll, layout/source-scope change, device replacement, unprepared view | `TestRichFileDropNativeGeometryStaleness` | `TestRichFileDropEmbeddedGeometryStaleness` |
| Reentrant GetData changes source/base/selection/focus/root or destroys target | `TestRichFileDropNativeReentrancy` | `TestRichFileDropEmbeddedReentrancy`; strong no-edit checks |
| Nested Drop, detach/revoke, retained COM references and attachment reuse | `TestRichFileDropNativeLifetime` | `TestRichFileDropEmbeddedLifetime` |
| COPY mask, modifiers, refused MOVE/LINK, leave/cancel/repeated callbacks | `TestRichFileDropNativeEffects` | `TestRichFileDropEmbeddedEffects`; shared state-machine vectors |
| Wide format/media/terminators/offsets/limits; reject virtual/ANSI/bitmap/text | `TestRichFileDropNativeDataObject` | `TestRichFileDropEmbeddedDataObject`; private COM fixture |
| STGMEDIUM release including pUnkForRelease and every GetData failure path | `TestRichFileDropNativeMediumOwnership` | shared ownership counters exercised through embedded adapter |
| Before-commit allocation/size failure preserves state and permits exact retry | `TestRichFileDropNativeFailures` | `TestRichFileDropEmbeddedFailures`; observed every-allocation sweep |
| Notification throws/retires target after commit; success effect, no duplicate | `TestRichFileDropNativeCommittedNotification` | `TestRichFileDropEmbeddedCommittedNotification` |
| Paste transport and existing clients remain compatible | existing named paste suites | existing retained/TSF mixed-paste fixtures |
| Source and alternative names/ranges/events seen by an actual UIA client | `TestRichFileDropNativeAccessibility` | embedded application-window UIA bridge/client |
| Image request reaches caller worker; one real local PNG and one text link | `TestRichFileDropNativeImageCompletion` | `TestRichFileDropEmbeddedImageCompletion`; WARP pixels and source/history |

Allocation tests cover decoder, owned payload, insertion planning and transaction
staging separately. Preserve the exact failed state and original receipts; a
successful retry does not erase a prior failure. Transport tests must show
QueryGetData-only preview, no clipboard ownership, no path I/O and balanced medium
release. Resource tests must show no new HWND/device/thread/timer, bounded session
state and zero active session after leave, hide, detach, failure and shutdown.

Real Explorer-to-sample drag, Shift/Ctrl feedback, multi-file order, cancellation,
OS focus and physical IME/AT remain separate explicitly scheduled checks. Ordinary
tests use nonactivating harnesses. Foreground-taking suites require the existing
interactive lease and the person's agreement to the time.

## Qualification and documentation gates

Before implementation capture the exact baseline source/dependency/artifact and
fixture identities. Measure repeated Enter/Over/Leave and drop preparation on the
same fixture before/after; include queue/session memory and idle behavior. DragOver
must not scale with file bytes/count by repeatedly fetching or parsing payloads.
Record dispatch counts, allocations and a controlled latency distribution; report
unpaired complex-UI numbers as diagnostics, not D4c acceptance. Never silently
rebaseline a failure or use this slice to clear the existing performance HOLD.

Use `Test-Changes.ps1 -Explain` and affected iteration. Closeout requires the
applicable x64 Debug/Release/ASan Debug ordinary suites, a fresh intentional ASan
fault probe, all three ARM64 builds, `validate.ps1`, pinned `format.ps1 -Check`,
updated domain/usage/design-system docs and actual harness captures when visuals
change. Native ARM64 execution, physical input and consumers are independent
gates. Account for exact matching CI coverage under PrePush; pending checks are
never passes. Preserve original receipts and source/artifact identities.

The current spec-only change alters no rendering, example state, source or test
binary: published gallery pixels remain unchanged. No native test/build rerun is
claimed for this document. The existing file-reference paste qualification remains
source-specific [evidence](../../../Measurements/RichText/2026-10-20-file-reference-paste/README.md),
not drag-and-drop qualification. The draft RichTextEditor PR must name the parent
plan's remaining gates and any unresolved merge conflicts with current main.

## References and implementation anchors

- [Paste/image behavior](../../../docs/rich-text.md) and
  [input contract](../../UI/UI_InputAndAccessibility.md).
- `src/Controls/DxUi.RichTextEditor.cpp`: `TryPrepareClipboardPaste`,
  `CanPasteMarkdownFileReferences`, source hit testing and document edit plumbing.
- `src/Controls/DxUi.RichImagePaths.h`: shared lexical path/Markdown policy;
  `src/Controls/TextClipboard.h`: bounded wide file-list decoder declaration.
- `include/DxUi/Embedded.h` and `src/Rendering/Embedded.cpp`: prepared input
  coherence, text geometry/revision checks and application-owned input bridge.
- `Samples/EmbeddedControls/Main.cpp`, `EmbeddedTextClient.h` and
  `RichImageLoader.h`: HWND routing, application text service and image worker.
- [Microsoft OLE drag-and-drop overview](https://learn.microsoft.com/en-us/windows/win32/shell/dragdrop),
  [DragEnter effect negotiation](https://learn.microsoft.com/en-us/windows/win32/api/oleidl/nf-oleidl-idroptarget-dragenter),
  [Shell CF_HDROP transport](https://learn.microsoft.com/en-us/windows/win32/shell/clipboard),
  [ReleaseStgMedium ownership](https://learn.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-releasestgmedium).
