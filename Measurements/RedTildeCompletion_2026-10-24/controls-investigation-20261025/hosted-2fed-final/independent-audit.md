# Independent replay record

The replay was performed independently against the provider packet for this run. It matched every receipt digest, recomputed the same-side control flags from the raw receipt metrics, and independently recomputed paired round effects, p-values and Holm adjustments. This record summarizes the audit; the original receipts and host-specific collection files remain in private audit custody.

```json
{
  "providerRun": 40222621151,
  "prHead": "2fed82465dcee9f213753754e0e8e54f353cf38f",
  "measuredMerge": "9ac8cc0bb857a0a71f0ea0e94ef3a51c69be7371",
  "measuredBase": "4faf16ed647df71cd7d5623c1682738592771c3e",
  "providerArchiveSha256": "0BE9B0CF5837D64831988B247E45343E067307D1E2C125E829DE1AA2BB4DC545",
  "baselineExecutableSha256": "8306E4416B733B45966D6622FA462AF7DBF96843ABC0F7F92928472AC291DFF7",
  "candidateExecutableSha256": "2DFD6F8FBD50BAD75F753E443830F42170936D5FF96EADFC553917F5BEC6C02E",
  "receiptsMatched": 144,
  "pairedEffectsRecomputed": 936,
  "metricRowsRecomputed": 78,
  "sameSideControls": 72,
  "unstableControls": 71,
  "metricControlObservations": 1872,
  "driftedObservations": 260,
  "heldObservations": 1612,
  "keysHeldAll24": 37,
  "keysDriftedAll24": 0,
  "providerRegressions": 0,
  "providerVerdict": "inconclusive: base policy not independently qualified"
}
```

The row-level comparison and CSV preserve the exact staged projection. The replay validates recorded arithmetic and integrity; it does not qualify the base policy, establish a quiet host, or attribute timing variation to a specific process or mechanism. No source, fixture, threshold, band, workload metric, or acceptance rule was changed for this audit.
