# Release build-duration observations

| Before seconds | After seconds | Scope |
| ---: | ---: | --- |
| 365 | 346 | Build entry-point reported duration, one-second resolution |

Descriptive single-build observations only. Build caches, restore costs, concurrent workloads and clean/incremental state were not controlled across this pair. No build-time regression, improvement or compiler-version effect is established.

Both builds use MSVC 14.52.37025. Exact receipt SHA256 values and source paths are retained in build-duration-observations.json in this owner''s external report folder. These times do not replace the frozen paired runtime fixture experiment.

Captured command outcomes: before=True; after=True.
