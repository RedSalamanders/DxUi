# Performance diagnostic projection

Both unpaired comparisons require advice. The first memory flag did not repeat; timing flags differ. Retained diagnostics do not confirm cause or establish non-regression. Existing performance acceptance remains open; baseline unchanged.

Baseline code: 82c0bb25a23ada5c5e6d8c5f182fab004fbbab02

Baseline receipt SHA-256: A3FD4CC0A013523C1CC77678CEA67F7F9CB535BFE7479FA33578F23E610C1C01

| Attempt | Metric | Baseline | Candidate | Change % | Status |
| --- | --- | ---: | ---: | ---: | --- |
| first | dirty/privatePeakBytes | 26439680 | 26992640 | 2.0914 | advice-required |
| repeat | clean/composeCpuP95Ms | 0.0143 | 0.0183 | 27.9720 | advice-required |
| repeat | dirty/prepareP95Ms | 1.7644 | 1.8894 | 7.0846 | advice-required |
| repeat | dirty/composeCpuP95Ms | 0.0163 | 0.0182 | 11.6564 | advice-required |

Both comparisons use the original dxui-complex-ui-v2 Default fixture. Clean allocations remain zero; dirty rounds contain 2145–2160 allocations, with a reported median of 2160 per 40-frame round. Composition allocations remain zero and surface bytes remain 3686400. Unpaired flags do not establish cause or an accepted cost. The original baseline and thresholds are unchanged.

| Candidate | Raw native receipt SHA-256 |
| --- | --- |
| first | 871F246CC78E09A5D503D3193260185BD4699F5D96F5E03FACD8BA6D87963C30 |
| repeat | A6F32F7D5B415FC7CAA80B851825A3BA50676F8F5B8D98EBC43239B20B239AD3 |

Original comparison bytes, complete numeric changes and source/protocol identities remain in the private inventory in verification.md. This projection changes neither comparisons nor their advice-required status.
