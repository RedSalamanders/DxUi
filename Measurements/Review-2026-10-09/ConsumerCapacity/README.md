# Consumer source capacity audit

This read-only source audit informs Q9/Q18. It does not measure runtime concurrency or qualify consumer adoption.
The lead independently checked the described-history construction, history cap/load normalization, and the two
library registries after the delegated inventory.

On 9 October, RedSalamander was at `a0615dc359e133495ab7cfa95c75624884eb8dea` with 32 dirty entries. These are
observations of that working tree, including its in-progress popup source, rather than a pristine release claim.
`RedSalamander/FolderWindow.FileOperations.Popup.cpp` constructs described destination rows from `GetFolderHistory`,
using the parent location as `secondaryText` and the full location as the accessible name. History is capped at 50
by `FolderWindow.FileSystem.cpp` (`kFolderHistoryMaxMax`, `SetFolderHistoryMax`), the preferences clamp and
`Common/Common/SettingsStore.cpp` load normalization; its default is 20 in `Common/SettingsStore.h`.
Thus the largest statically supported described history list is 50 rows, plus the fixed other-pane command and
optional separator. Filtering can reduce it. This is a source-backed ceiling for this list, not an observed runtime
peak or a generic menu-row limit. The library's described scrolling fixture now uses 50 synthetic rows and checks
that the last row is visible and its exact command remains reachable by keyboard.

The delegated app-source inventory found no described `MenuFlyoutItem` lists in RedXe at clean
`d6e28802e2d05d6ff2a545944e80e0af27255ee1` or RedPrism at
`97dd25b70e83f81c339a4b4e7e7b7589268f2510` (dirty WIP documentation). Both use embedded hosting. The lead's check also
found RedPrism's `ChromeHost.cpp` calls `ShowNativeHMenuContextMenu` for its chrome menus; that use is distinct from
the described history lists. RedSalamander also converts Shell HMENU trees; Shell/provider row counts and submenu depth are data-dependent and were not inferred
from the history setting. These source snapshots do not update any consumer pin.

`src/Controls/DxUi.WindowHost.cpp` rejects the 129th attached host with `ERROR_NOT_ENOUGH_MEMORY`.
`src/Support/PostedPayload.h` independently bounds registered HWNDs and pending payloads at 128 and diagnoses
exhaustion. RedSalamander has panes, comparison windows, viewer hosts and on-demand modeless windows/popups;
source gives no process-wide peak for their combined lifetime or queued transport work. Keep the existing limits.
Representative application-side instrumentation and concurrent-popup execution remain required before claiming
capacity adequacy or changing those bounds.
