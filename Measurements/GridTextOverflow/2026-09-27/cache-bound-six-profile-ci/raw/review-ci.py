import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

root=Path(__file__).parent
repo=Path('Z:/src/DxUi-worktrees/i26-grid-line-clamp')
packet=repo/'Measurements/GridTextOverflow/2026-09-27/cache-bound-six-profile-ci'
run=json.loads((root/'run.json').read_text(encoding='utf-8-sig'))
head='15f98822b934b5014f0d69b6eb26b1d320d20eaf'
assert run['headSha']==head and run['conclusion']=='success'
assert not packet.exists()
rows=[]
selected=[root/'run.json',root/'artifacts.json']
for platform in ('x64','ARM64'):
 for config in ('Debug','Release','ASan Debug'):
  a=root/'artifacts'/f'native-{platform}-{config}'
  expected_arch='Arm64' if platform=='ARM64' else 'X64'
  suites=[]
  for suite in ('Grid','Rendering','Embedded','Accessibility'):
   receipt=a/'reports'/f'{suite}-{platform}-{config}.json'
   d=json.loads(receipt.read_text(encoding='utf-8-sig'))
   assert d['exitCode']==0 and not d['skips'] and d['nativeArchitecture']==expected_arch
   assert d['platform']==platform and d['configuration']==config
   assert d['performanceComparison']=='unpaired'
   log=a/'logs'/f'test-{suite}-{platform}-{config}.log'
   assert log.exists()
   selected += [receipt,log]
   perf=a/'reports'/Path(d['performanceReport'].replace('\\','/')).name
   selected += [perf,Path(str(perf)+'.comparison.json')]
   suites.append(dict(suite=suite, executable_sha256=d['sha256'], exit=0, skips=0))
  if config=='ASan Debug':
   probe=a/'reports'/f'AddressSanitizer-{platform}.json'
   d=json.loads(probe.read_text(encoding='utf-8-sig'))
   assert d['detected'] and d['exitCode']!=0
   selected += [probe, a/'logs'/f'test-AddressSanitizer-{platform}.log']
  selected += list((a/'logs').glob('build-*.log'))
  other_skips={}
  for p in (a/'reports').glob('*.json'):
   d=json.loads(p.read_text(encoding='utf-8-sig'))
   if isinstance(d,dict) and d.get('skips'):
    other_skips[d.get('suite',p.name)]=d['skips']
    selected += [p]
  rows.append(dict(platform=platform,configuration=config,native_architecture=expected_arch,
                   suites=suites,other_capability_skips=other_skips))
inventory=[]
for path in sorted((root/'artifacts').rglob('*')):
 if path.is_file():
  inventory.append(dict(path=path.relative_to(root).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),bytes=path.stat().st_size))
(root/'review.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
(root/'downloaded-files.json').write_text(json.dumps(inventory,indent=2)+'\n',encoding='utf-8')
selected += [root/'review.json',root/'downloaded-files.json',Path(__file__)]
packet.mkdir(parents=True)
mapping=[]
for source in sorted(set(selected)):
 relative=source.relative_to(root).as_posix()
 target=packet/'raw'/relative
 target=target.with_name(re.sub(r'-[0-9a-f]{32}(?=\.)','',target.name))
 if source.suffix=='.json': target=Path(str(target)+'.receipt.txt')
 target.parent.mkdir(parents=True,exist_ok=True)
 assert not target.exists(), target
 shutil.copyfile(source,target)
 mapping.append(dict(source=str(source),path=target.relative_to(packet).as_posix(),sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for relative in ('Tests/Controls/DxUiTests.Rendering.cpp','Tests/Controls/DxUiTests.Accessibility.cpp','src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h'):
 source=repo/relative
 original=subprocess.check_output(['git','show',head+':'+relative],cwd=repo)
 assert source.read_bytes().replace(b'\r\n',b'\n')==original.replace(b'\r\n',b'\n')
 target=packet/'raw'/(Path(relative).name+'.source.txt')
 target.write_bytes(original)
(packet/'raw/file-map.receipt.txt').write_bytes((json.dumps(mapping,indent=2)+'\n').encode('utf-8'))
(packet/'.gitattributes').write_bytes(b'raw/** -text -whitespace\nSHA256SUMS.txt -text -whitespace\n')
(packet/'README.md').write_bytes(('''# Grid cache-bound witness: six native CI profiles

Root reviewed the archived [DxUi validation run 36324654165](https://github.com/RedSalamanders/DxUi/actions/runs/36324654165), executed September 27 on exact source `15f98822b934b5014f0d69b6eb26b1d320d20eaf`. This includes the over-capacity French multiline cache witness and borrowed-model lifetime correction. This documentation-only path correction preserves the compiled witness from `826fa563`; the earlier aggregate CI failed its relocated clone on long archived receipt names. The current run has successful validation and all six native jobs. Its job metadata, artifact IDs, downloaded-file hashes, source snapshots and selected raw logs/receipts are retained here.

Grid, Rendering, Embedded and Accessibility each pass with zero skips in x64 Debug, Release, ASan Debug and native ARM64 Debug, Release, ASan Debug: 24 focused suite receipts. Both ASan profiles diagnose the intentional isolated use-after-free probe. Each suite receipt identifies the executable hash and native architecture; ARM64 is actual native execution, not inferred from a cross-build. The matching build logs and exact witness source bind the new cache-bound, scroll/reattach pixel, full-copy, oversized-value and detach checks to this run, alongside the existing full-value UIA checks.

The automatic complex-UI benchmark is unpaired in every profile. It does not replace the retained matched resource measurements or expand the user's explicitly accepted V11 memory cost. Other suite capability skips are preserved verbatim in `raw/review.json.receipt.txt`; these do not waive native desktop/assistive-technology acceptance. The review closes the new cache witness's native ARM64 execution gate; local cross-builds alone did not close it. Combined grid/menu qualification, menu timing advice and explicit consumer adoption remain separate gates.

`raw/downloaded-files.json.receipt.txt` inventories the full six downloaded artifacts. The packet selects the four focused suites, sanitizer probes, related benchmarks, build logs and skip-bearing receipts. Other artifact files remain in the external evidence root. Raw JSON uses `.receipt.txt` to distinguish nonacceptance metadata; all copied bytes are preserved, with shortened archive basenames mapped to their original external paths. `review-ci.py` records the root's machine-checked assertions; `SHA256SUMS.txt` binds every payload.
''').encode('utf-8'))
files=sorted(p for p in packet.rglob('*') if p.is_file())
(packet/'SHA256SUMS.txt').write_bytes(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(packet).as_posix()+'\n' for p in files).encode('ascii'))
print(json.dumps(dict(profiles=len(rows),focused_passes=24,payloads=len(files),bytes=sum(p.stat().st_size for p in files),other_skips=[{k:v for k,v in r.items() if k in ('platform','configuration','other_capability_skips')} for r in rows]),indent=2))
