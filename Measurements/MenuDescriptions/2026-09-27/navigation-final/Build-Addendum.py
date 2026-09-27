"""Stage the completed local navigation-profile/resource comparison without repo writes."""
import hashlib
import json
from pathlib import Path

SOURCE = Path(r"C:\RedSalamander.Perf\evidence\i26-ui\menu-plain-uia-20260927")
OUT = Path(r"C:\RedSalamander.Perf\evidence\i26-ui\ordinary-menu-navigation-resource-addendum-v2-20260927")
DIRS = ("profiles-navigation-final", "profiles-navigation-comparison-20260927")
TOOL = Path(r"C:\Users\eric\.codex\worktrees\fileops-ui-qualified\DxUi\Tools\compare_performance.py")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    OUT.mkdir(exist_ok=True)
    raw = OUT / "raw"
    raw.mkdir(exist_ok=True)
    if any(raw.iterdir()) or (OUT / "original-file-map.json.receipt.txt").exists():
        raise RuntimeError("Reviewed output exists; refuse overwrite")
    entries = []
    for dirname in DIRS:
        root = SOURCE / dirname
        if not root.is_dir():
            raise RuntimeError(f"Missing completed source: {root}")
        for path in sorted((p for p in root.rglob("*") if p.is_file()), key=lambda p: p.relative_to(root).as_posix()):
            data = path.read_bytes()
            digest = sha(data)
            staged = f"raw/{digest[:32]}.receipt"
            target = OUT / staged
            if target.exists():
                if sha(target.read_bytes()) != digest:
                    raise RuntimeError(f"Hash collision: {path}")
            else:
                target.write_bytes(data)
            entries.append({"original_relative": path.relative_to(SOURCE).as_posix(), "staged": staged,
                            "sha256": digest, "size_bytes": len(data)})
    mapping = {"schema": "dxui.measurement-file-map.v1", "source_root_diagnostic": str(SOURCE),
               "target_suggestion": "Measurements/MenuDescriptions/2026-09-27/navigation-final",
               "comparison_tool_sha256": sha(TOOL.read_bytes()),
               "entry_count": len(entries), "unique_payload_count": len({e["staged"] for e in entries}), "entries": entries}
    (OUT / "original-file-map.json.receipt.txt").write_bytes((json.dumps(mapping, indent=2) + "\n").encode("utf-8"))
    (OUT / ".gitattributes").write_bytes(b"# Preserve byte-exact receipts.\n* -text\nraw/** -whitespace\n")
    readme = """# Local navigation-profile resource addendum

This addendum preserves all completed `profiles-navigation-final` x64 Release, Debug and ASan Debug nonactivating receipts and the unchanged-tool offline comparison against the prior d45 profiles already stored in the ordinary-menu packet. It contains 18/18 passing suite receipts per configuration. The source receipts say d45 `sourceDirty:true` because this local run preceded the 65b/73ba commits; captured Accessibility.cpp and Menu test bytes match the later clean 73ba checkout, but these are not exact-head CI receipts. `original-file-map.json.receipt.txt` records every source-relative path, SHA-256, byte length and short content-addressed payload. The repository comparator script SHA is recorded; neither baseline receipts nor build outputs are duplicated here.

`profiles-navigation-comparison-20260927/assessment.md` retains frame median/tail and count analysis. Offline `compare_performance.py` reports Release and ASan Debug `advice-required` and Debug `within-noise-budget`. The common offscreen `dxui-complex-ui-v2` fixture never attaches accessibility, so its flagged variation does not isolate snapshot-builder cost. Retain every flag; earlier accepted menu timing costs are not a blanket waiver. `resource-sample-summary.json` verifies 24 ordinary-menu observations per configuration, exact 12/128 children at 476×322 px/96 DPI and no added short-fixture live-heap growth versus local d45. ASan process-heap busy-byte deltas are uninformative. Exact-73ba CI remains the independent gate.

This is a proposed sibling of the already curated ordinary-menu Measurements packet; it does not alter its existing raw file map. Run `pwsh -NoProfile -File ./Verify-Packet.ps1` to check exact membership and hashes before copying.
"""
    (OUT / "README.md").write_bytes(readme.encode("utf-8"))
    all_files = sorted((p for p in OUT.rglob("*") if p.is_file() and p.name != "SHA256SUMS"), key=lambda p: p.relative_to(OUT).as_posix())
    (OUT / "SHA256SUMS").write_bytes(("\n".join(f"{sha(p.read_bytes())}  {p.relative_to(OUT).as_posix()}" for p in all_files) + "\n").encode("ascii"))
    print(f"Staged {len(entries)} logical files as {mapping['unique_payload_count']} unique payloads")


if __name__ == "__main__":
    main()
