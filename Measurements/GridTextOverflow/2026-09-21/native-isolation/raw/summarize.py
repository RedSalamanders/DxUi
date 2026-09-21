import json
import statistics
from pathlib import Path

root = Path(__file__).resolve().parent
rows = []
for name in ["v11-control", "no-draw", "no-trimming", "bounded-inline", "bounded-marker-paced"]:
    data = json.loads((root / (name + ".json")).read_text(encoding="utf-8-sig"))
    dirty = next(s["rounds"] for s in data["scenarios"] if s["name"] == "dirty")
    row = dict(name=name, fixture=data["fixture"], benchmarkSha256=data["benchmarkSha256"],
               fps=statistics.median(r["fps"] for r in dirty),
               roundPrivateMedian=statistics.median(r["privateBytes"] for r in dirty),
               cppAllocations=statistics.median(r["cppAllocations"] for r in dirty))
    if "retention" in data:
        scroll = [r for r in data["retention"] if r["phase"] == "scroll"]
        row.update(privateMedian=statistics.median(r["privateBytes"] for r in scroll),
                   privatePeak=max(r["privateBytes"] for r in scroll),
                   privateEnd=scroll[-1]["privateBytes"],
                   busyMedian=statistics.median(sum(h["busyBytes"] for h in r["heaps"]) for r in scroll),
                   freeMedian=statistics.median(sum(h["freeBytes"] for h in r["heaps"]) for r in scroll),
                   heapErrors=sum(h["error"] != 0 for r in data["retention"] + [data["detached"]] for h in r["heaps"]),
                   seconds=scroll[-1]["elapsedMs"] / 1000)
    rows.append(row)
(root / "summary.txt").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
print(json.dumps(rows, indent=2))
