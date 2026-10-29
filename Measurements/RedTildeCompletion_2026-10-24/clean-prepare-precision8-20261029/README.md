# Clean Prepare precision diagnostic — 29 October 2026

**Status:** reviewed diagnostic evidence. This is a separate diagnostic summary, not a qualification or performance-acceptance result.

## Bindings

- Native source: `8ccb8a148b392be3712d4c0d6048dfeb1fd0be94`; metadata head: `c1c207a8e1038789241ca4bc975e72581b2d9616`.
- Timed artifact: x64 Release `DxUi.EmbeddedTests.exe`, SHA-256 `964D743346F0514D4F9ACB0672CA99AAE61E413C76061C4CC7641F8769EE1611`; PDB SHA-256 `6979FAE80BC4EB0EC92CDC45151AB89A475688842EDFE2AD409BB888407EC9B0`.
- Archived binary/PDB members are in the compressed preservation archive (SHA-256 `1F5249B8E2D22D04E0668D6E75A0BA7C8A6691B06FAD26FB2152310FD322AA9C`); preservation manifest SHA-256 `2509F98875E08692832DC70A58664F20717A1A5D52C212C6EDE59BBF396A6160`.
- Controller SHA-256: `639FFD3EE6B62DC9B9A676DE0BC936BF45FF028E197EA063698163E7F913A15A`; offline auditor SHA-256: `193F21B8EFE0ABC21605DDCD82D8245BA788267F2FFF65807F965A4C0028116A`; independent replay SHA-256: `B11B31BF79E32E58D12BFE74E0DC3BC250FF783CF97ECFDC848D67A4C5E6E3D9`.

## Protocol and clock

- Eight sequential process replicates used seed 20261029. Each process recorded 128 samples: batch sizes 1, 8, 64, and 512, with 16 repetitions per size and alternating preparation/control order.
- Each process executed 9,360 timed Prepare calls and 9,360 empty-loop calls. Every process exited 0 and none timed out.
- QPC frequency was 10 MHz (100 ns per tick). Dividing one tick across 512 calls gives 0.1953125 ns/call arithmetic granularity; this is not a measurement-accuracy claim.
- For each process and size, the table reports the median and min–max of 16 normalized batch means. Prepare and empty-loop controls are reported separately.

## Per-process results

Times are nanoseconds per call. The empty-loop control is not subtracted from Prepare. Each row is one process replicate; values are not pooled.

### Process 1

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 18.750000 | 12.500000–62.500000 | 16 |
| 64 | 15.625000 | 15.625000–17.187500 | 16 |
| 512 | 16.015625 | 15.820312–16.601562 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 0.000000 | 0.000000–1.562500 | 16 |
| 512 | 0.390625 | 0.195312–41.601562 | 16 |

### Process 2

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–500.000000 | 16 |
| 8 | 25.000000 | 12.500000–37.500000 | 16 |
| 64 | 17.187500 | 15.625000–45.312500 | 16 |
| 512 | 16.210938 | 16.015625–25.781250 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 1.562500 | 0.000000–1.562500 | 16 |
| 512 | 0.390625 | 0.195312–0.585938 | 16 |

### Process 3

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–400.000000 | 16 |
| 8 | 18.750000 | 12.500000–87.500000 | 16 |
| 64 | 16.406250 | 15.625000–32.812500 | 16 |
| 512 | 16.015625 | 15.820312–68.750000 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 50.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 1.562500 | 0.000000–1.562500 | 16 |
| 512 | 0.390625 | 0.195312–0.585938 | 16 |

### Process 4

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–500.000000 | 16 |
| 8 | 12.500000 | 12.500000–25.000000 | 16 |
| 64 | 15.625000 | 15.625000–18.750000 | 16 |
| 512 | 16.015625 | 15.820312–23.046875 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 1.562500 | 0.000000–1.562500 | 16 |
| 512 | 0.390625 | 0.195312–0.390625 | 16 |

### Process 5

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 50.000000 | 0.000000–300.000000 | 16 |
| 8 | 25.000000 | 12.500000–25.000000 | 16 |
| 64 | 15.625000 | 15.625000–21.875000 | 16 |
| 512 | 16.015625 | 15.820312–18.554688 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 0.000000 | 0.000000–1.562500 | 16 |
| 512 | 0.390625 | 0.195312–0.390625 | 16 |

### Process 6

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 100.000000 | 0.000000–100.000000 | 16 |
| 8 | 12.500000 | 12.500000–25.000000 | 16 |
| 64 | 15.625000 | 15.625000–18.750000 | 16 |
| 512 | 16.015625 | 15.820312–20.703125 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 0.000000 | 0.000000–1.562500 | 16 |
| 512 | 0.390625 | 0.195312–0.390625 | 16 |

### Process 7

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 100.000000 | 0.000000–100.000000 | 16 |
| 8 | 18.750000 | 12.500000–25.000000 | 16 |
| 64 | 17.187500 | 15.625000–17.187500 | 16 |
| 512 | 16.210938 | 16.210938–16.406250 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 1.562500 | 0.000000–1.562500 | 16 |
| 512 | 0.195312 | 0.195312–0.390625 | 16 |

### Process 8

**Prepare**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 100.000000 | 0.000000–400.000000 | 16 |
| 8 | 25.000000 | 12.500000–25.000000 | 16 |
| 64 | 15.625000 | 15.625000–17.187500 | 16 |
| 512 | 16.015625 | 15.820312–16.406250 | 16 |

**Empty-loop control**

| Batch calls | Median | Min–max | n |
|---:|---:|---:|---:|
| 1 | 0.000000 | 0.000000–100.000000 | 16 |
| 8 | 0.000000 | 0.000000–12.500000 | 16 |
| 64 | 0.000000 | 0.000000–1.562500 | 16 |
| 512 | 0.195312 | 0.195312–0.390625 | 16 |

The largest observed 512-call Prepare range includes process 3 at 68.750000 ns/call; the largest 512-call empty-loop range includes process 1 at 41.601562 ns/call. These outliers remain visible in the ranges.

## Interpretation limits

- The Prepare interval includes HRESULT checks, S_FALSE classification, and loop bookkeeping. S_FALSE is enforced by the producer code and successful exit; the raw report does not serialize every HRESULT.
- Empty-loop results are a separate context, not matched-call subtraction. No p-values, pooling, policy thresholds, or canonical metric changes are applied.
- The diagnostic does not observe loaded module identities, the actual renderer, GPU state, or quiet-host state. It does not establish a global paired-control fix or performance acceptance.
- The normalized batch means are not isolated method latency and do not replace canonical per-call P95. The earlier 600 ns P95 result is not directly comparable, and this report does not attribute the difference to a single cause.

## Published encoding

The `*.json.template` files contain literal reviewed JSON projections and their byte manifest. This follows the existing qualification-packet convention and distinguishes these derived summaries from native runner measurement receipts; there are no placeholders to fill. The raw reports, receipts and sealed audits remain unchanged.
