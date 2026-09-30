# Invalid original-policy run

The source snapshot is original direct-map, but restored Grid.cpp/header preserved older mtimes (2026-09-23 12:09 UTC) than the retained candidate Grid.obj (12:17) and DxUi.lib (12:20). The 15:30 build log never compiles Grid.cpp. Newly built tests therefore cannot qualify the original policy and may have a header/library ABI mismatch. Do not use this run as an original/candidate performance pair or treat 64 hits / zero misses as original-policy behavior.

Rendering exits 0xC0000005 during host teardown, after cache counters. No stack trace attributes that crash conclusively. Independent source review also found the fixture's borrowed model declared after its host; the corrected fixture declares the model first so it outlives detach. Both follow-up policy builds must use -Rebuild and retain identical corrected fixture bytes. This failed run and original receipts remain unchanged.
