# Source ownership and consumer migration

DxUI owns the library source and tests. Historical source baseline: RedSalamander
`5f83dc4b0b7c5d66de4f96895da43298532dd046`; see [source origin](source-origin.json).

| Slice | Canonical location | State / intentional changes |
| --- | --- | --- |
| Frame runtime | include/DxUi/FrameRuntime.h; src/Foundation/FrameRuntime.cpp | Built and tested; one implementation, neutral namespace and injected diagnostics. |
| Controls, text, accessibility and window host | src/Controls | Owned editable source in namespace DxUi. Remaining application includes are enumerated in pending-dependencies.json; standalone build decoupling is pending. |
| Control tests and baselines | Tests/Controls | Owned tests with local control-header paths; application-specific fixtures still need separation before standalone execution. Baseline image bytes are preserved. |
| Original application-bound projects | Git history only | Retired; they referenced RedSalamander build properties and application sources. Supported projects are owned by this repository. |
| RedXe consumer | Application adapter | First consumer; integration pending. |
| RedSalamander consumer | Existing in-tree implementation | Later migration on HOLD; this older copy does not own ongoing library development. |

The 2026-09-05 ownership correction moved source into this repository's normal directories, consolidated the
duplicate frame runtime, removed obsolete application projects and adopted the neutral DxUi namespace.
Original hashes remain historical; current files evolve through Git reviews and tests.
Record behavior fixes that need an explicit backport. Do not synchronize source directories automatically.


## Single-library extraction (2026-09-05)

Controls, native services and window hosting now build in DxUi.lib together with Foundation and EmbeddedHost.
The public control header lives in include/DxUi; the old private path forwards to it. Application includes are gone.
Neutral replacements cover theme input, diagnostics, posted payloads, HWND hook utilities and small color/DPI helpers.
The owned AnimationDispatcher and test activation guard retain explicit source-origin entries. Helper algorithms
were extracted from RedSalamander Common/Helpers.h and Common/WindowSizing.h at the recorded origin
revision; they are maintained here. The payload registry is newly bounded rather than copied wholesale.

All 941 named inherited cases have a disposition in test-port.json (853 retained, 88 excluded). Original baselines
are retained. New tests cover public catalog construction, supplied-device drawing, typed slider lifecycle,
rendering resource behavior and bounded payload ownership. Application-specific helpers and source-text-only
assertions remain historical records rather than dependencies of the shared library.
