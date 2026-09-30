# Grid slot-hash experiment, 29 September 2026

The review found that the multiline Grid's 32 direct-mapped text-layout slots take the low bits of `std::hash`, which
FNV-1a leaves nearly unmixed, so texts that differ by a counter crowd a few slots. A candidate spread the key by
Fibonacci hashing. This first paired set measured that candidate (B) against `40c6c21` (A), with the same trees,
machine and harness as the [final set](../paired-local/README.md).

- A1/A2 and B1/B2 are the candidate set. On the MultilineGrid fixture B kept the memory reduction of the CLIP change
  but made 1,083 dirty-round C++ allocations per 40 frames against A's 1,080 (+0.28%, deterministic in every round).
- `MultilineGrid-B-oldhash.json` is B rebuilt with only the original slot expression: 1,080 allocations and a 32.06 MB
  dirty private median, so the allocation growth was the hash and the memory reduction was not.

The fixture shows only about six distinct keys per frame (all four columns of a row share one text), which FNV happens
to spread. Because deterministic allocation budgets allow no growth, the hash change was reverted and left for a
developer decision against a fixture with distinct per-column text.

`SHA256SUMS` covers every file here except itself and this README.
