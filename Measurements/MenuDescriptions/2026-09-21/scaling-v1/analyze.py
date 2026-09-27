"""Summarize whole-process deltas; these are not allocation attribution."""
import json
import statistics
import sys
from pathlib import Path

source = Path(sys.argv[1])
rows = []
for line in source.read_text(encoding="utf-8-sig").splitlines():
    if line.startswith('{"fixture":"dxui-menu-resource-scaling-v1"'):
        rows.append(json.loads(line))
samples = {}
for row in rows:
    if "phase" in row:
        samples.setdefault((row["entries"], row["descriptions"], row["cycle"]), {})[row["phase"]] = row
if not samples:
    raise SystemExit("No scaling samples found")
assert len({key[:2] for key in samples}) == 10
summary = []
for entries, descriptions in sorted({key[:2] for key in samples}):
    cycles = [(key[2], phases) for key, phases in samples.items() if key[:2] == (entries, descriptions)]
    assert len(cycles) == 32, (entries, descriptions, len(cycles))
    assert all(set(phases) == {"before", "rendered", "closed"} for _, phases in cycles)
    result = {"entries": entries, "descriptions": descriptions}
    for period, selected in (("all32", cycles), ("last16", [(c, p) for c, p in cycles if c >= 16])):
        values = {}
        for metric in ("privateBytes", "workingSetBytes", "handles", "gdi", "user"):
            for phase in ("rendered", "closed"):
                deltas = [p[phase][metric] - p["before"][metric] for _, p in selected]
                values[f"{phase}-{metric}"] = {"median": statistics.median(deltas), "min": min(deltas), "max": max(deltas)}
        result[period] = values
    summary.append(result)
output = {"source": str(source), "metadata": [r for r in rows if "phase" not in r], "variants": summary}
source.with_suffix(".summary.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
print("entries descriptions open-private-median closed-private-median (last16, bytes)")
for row in summary:
    print(row["entries"], row["descriptions"], row["last16"]["rendered-privateBytes"]["median"], row["last16"]["closed-privateBytes"]["median"])
print(json.dumps(output["metadata"], indent=2))
