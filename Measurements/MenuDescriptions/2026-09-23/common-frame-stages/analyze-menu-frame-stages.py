#!/usr/bin/env python3
"""Read-only, exact-round analysis of the opt-in A1,A2,B1,B2,A3 stage reports."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

DEFAULT_ROOT = Path("C:/RedSalamander.Perf/evidence/i26-ui/menu-frame-stages-20260923")
RUNS = ("A1", "A2", "B1", "B2", "A3")
STAGES = ("update", "prepare", "bind", "composeCpu", "gpuCompletion")
SCENARIOS = ("clean", "dirty")
FIXTURE_INPUTS = (
    "Tests/Embedded/BenchmarkMain.h",
    "Tests/Embedded/ComplexUiBenchmark.h",
    "Tests/Embedded/ComplexUiFrameStages.h",
    "Samples/ComplexUi/ComplexUiScene.h",
    "Samples/EmbeddedControls/GraphicsFixture.h",
)


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def number(value: object, where: str) -> float:
    require(type(value) in (int, float) and math.isfinite(value) and value >= 0, f"{where}: invalid nonnegative number")
    return float(value)


def load_provenance(root: Path, verify_current: bool) -> dict:
    values = {}
    for variant in ("A", "B"):
        folder = root / variant
        require((folder / "build-exit.txt").read_text(encoding="utf-8-sig").strip() == "0", f"{variant}: build failed")
        head = (folder / "head.txt").read_text(encoding="utf-8-sig").strip()
        require(len(head) == 40 and all(ch in "0123456789abcdefABCDEF" for ch in head), f"{variant}: invalid HEAD")
        manifest = json.loads((folder / "inputs.json").read_text(encoding="utf-8-sig"))
        require(type(manifest) is list and len(manifest) == len(FIXTURE_INPUTS), f"{variant}: fixture input count")
        paths = {item["path"]: item["sha256"].lower() for item in manifest}
        require(tuple(item["path"] for item in manifest) == FIXTURE_INPUTS, f"{variant}: fixture input paths/order")
        for rel, expected in paths.items():
            require(sha(folder / "inputs" / rel) == expected, f"{variant}: snapshot input hash mismatch: {rel}")
        binaries = json.loads((folder / "binary-hashes.json").read_text(encoding="utf-8-sig"))
        require(type(binaries) is list and len(binaries) == 2, f"{variant}: binary hash count")
        binary_hashes = {}
        for item in binaries:
            name = Path(item["Path"]).name
            expected = item["Hash"].lower()
            require(name in ("DxUi.EmbeddedTests.exe", "DxUi.lib") and len(expected) == 64, f"{variant}: binary hash entry")
            require(name not in binary_hashes, f"{variant}: duplicate binary entry")
            binary_hashes[name] = expected
            if verify_current and Path(item["Path"]).is_file():
                require(sha(Path(item["Path"])) == expected, f"{variant}: current binary differs from saved hash: {name}")
        require(set(binary_hashes) == {"DxUi.EmbeddedTests.exe", "DxUi.lib"}, f"{variant}: incomplete binary hashes")
        values[variant] = {
            "head": head,
            "inputs": paths,
            "binary_hashes": binary_hashes,
            "patch_sha256": sha(folder / "instrumentation.patch"),
        }
    require(values["A"]["inputs"] == values["B"]["inputs"], "A/B test and scene input hashes differ")
    require(values["A"]["patch_sha256"] == values["B"]["patch_sha256"], "A/B instrumentation patch bytes differ")
    require(values["A"]["binary_hashes"]["DxUi.EmbeddedTests.exe"] != values["B"]["binary_hashes"]["DxUi.EmbeddedTests.exe"],
            "A/B executable identity unexpectedly equal")
    return values


def load_runs(root: Path) -> dict:
    reports = {}
    for run in RUNS:
        require((root / f"{run}-exit.txt").read_text(encoding="utf-8-sig").strip() == "0", f"{run}: nonzero exit")
        path = root / f"{run}.json"
        report = json.loads(path.read_text(encoding="utf-8-sig"))
        for key, expected in (
            ("fixture", "dxui-complex-ui-frame-stages-v1"),
            ("renderer", "WARP"),
            ("width", 1280),
            ("height", 720),
            ("dpi", 96),
            ("controls", 83),
            ("modelRows", 1000),
            ("warmFrames", 20),
            ("framesPerRound", 40),
            ("roundCount", 5),
            ("hiddenPreparations", 0),
            ("hiddenComposites", 0),
        ):
            require(report.get(key) == expected, f"{run}: {key} mismatch")
        scenarios = report.get("scenarios")
        require(type(scenarios) is list and len(scenarios) == 2, f"{run}: scenario count")
        parsed = {}
        for scenario, expected_name in zip(scenarios, SCENARIOS, strict=True):
            require(scenario.get("name") == expected_name, f"{run}: scenario order")
            rounds = scenario.get("rounds")
            require(type(rounds) is list and len(rounds) == 5, f"{run}/{expected_name}: round count")
            for index, row in enumerate(rounds):
                where = f"{run}/{expected_name}/{index}"
                require(row.get("round") == index, f"{where}: round index")
                require(row.get("surfaceBytes") == 3_686_400, f"{where}: surface bytes")
                frame_total = number(row.get("frameTotalMs"), f"{where}/frameTotalMs")
                number(row.get("frameP95Ms"), f"{where}/frameP95Ms")
                stages = row.get("stages")
                require(type(stages) is dict and tuple(stages) == STAGES, f"{where}: stage names/order")
                stage_sum = 0.0
                for stage in STAGES:
                    require(type(stages[stage]) is dict, f"{where}/{stage}: expected object")
                    stage_sum += number(stages[stage].get("totalMs"), f"{where}/{stage}/totalMs")
                    number(stages[stage].get("p95Ms"), f"{where}/{stage}/p95Ms")
                require(abs(stage_sum - frame_total) <= 0.001, f"{where}: stage total fails to reconcile with frame total")
            parsed[expected_name] = rounds
        reports[run] = {"sha256": sha(path), "scenarios": parsed}
    return reports


def median_row(reports: dict, run: str, scenario: str, key: str, metric: str = "totalMs") -> float:
    rounds = reports[run]["scenarios"][scenario]
    if key == "frame":
        field = "frameTotalMs" if metric == "totalMs" else "frameP95Ms"
        return statistics.median(row[field] for row in rounds)
    return statistics.median(row["stages"][key][metric] for row in rounds)


def signed(value: float) -> str:
    return f"{value:+.4f}"


def print_analysis(root: Path, provenance: dict, reports: dict, verify_current: bool) -> None:
    print("# Opt-in common-scene frame-stage A1,A2,B1,B2,A3 analysis")
    print()
    print("Diagnostic timing only. No default-benchmark performance acceptance, menu resource waiver, or causal live-menu allocation claim.")
    print()
    print("## Identity")
    print()
    print(f"A HEAD {provenance['A']['head']}; B HEAD {provenance['B']['head']}.")
    print(f"Five saved fixture inputs match byte-for-byte between A and B; stage header SHA-256 "
          f"{provenance['A']['inputs']['Tests/Embedded/ComplexUiFrameStages.h']}; "
          f"instrumentation patch SHA-256 {provenance['A']['patch_sha256']}.")
    for variant in ("A", "B"):
        p = provenance[variant]
        print(f"{variant} saved Release executable SHA-256 {p['binary_hashes']['DxUi.EmbeddedTests.exe']}, "
              f"DxUi.lib {p['binary_hashes']['DxUi.lib']}.")
    if verify_current:
        print("Current executable/library files, where still present, match their saved hashes.")
    print("All five exits and both incremental-build exits are 0. Each report has 5 clean and 5 dirty 40-frame rounds, "
          "identical geometry/surface bytes, stage totals reconciling with frame totals, and zero hidden work.")
    print("The JSON reports do not embed executable hashes per invocation, and this evidence folder has no archived "
          "serial driver script. Saved build hashes and current post-run file hashes agree, but per-run executable "
          "identity and exact command order cannot be independently proven from the retained reports alone.")
    print()
    print("Raw report SHA-256:")
    for run in RUNS:
        print(f"- {run}: {reports[run]['sha256']}")
    print()

    print("## Exact per-round stage totals and p95")
    print()
    print("Each stage cell is total ms across 40 frames / within-round p95 ms. P95 values are not additive.")
    print()
    print("| Run | Scene | Round | Frame total | Frame p95 | Update total/p95 | Prepare total/p95 | Bind total/p95 | Compose CPU total/p95 | GPU completion total/p95 |")
    print("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for run in RUNS:
        for scenario in SCENARIOS:
            for row in reports[run]["scenarios"][scenario]:
                parts = [f"{row['stages'][stage]['totalMs']:.4f} / {row['stages'][stage]['p95Ms']:.4f}" for stage in STAGES]
                print(f"| {run} | {scenario} | {row['round']} | {row['frameTotalMs']:.4f} | {row['frameP95Ms']:.4f} | "
                      + " | ".join(parts) + " |")
    print()

    print("## Five-round medians")
    print()
    print("| Run | Scene | Frame total | Update | Prepare | Bind | Compose CPU | GPU completion | Frame p95 | GPU p95 | Prepare p95 |")
    print("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for scenario in SCENARIOS:
        for run in RUNS:
            vals = [median_row(reports, run, scenario, x) for x in STAGES]
            print(f"| {run} | {scenario} | {median_row(reports, run, scenario, 'frame'):.4f} | "
                  + " | ".join(f"{v:.4f}" for v in vals)
                  + f" | {median_row(reports, run, scenario, 'frame', 'p95Ms'):.4f} | "
                  + f"{median_row(reports, run, scenario, 'gpuCompletion', 'p95Ms'):.4f} | "
                  + f"{median_row(reports, run, scenario, 'prepare', 'p95Ms'):.4f} |")
    print()

    print("## Matched and same-binary median-total contrasts")
    print()
    print("B1-A2 and B2-A3 are adjacent crossover contrasts. A2-A1 and B2-B1 measure adjacent same-binary movement; "
          "A3-A2 shows the baseline's span across the candidate runs. Values are ms per 40-frame round.")
    print()
    print("| Scene | Contrast | Frame | Update | Prepare | Bind | Compose CPU | GPU completion |")
    print("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    contrasts = (("A2-A1", "A2", "A1"), ("B2-B1", "B2", "B1"), ("A3-A2", "A3", "A2"),
                 ("B1-A2", "B1", "A2"), ("B2-A3", "B2", "A3"), ("B1-A1", "B1", "A1"))
    for scenario in SCENARIOS:
        for label, later, earlier in contrasts:
            keys = ("frame",) + STAGES
            diffs = [signed(median_row(reports, later, scenario, key) - median_row(reports, earlier, scenario, key))
                     for key in keys]
            print(f"| {scenario} | {label} | " + " | ".join(diffs) + " |")
    print()

    print("## Interpretation")
    print()
    print("Clean adjacent candidate contrasts reverse sign: B1-A2 frame +1.2042 ms and B2-A3 -0.1476 ms; "
          "their GPU-completion components are +1.1247 and -0.1223 ms. Adjacent A/A clean frame movement is "
          "-5.7590 ms and B/B is -2.7440 ms. Dirty candidate contrasts also reverse sign: B1-A2 -0.9480 ms "
          "and B2-A3 +1.1081 ms; Prepare changes -1.8222 and +1.0572 ms. Same-binary dirty A/A shifts "
          "-12.5128 ms and B/B -3.3117 ms. The stage shifts do not meet the predeclared repeated-sign and "
          "greater-than-same-binary-spread criteria. GPU completion dominates clean rounds; Prepare and GPU completion "
          "dominate dirty rounds. This localizes the run-order drift, but does not establish a repeatable candidate slowdown.")
    print()
    print("The earlier heap-phase pair independently showed extra candidate free/committed heap capacity after dirty "
          "rounds, while live heap busy was slightly lower and the changed menu-layout map is not entered by this scene. "
          "That supports retained allocator capacity in one pair, not a causal menu allocation or settled memory budget. "
          "Keep the original uninstrumented benchmark flags and the user's scoped resource decisions unchanged.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--verify-current", action="store_true",
                        help="Also hash currently present executable/library paths; omit for archived portability")
    args = parser.parse_args()
    try:
        provenance = load_provenance(args.root, args.verify_current)
        reports = load_runs(args.root)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"INVALID FRAME-STAGE EVIDENCE: {exc}", file=sys.stderr)
        return 2
    print_analysis(args.root, provenance, reports, args.verify_current)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
