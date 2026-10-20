# Unicode image-path paste

RichTextEditor prepares displayed-image Markdown syntax from absolute Unicode
text paths and wide Explorer file lists. Explicit document-file metadata supplies
the relative base; unsaved/cross-root destinations use file URIs. One native or
application-owned embedded TSF paste records one exact undoable source edit.
Source/code/opaque contexts retain literal text. Existing Unicode text controls
retain their old transport. Decoded image painting is a separate pending step.

## Focused evidence

The canonical x64 Debug build passes in 4:17. Five directed tests, all 31 observed
path/codec allocation failures, and the complete EditorControls, Accessibility,
Embedded and Foundation suites pass. EditorControls takes 47.641 s and
Accessibility 119.033 s; Embedded reports 443,742 checks. Seven additional ordinary
host suites pass through test.ps1: TextField, MultilineText, ReadOnly, NewControls,
Control, Grid and WindowHost, with zero capability skips.

Tests exercise independent relative/parent/sibling/UNC/cross-volume/unsaved path
vectors, UTF-8 URI and Markdown escaping, supplementary Unicode, invalid/ambiguous
paths, encoded capacity, real HGLOBAL wide-file transport with one open/close,
malformed/ANSI/unterminated buffers, multi-file policy, opaque contexts, exact
undo/redo and backward selections. The actual sample EmbeddedTextClient and
TextInputServices reject stale source/selection/document-location callbacks and
read-only/composing paste. Pure structural command/deletion planners reject
presentation-only source-region syntax before staging.

[Qualification identities](qualification.json) bind the changed native inputs,
base/patch, verified build state and copied binaries. [Run receipts](runs.receipt.txt)
and the [driver](harness.ps1.receipt.txt) preserve scope and provenance. Full raw
packets and original include-order/attestation failures remain locally under
`.build/reports/rich-image-path-m5g1-20261020`. The final before/after v2 input and
artifact identity check passes. All tested native hashes were rechecked before
this metadata closeout. The 135-file native inventory, full validators/tooling
and pinned formatter checks pass.

The 1,000 warm composites take 7,824 us with zero tracked C++ allocations. The
ordinary runner also records an unpaired complex-UI report. These are diagnostics,
**not performance acceptance**; D4c remains HOLD. Final Release/ASan/ARM64,
foreground/physical IME/AT, resource and consumer gates remain open.

Gallery review: this component changes clipboard/source admission and metadata,
without changing image/paragraph painting. Existing rich-text gallery tiles and
the prior reviewed source-editing captures remain the current presentation.
There is no new decoded-image screenshot or raster-support claim in this packet.
