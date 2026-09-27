"""Stage completed exact-head DxUi CI artifacts without touching the repository."""
import hashlib
import json
from pathlib import Path

SOURCE = Path(r"C:\RedSalamander.Perf\evidence\i26-ui\menu-plain-uia-20260927\ci-73ba")
OUT = Path(r"C:\RedSalamander.Perf\evidence\i26-ui\ordinary-menu-ci73-packet-proposal-20260927")
HEAD = "73ba9365726cce30299290ab9ae8afa065c4fe3d"
RUN = 36352406442


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load(name):
    return json.loads((SOURCE / name).read_text(encoding="utf-8"))


def main():
    status = load("run-status.json")
    audit = load("audit.json")
    artifacts = load("run-artifacts.json")
    if status["databaseId"] != RUN or status["headSha"] != HEAD or status["status"] != "completed" or status["conclusion"] != "success":
        raise RuntimeError("Run identity or successful completion differs")
    jobs = [job for job in status["jobs"] if job["name"].startswith("native (")]
    if len(jobs) != 6 or any(job["conclusion"] != "success" for job in jobs):
        raise RuntimeError("Six native CI jobs have not all succeeded")
    if audit["run_id"] != RUN or audit["head_sha"] != HEAD or len(audit["profiles"]) != 6:
        raise RuntimeError("Audit identity/profile count differs")
    for profile in audit["profiles"]:
        if profile["suite_failures"] or not profile["gallery_done"] or not profile["consumer_receipts"]:
            raise RuntimeError(f"Native profile not fully qualified: {profile['profile']}")
        if any(receipt["exit_code"] != 0 or receipt["commit"] != HEAD for receipt in profile["consumer_receipts"]):
            raise RuntimeError(f"Consumer result differs: {profile['profile']}")
        if not profile["performance_receipts"] or any(
            receipt["sourceCommit"] != HEAD or receipt["sourceDirty"] or
            receipt["roundCount"] != 5 or receipt["framesPerRound"] != 40 or
            receipt["benchmarkSha256"] != audit["benchmark_sha256"]
            for receipt in profile["performance_receipts"]
        ):
            raise RuntimeError(f"Performance provenance differs: {profile['profile']}")
    artifact_rows = artifacts.get("artifacts", artifacts) if isinstance(artifacts, dict) else artifacts
    if len(artifact_rows) != 6 or any(row.get("expired") for row in artifact_rows):
        raise RuntimeError("Six retained GitHub artifacts required")
    OUT.mkdir(exist_ok=True)
    raw = OUT / "raw"
    raw.mkdir(exist_ok=True)
    if any(raw.iterdir()) or (OUT / "original-file-map.json.receipt.txt").exists():
        raise RuntimeError("Reviewed packet already exists; refuse overwrite")
    files = sorted((path for path in SOURCE.rglob("*") if path.is_file()), key=lambda path: path.relative_to(SOURCE).as_posix())
    entries = []
    for path in files:
        data = path.read_bytes()
        sha = digest(data)
        staged = f"raw/{sha[:32]}.receipt"
        target = OUT / staged
        if target.exists():
            if digest(target.read_bytes()) != sha:
                raise RuntimeError(f"Content-address collision: {path}")
        else:
            target.write_bytes(data)
        entries.append({"original_relative": path.relative_to(SOURCE).as_posix(), "staged": staged,
                        "sha256": sha, "size_bytes": len(data)})
    mapping = {"schema": "dxui.measurement-file-map.v1", "source_root_diagnostic": str(SOURCE),
               "target_suggestion": "Measurements/MenuDescriptions/2026-09-27/ordinary-menu-ci-73ba",
               "run_id": RUN, "head_sha": HEAD, "entry_count": len(entries),
               "unique_payload_count": len({entry["staged"] for entry in entries}), "entries": entries}
    (OUT / "original-file-map.json.receipt.txt").write_bytes((json.dumps(mapping, indent=2) + "\n").encode("utf-8"))
    (OUT / ".gitattributes").write_bytes(b"# Preserve byte-exact CI receipts.\n* -text\nraw/** -whitespace\n")
    readme = (f"# Ordinary-menu exact-head native CI, {HEAD[:7]}\n\n"
              f"GitHub Actions run {RUN} completed successfully at exact commit `{HEAD}`. "
              "The six extracted native artifacts, GitHub job/artifact metadata, suite and performance receipts, "
              "external-consumer results, gallery output, audit and SHA-256 map are preserved byte-exact. "
              "Formatting CI and RedSalamander consumer adoption are separate. "
              "See the mapped `audit.md`/`audit.json` and original receipts for profile-specific skips.\n\n"
              "The map uses short content-addressed payload names; it does not represent a new benchmark run. "
              "Run `pwsh -NoProfile -File ./Verify-Packet.ps1` before copying to Measurements.\n")
    (OUT / "README.md").write_bytes(readme.encode("utf-8"))
    manifest = sorted((path for path in OUT.rglob("*") if path.is_file() and path.name != "SHA256SUMS"),
                      key=lambda path: path.relative_to(OUT).as_posix())
    (OUT / "SHA256SUMS").write_bytes(("\n".join(f"{digest(path.read_bytes())}  {path.relative_to(OUT).as_posix()}" for path in manifest) + "\n").encode("ascii"))
    print(f"Staged {len(entries)} CI files as {mapping['unique_payload_count']} unique payloads")


if __name__ == "__main__":
    main()
