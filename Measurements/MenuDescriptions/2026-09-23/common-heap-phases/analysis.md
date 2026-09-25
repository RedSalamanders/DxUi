# Paired opt-in ComplexUi heap census
Baseline HEAD: `fb857d44f093da3ad62a2cb0cf090e2b5bc13bef`; candidate HEAD: `1cb1188a25609a58fc0c8677c2ce29de9d52e234`.
baseline raw SHA-256: heaps `e1bd516a017e74d9b665d5b1f612ba8d0ce22497f91f4a2164b78e214f382b2b`, report `b0e1f7adc7cb9ba43527dcc69db8cabbfd28963ffdd58d7f8c9d80f72e4eb05d`.
candidate raw SHA-256: heaps `eb5175dafd223d73cf403c23eea4638598412fd99cd427c8afbeca4e8714400f`, report `649b9597fd1f53d9e4ee004d24b4e0824eaaed67f64e59ac54d64c10c9352c68`.
Both variants have nine ordered, error-free census records and matching diagnostic fixture source manifests. All quantities below are bytes except heap counts. Diagnostic-run timings are excluded.

### baseline: exact process-local phase bytes
| Phase | Heaps | Private | Heap busy | Heap free | Heap committed | Heap overhead | Private - busy | dPrivate | dBusy | d(Private - busy) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| entry | 5 | 2,060,288 | 241,129 | 63,760 | 380,928 | 22,215 | 1,819,159 | - | - | - |
| device | 5 | 6,647,808 | 576,582 | 56,512 | 720,896 | 28,282 | 6,071,226 | +4,587,520 | +335,453 | +4,252,067 |
| scene | 5 | 8,200,192 | 1,580,137 | 1,726,864 | 3,612,672 | 58,695 | 6,620,055 | +1,552,384 | +1,003,555 | +548,829 |
| warm | 5 | 22,581,248 | 7,991,076 | 2,233,136 | 10,694,656 | 132,988 | 14,590,172 | +14,381,056 | +6,410,939 | +7,970,117 |
| capture | 5 | 26,820,608 | 8,056,092 | 2,604,096 | 11,173,888 | 138,804 | 18,764,516 | +4,239,360 | +65,016 | +4,174,344 |
| after-clean | 5 | 27,983,872 | 7,639,874 | 4,121,904 | 12,324,864 | 143,470 | 20,343,998 | +1,163,264 | -416,218 | +1,579,482 |
| after-dirty | 5 | 29,110,272 | 8,178,708 | 4,505,840 | 13,344,768 | 156,252 | 20,931,564 | +1,126,400 | +538,834 | +587,566 |
| hidden | 5 | 29,110,272 | 8,176,128 | 4,508,400 | 13,344,768 | 156,272 | 20,934,144 | +0 | -2,580 | +2,580 |
| after-run-teardown | 5 | 23,228,416 | 1,137,151 | 11,164,688 | 13,344,768 | 101,121 | 22,091,265 | -5,881,856 | -7,038,977 | +1,157,121 |

baseline entry -> after-run-teardown: dPrivate +21,168,128 B; dHeap busy +896,022 B; dHeap free +11,100,928 B; dHeap committed +12,963,840 B; d(Private - busy) +20,272,106 B.
baseline hidden -> after-run-teardown: dPrivate -5,881,856 B; dHeap busy -7,038,977 B; dHeap free +6,656,288 B; dHeap committed +0 B; d(Private - busy) +1,157,121 B.

### candidate: exact process-local phase bytes
| Phase | Heaps | Private | Heap busy | Heap free | Heap committed | Heap overhead | Private - busy | dPrivate | dBusy | d(Private - busy) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| entry | 5 | 2,052,096 | 241,095 | 56,864 | 372,736 | 21,641 | 1,811,001 | - | - | - |
| device | 5 | 6,627,328 | 576,244 | 46,080 | 704,512 | 27,804 | 6,051,084 | +4,575,232 | +335,149 | +4,240,083 |
| scene | 5 | 8,192,000 | 1,579,799 | 1,931,952 | 3,825,664 | 58,825 | 6,612,201 | +1,564,672 | +1,003,555 | +561,117 |
| warm | 5 | 21,815,296 | 8,012,872 | 1,573,104 | 9,973,760 | 131,096 | 13,802,424 | +13,623,296 | +6,433,073 | +7,190,223 |
| capture | 5 | 26,087,424 | 8,103,850 | 1,965,200 | 10,452,992 | 134,918 | 17,983,574 | +4,272,128 | +90,978 | +4,181,150 |
| after-clean | 5 | 27,324,416 | 7,693,168 | 3,248,624 | 11,636,736 | 149,872 | 19,631,248 | +1,236,992 | -410,682 | +1,647,674 |
| after-dirty | 5 | 29,548,544 | 8,146,106 | 4,957,136 | 13,873,152 | 161,798 | 21,402,438 | +2,224,128 | +452,938 | +1,771,190 |
| hidden | 5 | 29,548,544 | 8,143,526 | 4,959,728 | 13,873,152 | 161,786 | 21,405,018 | +0 | -2,580 | +2,580 |
| after-run-teardown | 5 | 23,879,680 | 1,136,813 | 11,699,344 | 13,873,152 | 104,467 | 22,742,867 | -5,668,864 | -7,006,713 | +1,337,849 |

candidate entry -> after-run-teardown: dPrivate +21,827,584 B; dHeap busy +895,718 B; dHeap free +11,642,480 B; dHeap committed +13,500,416 B; d(Private - busy) +20,931,866 B.
candidate hidden -> after-run-teardown: dPrivate -5,668,864 B; dHeap busy -7,006,713 B; dHeap free +6,739,616 B; dHeap committed +0 B; d(Private - busy) +1,337,849 B.

### Candidate - baseline at corresponding phases (exact bytes)
| Phase | dPrivate | dHeap busy | dHeap free | dHeap committed | d(Private - busy) | Entry-normalized dPrivate | Entry-normalized dBusy |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| entry | -8,192 | -34 | -6,896 | -8,192 | -8,158 | +0 | +0 |
| device | -20,480 | -338 | -10,432 | -16,384 | -20,142 | -12,288 | -304 |
| scene | -8,192 | -338 | +205,088 | +212,992 | -7,854 | +0 | -304 |
| warm | -765,952 | +21,796 | -660,032 | -720,896 | -787,748 | -757,760 | +21,830 |
| capture | -733,184 | +47,758 | -638,896 | -720,896 | -780,942 | -724,992 | +47,792 |
| after-clean | -659,456 | +53,294 | -873,280 | -688,128 | -712,750 | -651,264 | +53,328 |
| after-dirty | +438,272 | -32,602 | +451,296 | +528,384 | +470,874 | +446,464 | -32,568 |
| hidden | +438,272 | -32,602 | +451,328 | +528,384 | +470,874 | +446,464 | -32,568 |
| after-run-teardown | +651,264 | -338 | +534,656 | +528,384 | +651,602 | +659,456 | -304 |

The phase and teardown tables exclude FPS and frame-time comparisons. Private - heap busy includes free/metadata within enumerated heaps and non-heap private memory; its movement is only the amount not explained by the HeapWalk busy-byte movement. Region-committed bytes can overlap busy/free entries and must not be added to them. Heap addresses were compared only between phases of the same process.
