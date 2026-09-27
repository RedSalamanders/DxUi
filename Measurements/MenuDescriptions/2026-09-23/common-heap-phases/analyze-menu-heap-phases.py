#!/usr/bin/env python3
"""Read-only analyzer for the paired opt-in ComplexUi heap phase census.

This diagnostic excludes invalid censuses and never compares heap addresses
between the two separate processes. It deliberately does not analyze FPS.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


PHASES = (
    "entry",
    "device",
    "scene",
    "warm",
    "capture",
    "after-clean",
    "after-dirty",
    "hidden",
    "after-run-teardown",
)
REPORT_PHASES = ("entry", "device", "scene", "warm", "capture", "hidden")
HEAP_FIELDS = (
    "busyBytes",
    "freeBytes",
    "entryOverheadBytes",
    "regionCommittedBytes",
    "regionUncommittedBytes",
)
DEFAULT_ROOT = Path("C:/RedSalamander.Perf/evidence/i26-ui/menu-heap-phases-20260923")


class InvalidCensus(ValueError):
    pass


def need(condition: bool, message: str) -> None:
    if not condition:
        raise InvalidCensus(message)


def uint(value: object, where: str) -> int:
    need(type(value) is int and 0 <= value <= (1 << 64) - 1, f"{where}: expected unsigned 64-bit integer")
    return value


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_variant(folder: Path) -> dict:
    name = folder.name
    for marker in ("build-exit.txt", "probe-exit.txt"):
        path = folder / marker
        need(path.is_file(), f"{name}: missing {marker}")
        need(path.read_text(encoding="utf-8-sig").strip() == "0", f"{name}: {marker} is not zero")

    sidecar = folder / "heaps.jsonl"
    report_path = folder / "report.json"
    need(sidecar.is_file() and report_path.is_file(), f"{name}: missing heap sidecar or benchmark report")
    lines = sidecar.read_text(encoding="utf-8-sig").splitlines()
    need(len(lines) == len(PHASES), f"{name}: expected nine JSONL phase lines, got {len(lines)}")

    phases = []
    previous_handles: set[int] = set()
    for index, (line, phase) in enumerate(zip(lines, PHASES, strict=True)):
        try:
            raw = json.loads(line)
        except json.JSONDecodeError as exc:
            raise InvalidCensus(f"{name}: invalid JSONL line {index + 1}: {exc}") from exc
        need(type(raw) is dict, f"{name}/{phase}: expected object")
        need(raw.get("fixture") == "dxui-complex-ui-heap-phases-v1", f"{name}/{phase}: fixture mismatch")
        need(raw.get("sample") == index and type(raw.get("sample")) is int, f"{name}/{phase}: sample/order mismatch")
        need(raw.get("phase") == phase, f"{name}/{phase}: phase/order mismatch")
        heaps = raw.get("heaps")
        need(type(heaps) is list and 1 <= len(heaps) <= 128, f"{name}/{phase}: invalid heap count")
        totals = {key: 0 for key in HEAP_FIELDS}
        handles: set[int] = set()
        for heap_index, heap in enumerate(heaps):
            where = f"{name}/{phase}/heap{heap_index}"
            need(type(heap) is dict, f"{where}: expected object")
            handle = uint(heap.get("heap"), f"{where}/heap")
            need(handle != 0 and handle not in handles, f"{where}: null or duplicate heap handle")
            handles.add(handle)
            need(uint(heap.get("error"), f"{where}/error") == 0, f"{where}: HeapLock/HeapWalk error")
            for key in HEAP_FIELDS:
                totals[key] += uint(heap.get(key), f"{where}/{key}")
        private = uint(raw.get("privateBytes"), f"{name}/{phase}/privateBytes")
        working = uint(raw.get("workingSetBytes"), f"{name}/{phase}/workingSetBytes")
        phases.append(
            {
                "phase": phase,
                "sample": index,
                "heapCount": len(heaps),
                "heapsAddedSincePrevious": len(handles - previous_handles) if index else None,
                "heapsRemovedSincePrevious": len(previous_handles - handles) if index else None,
                "privateBytes": private,
                "workingSetBytes": working,
                **totals,
                # This includes heap free/metadata and non-heap process private
                # allocations; it is not an ownership attribution.
                "privateMinusHeapBusyBytes": private - totals["busyBytes"],
            }
        )
        previous_handles = handles

    try:
        report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        raise InvalidCensus(f"{name}: invalid report JSON: {exc}") from exc
    need(type(report) is dict and report.get("fixture") == "dxui-complex-ui-v2", f"{name}: report fixture mismatch")
    scenarios = report.get("scenarios")
    need(type(scenarios) is list and len(scenarios) == 2, f"{name}: expected two benchmark scenarios")
    for scenario, expected in zip(scenarios, ("clean", "dirty"), strict=True):
        need(type(scenario) is dict and scenario.get("name") == expected, f"{name}: scenario order mismatch")
        need(type(scenario.get("rounds")) is list and len(scenario["rounds"]) == 5,
             f"{name}/{expected}: expected five rounds")
    report_phases = report.get("memoryPhases")
    need(type(report_phases) is list and len(report_phases) == len(REPORT_PHASES)
         and all(type(item) is dict for item in report_phases)
         and tuple(item.get("name") for item in report_phases) == REPORT_PHASES,
         f"{name}: report memory phase mismatch")
    need(report.get("hiddenPreparations") == 0 and report.get("hiddenComposites") == 0,
         f"{name}: hidden work did not remain zero")

    inputs_path = folder / "inputs.json"
    need(inputs_path.is_file(), f"{name}: missing inputs.json")
    inputs = json.loads(inputs_path.read_text(encoding="utf-8-sig"))
    need(type(inputs) is list and len(inputs) == 5 and all(type(item) is dict and type(item.get("path")) is str
                                      and type(item.get("sha256")) is str for item in inputs),
         f"{name}: malformed source input manifest")
    need(len({item["path"] for item in inputs}) == len(inputs)
         and all(len(item["sha256"]) == 64 and all(ch in "0123456789abcdefABCDEF" for ch in item["sha256"])
                 for item in inputs), f"{name}: duplicate path or malformed source hash")
    head_path = folder / "head.txt"
    need(head_path.is_file(), f"{name}: missing head.txt")
    head = head_path.read_text(encoding="utf-8-sig").strip()
    need(len(head) == 40 and all(ch in "0123456789abcdefABCDEF" for ch in head),
         f"{name}: malformed HEAD")
    return {
        "name": name,
        "head": head,
        "inputs": {item["path"]: item["sha256"].lower() for item in inputs},
        "phases": phases,
        "sidecarSha256": sha256(sidecar),
        "reportSha256": sha256(report_path),
    }


def delta(a: dict, b: dict, key: str) -> int:
    return a[key] - b[key]


def signed(number: int) -> str:
    return f"{number:+,}"


def print_variant(value: dict) -> None:
    name = value["name"]
    print(f"### {name}: exact process-local phase bytes")
    print("| Phase | Heaps | Private | Heap busy | Heap free | Heap committed | Heap overhead | Private - busy | dPrivate | dBusy | d(Private - busy) |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    previous = None
    for row in value["phases"]:
        dp = db = dr = "-"
        if previous is not None:
            dp = signed(delta(row, previous, "privateBytes"))
            db = signed(delta(row, previous, "busyBytes"))
            dr = signed(delta(row, previous, "privateMinusHeapBusyBytes"))
        print(f"| {row['phase']} | {row['heapCount']} | {row['privateBytes']:,} | {row['busyBytes']:,} | "
              f"{row['freeBytes']:,} | {row['regionCommittedBytes']:,} | {row['entryOverheadBytes']:,} | "
              f"{row['privateMinusHeapBusyBytes']:,} | {dp} | {db} | {dr} |")
        previous = row
    print()
    for start, end in ((0, 8), (7, 8)):
        a, b = value["phases"][start], value["phases"][end]
        print(f"{name} {a['phase']} -> {b['phase']}: "
              f"dPrivate {signed(delta(b, a, 'privateBytes'))} B; "
              f"dHeap busy {signed(delta(b, a, 'busyBytes'))} B; "
              f"dHeap free {signed(delta(b, a, 'freeBytes'))} B; "
              f"dHeap committed {signed(delta(b, a, 'regionCommittedBytes'))} B; "
              f"d(Private - busy) {signed(delta(b, a, 'privateMinusHeapBusyBytes'))} B.")
    print()


def print_pair(baseline: dict, candidate: dict) -> None:
    print("### Candidate - baseline at corresponding phases (exact bytes)")
    print("| Phase | dPrivate | dHeap busy | dHeap free | dHeap committed | d(Private - busy) | Entry-normalized dPrivate | Entry-normalized dBusy |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    b0, c0 = baseline["phases"][0], candidate["phases"][0]
    for b, c in zip(baseline["phases"], candidate["phases"], strict=True):
        entry_private = delta(c, c0, "privateBytes") - delta(b, b0, "privateBytes")
        entry_busy = delta(c, c0, "busyBytes") - delta(b, b0, "busyBytes")
        print(f"| {b['phase']} | {signed(delta(c, b, 'privateBytes'))} | {signed(delta(c, b, 'busyBytes'))} | "
              f"{signed(delta(c, b, 'freeBytes'))} | {signed(delta(c, b, 'regionCommittedBytes'))} | "
              f"{signed(delta(c, b, 'privateMinusHeapBusyBytes'))} | {signed(entry_private)} | {signed(entry_busy)} |")
    print()
    print("The phase and teardown tables exclude FPS and frame-time comparisons. "
          "Private - heap busy includes free/metadata within enumerated heaps and non-heap private memory; "
          "its movement is only the amount not explained by the HeapWalk busy-byte movement. "
          "Region-committed bytes can overlap busy/free entries and must not be added to them. "
          "Heap addresses were compared only between phases of the same process.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    args = parser.parse_args()
    try:
        baseline = parse_variant(args.root / "baseline")
        candidate = parse_variant(args.root / "candidate")
        need(baseline["inputs"] == candidate["inputs"], "baseline/candidate diagnostic fixture source manifests differ")
    except (OSError, InvalidCensus, ValueError) as exc:
        print(f"INVALID CENSUS: {exc}", file=sys.stderr)
        return 2
    print("# Paired opt-in ComplexUi heap census")
    print(f"Baseline HEAD: `{baseline['head']}`; candidate HEAD: `{candidate['head']}`.")
    for value in (baseline, candidate):
        print(f"{value['name']} raw SHA-256: heaps `{value['sidecarSha256']}`, "
              f"report `{value['reportSha256']}`.")
    print("Both variants have nine ordered, error-free census records and matching diagnostic fixture source manifests. "
          "All quantities below are bytes except heap counts. Diagnostic-run timings are excluded.\n")
    print_variant(baseline)
    print_variant(candidate)
    print_pair(baseline, candidate)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
