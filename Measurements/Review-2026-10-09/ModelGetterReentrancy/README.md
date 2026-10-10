# Model getter and action regressions

These are retained failing diagnostics from the accepted production review. They prove the named failure in an
intermediate uncommitted source state; they are not qualification receipts for the final candidate. Source changed
after each reproduction. The full old compiled-source closure and old executable hashes were not preserved, so
these logs must not be used to authorize scoped-result reuse or a before/after performance comparison.

| Retained log | Failure | Canonical x64 ASan Debug build ID |
|---|---|---|
| `Tree-root-replacement-ASan-red.log` | A reentrant item getter replaces the root; snapshot construction reads the retired Tree selection. | `b2a12bcd3b8b44518baf982457dce66b` |
| `Grid-root-replacement-ASan-red.log` | A reentrant cell getter replaces the root; snapshot construction reads the retired Grid selection. | `b2a12bcd3b8b44518baf982457dce66b` |
| `Empty-Grid-paint-cleanup-ASan-red.log` | The paint metrics scope cleanup dereferences a null borrowed model. | `33817305d74b4cbe9e594fe24cd38ced` |
| `Tree-notified-expansion-red.log` | An existing UI Automation expansion test rejects a successful delegate edit because `NotifyDataChanged` changes the query revision. | `ba997490c92942e7904eaac283db0587` |

The repairs abort unpublished getter transactions after retirement or model mutation, capture paint metrics before
cleanup, and distinguish a binding change from an action that publishes fresh data in the same binding. Final
qualification must cover the complete affected suites on exact current source, including the existing successful
selection, checkbox, expansion and stable-identity double-click behavior.

SHA-256 of the retained logs:

```text
C5839C3220731EBC94C0D4AFEA7F62589168FCF6FFC6AAEBA64027160D7E2362  Tree-root-replacement-ASan-red.log
748CE33912525E4BD9BD1BC4162119D09978EE0152368F406A4A09D65301EC40  Grid-root-replacement-ASan-red.log
1332208A0143E29CDD240F8F1018811A092026D8A9F70F80C95469324220CEF2  Empty-Grid-paint-cleanup-ASan-red.log
D1E5C9BB6D357B6CC6DA532C4408AAFB63BFBD53A5F468AB765538529CD4D2F4  Tree-notified-expansion-red.log
```
