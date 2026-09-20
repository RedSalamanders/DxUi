# Native disclosure subscription diagnosis

At source `1ea876fc5a432c2c667bd8d9b90a0104475decae`, local x64 Release `test.ps1` passes all 18 suites. Accessibility has zero skips and reports native event subscription success at AddPropertyChangedEventHandlerNativeArray, HRESULT 0, 94 ms. Menu records nine foreground capability skips; those are not passes. The runner retains performance measurements as unpaired observations; this diagnostic changes no production code or visual baseline.

The original remote push-run failure at the three-second subscription check is preserved in prior-ci-failure.log. Stage/HRESULT were absent there, so this local success does not establish its cause or waive it. The diagnostic keeps every original assertion and deadline. Updated CI and subsequent investigation remain in the active plan. Files are hashed in manifest.txt; executable and performance identities are in the suite receipts.
