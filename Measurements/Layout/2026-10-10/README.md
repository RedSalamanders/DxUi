# Intrinsic layout and forms: native evidence, 10 October 2026

S0 intrinsic measurement, S1 opt-in Flex layout and S2 form composition are implemented on
`codex/performance-forms`, based on `60e1cbff8f7a75757e9447073965d09101d9833c`. These local receipts identify
the dirty candidate by its actual compiled inputs and artifact bytes. They do not describe an unchanged base,
an accepted paired regression result or a consumer pin update.

All nineteen noninteractive suites pass without capability skips in x64 Debug, Release and ASan Debug: 57
suite/log pairs. All 38 newly registered intrinsic/Flex/form/form-accessibility tests complete in each profile.
The four focus suites additionally pass in Debug, Release and ASan Debug with zero skips: twelve completed
suite/log pairs. All three completed leases restore the desktop successfully. ASan Debug's three earlier Start
confirmations time out before any focus child runs; the third reports failed foreground/pointer restoration,
followed by an unavailable-input-desktop check. Those failed attempts remain independently retained.
All six x64/ARM64 Debug, Release and ASan Debug configurations build successfully. ARM64 code was not executed
on this x64 computer. Complete Windows validators/tooling and clang-format 22.1.3 checking passed at the frozen
native source; later documentation/evidence changes require their own tooling check.

Native JSON receipts are retained byte-for-byte as `*.receipt.txt`, with their corresponding logs. This naming
distinguishes test/build receipts from the performance receipts validated under Measurements. The scoped
receipts retain their source/dependency/artifact/scope identities. `native-scopes.txt` and `new-tests.txt` freeze
the proof inventory. [Analyze.ps1](Analyze.ps1) reopens the native/form/Grid evidence and writes
[analysis.txt](analysis.txt); [SHA256SUMS](SHA256SUMS) seals the packet. Text conversion is disabled locally so
original bytes survive Git checkout. Executables and other `.build` products are not included.

The final ASan ControlTests executable is
`68EBDB54DDB3E91CF418F3A35A243D2033F03AF6A7E1F37024CFD64407E9749A`.
The Release EmbeddedTests executable used by both diagnostics is
`198D5A32F3EC51631AAA4A7C20120997A344D0CBB6ED2749E474291EA44E61AF`.
The ASan detection probe deliberately reports a heap-use-after-free and a nonzero exit; that isolated expected
failure is recorded separately from the passing suites.

## Initial failures and focus gate

The first integrated Debug Embedded run failed a stale assertion expecting thirty catalog controls after all
thirty-three factories had already created/prepared/composed successfully. The assertion and current gallery
guide were corrected, all six configurations rebuilt, and the complete native runs above used those new binaries.
`first-Embedded-Debug.*` retains the original failure; it is not reused as passing evidence.

The first ASan Accessibility run failed
`TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow`: the client heard a selected `Ligne 3`
event during the unchanged-selection step. The same executable subsequently passed that isolated test and a
complete nineteen-suite rerun, including all ten new form accessibility tests. `first-Accessibility-ASanDebug.*`
and `asan-selection-isolated.txt` retain both observations.

Lead and independent source review found no new S2 Grid selection emission path: equal selected-ID sequences
are skipped and the native event baseline advances before outgoing callbacks. The client helper accepts 500 ms
of silence, discards its 10 s quiescence timeout result and deduplicates only within each step's event mark. A
late repeat crossing that mark is plausible, but the retained log has no provider emission counts or arrival
timestamps to establish the cause. The original failure remains visible; neither the isolated pass nor the
rerun proves transport attribution.

