# Source notices

The imported DxUi source and tests originate in RedSalamander, copyright (c) 2026 RedSalamander,
under the MIT license retained in [LICENSE.txt](LICENSE.txt). See the historical per-file hashes in
[source origin](Specs/Done/SourceImport/source-origin.json). Copied reference images are test baselines, not shipping assets.

DxUi uses WIL (Windows Implementation Libraries), Microsoft Corporation, MIT.
The exact restored version is governed by [vcpkg.json](vcpkg.json) and its pinned baseline; the current dependency
is 1.0.260126.7. Its full [MIT notice](docs/licenses/WIL.txt) is retained and must accompany redistributions.
WIL is used by the implemented controls, graphics and host services; it is no longer a planned dependency.

DxUi uses [toml++ 3.4.0](https://github.com/marzer/tomlplusplus/tree/v3.4.0), Mark Gillard, MIT, as a private header-only TOML validator. Its full [notice](docs/licenses/tomlplusplus.txt) accompanies redistributions. The private vendored header records its upstream hash and local allocation-safety patch in [its README](ThirdParty/tomlplusplus/README.md).

DxUi uses [rapidyaml 0.16.0](https://github.com/biojppm/rapidyaml/tree/v0.16.0) as a private YAML event parser. Its [MIT license](docs/licenses/rapidyaml.txt) and [bundled notices](docs/licenses/rapidyaml-bundled.txt) accompany redistributions. [Source provenance and local patches](ThirdParty/rapidyaml/README.md) are retained.

DxUi also links Windows SDK/system libraries, using the consumer's matching MSVC runtime. Those components are
supplied under their respective Microsoft terms, not relicensed by this project's MIT license. System fonts are
selected at runtime; DxUi ships no fonts or icon fonts. Generated gallery images illustrate the library's own
palettes and controls. The pinned clang-format that `Tools/Install-ClangFormat.ps1` fetches for formatting is a
development tool, not a linked or packaged library dependency.

The former RedSalamander application dependency list (archive/image/network/viewer libraries and external theme
palettes) did not describe DxUi and has been removed from LICENSE.txt. The original RedSalamander copyright notice
and MIT grant remain intact; renaming the project does not erase that attribution. New dependencies or assets must
extend this inventory, with their full applicable notices, before distribution.
