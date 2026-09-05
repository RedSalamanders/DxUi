# Source notices

The imported DxUi source and tests originate in RedSalamander, copyright (c) 2026 RedSalamander,
under the MIT license retained in [LICENSE.txt](LICENSE.txt). See the historical per-file hashes in
[source origin](Specs/Done/SourceImport/source-origin.json). Copied reference images are test baselines, not shipping assets.

DxUI's only vcpkg library dependency is WIL (Windows Implementation Libraries), Microsoft Corporation, MIT.
The exact restored version is governed by [vcpkg.json](vcpkg.json) and its pinned baseline; the current dependency
is 1.0.260126.7. Its full [MIT notice](docs/licenses/WIL.txt) is retained and must accompany redistributions.
WIL is used by the implemented controls, graphics and host services; it is no longer a planned dependency.

DxUI also links Windows SDK/system libraries, using the consumer's matching MSVC runtime. Those components are
supplied under their respective Microsoft terms, not relicensed by this project's MIT license. System fonts are
selected at runtime; DxUI ships no fonts or icon fonts. Generated gallery images illustrate the library's own
palettes and controls. Python/PyYAML are validation tools, not linked or packaged library dependencies.

The former RedSalamander application dependency list (archive/image/network/viewer libraries and external theme
palettes) did not describe DxUI and has been removed from LICENSE.txt. The original RedSalamander copyright notice
and MIT grant remain intact; renaming the project does not erase that attribution. New dependencies or assets must
extend this inventory, with their full applicable notices, before distribution.
