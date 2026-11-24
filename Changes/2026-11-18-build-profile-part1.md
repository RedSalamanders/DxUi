- Moved the toolset (`v145`), the Windows SDK (`10.0.26100.0`) and the debug-library choice that selects
  optimization (`UseDebugLibraries`) from the seven project files into `Directory.Build.props`; each project file
  now holds only its own facts (`RedHome` convergence CV-6-04 part 1, register C-11). `Build/NativeBuildPolicy.targets`
  no longer names a consumer: the three triplet properties `RedXeVcpkgTriplet`, `RedPrismVcpkgTriplet` and
  `RSVcpkgTriplet` become one `NativeBuildVcpkgTriplet` that a consumer sets (C-19). DxUi sets none of them, and a
  consumer reads the new text only when it pins this commit or a later one.
  No compiler or linker input changes: an MSBuild evaluation of all seven projects in the six profiles before and
  after shows the same properties read by the build targets and the same metadata on every compile, link, library,
  resource and build-event item.
