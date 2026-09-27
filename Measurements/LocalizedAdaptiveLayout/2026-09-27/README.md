# Combined Grid and described-menu qualification

The tested source is `36b2f4b49d42248790fa1a4ad6b6409ac56207dc`, merging the
independently reviewed Grid and menu components. Consumer adoption is explicit and separate.

- [Native CI packet](combined-ci/README.md): run
  [36332676309](https://github.com/RedSalamanders/DxUi/actions/runs/36332676309)
  succeeds on x64 and native ARM64 Debug, Release and ASan Debug. Each profile retains
  its nineteen core suites, native architecture, source/binary identities and relocated
  consumer result. Grid, Rendering, Embedded and Accessibility have zero skips. Nine
  Menu interactive-desktop capability skips per ARM64 profile remain unexecuted.
  The x64 Release resource probes and both intentional ASan detection probes also pass.
- [Matched performance packet](combined-pair/README.txt) retains all eight serial runs:
  A1/B1/B2/A2 for Default, then A1/B1/B2/A2 for MultilineGridRetention. A is `c52a8f5`
  (production identical to the old `78b3de3` pin), B is `36b2f4b`. The same eight
  benchmark/tool inputs are used on both sides. Source, executable and archive identities
  stay fixed within each variant. [Analysis](combined-pair/analysis/curated.md) keeps all
  twelve `advice-required` comparisons, including same-variant controls. It is not a
  clean comparator pass or a causal attribution.

Root checked the cheaper-model curation against the original receipts: all 192 CI payloads,
196 compact CI checksums, 95 original paired files and 97 paired packet checksums match.
The CI path map is reversible; its original manifest is retained. Paired archive paths are
short enough for relocated Windows consumers. No failed or noisy result was replaced.

Deterministic library allocation/surface budgets remain unchanged, hidden preparation and
composition are zero, and the retained 32-step Grid exercise satisfies its cleanup bounds.
Observed combined retention private memory remains within the previously accepted roughly
6–7 MiB Grid cost; individual timing observations and repeat variability remain visible.
The developer's September 27 acceptance covers only the recorded historical timing costs,
not blanket acceptance of additional regressions. Product integration and standalone
WindowHost surface accounting remain separate checks.

Root's follow-up checks the retained scroll samples against the original V11 attribution:
the four-cross-pair median private-byte increase is 5.49 MiB and working set is 2.38 MiB,
below V11's recorded 6.76–7.27 MiB and 2.84 MiB respectively. The separate dirty-round
working-set high-water field reaches about 0.93 MiB above the older observed maximum in
B2; B1 is essentially within it. Different retention/heap-walk fixtures prevent treating
that historical comparison as a new paired peak budget or proving its cause.
Default dirty CPU-composition P95 cross-pair deltas are +0.0107, +0.0018, +0.0094 and
+0.0005 ms. Same-source A changes +0.0013 ms and B changes -0.0089 ms. These observations
do not confirm a stable additional timing cost; keep them available with the product pair
instead of declaring either zero overhead or a new accepted budget.

`gallery.ps1 -PublishDocs` regenerated all five themes from the combined Release build.
Root and a second reviewer inspected the original sheets for clipping and missing content.
The published generation receipt preserves the actual source and dirty-worktree flag;
the reviewed changes during generation are documentation/gallery outputs, not a different
production implementation. This documentation-only publication leaves compiled inputs unchanged.
