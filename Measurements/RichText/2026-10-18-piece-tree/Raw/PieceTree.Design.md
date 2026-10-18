# Next storage implementation draft (not an implemented capability)

Keep SourceSnapshot's one shared pointer and Document's public size/API. Move
State behind a private forward declaration; define immutable nodes in the cpp.
Use fanout 32, minimum 16 entries outside the root. Leaves retain existing
buffer/bufferStart/cumulativeEnd pieces; branches retain child/cumulativeEnd.
Read offsets by binary search at each level. Range traversal starts at its first
intersecting child/piece. No mutable lookup cache, view/device or worker state.

Splice returns zero/one/two nodes of the input height. Copy only two boundary
paths, skip fully removed subtrees and reuse unaffected children. Leaf staging
holds prefix fragments, optional inserted buffer and suffix, coalescing contiguous
buffer slices. Branch staging rebalances underfull adjacent siblings; merging a
branch also normalizes its child level so an underfull boundary chain cannot
survive inside a newly combined parent. Split overfull arrays evenly. Preserve
height inside recursion, then collapse one-child root chains at publication.
This prevents adversarial height growth after repeated splitting/deletion.

Before storage allocation, check resulting selections through a virtual candidate
character accessor (old prefix, inserted text, old suffix), sharing scalar/CRLF
join checks. Stream equal-range comparison; same-content replacement changes
selection with zero new storage/history. Build the candidate and copied capped
history completely before publication, preserving failure atomicity.

Baseline first: fragmented source + capped history retained C++ bytes, no-op
replacement allocations, edit/read trials and fixed independent text/history
oracles. Retain snapshots during deletion/undo/redo/reset; concurrent readers;
random scalar/CRLF splits and hostile append/prepend/middle deletion cycles.
Keep the source/history capacity unchanged. Target a measured retained-byte bound
after the baseline, without hiding any read-time tradeoff or claiming Default
performance acceptance. Whole-document projection/layout/shared scheduling stay
open until their separate implementation and qualification.

Carry forward the common one-character backward UIA path: bulk negative movement
currently counts then revisits its target ordinal. A count of -1 can use the
existing single previous-boundary scan instead of walking almost the prefix
twice. Add the shortcut with a focused large-source single-step observation when
the next active source changes; do not edit the running bulk qualification's
inputs. This preserves the bulk corpus semantics and its at-most-three-pass bound.
