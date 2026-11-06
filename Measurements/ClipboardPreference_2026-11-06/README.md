# Text-first clipboard format preference

Implementation: `a1915ffdd94b18fd1817371f1dc074f6a0b547f1`, API 4.
The [verified qualification](qualification.json) binds six zero-warning builds, nineteen noninteractive
suites in each x64 configuration, actual ASan detection and the original native/run archive identities.
All 594 original compiled inputs match identity
`d32a8167669ca316cd97ff1989080a39ff8a676be4082041e7bceec6b516cca6`.
ARM64 entries are cross-builds, not native runtime execution.

Unicode text wins over coexisting bitmap offers by default. Applications with image-asset handling may
explicitly choose `TextClipboardPreference::ImageFirst`. File lists keep priority; forced plain paste remains
Unicode-only. The additive preference-aware transport selects formats in one clipboard ownership attempt
without changing existing clipboard virtual signatures. Native hosts and embedded text services both expose
the policy. Invalid values, malformed text, empty Unicode, image decline and reentrant policy changes preserve
the source, selection and history according to the [input contract](../../Specs/UI/UI_InputAndAccessibility.md).

Independent private HGLOBAL/clipboard fixtures count ownership and format reads, verify exact source and
history, and cover both native and embedded commands. They never access the person's desktop clipboard.
The retained fail-before/pass-after probe uses one unchanged compiled object: the old archive selected a
bitmap despite Unicode, while the candidate selected text with one ownership attempt. That isolated probe
is diagnostic evidence; the complete final matrix independently qualifies the frozen implementation.

The [retained attempts](retained-attempts.json) preserve the original disk-full run, the later 18-pass/one-fail
cohort and the deliberately interrupted ARM64 build at `edef7bc4`. Its existing bitmap-asset regression
still assumed the former default. The final implementation explicitly selects ImageFirst in that fixture
and keeps its asset assertions; new tests independently assert the text-first default. None of those earlier
attempts is relabelled as a pass. Conservative storage preflight stops and blocked cleanup attempts likewise
provide no native acceptance.

The first specification validation reported missing workload/fixture identity on the two new measurement
records. Both now carry their DxUi fixture names and exact input hashes; focused specification validation
and C++ formatting pass. Original failed validation and native attempts remain preserved separately.

All six native [gallery sheets](../../docs/gallery/README.md) were regenerated from the clean implementation
and visually reviewed. Their pixels match the preceding gallery; provenance now records this implementation.
Usage is documented in [rich text](../../docs/rich-text.md#clipboard-format-preference) and
[hosting](../../docs/hosting.md). The SDK supplies the application policy API; this packet does not claim
that RedTilde has implemented bitmap-asset persistence or an image-first settings UI.

Real clipboard interoperability, physical input/AT, native ARM64 and source-matched paired-performance
acceptance retain their owners in the [single active plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
Unpaired smoke measurements do not establish performance non-regression.
