# Selection blocks and input lifetime

Implementation: `f765aeca5e059dbe768841b605b1be6a5f6ea942`, additive API 4.
The [source-bound qualification](qualification.json) records all six zero-warning/zero-error builds,
19 noninteractive suites in each x64 profile (57 suite executions, zero skips), and the real ASan detector.
The exact 593 working compiled inputs and all six native outputs/symbols were archived and member-verified
before compiler caches were retired. Their hashes preserve the actual input bytes, including line endings.

The original `0acd5608` sanitizer executable independently detected heap use after free in direct TextField
import, ComboBox import/variant changes and FormField clear. The original native archive and diagnostic
receipts/log hashes remain retained. Release also reproduced premature preparation-wake retirement.
The candidate exercises those retiring and live paths, nested input, detach/reattach and independent views.
Preparation wakes now wait for the outer mutating input call; direct setters remain synchronous and stop
when a callback retires their control. The host itself must outlive its synchronous input call.

The new selected-source transaction has independent exact outputs in Rich/Source modes for partial and
multiline ranges, mixed CRLF/LF/CR, Unicode, terminal line endings and embedded backticks. It preserves
direction and one-step Undo/Redo and checks read-only/disabled, capacity, language and preview refusals.
RedTilde's original sample-replacement counterexample and its exact-pin adoption belong to that repository.

Retained intermediate compiler/fixture failures are listed in the packet. They include an invalid preview
request that omitted its required composition range and a Return fixture whose input-modality wake was
not settled before arming retirement. Corrected fixtures use the public input protocol; assertions remain.

ARM64 entries are cross-builds. Native ARM64 runtime, real-focus/physical IME/AT/touch and API 5 remain
separate gates. Performance reports are unpaired diagnostics and do not establish performance acceptance.
