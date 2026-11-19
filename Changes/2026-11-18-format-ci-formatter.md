- `format.ps1` now formats with the formatter CI uses and over the files CI formats. It takes clang-format from
  `Install-CiFormatter` in the pinned `.ci-engine` (22.1.3 today, under `.build/shared-format`) and selects the
  eight C and C++ extensions under the `formatRoots` of `.ci/project.json`, minus `formatExclude`. `-Check`
  still verifies without writing, and now lists every file that needs formatting.
  `Tools/Install-ClangFormat.ps1`, the second copy of the version pin, `-FormatterPath`, `DXUI_CLANG_FORMAT`,
  and the `PATH` and Visual Studio lookups are removed. `Tools` was outside the old local scope, and
  `Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp`, its one unformatted file, is formatted: whitespace only.
  The build contract, contributing guide, getting-started page, tools index, and notices describe the new
  command. This is step CV-6-05 of the `RedHome` convergence plan (register D-02, D-05, D-06).
