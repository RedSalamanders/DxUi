# M2: owned Unicode block grammar and node identities

This packet qualifies the opt-in bounded block-tree preparation slice in DxUi.
Full inline/GFM-extension grammar, semantic rendering/editing, physical IME/AT,
native ARM64 runtime and frame performance remain separately open.

Qualified compiled benchmark-source fingerprint: `%SOURCE%`.
The frozen source manifest/snapshots bind the additional control/test inputs;
native receipts bind exact executable hashes.

The owned implementation covers nested/lazy quotes and ordered/bullet lists,
empty/tight/loose items, interruption precedence, headings, thematic breaks,
indented/fenced code, seven versioned HTML-block forms and multiline references.
Source, lexical and root partitions preserve exact UTF-16 spelling/EOLs. Content
lines retain stripped prefixes and partial-tab columns. Snapshot ancestry
reconciles IDs under compatible parents, including identical paragraphs and
first/last-scalar deletion. Full bounded block reparse resolves dependencies
before ID reuse. IDs are logical handles, not cache-validity keys.

HTML stays opaque exact source. Expected standard HTML is a test oracle, not a
browser DOM or native display engine. Future native rendering uses an explicit
safe supported subset, with no scripts/events/implicit remote loads. The current
default formatted renderer remains the bounded v0 preview.

## Validation

- 57 ordinary x64 scope/profile obligations pass without capability skips:
  55 matrix executions and two exact whole-suite Release receipts reused after
  verifying frozen source and artifact hashes.
- Six root build profiles pass; ARM64 is compile-only.
- Eight isolated Debug/ASan failure probes pass, retaining complete allocation
  and pending-replacement admission schedules.
- All 1,324 original inputs retain exact source. All 568 pinned CommonMark/GFM
  block-section structural/code-content/ordered-start expectations pass.
- Native/embedded scenarios cover block behaviors, Unicode edge edits, duplicates,
  two views, pending input, stale-result rejection and retirement.
- Deterministic prefix-work fixtures cover 128/512/4096 nested list levels and
  2048 quote levels with 512 lazy lines.
- 324 exact WARP raster pairs pass (108 per x64 profile), with the unchanged
  0.125-DIP geometry tolerance. No catalog/style/default-pixel change means no
  gallery regeneration for this metadata slice.
- Full repository validation (five validators and tooling) and clang-format
  22.1.3 checks pass. Post-closeout prose checks are separately retained.

| Profile | Allocation / admission points | Retained C++ bytes | Replacement peak bytes | Unreleased bytes / idle allocations |
| --- | --- | --- | --- | --- |
%RESOURCES%

Storage figures concern the maximum all-newline trivia/lexical fixture, not all
pathological ASTs or process/SDK/GPU memory. Source stays capped at 65,536 UTF-16
units. Begin materialization, line transitions, reference finalization and
compaction are bounded indivisible work. Step budgets/counters are not hard
latency guarantees or complete CPU counters.

## Failures and provenance

Raw preserves original attestation failures and structural mismatches.
Interrupted1 was deliberately stopped for prefix-work review. Failed2 records
the Debug point-8 noexcept termination and exact privately retained failing
EXE/PDB/library. Diagnostic1 captures bounded stack addresses resolved offline
against its PDB: MSVC's noexcept default vector constructor allocated a Debug
iterator proxy. Diagnostic2 passes after explicit fallible zero-size/copy
construction. Incremental diagnostic builds are not root qualification passes.
Earlier experiments do not all retain complete executable images; exact artifact
capture is stated only where recorded.

All available raw text/PNG receipts and original reference data/notices are
preserved. Long raw paths have inert aliases in summary.receipt.txt. Archived
source/policy files do not become active instructions or build inputs. Compiled
EXE/DLL/PDB/LIB files stay private in the owning .build report directory; their
identities, rather than contents, are in private-artifacts.receipt.txt.

Canonical benchmark observations are copied byte for byte as unpaired diagnostics.
There is no quiet-host, causal speedup or frame non-regression claim. The original
performance HOLD, judges and thresholds remain unchanged.

## Offline verification

```powershell
./Verify.ps1 -Packet $PWD
```

The manifest seals all payloads, including this README and verification code.
Pass `-ExpectedSeal` using manifest.sha256 for an external anchor. Verification
checks native/canonical/source/artifact bindings, raster equality, allocation
traces/storage predicates, profile/tooling completeness and standard data/license
pins. It verifies receipts; it does not execute tests or use external files,
network, debuggers or development tools.

See [the plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md),
[the API](../../../include/DxUi/MarkdownSyntax.h),
[the guide](../../../docs/rich-text.md) and
[the original inputs/notices](../../../ThirdParty/Markdown/README.md).
