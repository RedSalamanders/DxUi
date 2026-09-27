# Menu count scaling without capture

Source `c1f047b1567607eb331852255e1ed563855e638d`, native x64 Release
[CI 35646866096](https://github.com/RedSalamanders/DxUi/actions/runs/35646866096).
Fixture `dxui-menu-resource-scaling-v1`: 32 rotated/reversed cycles of ten
plain/partially described/fully described menus. All 320 openings complete at
476x322 physical pixels, 96 DPI, without screenshot capture. Suite exit 0, no
skips. The raw suite includes executable SHA256 and its unpaired common benchmark.
`raw/sha256.receipt.txt` hashes the retained samples, summary and suite receipt.

The native Release object sizes are 248 bytes for `MenuFlyoutItem` and 760 bytes
for `Toggle`. These exclude dynamic strings, native text layouts, allocator and
host resources. The caller's vector and strings already exist before sampling.
Thus dividing the earlier whole-process 1.15 MiB delta by twelve does not
measure the size of a menu entry.

Median within-opening private-byte changes, last sixteen cycles:

| Entries | Descriptions | Rendered minus before | Closed minus before |
| --- | --- | --- | --- |
| 12 | 0 | 264,192 | 155,648 |
| 12 | 1 | 2,048 | -462,848 |
| 12 | 2 | 1,394,688 | 1,269,760 |
| 12 | 4 | -1,171,456 | -1,431,552 |
| 12 | 8 | 458,752 | 108,544 |
| 12 | 12 | 985,088 | 137,216 |
| 24 | 0 | 397,312 | -415,744 |
| 24 | 24 | 1,589,248 | 206,848 |
| 48 | 0 | 239,616 | -194,560 |
| 48 | 48 | 3,012,608 | 204,800 |

This strong non-monotonicity and the negative values prevent attributing a
fixed cost or a per-row slope from process-private counters. They do not prove
that descriptions save memory, nor invalidate the separately retained earlier
1.15 MiB observation on its original fixture. Native allocations, release timing
and allocator capacity are not distinguished here. No tradeoff is approved.

Reproduce the summary with `python analyze.py raw/scaling.log`; the script also
retains all32/last16 medians and extrema for private bytes, working set and handle
counts. It checks all ten variants contain 32 complete before/rendered/closed
triples. CI machines differ from local baseline machines; this is characterization,
not matched original-source non-regression or a performance acceptance receipt.

The next separately identified v2 diagnostic measures process heap busy/free
bytes and errors with bounded storage and no per-walk allocation. It does not
change menu production behavior or the gallery and cannot claim native call-stack
attribution. Original v1 evidence remains intact.

**Archive pruning (2026-09-25):** 4 verbose logs and raw receipt copies (208 KB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../README.md).
