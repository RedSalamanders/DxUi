# Source snapshot qualification — 26 October 2026

The [CI 4077 native and paired evidence](ci-4077f57d/native-and-paired/README.md) binds six passing native profiles and the paired candidate `646b257a` to workflow source `4077f57d`, with equal source trees. CI is not green: the paired policy is unqualified; 62/72 controls are unstable and 163/1,872 observations exceed bands.

The [native and input packet](native-input-47a9/README.md) binds six hosted native profiles and four local
foreground suites to source `47a9d259`, merged in PR #105 as `6c3205cf` with the same reviewed tree. It retains
native ARM64 execution separately from unavailable ARM64 initial focus, and local automated input separately
from physical IME/AT qualification.

The [hosted paired packet](paired-47a9/README.md) preserves 144 reports, 78 metric rows and independent raw
arithmetic replay. There are no reported regression rows, but 71/72 controls are unstable and the provider
verdict is inconclusive because the base policy is unqualified. Neither integration nor successful native tests
closes performance acceptance or RedTilde's separate original-UI and hosted ARM64 gates.
