import hashlib
import json
import re
import subprocess
from pathlib import Path

root = Path(__file__).parent
repo = Path('Z:/src/DxUi-worktrees/i26-grid-line-clamp')
packet = repo / 'Measurements/GridTextOverflow/2026-09-27/cache-bound-witness'
assert not packet.exists(), 'Never overwrite retained evidence'
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
baseline = read(root / 'baseline.json')
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip() == baseline['head']
inputs = read(root / 'candidate-inputs.json')
changed = subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=repo,text=True).splitlines()
assert set(changed) == {row['path'] for row in inputs}, changed
for row in inputs:
    assert sha(repo / row['path']) == row['sha256'], row['path']
profiles = [('release','Release'),('debug','Debug'),('asan-debug','ASan Debug')]
rows = []
build_logs = {}
for directory, config in profiles:
    source = root / directory
    if config == 'Release':
        outcome = read(source / 'collection-recovery.json')
        assert outcome['driverExit'] == 1 and outcome['nativeReceiptExits'] == [0,0,0,0]
    else:
        assert read(source / 'outcome.json')['exit'] == 0
    for suite in ['Grid','Rendering','Embedded','Accessibility']:
        receipt = read(source / f'{suite}-receipt.json')
        assert receipt['configuration'] == config and receipt['platform'] == 'x64'
        assert receipt['exitCode'] == 0 and receipt['skips'] == []
        assert sha(Path(receipt['executable'])).upper() == receipt['sha256'].upper()
        assert receipt['performanceComparison'] == 'unpaired'
    if config == 'ASan Debug':
        probe = read(source / 'AddressSanitizer-receipt.json')
        assert probe['detected'] is True and probe['exitCode'] != 0
    rows.append(dict(configuration=config,platform='x64',suites=4,skips=0,native=True))
for directory, config in profiles:
    source = root / f'arm64-{directory}'
    outcome = read(source / 'outcome.json')
    assert outcome['configuration'] == config and outcome['exit'] == 0
    assert outcome['nativeRuntimeExecuted'] is False
    for artifact in outcome['artifacts']:
        assert sha(Path(artifact['path'])) == artifact['sha256']
    rows.append(dict(configuration=config,platform='ARM64',build='pass',native=False))
for directory in [name for name, _ in profiles] + ['arm64-'+name for name,_ in profiles]:
    source = root / directory
    log = source / ('build.log' if directory.startswith('arm64-') else 'test.log')
    text = log.read_text(encoding='utf-8-sig')
    match = re.search(r'^Log:\s*(.+?)\s*$', text, re.M)
    assert match, log
    full = Path(match.group(1))
    fulltext = full.read_text(encoding='utf-8-sig')
    assert 'Build succeeded.' in fulltext and '0 Warning(s)' in fulltext and '0 Error(s)' in fulltext
    build_logs[directory] = full

packet.mkdir(parents=True)
mapping = []
def copy(source, name):
    target = packet / name
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(source.read_bytes())
    mapping.append(dict(source=str(source),path=name,sha256=sha(source),bytes=source.stat().st_size))
for source in sorted(root.rglob('*')):
    if source.is_file():
        name = 'raw/' + source.relative_to(root).as_posix()
        if name.endswith('.json'):
            name += '.receipt.txt'
        copy(source, name)
for directory, full in build_logs.items():
    copy(full, f'raw/{directory}/msbuild-full.log')
patch = subprocess.check_output(['git','diff','--','include/DxUi/DxUi.h','src/Controls/DxUi.Grid.cpp','Tests/Controls/DxUiTests.Rendering.cpp'],cwd=repo)
assert b'TestGridMultilineCacheBoundThroughScrollAndDetach' in patch
(packet / 'raw/source.patch').write_bytes(patch)
(packet / 'raw/file-map.receipt.txt').write_text(json.dumps(mapping,indent=2)+'\n',encoding='utf-8')
(packet / 'raw/profile-summary.receipt.txt').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
(packet / '.gitattributes').write_bytes(b'raw/** -text -whitespace\nSHA256SUMS.txt -text -whitespace\n')
(packet / 'README.md').write_text('''# Multiline grid cache bounds and borrowed-model lifetime

The added Rendering witness exercises more than the 32-entry cache capacity with distinct French multiline values. It scrolls to new rows and back, compares every cached frame with a fresh attachment, checks complete copied text, excludes values longer than 4096 UTF-16 units from retention, and verifies detach releases cached layouts and source strings. A read-only diagnostic exposes observed retention without extra paint counters, fields or shaping. The preceding pixel test now declares its borrowed models before the host, including failure unwinding; no crash is attributed to the old order.

The frozen candidate is based on `d2f2e3a96a03981400c08dbc452dff9ed0f1ab57`. Original and candidate inputs, exact three-file source patch and drivers are retained. Production rendering/cache policy remains V11. The existing V11 screenshots remain applicable because no user-facing behavior, geometry, theme or gallery input changes.

Grid, Rendering, Embedded and Accessibility pass without capability skips in x64 Release, Debug and ASan Debug: 12 suite passes. The ASan isolated fault probe is detected as required. All three ARM64 configurations build successfully; no ARM64 runtime was executed in this packet. All six builds have zero warnings and errors, with complete MSBuild logs and output hashes retained. The previous six-profile CI packet qualifies the earlier source, not this new witness.

Release's native build and all four suites passed, but the archive wrapper then exited 1 because PowerShell automatically parsed JSON timestamps into DateTime and the driver reparsed their culture-formatted text. The original driver and explicit recovery record are retained. Fresh receipt timestamps and executable hashes were checked and copied without rerunning native tests. The evidence directory creation UTC is the recovery freshness floor, not a reconstructed exact test start. The corrected driver handles DateTime directly; Debug and ASan complete with wrapper exit 0.

Every mandatory automatic benchmark is unpaired. These runs establish functional bounds and teardown only; they do not accept V11's retained timing flags or prove a settled whole-process allocator bound. The user-approved recorded grid memory tradeoff remains limited to its original scope. Native ARM64 execution of this witness, assistive technology, combined library qualification and explicit consumer pin adoption remain separate gates.

`raw/file-map.receipt.txt` maps unchanged copied files to the external evidence directory `C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-bound-witness-20260927`. JSON receipts use `.receipt.txt` without changing bytes. `SHA256SUMS.txt` binds every payload. Earlier failed formatting is preserved alongside the passing static validators; no failed attempt or baseline is replaced.
''',encoding='utf-8',newline='\n')
files = sorted(p for p in packet.rglob('*') if p.is_file())
(packet / 'SHA256SUMS.txt').write_bytes(''.join(sha(p)+'  '+p.relative_to(packet).as_posix()+'\n' for p in files).encode('ascii'))
print(json.dumps(dict(payloads=len(files),bytes=sum(p.stat().st_size for p in files),profiles=rows),indent=2))
