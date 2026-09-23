"""Summarize whole-process deltas; these are not allocation attribution."""
import json
import statistics
import sys
from pathlib import Path

source = Path(sys.argv[1])
rows = []
for line in source.read_text(encoding="utf-8-sig").splitlines():
    if line.startswith('{"fixture":"dxui-menu-resource-scaling-v4"'):
        rows.append(json.loads(line))
geometry = [r for r in rows if 'widthPx' in r]
assert len(geometry) == 320, len(geometry)
assert len({(r['widthPx'], r['heightPx'], r['dpi']) for r in geometry}) == 1
assert all(r['widthPx'] > 1 and r['heightPx'] > 1 and r['dpi'] > 0 for r in geometry)
expected_variants = {(12, n) for n in (0, 1, 2, 4, 8, 12)} | {(24, 0), (24, 24), (48, 0), (48, 48)}
expected_keys = {(e, d, c) for e, d in expected_variants for c in range(32)}
assert {(r['entries'], r['descriptions'], r['cycle']) for r in geometry} == expected_keys
for row in rows:
    if "phase" in row:
        assert row.get("heaps"), "Missing heap accounting"
        assert all(h["error"] == 0 for h in row["heaps"]), "Partial heap walk cannot establish totals"
        for metric in ("busyBytes", "freeBytes", "entryOverheadBytes"):
            row["heap" + metric] = sum(h[metric] for h in row["heaps"])
samples = {}
for row in rows:
    if "phase" in row:
        phases = samples.setdefault((row["entries"], row["descriptions"], row["cycle"]), {})
        assert row['phase'] not in phases, 'Duplicate phase'
        phases[row['phase']] = row
assert set(samples) == expected_keys
if not samples:
    raise SystemExit("No scaling samples found")
summary = []
for entries, descriptions in sorted({key[:2] for key in samples}):
    cycles = [(key[2], phases) for key, phases in samples.items() if key[:2] == (entries, descriptions)]
    assert len(cycles) == 32, (entries, descriptions, len(cycles))
    assert all(set(phases) == {"before", "rendered", "closed"} for _, phases in cycles)
    result = {"entries": entries, "descriptions": descriptions}
    for period, selected in (("all32", cycles), ("last16", [(c, p) for c, p in cycles if c >= 16])):
        values = {}
        for metric in ("privateBytes", "workingSetBytes", "handles", "gdi", "user", "heapbusyBytes", "heapfreeBytes", "heapentryOverheadBytes"):
            for phase in ("rendered", "closed", "released"):
                deltas = [p["rendered"][metric] - p["closed"][metric] if phase == "released" else p[phase][metric] - p["before"][metric] for _, p in selected]
                values[f"{phase}-{metric}"] = {"median": statistics.median(deltas), "min": min(deltas), "max": max(deltas)}
        result[period] = values
    summary.append(result)
output = {"source": str(source), "metadata": [r for r in rows if "phase" not in r], "variants": summary}
source.with_suffix(".summary.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
print("entries descriptions open-private-median closed-private-median (last16, bytes)")
for row in summary:
    print(row["entries"], row["descriptions"], row["last16"]["rendered-privateBytes"]["median"], row["last16"]["closed-privateBytes"]["median"])
print("entries descriptions open-busy-median open-free-median closed-busy-median (last16, bytes)")
for row in summary:
    print(row["entries"], row["descriptions"], row["last16"]["rendered-heapbusyBytes"]["median"], row["last16"]["rendered-heapfreeBytes"]["median"], row["last16"]["closed-heapbusyBytes"]["median"])
print(json.dumps(output["metadata"], indent=2))
print("entries descriptions released-busy-median released-private-median (last16, bytes)")
for row in summary:
    print(row["entries"], row["descriptions"], row["last16"]["released-heapbusyBytes"]["median"], row["last16"]["released-privateBytes"]["median"])

