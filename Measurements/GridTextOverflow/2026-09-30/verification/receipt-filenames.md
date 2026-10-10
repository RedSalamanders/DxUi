# Raw receipt filenames

Every raw JSON is renamed to `.txt` with its byte content unchanged, because the measurement validator reads each `*.json` under
Measurements as a benchmark receipt. Names are shortened and freed of spaces so every tracked path stays within 150 characters.

- `Grid-x64-Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-Debug/Grid.receipt.txt`
- `Rendering-x64-Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-Debug/Rendering.receipt.txt`
- `Accessibility-x64-Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-Debug/Accessibility.receipt.txt`
- `Embedded-x64-Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-Debug/Embedded.receipt.txt`
- `MultilineText-x64-Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-Debug/MultilineText.receipt.txt`
- `Performance-x64-Debug-21b4a67dcb504551993155946d43ee33.json` (complex-UI benchmark receipt) -> `raw/x64-Debug/performance.receipt.txt`
- `Performance-x64-Debug-21b4a67dcb504551993155946d43ee33.json.comparison.json` (its comparison) -> `raw/x64-Debug/performance.comparison.txt`
- `Grid-x64-Release.json` (suite receipt, `.build/reports`) -> `raw/x64-Release/Grid.receipt.txt`
- `Rendering-x64-Release.json` (suite receipt, `.build/reports`) -> `raw/x64-Release/Rendering.receipt.txt`
- `Accessibility-x64-Release.json` (suite receipt, `.build/reports`) -> `raw/x64-Release/Accessibility.receipt.txt`
- `Embedded-x64-Release.json` (suite receipt, `.build/reports`) -> `raw/x64-Release/Embedded.receipt.txt`
- `MultilineText-x64-Release.json` (suite receipt, `.build/reports`) -> `raw/x64-Release/MultilineText.receipt.txt`
- `Performance-x64-Release-b87064d1341b458a81c8462300cbe251.json` (complex-UI benchmark receipt) -> `raw/x64-Release/performance.receipt.txt`
- `Performance-x64-Release-b87064d1341b458a81c8462300cbe251.json.comparison.json` (its comparison) -> `raw/x64-Release/performance.comparison.txt`
- `Grid-x64-ASan Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-ASan-Debug/Grid.receipt.txt`
- `Rendering-x64-ASan Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-ASan-Debug/Rendering.receipt.txt`
- `Accessibility-x64-ASan Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-ASan-Debug/Accessibility.receipt.txt`
- `Embedded-x64-ASan Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-ASan-Debug/Embedded.receipt.txt`
- `MultilineText-x64-ASan Debug.json` (suite receipt, `.build/reports`) -> `raw/x64-ASan-Debug/MultilineText.receipt.txt`
- `Performance-x64-ASan Debug-9f1058ab7822471d91ede229fc3e3af8.json` (complex-UI benchmark receipt) -> `raw/x64-ASan-Debug/performance.receipt.txt`
- `Performance-x64-ASan Debug-9f1058ab7822471d91ede229fc3e3af8.json.comparison.json` (its comparison) -> `raw/x64-ASan-Debug/performance.comparison.txt`
- `AddressSanitizer-x64.json` (detection probe receipt) -> `raw/x64-ASan-Debug/asan-probe.receipt.txt`

