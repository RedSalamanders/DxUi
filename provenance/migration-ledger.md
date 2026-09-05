# Extraction and migration ledger

Source baseline: RedSalamander `5f83dc4b0b7c5d66de4f96895da43298532dd046`.

| Slice | Active destination | State / intentional changes |
| --- | --- | --- |
| DxUi.FrameRuntime.h/.cpp | include/DxUi/FrameRuntime.h; src/Foundation/FrameRuntime.cpp | Extracted: neutral namespace, explicit per-thread diagnostics sink, no Helpers.h/application logger. Clock/stage/motion semantics retained and tested. |
| All other controls, hosts and generic tests | upstream reference | Pending decoupling and extraction; not compiled as standalone support. |
| RedXe consumer | Application adapter | Pending first integration; no application source changed by bootstrap. |
| RedSalamander consumer | Existing in-tree implementation | Later migration on HOLD. |

Record any behavior fix that may require an explicit backport. Do not synchronize directories automatically.
