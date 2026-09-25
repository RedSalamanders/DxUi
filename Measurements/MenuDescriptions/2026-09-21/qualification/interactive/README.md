# Foreground interaction evidence

The Release Menu suite passes with zero skips and verifies original child focus,
foreground and physical cursor restoration in the V3 run. A subsequent Release
NativeTextInput run passes functionally, but its cursor check fails; the outer run
stops rather than overwriting unexpected cursor movement. It is not a fully qualified
restoration receipt. Remaining Debug/ASan directed runs are open.

The fixture now saves the physical original and actual aligned cursor positions,
preserves its established popup-context placement, and restores only if no outside
movement occurred. The external wrapper verifies physical coordinates without forcing
the original position. It reuses the existing warning/desktop lease/owned-child lifetime
guard; it is orchestration evidence, not a consumer dependency in the library targets.

Retained failures: the first shortened direct Menu run passed its tests but failed the
cursor check; an attempted logical-to-physical conversion doubled the requested point
and failed hover. V3 removes that extra conversion. The earlier wrapper/powershell run
stopped at an unmatched performance comparison before Menu execution and also failed
restoration; its raw log remains in the worktree and external evidence location.

No passing native ARM64, product mixed-DPI or human screen-reader claim follows from
these runs. See the [overall findings](../../README.md) and the WIP plan.

**Archive pruning (2026-09-25):** 4 verbose logs and raw receipt copies (57 KB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../../README.md).
