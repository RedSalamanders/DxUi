"""Stage immutable ordinary-menu resource and d45 CI receipts for review."""
import hashlib
import json
from pathlib import Path


SOURCE = Path(r"C:\RedSalamander.Perf\evidence\i26-ui\menu-plain-uia-20260927")
OUT = Path(r"C:\RedSalamander.Perf\evidence\i26-ui\ordinary-menu-measurements-proposal-v2-20260927")
DIRECTORIES = ["final-abba-20260927", "excluded-aborted-performance", "A1", "A2", "B1", "B2", "B3", "B4", "B5", "profiles-d45", "ci-d45", "ci-d45-diagnosis"]
ROOT_FILES = ["correction-metadata.json", "README.md", "resource-analysis.md", "resource-derived-observations.json", "resource-warm-summary.json"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    OUT.mkdir(exist_ok=True)
    raw = OUT / "raw"
    raw.mkdir(exist_ok=True)
    if any(raw.iterdir()) or (OUT / "original-file-map.json").exists():
        raise RuntimeError("Output already populated; refuse to overwrite reviewed evidence")
    diagnosis = SOURCE / "ci-d45-diagnosis"
    pair = json.loads((diagnosis / "rebuilt-navigation-pair-result.json").read_text(encoding="utf-8"))
    if pair["baselineExit"] != 1 or pair["candidateExit"] != 0:
        raise RuntimeError("Forced-rebuild pair outcome differs")
    correction = json.loads((diagnosis / "paired-navigation-correction.json").read_text(encoding="utf-8"))
    if correction["excludedBaseline"] != "paired-navigation-a":
        raise RuntimeError("Invalid incremental baseline correction missing")
    for name in ("rebuilt-navigation-a", "rebuilt-navigation-b"):
        build = (diagnosis / name / "build.log").read_text(encoding="utf-8", errors="replace")
        if not any(line.strip() == "DxUi.Accessibility.cpp" for line in build.splitlines()):
            raise RuntimeError(f"Forced build did not compile Accessibility.cpp: {name}")
    for name, end_us, open_us in (("rebuilt-navigation-a", 1022545, 3090947), ("rebuilt-navigation-b", 344345, 1087745)):
        events = [json.loads(line) for line in (diagnosis / name / "test.log").read_text(encoding="utf-8", errors="replace").splitlines() if line.startswith('{"fixture":"dxui-large-menu-end-v1"')]
        open_event = next(event for event in events if event["stage"] == "open")
        end_event = next(event for event in events if event["stage"] == "end")
        if (open_event["openToFirstPaintUs"], end_event["endToVisibleUs"]) != (open_us, end_us):
            raise RuntimeError(f"Forced-rebuild timing differs: {name}")
    entries = []
    files = []
    for dirname in DIRECTORIES:
        root = SOURCE / dirname
        if not root.is_dir():
            raise RuntimeError(f"Missing source directory: {root}")
        files.extend((p, p.relative_to(SOURCE).as_posix()) for p in root.rglob("*") if p.is_file())
    files.extend((SOURCE / name, name) for name in ROOT_FILES)
    for path, logical in sorted(files, key=lambda pair: pair[1].casefold()):
        data = path.read_bytes()
        digest = sha(data)
        staged = f"raw/{digest[:32]}.receipt"
        target = OUT / staged
        if target.exists():
            if sha(target.read_bytes()) != digest:
                raise RuntimeError(f"Content-address collision: {logical}")
        else:
            target.write_bytes(data)
        entries.append({"original_relative": logical, "staged": staged, "sha256": digest, "size_bytes": len(data)})
    mapping = {"schema": "dxui.measurement-file-map.v1", "source_root_diagnostic": str(SOURCE),
               "target_suggestion": "Measurements/MenuDescriptions/2026-09-27/ordinary-menu-accessibility",
               "entry_count": len(entries), "unique_payload_count": len({e["staged"] for e in entries}), "entries": entries}
    (OUT / "original-file-map.json").write_text(json.dumps(mapping, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (OUT / ".gitattributes").write_text("# Preserve raw measurement and CI receipt bytes.\n* -text\nraw/** -whitespace\n", encoding="utf-8")
    readme = """# Ordinary-menu accessibility, resource and CI evidence — 27 September 2026

This packet stages the matched nonactivating plain-menu evidence and the exact-head `d45c3610a586fab926a98a8656bb337792a38b6c` six-profile CI run. `original-file-map.json` maps each original relative path to a short, content-addressed raw payload, its byte count and SHA-256. Identical bytes are stored once. `SHA256SUMS` covers the staged payloads and metadata. The proposal is external; review before copying to the suggested `Measurements` destination.

The A3/C1/C2/A4 fresh-build ABBA fixture reproduced the old-code zero-UIA-child failure and candidate 12/128-child pass, with 24 observations per run at 476×322 px/96 DPI. Warm open-minus-prior-closed live heap increases by 22,935 B for 12 rows and 231,594 B for 128 rows; the 128-row closed-heap cross-variant difference is 36 B in four cycles. The user explicitly accepted that measured accessibility cost. The original comparator outputs remain `advice-required`; same-variant controls also flag, so common WARP non-regression is not established. See the raw `final-abba-20260927/abba-analysis.md` via the map for calculations and exact caveats.

`correction-metadata.json` excludes B1/B2 suite receipts copied from A2 when those runs aborted before the suite; their actual aborted performance reports and comparisons are retained without rewriting the original B1/B2 evidence. B3 is excluded because its collector failed after the suite. A1/A2/B4/B5 resource runs and local d45 profiles are retained for context. The final ABBA is the matched decision evidence.

GitHub Actions run `36349836006` completed at exact d45 head with validation and five native profiles green. x64 ASan Debug `Menu` fails `TestLargeMenuPaintsOnlyVisibleRowsWithCachedOffsets` on its End-to-visible latency bound; `MenuAccessibility` passes. ARM64 Release, Debug and ASan Debug each have 20 passing suite receipts and nine explicit interactive-desktop Menu skips; ASan has both passing consumer annotation settings. Five successful profiles have gallery output. Seven retained complex-UI performance receipts are exact-head, clean-source, 5×40 frames with one workload fingerprint. This is a failed six-profile gate and not final qualification. Formatting CI is separate. See the mapped `ci-d45/audit.md`, `audit.json`, full extracted artifacts, GitHub artifact metadata and failed-step log.

The retained `ci-d45-diagnosis` investigation includes the original CI failure, stage probes, a strict-foreground run interrupted at the unchanged focus invariant, and the excluded incremental A/B attempt. In that attempt, `paired-navigation-a` copied old source bytes but its incremental build did **not** compile `DxUi.Accessibility.cpp`; its passing timing cannot serve as a baseline. `paired-navigation-correction.json` preserves that exclusion. The forced clean-rebuild pair compiled `DxUi.Accessibility.cpp` in **both** build logs: A open 3,090,947 µs and End 1,022,545 µs (the latter fails); B open 1,087,745 µs and End 344,345 µs (passes). The fixture retains the original 5,000,000 µs open and 1,000,000 µs End limits. These temporary instrumented runs diagnose a specific 4,096-row path; they do not replace final three-profile tests or CI. The final `profiles-navigation-final` run was still active when this packet froze, so no incomplete profile directory is included.

The packet preserves d45 rather than claiming that an in-progress fix passed. It does not change a RedSalamander consumer pin or assert consumer adoption. Run `pwsh -NoProfile -File ./Verify-Packet.ps1` after copying to verify exact membership and every staged hash.
"""
    (OUT / "README.md").write_text(readme, encoding="utf-8")
    manifest_files = [p for p in OUT.rglob("*") if p.is_file() and p.name != "SHA256SUMS"]
    lines = [f"{sha(p.read_bytes())}  {p.relative_to(OUT).as_posix()}" for p in sorted(manifest_files, key=lambda p: p.relative_to(OUT).as_posix())]
    (OUT / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"Staged {len(entries)} logical files as {mapping['unique_payload_count']} unique payloads")


if __name__ == "__main__":
    main()
