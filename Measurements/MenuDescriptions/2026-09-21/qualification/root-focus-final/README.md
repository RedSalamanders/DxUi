# Final root-menu focus correction

All three focused x64 suites (NewControls, Accessibility, WindowHost) pass in Debug, Release and ASan Debug with zero skips. All three ARM64 cross-builds pass.
The ASan probe detected its deliberate isolated failure. Final foreground and paired resource acceptance remain open.
The corrected role guard covers both root menus and submenus; new semantic-row allocation failures reject the unpublished popup.
See [the complete scenario and limitations](../../README.md). Suite receipts retain their original bytes as *-receipt.txt; performance JSON is unpaired here.
