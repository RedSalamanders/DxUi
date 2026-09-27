import hashlib
import json
from pathlib import Path

packet = Path('Z:/src/DxUi-worktrees/i26-grid-line-clamp/Measurements/GridTextOverflow/2026-09-27/cache-bound-witness').resolve()
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
renames = {}
for source in sorted((packet / 'raw').rglob('*')):
    if source.is_file() and source.suffix in {'.md','.cpp','.h'}:
        target = Path(str(source) + '.source.txt')
        assert source.resolve().is_relative_to(packet) and target.resolve().is_relative_to(packet)
        assert not target.exists()
        digest = sha(source)
        source.rename(target)
        assert sha(target) == digest
        renames[source.relative_to(packet).as_posix()] = target.relative_to(packet).as_posix()
mapping_path = packet / 'raw/file-map.receipt.txt'
mapping = json.loads(mapping_path.read_text(encoding='utf-8'))
for row in mapping:
    row['path'] = renames.get(row['path'],row['path'])
    assert sha(packet / row['path']) == row['sha256']
mapping_path.write_text(json.dumps(mapping,indent=2)+'\n',encoding='utf-8')
(packet / 'raw/archive-name-repair.receipt.txt').write_text(json.dumps(dict(
    reason='Archive Markdown source snapshots are evidence, not active specs; source suffixes also exclude archived code from live formatting',
    renamed=renames,bytesChanged=False),indent=2)+'\n',encoding='utf-8')
(packet / 'raw/repair-archive-names.py').write_bytes(Path(__file__).read_bytes())
readme = packet / 'README.md'
text = readme.read_text(encoding='utf-8')
text += '\nArchived Markdown, C++ and header snapshots have `.source.txt` suffixes; their bytes are unchanged. The retained original curator produced the initial names; `raw/archive-name-repair.receipt.txt` and its repair script record this packaging correction after spec validation found the archived relative links. These snapshots are not additional normative specs or build inputs.\n'
readme.write_text(text,encoding='utf-8',newline='\n')
files = sorted(p for p in packet.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt')
(packet / 'SHA256SUMS.txt').write_bytes(''.join(sha(p)+'  '+p.relative_to(packet).as_posix()+'\n' for p in files).encode('ascii'))
print(json.dumps(dict(renamed=len(renames),payloads=len(files),bytes=sum(p.stat().st_size for p in files))))
