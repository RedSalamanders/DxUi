import hashlib
import json
import subprocess
from pathlib import Path

repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
packet = 'Measurements/GridTextOverflow/2026-09-27/cache-bound-witness'
def blob(name):
    return subprocess.check_output(['git','show',':'+name],cwd=repo)
manifest = blob(packet+'/SHA256SUMS.txt').decode('ascii')
for line in manifest.splitlines():
    expected,name = line.split('  ',1)
    assert hashlib.sha256(blob(packet+'/'+name)).hexdigest() == expected,name
inputs = json.loads(Path('C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-bound-witness-20260927/candidate-inputs.json').read_text(encoding='utf-8-sig'))
normalization = []
for row in inputs:
    if row['path'].endswith(('.cpp','.h')):
        compiled = (Path('C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-bound-witness-20260927/candidate-inputs')/row['path']).read_bytes()
        staged = blob(row['path'])
        assert hashlib.sha256(compiled).hexdigest() == row['sha256']
        assert compiled.replace(b'\r\n',b'\n') == staged.replace(b'\r\n',b'\n'),row['path']
        normalization.append(dict(path=row['path'],compiledSha256=row['sha256'],stagedSha256=hashlib.sha256(staged).hexdigest(),
            byteIdentical=compiled==staged,onlyCrLfNormalization=compiled!=staged))
Path('C:/RedSalamander.Perf/evidence/i26-ui/grid-staged-source-normalization-20260927.json').write_text(json.dumps(normalization,indent=2)+'\n',encoding='utf-8')
print(f'{len(manifest.splitlines())} staged evidence hashes verified; all 3 compiled source files match staged content after CRLF normalization.')
print(json.dumps(normalization,indent=2))
