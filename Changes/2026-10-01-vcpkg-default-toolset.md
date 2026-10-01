- `vcpkg-install.ps1` restores with the Visual Studio installation and default MSVC toolset that MSBuild compiles DxUi
  with, not the newest toolset vcpkg finds. The development machine's VS 18 Insiders has MSVC 14.52.36725, which has no
  x64-hosted ARM64 compiler, beside the default 14.51.36231, so every fresh ARM64 restore failed configuring
  `wil:arm64-windows` (vcpkg chose
  `14.52.36725\bin\Hostx64\x64\cl.exe` for it and the compile test ended in `LNK1104` on `MSVCRTD.lib`), and a consumer's
  compiled vcpkg libraries could be built by a newer toolset than the one that links them.
  - The installation comes from the discovery `build.ps1` uses, now one function (`Get-DxUiVisualStudioInstallation` in
    `Tools/VisualStudio.psm1`, which `test-consumer.ps1` and `Test-AsanRuntime.ps1` call too), and the toolset from that
    installation's `VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt`; nothing is hard-coded. A missing or
    malformed version file fails before anything is cloned, naming the file and the repair.
  - Each platform gets an overlay triplet, `<output root>\vcpkg-triplets\<platform>\<triplet>.cmake`
    (`Tools/VcpkgTriplet.psm1`): the pinned vcpkg checkout's triplet copied unchanged in its own line endings, then
    `VCPKG_VISUAL_STUDIO_PATH` and `VCPKG_PLATFORM_TOOLSET_VERSION` (major.minor), passed to `vcpkg install` as
    `--overlay-triplets`. It is rewritten only when its bytes change, so a patch update of the default toolset leaves it
    alone. vcpkg's package ABI hash changes with the triplet, so the first restore after this change rebuilds the
    packages (WIL here, built in 5-6 s). In an ARM64 restore on an x64 host the host triplet's tool ports keep the stock
    triplet; they run during the restore and are not linked.
  - A fresh ARM64 restore and a fresh x64 restore now succeed with vcpkg's own `Compiler found:` line naming
    `14.51.36231\bin\Hostx64\arm64\cl.exe` and `...\Hostx64\x64\cl.exe`. The interface (`-Platform`, `-OutputRoot`) is
    unchanged, so a consumer that runs the pinned checkout's script (RedXe, RedPrism and RedSalamander do) needs no
    change, and the script still parses in Windows PowerShell 5.1, which RedSalamander runs it with. A consumer's own
    installer for compiled libraries needs the same pins. `Tools/tests/Test-VcpkgTriplet.ps1` covers the fixture
    installations, the overlay text, the write-only-on-change rule and the wiring; each of 26 throwaway mutants (dropping
    either pin, losing the stock text, always rewriting, a second discovery, a new installer parameter, and more) fails it
    (`Build_ToolchainAndConsumption`). A nonvisual tooling change: no gallery image or design-system preview changes.