Two initial Debug `test.ps1 -Interactive` attempts timed out before Start was chosen. Both retained lease
results report foreground, keyboard focus and pointer unchanged; no focus child ran in those attempts.
After the form/Grid diagnostics and calibration, the confirmed Debug and Release runs passed Menu,
NativeTextInput, MenuResources and MenuResourceScaling with zero skips. Their summed child runtimes are
301.1 and 118.5 seconds. Both leases completed, left foreground/focus unchanged and restored the pointer.
Two ASan Debug confirmations subsequently timed out without launching a child and preserved the desktop.
A third attempt, after the person reported being at the desktop, also timed out before launching a child.
This attempt reports foreground and pointer restoration failed, focus restoration skipped and an unknown final
cursor position. Its subsequent native `DxUi.InteractiveLease.exe --check` exits 20 and reports that the input
desktop cannot be opened (Windows error 5: a locked session or secure desktop has input). This observes desktop
unavailability after the timeout; it does not prove when the desktop changed or claim successful restoration.
After Windows is unlocked and the native desktop check passes, a fourth ASan attempt accepts local Start and
passes all four focus suites with zero skips. Its summed child runtime is 174.7 seconds; foreground and keyboard
focus remain unchanged and the pointer is restored. This completes the local x64 foreground-suite gate.
The analyzer checks every completed raw lease against its four suite receipts and retains all three earlier
incomplete ASan attempts, including the third attempt's restoration failure, independently of the successful run.
The preserved third-attempt wrapper log also contains a contradictory suffix saying that the run left the
desktop as the person had it. The frozen `test.ps1` chooses that suffix from `RunMovedSomething=false`, which
also occurs when restoration fails; it does not establish an unchanged desktop. Its failure accounting rejects
the run correctly. The raw restoration statuses and subsequent native refusal are authoritative; the reporting
defect remains a separate runner follow-up in the production review plan.
All failures/timeouts and successful results are retained separately. The person's time authorization remains
recorded in the task; the restoring lease still requires its local Start choice.

The successful leases warn that their final pointer differs from the pre-lease position. All four cursor-moving
Menu cases in all three completed profiles log `restored=1 exact=1`. Source review confirms that the outer lease
captures its restoration target before requesting Start; the confirmation itself can move the pointer before
a fixture records its entry position. No post-confirmation or per-child pointer observations identify the
source of the outer displacement, so the warning does not establish a fixture leak. The cursor helpers and
lease are unchanged from the baseline. All three outer restorations passed; a future attribution diagnostic should
retain positions after confirmation and around each child while preserving the pre-confirmation restore target.
The retained `interactive-performance-*.json` reports are unpaired preflight measurements, not accepted
non-regression evidence. The repeated isolated ASan detection probes are also retained.

## Candidate-only manual/form diagnostic

The successful invocation was:

```powershell
./performance-forms.ps1 -Configuration Release -Platform x64 -SkipBuild -Seed 20261010 -Blocks 12 -OutputPath C:/Users/eric/.codex/tmp/dpf-forms/forms-release-12.json
```

[forms.json](forms.json) is the stamped v2 receipt; [forms.raw.receipt.txt](forms.raw.receipt.txt) contains the
exact native bytes, renamed from the original `.json.raw.json` path recorded in the stamp. Raw SHA256 is
`5CB0E32D951F307FC707515590141CE44C27EBC134F47C511831F91BF558BEAE`; stamped SHA256 is
`D1CE259B6F4E56B038AA0D03D1636EA3B1CA0B30CCFDA55DD513AB99ED7F9596`.
The runner checked actual compiler/SDK/dependency/artifact inputs before and after the report and retained
the form fixture, dispatcher and runner hashes. Lead and independent audits verified the raw payload, recorded
build identity, schedule and every frame/counter.

Each profile contains eight fields, five pre-timing parity states, twelve independently drawn ABBA/BAAB blocks,
four fresh fixtures per block and forty frames per phase. The seed happened to produce six blocks of each order;
balance was not forced. Widths are physical pixels: Standard LTR/above uses two columns at 640/520 pixels and
96 DPI (640/520 DIP); compact RTL/beside uses one column at 720/600 pixels and 144 DPI (480/400 DIP).
Leaf hierarchy, metadata, bounds within 0.01 DIP and exact populated pixels
match before timing. There are 96 runs and 11,520 timed frames.

The following descriptive milliseconds are medians of twelve block centers. Each center averages its two
variant run frame medians. No significance test or acceptance verdict is inferred.

| Profile / phase | Layout + prepare Manual / Form | Completed frame Manual / Form |
| --- | ---: | ---: |
| Standard LTR, unchanged | 0.000175 / 0.000200 | 0.24154 / 0.24276 |
| Standard LTR, width | 2.93260 / 2.92886 | 3.28324 / 3.27096 |
| Standard LTR, validation | 2.66394 / 2.70115 | 3.01345 / 3.05138 |
| Compact RTL, unchanged | 0.000200 / 0.000225 | 0.42248 / 0.42821 |
| Compact RTL, width | 3.28325 / 3.31748 | 3.87849 / 3.89439 |
| Compact RTL, validation | 2.83905 / 2.84960 | 3.37205 / 3.38676 |

