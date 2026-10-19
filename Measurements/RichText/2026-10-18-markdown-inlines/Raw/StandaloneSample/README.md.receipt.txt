# RichEditRebuild

A separate project for an owned Unicode C++ rich-text control and Markdown editor.
Microsoft Rich Edit DLLs are behavioral/decompilation references, not the production engine.
The core is supplied by one exact clean DxUi.lib pin through the normal consumer imports.

Current state: a working bounded Unicode rich-text prototype with source/formatted
editing, DirectWrite/Direct2D rendering, transactional history, worker parsing,
UTF-8 persistence, automated tests and real capture. It owns its editing engine.
The reusable core is canonical in DxUi; full Markdown/extensions and the retained
DxUi control/input/accessibility integration remain pending.

```powershell
.\restore-dxui.ps1 -SourceRepository 'Z:\src\DxUi'
.\build.ps1 -Configuration Release -Test
& .\build-dxui-x64-Release\Release\RichEditRebuild.exe
```

See [prototype usage, behavior and limits](docs/Prototype.md),
[the editable feature laboratory](samples/FeatureShowcase.md),
[the actual renderer capture](validation/d2-prototype-Release.png), and
[DxUi migration and qualification](Specs/DxUiIntegration.md).

The accepted document scope is CommonMark + GFM with math, Mermaid, footnotes and
front matter, including formatted editing and a source view. There is no single
universal "all Markdown" specification; the exact profiles and acceptance gates
are in [Specs/RichMarkdownControl.md](Specs/RichMarkdownControl.md).

- [Installed DLL comparison](Specs/RichEditDllComparison.md)
- [Initial decompilation module map](Specs/RichEditModuleMap.md)
- [Document/control specification](Specs/RichMarkdownControl.md)
- [Prototype behavior and qualification](docs/Prototype.md)
- [DxUi migration plan](Specs/DxUiIntegration.md)
- [Implementation checklist](WIP/RichEditReconstruction.md)
- [Exact PE versions/imports/exports/PDB identities](reference/dll-inventory.json)

## Reference probe

```powershell
.\build.ps1 -Configuration Release -Test `
  -OfficeReferenceDll 'C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX64/Microsoft Shared/Office16/RICHED20.DLL'
.\build-dxui-x64-Release\Release\inspect_rich_edit.exe C:\Windows\System32\msftedit.dll
```

Use an absolute path and a separate process for each engine. Office and system
DLLs register overlapping class names; modern variants can also have companion
modules with overlapping names. Probe output lives in `validation/*-probe.txt`.
The original pre-migration probe passed 3/3 reference tests; current linked-core
qualification is recorded separately. A successful probe is not
rich-text, Markdown or accessibility conformance. The Office cache option is
optional on machines without that installed reference. Private Notepad direct
loading failed with access denied and is recorded separately.

Open `reference/ghidra/RichEditReference.gpr` in Ghidra for the separate reference
project. `tools/create-reference.json` records the initial actual MCP script;
`tools/CreateRichEditReference.java` includes a later analysis-wait correction.
Do not recreate over an existing project. `tools/verify-reference.json` and its
MCP receipt verify saved identities and decompilation read-only. Diagnostics and
verification limits are documented in the comparison report.

## Reconstruction rules

- Own document storage, selection, transactions, undo/redo, layout and rendering.
- Unicode text APIs and UTF-16 control offsets; UTF-8 Markdown file storage.
- No hidden Edit/RichEdit child, superclass, or forwarding production engine.
- Public SDK DirectWrite/Direct2D/TSF/UIA are permitted platform services.
- Source names are descriptive; comments identify DLL/version/RVA and uncertain
  interpretations. Use SDK declarations rather than redefining Windows constants.
- Reference pseudocode/PDBs do not restore original source/comments/private ABIs.
- Existing `Z:\src\notepad\rebuild` is unchanged by this project.
