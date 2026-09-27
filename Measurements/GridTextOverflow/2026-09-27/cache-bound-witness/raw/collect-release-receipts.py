import hashlib
import json
from datetime import datetime
from pathlib import Path

root = Path(__file__).parent
repo = Path('Z:/src/DxUi-worktrees/i26-grid-line-clamp')
out = root / 'release'
floor = datetime.fromisoformat('2026-09-27T13:28:47.8161086+00:00')
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
for row in read(root / 'candidate-inputs.json'):
    assert digest(repo / row['path']) == row['sha256']
log = (out / 'test.log').read_text(encoding='utf-8-sig')
assert 'DxUi build passed' in log and 'All 4 requested suites passed.' in log
perf = set()
mapping = []
def copy(source, target):
    assert not target.exists()
    target.write_bytes(source.read_bytes())
    mapping.append(dict(source=str(source),path=target.name,sha256=digest(source)))
for suite in ['Grid','Rendering','Embedded','Accessibility']:
    receipt = repo / f'.build/reports/{suite}-x64-Release.json'
    obj = read(receipt)
    assert obj['configuration'] == 'Release' and obj['platform'] == 'x64'
    assert obj['exitCode'] == 0 and not obj['skips']
    assert datetime.fromisoformat(obj['completedUtc'].replace('Z','+00:00')) > floor
    assert digest(Path(obj['executable'])).lower() == obj['sha256'].lower()
    copy(receipt, out / f'{suite}-receipt.json')
    copy(repo / f'.build/logs/test-{suite}-x64-Release.log', out / f'{suite}.log')
    perf.add(obj['performanceReport'])
for filename in perf:
    source = Path(filename)
    copy(source, out / source.name)
    source = Path(filename + '.comparison.json')
    copy(source, out / source.name)
outcome = dict(configuration='Release',platform='x64',driverExit=1,
    nativeReceiptExits=[0,0,0,0],capabilitySkips=0,buildReportedPass=True,
    collectionFailure='PowerShell re-parsed a ConvertFrom-Json DateTime through French culture; no native failure',
    freshnessFloorUtc=floor.isoformat(),freshnessFloorSource='Original evidence directory creation UTC; not a reconstructed exact test start',
    recovery='Copied fresh source/binary-bound receipts in place; no native rerun',performance='unpaired',files=mapping)
(out / 'collection-recovery.json').write_text(json.dumps(outcome,indent=2)+'\n',encoding='utf-8')
print(json.dumps(outcome,indent=2))