The common scopes include mutation setters and all layout/preparation. Completed frames also include target
bind/clear and blocking GPU completion. `prepareMs` excludes manual arrangement but includes Form intrinsic
arrangement, so it is not an equivalent comparison interval. No-op timings approach the recorded 0.1 microsecond
granularity; completed-frame outliers reach 12.62 ms. These observations establish no speedup.

All 3,840 unchanged frames perform zero measures, preparations, surface allocations or tracked C++ allocations.
Composition performs zero tracked C++ allocations throughout all 11,520 frames; the native fixture also asserts
no measurement/preparation during composition. Width/validation phases have equal tracked work/allocation
counters for both variants. Form adds 36 construction allocations, 6,048 requested allocation bytes and 7,168
bytes of warm retained plain C++ memory. Every run's destruction returns that tracked live delta to zero.

The four ten-stage lifecycle records exercise hide/show, visible and hidden graphics-generation replacement,
root retirement, detach and destruction. Hidden views have zero surface bytes and perform no work. Showing and
both recoveries restore matching pixels. Form's measurement cache stays at 24/32 calls through replacement;
Manual remeasures to 72/96 calls after both recoveries. Root retirement retains the view's blank surface;
detach clears reported surfaces/caches/statistics and destruction leaves zero tracked plain-live delta.

This fixture covers two eight-field profiles with ample editor space. Separate correctness tests cover narrow
units/minima, bounded convergence, callbacks and the 256-field limit. C++ counters exclude COM/driver heaps;
plain-live tracking excludes aligned allocations. Caller-owned graphics pools survive the lifecycle tracking
window, and process peak working set is a high-water mark. Graphics-generation replacement is synthetic WARP
recovery, not a hardware device-removal test. Neither these costs nor retirement observations qualify consumer
performance, an arbitrary form, a leak-free process or the paired regression policy.

## Grid retention and process heaps

All three Release/x64 runs used `performance.ps1 -SkipBuild` with scenarios MultilineGridRetention,
MultilineGridHeap and MultilineGridHeapPaced and separate output paths under `C:/Users/eric/.codex/tmp/dpf-grid`.
Original stamped reports and their unpaired comparisons are retained. Each runs 6,000 scrolling frames with
thirty 200-frame samples, plus start/model-clear/detach observations. Heap walks occur outside timed frames,
are bounded to 128 process heaps and report any partial walk; both requested heap runs walked five heaps with
zero errors in every retained sample. The paced run takes 120.007 seconds at a 50 FPS target. No trimming,
purging or forced collection is used.

| Run | Private bytes start / model clear | Working set start / model clear | Heap busy start / model clear / detach |
| --- | ---: | ---: | ---: |
| Retention | 34,963,456 / 31,887,360 | 50,888,704 / 47,640,576 | Not requested |
| Heap | 34,304,000 / 31,817,728 | 50,614,272 / 48,132,096 | 9,726,710 / 11,296,768 / 10,763,538 |
| Heap paced | 35,282,944 / 31,776,768 | 50,999,296 / 48,078,848 | 10,810,376 / 9,684,808 / 9,151,578 |

All scrolling observations retain 236 handles and 3,686,400 view surface bytes. Detach reduces handles to 234;
hidden preparation/composition counts are zero. Heap committed bytes remain above busy bytes and include free
capacity. Direction differs between the two heap runs, and process memory ends below its starting sample in all
three. This provides bounded residency/accounting evidence, not attribution of the earlier approximately 1.05 MB
paired working-set flag or proof against long-duration growth. WARP/DirectWrite/process pools and unaccounted
virtual allocations remain part of the process snapshots.

QPC runs at 10 MHz (100 ns per tick). The earlier Grid clean-prepare flag from 600 to 700 ns is one tick; a small
real overhead remains plausible. The unchanged raw flags, 5% timing/FPS and 2% memory bands and exact budgets
remain in force. A separate batched no-op diagnostic would change metric scope and require contract/policy
review. The [new guarded same-binary calibration](../../Performance/2026-10-10-aa-v2/README.md) completes all
144 reports but retains 72/72 unstable controls. Trusted-policy and ordinary-candidate gates remain open.
