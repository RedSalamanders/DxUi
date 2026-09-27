import hashlib,json,re,shutil,subprocess
from pathlib import Path
repo=Path('Z:/src/DxUi-worktrees/i26-grid-line-clamp').resolve()
packet=repo/'Measurements/GridTextOverflow/2026-09-27/uia-six-profile-ci'
sha=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
manifest=packet/'SHA256SUMS.txt'
for line in manifest.read_text().splitlines():
    digest,relative=line.split('  ',1)
    assert sha(packet/relative)==digest,relative
repair=packet/'raw/path-repair'
repair.mkdir()
shutil.copyfile(manifest,repair/'original-sha256.txt')
shutil.copyfile(packet/'raw/file-map.receipt.txt',repair/'original-map.txt')
mapping=json.loads((packet/'raw/file-map.receipt.txt').read_text())
renamed=[]
for item in mapping:
    old=packet/item['path']
    new=old.with_name(re.sub(r'-[0-9a-f]{32}(?=\.)','',old.name))
    if new==old: continue
    assert old.resolve().is_relative_to(packet.resolve()) and new.resolve().is_relative_to(packet.resolve())
    assert sha(old)==item['sha256'] and not new.exists()
    old.rename(new)
    assert sha(new)==item['sha256']
    renamed.append(dict(old_path=item['path'],new_path=new.relative_to(packet).as_posix(),sha256=item['sha256']))
    item['path']=new.relative_to(packet).as_posix()
(packet/'raw/file-map.receipt.txt').write_text(json.dumps(mapping,indent=2)+'\n',encoding='utf-8')
(repair/'rename-map.json.receipt.txt').write_text(json.dumps(renamed,indent=2)+'\n',encoding='utf-8')
shutil.copyfile(Path(__file__),repair/'shorten-paths.py')
evidence=Path(__file__).parent/'grid-cache-ci-826fa56-20260927'
for name in ['run.json','x64-debug-job.log']:
    shutil.copyfile(evidence/name,repair/(name+'.receipt.txt'))
readme=packet/'README.md'
readme.write_bytes(readme.read_bytes()+b'\nThe later cache-witness CI run `36323665013` failed its relocated consumer clone because deeply nested archived benchmark names exceeded Windows path limits. `raw/path-repair/rename-map.json.receipt.txt` maps shortened destination basenames to their originals and unchanged hashes. Source paths and receipt bytes remain exact; the original map/checksum, failing job log and repair script are retained. This packaging correction changes no library or test code, and the original curator remains historical.\n')
files=sorted(p for p in packet.rglob('*') if p.is_file() and p!=manifest)
manifest.write_bytes(''.join(sha(p)+'  '+p.relative_to(packet).as_posix()+'\n' for p in files).encode('utf-8'))
tracked=subprocess.check_output(['git','ls-files'],cwd=repo,text=True).splitlines()
existing=[p for p in tracked if (repo/p).exists()]
longest=max(existing,key=len)
print(json.dumps(dict(renamed=len(renamed),maxTrackedRelativePath=len(longest),longest=longest,payloads=len(files))))
