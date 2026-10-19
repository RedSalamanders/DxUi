# M4 semantic accessibility and component closeout

Qualified native source: `5717ff85a7e0aecb5374cd2ff7d276f368d9e578`.
This packet publishes the 2026-10-19 UTC qualification on 2026-10-20 local time.
The retained preview now supplies acknowledged semantic UIA style and structure
through native and application-bridged embedded hosts. TSF/TextPattern coordinates
remain exact source UTF-16 ACPs. This closes M4's bounded component work;
the overall plan remains WIP, and D4c performance acceptance remains **HOLD**.

## Behavior and independent checks

Prepared source-format runs partition the entire raw source. Hidden markers stay
regular; transformed values and source objects retain their complete source owner.
Typed FontWeight/Italic/Underline/Strike/StyleId/FontName/FontSize/ReadOnly values,
mixed/unsupported values, FindAttribute and Format navigation use those runs.
Semantic children expose headings, lists, inert links, image alternatives, tasks
and tables with grid/header relationships. Empty padded cells retain distinct
element identity even when their source ACP is shared.

Task Toggle validates the owner, binding epoch, acknowledged publication,
effective enabled/read-only state and composition before one source-character
transaction. It preserves selection and supplies one undo unit/notification.
Children retire after edits or rebinds; geometry changes may retain CPU semantics.
Native other-thread calls use the existing bounded host dispatch; actual embedded
clients use COM apartment routing. Tests cover destructive callbacks and failed
allocations without exceptions crossing COM.

The eight directed tests are source-format ownership, all **1,324 pinned source
admissions**, and style/structure/task/composition/lifetime cases in both hosts.
Composition uses a rootless native IMM fixture and embedded host fixture;
this does **not** establish real HWND IME or physical assistive-technology support.

## Frozen qualification

All 19 ordinary scopes passed in x64 Debug, Release and ASan Debug: **57/57**,
zero capability skips. The intentional ASan detection probe passed. Debug,
Release and ASan Debug ARM64 builds passed; these are **compile-only**.
Fresh ARM64 Debug/Release compiler receipts match 18 final binary/PDB/library
hashes. The private audit rehashed **208** retained artifacts and report files.
Six archived verified build states bind the source, toolchain, dependency and
native artifacts to v2 build identities; scoped receipt format version 1 is not
the build identity version. Incremental-cache receipts have their own identity
and are retained without reconstructing the private intermediate inventory.
Raw receipt bytes are retained under `.receipt.txt`.

All five validators, complete tooling tests and pinned clang-format 22.1.3 passed.
Final metadata validation and native-equivalence evidence accompany the packet.
Native test inventory remains 123 files. The final equivalence check rehashes
25 current native outputs against the frozen archive and rejects native edits.

| Observed failure/resource fixture | Debug / ASan Debug | Release |
| --- | ---: | ---: |
| UIA children/header allocation points, two scenarios | 19 | 14 |
| Other-thread native Toggle allocation points | 7 | 3 |
| Semantic document allocation points | 657 | 250 |
| Pending semantic Begin allocation requests | 17 | 5 |
| Idle/retire tracked C++ allocations | 0 | 0 |
| Maximum plain semantic document retained C++ bytes | 1,443,451 | 1,443,051 |
| Retained C++ bytes after destruction, delta | 0 | 0 |

Each Embedded run also completes 1,000 warm composites with zero tracked C++
allocations. These are fixture-specific C++ counters, excluding SDK/driver/GPU
and process memory. They establish neither all-input latency nor performance
acceptance. See the raw logs for exact check counts and timings.

## Preserved failures and diagnostics

Private iteration archives preserve the two compile failures, native composition
setup failures, missing COM-apartment setup and the auto-reset test-event failure.
The final fixture uses a manual-reset event. None of these earlier attempts is
substituted for the frozen matrix. Final focused outputs are retained here.

The separate negative diagnostic links the final test object to the retained
pre-fix library and exits **3** before the native dispatch sweep completes. The
same test with the fixed library completes every observed allocation point.
The negative link has missing-private-PDB linker warnings and is a manual
diagnostic, not a canonical build, symbolicated stack or qualification receipt.
Code inspection found Debug STL proxy allocation before the noexcept boundary's
catch. Explicit fallible request construction and an outer COM function try block
now return allocation failure; positive sweeps verify unchanged source and retry.
Commands, link inputs, result and hashes are retained in `Raw/NegativeDiagnostic`;
the native artifacts remain in the private archive.

The serial, **single unpaired** Default diagnostic retains its original baseline
and candidate. It returned **advice-required / exit 1**: clean private bytes
+2.48%, dirty private/peak bytes about +4%, dirty working-set/peak about +2.1%.
Clean-frame p95 was 0.4963 to 0.5087 ms (+2.50%, inside that comparator threshold).
This observation establishes neither causality nor non-regression. No passing
repeat or rebaseline clears the earlier HOLD. Controlled paired clear/readback
and scheduler evidence remains a separate D4c obligation.

## Visual review and remaining gates

Fresh Release harness captures were reviewed as five full sheets, unscaled
436x480 RichTextEditor crops, the generic embedded sample and the rich sample.
All five rich regions are pixel-identical to their published gallery regions;
the rich sample PNG is also identical. The accessibility change alters no drawing
inputs. Existing gallery assets remain valid; fresh crops and the actual sample
are included here. Other whole-sheet slider/tooltip timing differences are outside
the unchanged rich component.

![Fresh unchanged rich sample](rich-text.png)

M5 extensions, M6 admission/capacity, remaining D2a preparation work, D4c acceptance,
agreed focus-taking suites, physical IME/Narrator/NVDA, native ARM64 execution and
consumer/product qualification remain open. M5 follows the owner's **C++ only,
explicitly limited Mermaid subset** decision; unsupported diagrams keep exact
editable source. No JavaScript/headless-browser helper is deployed.

Run `pwsh -NoProfile -File ./Verify.ps1` for offline evidence replay. It checks
packet hashes, raw report bindings, six v2 build identities, fresh ARM64 matches,
directed test completions, preserved failures and captures. It never executes
native tests or claims to rehash absent private binaries. Reviewed hashes and
source identity are evidence provenance, not an independent signature.
