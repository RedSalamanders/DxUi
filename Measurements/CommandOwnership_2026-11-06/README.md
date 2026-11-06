# Markdown command ownership qualification

Implementation `c59094f5b9b49c0081164cca95473cac78e57001` (API 5) fixes task-paragraph
ownership, shared parent-marker movement, paragraph exchange and trailing footnote escapes.
The lasting behavior is specified in [UI controls](../../Specs/UI/UI_ControlsAndLayout.md)
and [rich-text usage](../../docs/rich-text.md).

[Qualification](qualification.json) binds the original 681 source files, six zero-warning
builds, all 66 ordinary noninteractive x64 suite executions, actual ASan detection, suite
binaries and every original native/run/failure-image archive member. ARM64 entries are
cross-builds and do not establish native execution.

[Counterfactuals](counterfactual.json) retain the same test-object baseline/candidate
results. The qualified retained baseline `1352cc7f` fails the task, sibling, paragraph
and footnote expectations; the candidate passes. Valid empty display math passes both
and is a characterization, not a newly fixed bug. This patch-object comparison is
focused functional evidence, separate from the complete candidate builds.

The corrected tooling deadline fixture uses a deliberately stalled child and a ten-second
fixture deadline; it verifies the timeout, execution of the later entry and survival of
an unrelated owned process. The production tooling deadline is unchanged. The original
one-second fixture failure under native-build load remains retained separately.

Compiler-free validation passes. Earlier zero-profile dependency/source-count/process
admission failures remain distinct from the accepted matrix. Physical input, native
ARM64 execution, formal paired performance and consumer API-5 adoption remain open
under their current owners.
