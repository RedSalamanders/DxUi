# Radio group accessible names

The [same-fixture comparison](counterfactual.json) fails with the retained SDK and
passes with two candidate translation units. It queries the actual embedded Group
provider, updates its header, checks explicit-name precedence and restoration, then
verifies that the retained provider becomes unavailable on detach. All original
source, shared fixture object, executable/PDB and log members are independently verified.

The native Accessibility regression additionally subscribes a UIA client to Group
Name changes and checks unchanged-header suppression. The embedded suite repeats
the retained-provider and naming checks through Prepare/UpdateAccessibility. Those
complete-suite results pass in the [source-bound local qualification](qualification.json)
at `21f63d32`: six warning-free builds, 66 ordinary native x64 suite executions without
skips, and actual ASan detection. Every original source/native/run/failure-image
member is independently byte-verified. Six clean-source harness gallery captures
were regenerated and reviewed. ARM64 entries are cross-builds.

The first candidate probe closed its COM apartment before draining asynchronous
registration releases. Its retirement failure, dump, executable/PDB and source are
retained. The corrected shared fixture keeps COM alive through the existing shutdown
drain; no SDK retirement invariant was weakened. Physical assistive technology,
consumer adoption and formal paired-performance acceptance remain separate.
