# Explicit native WARP policy: evidence summary

Runtime source `dd619ef0c5855307eb57758072658f2e3b777fd8` originally passes 19
noninteractive suites in each x64 Debug, Release and ASan Debug profile (57
zero-skip passes), all six builds, the real expected-failure ASan probe,
validators and pinned formatting. ARM64 results are cross-builds. No local
focus-taking tests were authorized for that implementation.

The isolated native policy case proves first-attachment WARP selection, actual
software-adapter flags, inheritance by default peers, idempotence, conflicting
policy refusal with unchanged attachment counts, replacement generation changes
and final host retirement. Geometry and default gallery content are unchanged.

[PR #91](https://github.com/RedSalamanders/DxUi/pull/91) retains the implementation
history. Original source/fixture manifests, binary hashes, complete receipts,
failed initial build diagnostics and unpaired resource rounds are preserved in
`pr91-host-original.zip` (SHA-256
`1FB26D339EA4EEA4198FE572DACC2DA4FBFA1444145B3E88E29F8A675E221777`)
and the verified pre-integration Git recovery bundle. Every archive entry was
compared with its original bytes before removing raw files from the merged tree.
No independent process was terminated to unblock a build.

Integration reapplies the two host-policy commits to current main. Earlier
source-specific results do not qualify the combined source; current affected
checks and integration CI are separate evidence. The consumer's exact pin remains
preserved; library integration changes no consumer pin implicitly. Unpaired
rounds establish no non-regression. Physical device loss/occlusion, GPU lifetime,
native ARM64 runtime, physical IME/AT, agreed-time focus checks, independent
performance policy and D4c remain open in the
[active plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md).
