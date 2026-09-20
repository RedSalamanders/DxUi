# Final native localized-layout qualification

Exact revision: `b9c9390732c2b586d55b6089f190fda097cec4d2`.
[Push CI](https://github.com/RedSalamanders/DxUi/actions/runs/35526329502) and
[PR CI](https://github.com/RedSalamanders/DxUi/actions/runs/35526332077) both pass all six native
x64/ARM64 Debug, Release and ASan Debug jobs. The retained push artifacts contain all eighteen
suite receipts per profile, external-consumer integration, and expected-failure ASan detection
where applicable. Native architecture, exit codes, probe detection and complete suite coverage
were checked while archiving. Explicit capability skips remain in the raw receipts and manifest.
Every CI job also passed gallery generation. The final five local theme sheets were visually
reviewed before publication in `docs/gallery`.

Full downloaded logs and gallery artifacts remain in
`C:/RedSalamander.Perf/evidence/i26-ui/ci-b9c9390-complete`. These receipts qualify the library
revision, including complete multiline Checkbox captions. They do not qualify RedSalamander,
physical consumer mixed-DPI presentation, or real assistive-technology use. PR #18 was explicitly approved and merged as `78b3de389a189c7f86f611787e0489fb6d474218`;
its tree is identical to this qualified revision. The consumer pin is unchanged while the main
commit receives its required push CI. Adopt through the normal validated-main workflow, preserving
`f5c7fc7403e352425b8e82b7a4da1c042d942ede` for rollback.
