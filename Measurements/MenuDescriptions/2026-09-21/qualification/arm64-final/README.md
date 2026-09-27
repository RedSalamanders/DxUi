# ARM64 compile qualification

Debug, Release and ASan Debug builds pass, including a final incremental rebuild of
the final cursor and retention probe sources. Earlier full build logs remain here.
These are cross-builds on x64; no native ARM64 runtime claim is made.
See the [overall findings and remaining gates](../../README.md).

**Archive pruning (2026-09-25):** 7 verbose logs and raw receipt copies (22 KB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../../README.md).
