# Semantic resource and Unicode verification

The [retained comparison](counterfactual.json) binds six independent baseline failures
and candidate passes to original source, shared fixture object, EXE/PDB and raw log hashes.
Every archive member is independently verified. The candidate checkpoint is `54bea833`;
three current translation units replace their counterparts in the retained `1352cc7f` SDK.
This focused proof is separate from complete current-source qualification.

The cases cover supplementary word starts and native/UIA boundaries, inherited definition/
callout/footer markers, independent nested lists, authored RTL direction, math reuse across
snapshots and image upload reuse across completion/frame replacement. Only the new or changed
image uploads; existing math stays prepared. The initial overbroad prefix suppression was
found by the nested-list case, and its source/images are retained before correction.

The first partial candidate retains its logs but its EXE/PDB were replaced before archival;
it is not accepted qualification. Original baseline and final accepted images are independently
retained. Native platform, consumer, physical input and performance-policy acceptance remain separate.
