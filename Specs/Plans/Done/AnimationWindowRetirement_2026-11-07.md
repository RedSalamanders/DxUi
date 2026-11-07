# Native animation window retirement — scoped completion

Status: DONE for native animation cache and reentrant-detachment lifetime repairs.
Implementation: `06b8d5188d063d16aa27f91eaa1092e3cc136a55` (API 5).

The final native attachment retires its module's animation HWND before failure-host uninstallation. Siblings
retain the warm cache and subscriptions. In-tick retirement waits until callback completion, and native
detachment callbacks cannot publish a replacement subscription. Durable behavior is merged into native hosting,
failure ownership, resource contracts and usage docs. Default gallery pixels are unchanged.

The [packet](../../../Measurements/AnimationWindowRetirement_2026-11-07/README.md) binds six zero-warning builds,
66 ordinary x64 suite executions, actual ASan detection and independently verified original archive members.
It retains the consumer's original-context teardown diagnosis, the old SDK public-fixture failure, the first
candidate's separate callback-rearming failure and unchanged-object passes after correction. Setup and tooling
failures are preserved. Consumer adoption, fresh hosted native results, physical/platform and paired-performance
acceptance remain with the [single active parent](../WIP/RemainingWork_2026-11-04.md).
