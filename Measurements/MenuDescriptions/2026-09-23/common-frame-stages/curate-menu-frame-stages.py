"""Draft curator for the opt-in DxUi common-frame-stage diagnostic packet.

Review before execution. This script was not executed by its author. It checks
the exact captured source inventory before making a staging directory and never
removes or overwrites an existing packet or staging directory.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import tempfile
from pathlib import Path


EVIDENCE = Path("C:/RedSalamander.Perf/evidence/i26-ui/menu-frame-stages-20260923")
DRIVER = Path("C:/RedSalamander.Perf/evidence/i26-ui/run-menu-stages-20260923.ps1")
TARGET = Path(
    "Z:/src/DxUi-worktrees/i26-menu-description/Measurements/MenuDescriptions/"
    "2026-09-23/common-frame-stages"
)
INVENTORY_SHA256 = "3b19fc49b00660a6cb19c298d1d7964bc0de3e01922e800e7cc1f352f43916df"
RUNS = ("A1", "A2", "B1", "B2", "A3")
INPUTS = (
    "Tests/Embedded/BenchmarkMain.h",
    "Tests/Embedded/ComplexUiBenchmark.h",
    "Tests/Embedded/ComplexUiFrameStages.h",
    "Samples/ComplexUi/ComplexUiScene.h",
    "Samples/EmbeddedControls/GraphicsFixture.h",
)
STAGES = {"update", "prepare", "bind", "composeCpu", "gpuCompletion"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_lf(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8"))


def source_for(label: str, evidence: Path, driver: Path, base: Path) -> Path:
    if label == "@driver/run-menu-stages-20260923.ps1":
        return driver
    if label in ("@analysis/analyze-menu-frame-stages.py", "@analysis/menu-frame-stages-analysis.md"):
        return base / label.removeprefix("@analysis/")
    require(not label.startswith("@") and ".." not in Path(label).parts, f"unsafe label: {label}")
    return evidence / label


def archive_for(label: str) -> str:
    if label.startswith("@driver/"):
        return "raw/" + label.removeprefix("@driver/")
    if label == "@analysis/analyze-menu-frame-stages.py":
        return "analyze-menu-frame-stages.py"
    if label == "@analysis/menu-frame-stages-analysis.md":
        return "analysis-original.md"
    return "raw/" + (label + ".receipt.txt" if label.endswith(".json") else label)


def preflight(evidence: Path, driver: Path, inventory_path: Path, base: Path) -> list[dict]:
    require(sha256(inventory_path) == INVENTORY_SHA256, "source inventory changed")
    entries = read_json(inventory_path)
    require(isinstance(entries, list) and len(entries) == 40, "unexpected source inventory count")
    labels = [entry["source"] for entry in entries]
    require(len(labels) == len(set(labels)), "duplicate inventory name")
    actual = {p.relative_to(evidence).as_posix() for p in evidence.rglob("*") if p.is_file()}
    listed = {label for label in labels if not label.startswith("@")}
    require(actual == listed, f"raw file set changed: missing={sorted(listed-actual)}, extra={sorted(actual-listed)}")
    for entry in entries:
        path = source_for(entry["source"], evidence, driver, base)
        require(path.is_file(), f"missing: {path}")
        require(path.stat().st_size == entry["size"], f"size changed: {path}")
        require(sha256(path) == entry["sha256"], f"SHA-256 changed: {path}")

    require((evidence / "A/inputs.json").read_bytes() == (evidence / "B/inputs.json").read_bytes(), "A/B input manifests differ")
    require((evidence / "A/instrumentation.patch").read_bytes() == (evidence / "B/instrumentation.patch").read_bytes(), "A/B instrumentation differs")
    for variant in "AB":
        vdir = evidence / variant
        require((vdir / "build-exit.txt").read_text(encoding="utf-8-sig").strip() == "0", f"{variant} build exit nonzero")
        head = (vdir / "head.txt").read_text(encoding="utf-8-sig").strip()
        require(re.fullmatch(r"[0-9a-fA-F]{40}", head) is not None, f"invalid {variant} HEAD")
        manifest = read_json(vdir / "inputs.json")
        require([item["path"] for item in manifest] == list(INPUTS), f"{variant} fixture input order/set changed")
        for item in manifest:
            require(sha256(vdir / "inputs" / item["path"]) == item["sha256"], f"{variant} fixture input changed: {item['path']}")
        binaries = read_json(vdir / "binary-hashes.json")
        require(len(binaries) == 2, f"{variant} binary receipt count")
        names = {Path(item["Path"]).name for item in binaries}
        require(names == {"DxUi.EmbeddedTests.exe", "DxUi.lib"}, f"{variant} binary receipt names")
        for item in binaries:
            require(item["Algorithm"] == "SHA256" and re.fullmatch(r"[0-9a-fA-F]{64}", item["Hash"]) is not None, f"{variant} binary receipt malformed")

    signatures = []
    for run in RUNS:
        require((evidence / f"{run}-exit.txt").read_text(encoding="utf-8-sig").strip() == "0", f"{run} exit nonzero")
        report = read_json(evidence / f"{run}.json")
        signature = tuple(report[key] for key in ("fixture", "renderer", "width", "height", "dpi", "controls", "modelRows", "warmFrames", "framesPerRound", "roundCount"))
        signatures.append(signature)
        require(signature == ("dxui-complex-ui-frame-stages-v1", "WARP", 1280, 720, 96, 83, 1000, 20, 40, 5), f"{run} fixture changed")
        require(report["hiddenPreparations"] == 0 and report["hiddenComposites"] == 0, f"{run} hidden work")
        require([s["name"] for s in report["scenarios"]] == ["clean", "dirty"], f"{run} scenario set")
        for scene in report["scenarios"]:
            require(len(scene["rounds"]) == 5, f"{run} round count")
            for index, row in enumerate(scene["rounds"]):
                require(row["round"] == index and row["surfaceBytes"] == 3_686_400, f"{run} round identity/surface")
                require(set(row["stages"]) == STAGES, f"{run} stage set")
                total = sum(stage["totalMs"] for stage in row["stages"].values())
                require(math.isfinite(total) and abs(total - row["frameTotalMs"]) <= 0.001, f"{run} stage reconciliation")
                values = [row["frameTotalMs"], row["frameP95Ms"]] + [number for stage in row["stages"].values() for number in (stage["totalMs"], stage["p95Ms"])]
                require(all(math.isfinite(number) and number >= 0 for number in values), f"{run} invalid stage value")
    require(len(set(signatures)) == 1, "report fixture signatures differ")
    return entries


def make_readme(entries: list[dict]) -> str:
    lines = [
        "# Opt-in common-scene frame-stage diagnostic",
        "",
        "This packet preserves the diagnostic A1, A2, B1, B2, A3 sequence from 2026-09-23. A is the menu-intern baseline worktree and B is the menu-description candidate worktree. The archived serial driver declares that order; the parent task observed all five runs complete. Individual JSON reports do not record invocation clock time or executable hash, so neither order nor per-invocation binary identity is independently established by the reports alone.",
        "",
        "The five fixture inputs, two source HEADs, instrumentation diff, two incremental-build exit logs, two saved executable/library SHA-256 receipts, five run exit files, five raw reports, and five raw logs are preserved below. The input snapshots and instrumentation are identical across A and B. The raw report JSON lacks `benchmarkInputs`, so these JSON files use the existing `.json.receipt.txt` archival convention and their names and hashes are mapped below. The binary receipts are post-build snapshots, not per-run hashes. These diagnostic timings are outside the ordinary performance acceptance body.",
        "",
        "The A/A and B/B spread exceeds the bracketed A/B differences and candidate differences reverse sign. This diagnostic does not establish a stable common-scene timing regression. It does not pass or waive the ordinary paired performance gate, which remains open. The process-local heap observations are separate evidence and do not establish live menu allocations in this common scene.",
        "",
        "The [reviewed numerical analysis](analysis-original.md) predates inclusion of the [actual serial driver](raw/run-menu-stages-20260923.ps1); its statement that the original evidence folder lacked an archived driver refers to that earlier folder. Its current-binary hash check was true at analysis time and is not a claim about later restored binaries. The reports still lack per-run hashes and clocks. [The analyzer](analyze-menu-frame-stages.py) can recompute the stage summaries from the original evidence layout. `source-inventory.json.receipt.txt` pins the input source names, lengths and hashes; `original-file-map.json.receipt.txt` maps each original name to its byte-identical archived path. `SHA256SUMS.txt` covers every packet file except itself. Raw files are marked binary in `.gitattributes`, including JSON receipts renamed to `.json.receipt.txt` without re-encoding.",
        "",
        "## Required archived paths",
        "",
        "| Original name | Archived path | SHA-256 | Bytes |",
        "| --- | --- | --- | ---: |",
    ]
    for entry in entries:
        archived = archive_for(entry["source"])
        lines.append(f"| `{entry['source']}` | [{archived}]({archived}) | `{entry['sha256']}` | {entry['size']} |")
    lines.extend([
        "",
        "Generated packet files: [.gitattributes](.gitattributes), [curator](curate-menu-frame-stages.py), [source inventory](source-inventory.json.receipt.txt), [original-name/hash mapping](original-file-map.json.receipt.txt), and [SHA-256 manifest](SHA256SUMS.txt).",
        "",
        "Source roots for the original-name table: ordinary relative names are under `C:/RedSalamander.Perf/evidence/i26-ui/menu-frame-stages-20260923`; `@driver` is `C:/RedSalamander.Perf/evidence/i26-ui`; `@analysis` is the external Codex visualization artifact directory. All archive copies are byte-for-byte copies. This packet records diagnostic evidence only; restore the temporary opt-in instrumentation before running the standard controlled pair.",
        "",
    ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, default=EVIDENCE)
    parser.add_argument("--driver", type=Path, default=DRIVER)
    parser.add_argument("--target", type=Path, default=TARGET)
    args = parser.parse_args()
    base = Path(__file__).resolve().parent
    inventory_path = base / "menu-frame-stages-source-inventory.json"
    entries = preflight(args.evidence, args.driver, inventory_path, base)
    require(not args.target.exists(), f"target already exists: {args.target}")
    require(args.target.parent.is_dir(), f"target parent missing: {args.target.parent}")

    staging = Path(tempfile.mkdtemp(prefix=".common-frame-stages-stage-", dir=args.target.parent))
    try:
        mapping = []
        for entry in entries:
            origin = source_for(entry["source"], args.evidence, args.driver, base)
            archived = archive_for(entry["source"])
            destination = staging / archived
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(origin, destination)
            require(sha256(destination) == entry["sha256"], f"copied bytes changed: {archived}")
            mapping.append({"original_name": entry["source"], "archived_path": archived, "size": entry["size"], "sha256": entry["sha256"]})
        shutil.copyfile(inventory_path, staging / "source-inventory.json.receipt.txt")
        shutil.copyfile(Path(__file__), staging / "curate-menu-frame-stages.py")
        write_lf(staging / ".gitattributes", "raw/** -text -whitespace\nanalysis-original.md -text -whitespace\nanalyze-menu-frame-stages.py -text -whitespace\nsource-inventory.json.receipt.txt -text -whitespace\ncurate-menu-frame-stages.py -text -whitespace\n")
        write_lf(staging / "original-file-map.json.receipt.txt", json.dumps(mapping, indent=2, ensure_ascii=False) + "\n")
        write_lf(staging / "README.md", make_readme(entries))
        manifest = []
        for path in sorted(p for p in staging.rglob("*") if p.is_file()):
            manifest.append(f"{sha256(path)}  {path.relative_to(staging).as_posix()}")
        write_lf(staging / "SHA256SUMS.txt", "\n".join(manifest) + "\n")
        # Validate every final byte before publishing. An error leaves staging for inspection.
        for line in manifest:
            expected, relative = line.split("  ", 1)
            require(sha256(staging / relative) == expected, f"final manifest mismatch: {relative}")
        require(not args.target.exists(), "target appeared while staging")
        staging.rename(args.target)
    except Exception:
        print(f"Staging retained for inspection: {staging}")
        raise
    print(f"Packet staged: {args.target}")


if __name__ == "__main__":
    main()
