# Selected independent YAML corpus cases

Inputs originate from [yaml-test-suite data](https://github.com/yaml/yaml-test-suite/tree/6ad3d2c62885d82fc349026c136ef560838fdf3d) commit `6ad3d2c62885d82fc349026c136ef560838fdf3d`; [MIT license](../../../../docs/licenses/yaml-test-suite.txt), from main commit `da267a5c4782e7361e82889e76c0dc7df0e1e870`. SHA-256 of original input bytes:

- `P76L b9cb381e23756cc00fb1e61b1fa7cfabc9180c824d64c5f02d12033e9fb1ac93`
- `2JQS ecd8d4ad8333dfa1d3e7ef5245afd8c578366c7cf555f30830233a3ce7b51aef`
- `X38W 020e50de93c1b42b4e39e998ebadd5ea9b68cd50942e9bf6e63ff66e317e34cd`

The test embeds equivalent raw content. P76L uses a comment after its document marker to avoid colliding with the exact outer front-matter closing line. The suite's syntax/event results admit 2JQS and X38W, but their mapping keys are duplicate YAML representation keys; DxUi deliberately reports those as duplicates under the named contract. An exploratory native driver using a Windows CRLF checkout matched 380 inputs, reported the two duplicate-key differences above, and skipped 20 multi-document inputs; the original run and its failed first attempt are retained under the isolated worktree `.build/metadata-research`. This is not a complete conformance verdict or final-source qualification.

The exact upstream commit tree is retained as `source.tar` (SHA-256 `8fc549bab9fc4b1e258c32ea42c484305b81d489a38c858d2eeaa2bec6828671`). `Tools/Generate-MetadataCorpus.ps1` verifies it and emits all 402 inputs and upstream expectations into the native test. It explicitly excludes 20 multi-document streams and records 2JQS and X38W as duplicate-key representation errors. The remaining 382 cases test validity, not decoded values or complete YAML conformance.
