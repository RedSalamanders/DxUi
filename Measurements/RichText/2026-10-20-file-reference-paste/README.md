# Unicode file-reference paste

Prepared Rich-mode paste now handles a quoted or unquoted absolute Unicode file
path and bounded wide Explorer file lists. Recognized image suffixes produce
displayed Markdown images; other files produce named Markdown links retaining
the complete filename. Mixed lists preserve order and blank source paragraphs.
Destinations are relative to the explicit document location, with file-URI
fallback for unsaved or cross-root cases. Source/code contexts retain literal
paths. One paste is one undoable source transaction; files are never opened,
copied or moved by this transformation. Drag-and-drop remains outside this slice.

## Verified results

All six canonical builds pass: x64 Debug 4:48, Release 5:19 and ASan Debug 4:43;
ARM64 Debug 4:51, Release 5:04 and ASan Debug 3:58. ARM64 evidence is compilation,
not native runtime execution. All 19 ordinary suites pass in each x64 profile:
57 scope runs with zero capability skips. The isolated deliberate ASan
use-after-free is detected, confirming active instrumentation.

The [qualification identities](qualification.json), [runtime output](runs.receipt.txt),
[profile driver](profile-harness.ps1.receipt.txt) and
[focused embedded output](embedded-directed.receipt.txt) bind the candidate source,
resolved build profiles, executables and successful suite receipts. Publication
checks fresh receipt timestamps, owned executable paths and hashes, six matching
source inventories, and unchanged before/after measurement identities. Complete
raw packets remain under `.build/reports/rich-file-reference-paste-20261020`.

Shared native/embedded cases check Unicode names, punctuation/URI escaping,
relative paths, parent traversal, case-sensitive components, UNC roots,
unsaved/cross-drive fallback, files without suffixes and dotfiles. Independent
semantic parsing verifies one image and two named links, full displayed filenames
and destinations. Only the image requests decoding. Exact source, backward
selection, one-step undo/redo, raw/code/source contexts, malformed lists,
capacity and read-only/disabled behavior are covered.

Native Ctrl+V exercises the real wide-file codec/transport with private HGLOBAL
ownership. The sample's embedded TSF adapter covers mixed and text-path paste,
one commit, and stale source/selection/location, read-only and composition guards.
Debug fault injection sweeps all 56 observed mixed-transform allocations with
unchanged output/source/history and exact retry. Pure image/file construction,
wide transport and URI resolution sweep another 94 observed allocation points.
The same functional cases run in ordinary Release and ASan Embedded suites.

No native failure required a repair. An initial changelog-shape validation failure
was corrected before compilation. A publication helper initially reparsed JSON
DateTime objects through a culture-dependent string; its corrected typed UTC
handling preserves the original failed publication log. Final schema validation
also required explicit fixture hashes, now included alongside the source
inventories. Native source/results were unchanged. Full repository
validation/tooling and pinned formatting pass.

The change reuses existing image/link rendering and changes no default gallery
source, geometry, typography or pixels; existing captures need no regeneration.
Ordinary benchmark output is **unpaired diagnostic evidence**. The existing D4c
performance HOLD, M5g.3 final image-resource acceptance, native ARM64 runtime,
physical IME/AT, agreed foreground suites and consumer adoption remain open.
