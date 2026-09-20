# Main merge: initial x64 ASan accessibility failure

Main revision `78b3de389a189c7f86f611787e0489fb6d474218`, push CI run `35531706045`, initial job `106133346270`.
The merge tree equals qualified PR head `b9c9390732c2b586d55b6089f190fda097cec4d2`.

Accessibility **failed**, with the UIA client still in `ElementFromHandle` at the unchanged 3000 ms subscription deadline (`ready=0`, `E_PENDING`). The HRESULT is the test's initial pending sentinel, not an error returned by ElementFromHandle. Notification assertions were not reached. The expected ASan detection probe passed. This is not evidence of a production notification failure or a known root cause.

Retained complete job/artifact logs: `C:/RedSalamander.Perf/evidence/i26-ui/main-78b3-first-asan` and `main-asan-job.log` alongside it. Raw reports use `.txt` here to distinguish test receipts from performance-schema JSON. One diagnostic retry is planned after the full initial run finishes; retain this failure even if the retry passes. Current consumer pin is unchanged until the standard updater accepts successful current-main CI.

Ten consecutive local x64 ASan Accessibility diagnostic executions passed unchanged, zero skips; subscription completed in 93–109 ms. `local-diagnostic.txt` retains executable identity and per-run timings. Full logs remain at `C:/RedSalamander.Perf/evidence/i26-ui/main-uia-diagnostic-local`. This bounds local reproduction attempts and does not explain or erase the native CI failure.
