- `vcpkg-install.ps1` keeps a complete clone of the pinned vcpkg checkout instead of a `--filter=blob:none`
  partial clone. vcpkg 2026-07-27 extracts each versioned port with `git read-tree` and `git checkout-index` in an
  environment that holds only `GIT_INDEX_FILE`; without `SystemRoot` that Git resolves no host, so a blob that the
  partial clone left on the server failed with "Could not resolve host: github.com". It started on 2026-11-18,
  when upstream `master` stopped carrying the `vcpkg-cmake` tree the baseline selects, and stopped every fresh
  restore on x64 and ARM64. The pinned commit is now fetched only when the default branch lacks it, without
  `--depth`, and a partial checkout left by an earlier version is completed in place once. The tooling test pins
  the clone, the fetch and the completion order.
