# Long-checkout native tooling diagnosis

This scope diagnoses native fixture launch/resource paths and adds explicit
admission. It does not qualify arbitrary-length Visual Studio installation paths
or turn the original failed long-root aggregate into a pass.

At baseline `7c6a18e60e5859b73383caab259d445b1ae7709c`, the 133-character
checkout's MSVC fixtures passed 5 of 14 cases. The downloaded fake installer
existed, but its long executable path failed at process launch. Adding an extended
prefix still passed only 5 of 14 and is retained as a rejected experiment.

The independent new admission case fails against the original setup script
(14 of 15), then passes with the guard. Both direct and nested entrypoints pass
15 of 15. The guard refuses a planned bootstrapper path of 260 or more UTF-16
units before download, launch or output publication. Existing discovery,
signature, owned timeout and exact-tool assertions remain.

The unchanged run-evidence suite passes 57 checks with a 133-character scratch
root, including hard links. At a 180-character scratch root it passes that
hard-link stage, then fails when classic C# creates/deletes its temporary resource
file (`CS1567`/`CS1610`). Compiling in a compact owned directory and copying the
exact image to the actual tested profile path completes 58 checks at the same
180-character root length. The extra check compares staged/executed image hashes;
the existing long Unicode payload, stream and process-ownership checks remain.

The earlier 261-character hard-link failure has no established cause. This new
compiler diagnosis must not be assigned to it. Current hard-link success is a
separate observation. No native product build, performance, physical input or
consumer acceptance is claimed by these tooling results.

The [evidence index](qualification.json) binds retained original sources, logs,
fixture source/images and the candidate results to their hashes. Original private
packets remain in the repository's generated evidence area; no live user process
was terminated and no desktop input was used.

The first complete validator entry rejected the new metadata's omitted measurement
owner/input fields while all tooling tests passed. After those metadata fields
were corrected, the next entry passed every static validator but retained a
ten-second watchdog failure in the unchanged failed-consumer-publication fixture.
Its progress stayed incomplete, with no body-readiness marker or counted case.
An explicit-exit proposal still failed and was rejected; the original source was
restored. A focused same-source replay passes all 14 consumer progress cases.
These observations do not assign a cause to the failed full-run attempt or erase
it. The parent D-T1 plan owns that separate fixture investigation.
