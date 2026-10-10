- Bind benchmark build reuse to compiled source, installed dependencies, the resolved toolchain and actual artifacts;
  verify unchanged source, executable, fixture, tools, environment and power policy around each measured run, and retain
  invalid samples separately instead of stamping a stale or concurrently changed executable as current evidence.
  Version the attestation to retain each side's expanded projects and resolved source inventory while comparing stable
  compiler, shader-role, linker and librarian profiles. Require a fresh build for old receipts and refuse unsupported
  heterogeneous compiler options, generators, build events or tool overrides rather than publishing incomplete provenance.
  Bind the actual solution configuration and build mappings; default-output builds use incremental compilation only when
  the prior v2 receipt verifies every input, tool and final artifact and a separate receipt binds every intermediate
  profile path and byte; otherwise rebuild. Keep final-only SkipBuild independent of disposable cache. Hash actual
  resolved C++/HLSL bytes and all owned native include inputs per side, and all selected MSVC/ATL/SDK/WinRT/VS/NETFX
  tool inputs, including extensionless headers and optional-root absence. Refuse unsupported external/ignored inputs,
  response/path overrides and foreign intermediate headers. Allow only each project's canonical MSBuild failed-build
  status marker so a failed compile can retry with guarded Rebuild; it never authorizes stale reuse or foreign headers.
  Consumer output roots retain their existing build behavior.
