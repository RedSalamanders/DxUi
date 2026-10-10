# Design-system publication record

The earlier proposal to upload the DxUi design system to a private external artifact is superseded. Authentication
and a prepared upload bundle did not establish publication. No external upload or returned blob ID is required for
DxUi's design-system acceptance.

The canonical design system is the versioned content under [`Specs/DesignSystem`](../../../Specs/DesignSystem/README.md).
The six authoritative rendered captures are under [`docs/gallery`](../../../docs/gallery/README.md). They were
generated from a native x64 Release build through the deterministic application harness and reviewed as actual
captures. Static HTML component previews remain explanatory; the gallery captures take precedence when they differ.

For future visual changes, update the affected tokens, guidance and previews in `Specs/DesignSystem`; regenerate
changed gallery captures with `gallery.ps1 -PublishDocs`; review the output; and commit the files together. The
gallery's `generation.json` records image hashes and source provenance. Repository validation checks the local
design-system coverage and gallery image hashes.
