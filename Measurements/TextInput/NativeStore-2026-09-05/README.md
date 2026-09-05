# Native text-store safety investigation

The candidate prevents stale control/selection access when a TextField or editable ComboBox text-change callback
destroys the tree, moves focus, replaces text, or throws. It also keeps ComboBox callback arguments alive through
control destruction. This archive predates the popup paint-priority follow-up and later OS-service implementation.

The baseline worktree is the unchanged ad888331931400063af71c4c567fcf502833d621 source. The candidate is based on
that revision with the native text-store fix. Both use the same four-input complex-ui-v2 fixture. Measurements
restrict only the measuring process and its children to affinity 0xFFFF, restoring affinity afterward; they make
no changes to other processes or power policy. The original TextStore-before receipts are retained from the prior
validated input component, before implementing this fix.

Performance acceptance remains **open**. Initial Debug comparisons exceeded different timing bands. An adjacent
unchanged/fixed pair also exceeded bands, but a subsequent B-then-A crossover passed for both candidate and
unchanged baseline against the same retained A1 report. The Release adjacent pair exceeded clean timing/FPS bands.
Those observations do not yet establish a reliable causal estimate or justify waiving a confirmed regression.
No threshold was changed and no failing report was replaced. Raw rounds and comparisons are retained here.

Both complete x64 functional runs subsequently passed all 18 suites, with nine interactive Menu capability skips
each. They ran without a paired-performance acceptance claim. See [validation](validation.md). Both ARM64
configurations cross-built; native CI for this follow-up is pending. The earlier input component's exact-branch CI
passed x64 Debug and both native ARM64 configurations, but x64 Release failed the inherited menu flood test.
That failure and the popup scheduling follow-up remain separate from functional text-input acceptance.

This is offscreen WARP evidence. It does not validate displayed FPS, physical input latency, consumer IME or UIA,
real audio/camera devices, or installed virtual-camera behavior.
