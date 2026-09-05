"""Verify immutable import hashes and independent active-source includes."""
from pathlib import Path
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    failures = []
    manifest = json.loads((ROOT / 'vcpkg.json').read_text(encoding='utf-8'))
    tool = json.loads((ROOT / 'vcpkg-tool.json').read_text(encoding='utf-8'))
    if manifest['builtin-baseline'] != tool['commit'] or not re.fullmatch('[0-9a-f]{40}', tool['commit']):
        failures.append('vcpkg baseline/tool revision mismatch')
    provenance = json.loads((ROOT / 'provenance/source-import.json').read_text(encoding='utf-8'))
    if not re.fullmatch('[0-9a-f]{40}', provenance['commit']):
        failures.append('Source import needs an exact commit')
    seen = set()
    for entry in provenance['files']:
        relative = entry['importedPath']
        if Path(relative).suffix.lower() in ('.user', '.suo'):
            failures.append(f'Developer-local settings must not be imported: {relative}')
            continue
        path = (ROOT / relative).resolve()
        if not path.is_relative_to(ROOT / 'upstream') or relative in seen:
            failures.append(f'Invalid or duplicate import path: {relative}')
            continue
        seen.add(relative)
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            failures.append(f'Changed or missing frozen import: {relative}')
    actual = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'upstream/RedSalamander').rglob('*') if p.is_file()}
    if actual != seen:
        failures.append('Imported file inventory does not match the frozen snapshot')
    for folder in ('src', 'include'):
        for path in (ROOT / folder).rglob('*'):
            if path.suffix not in ('.h', '.cpp'):
                continue
            text = path.read_text(encoding='utf-8')
            for name in re.findall(r'#include\s*[<"]([^">]+)[">]', text):
                if any(x.lower() in name.lower() for x in ('upstream', 'redsalamander', 'redxe', 'helpers.h', 'PlugInterfaces/Viewer')):
                    failures.append(f'{path.relative_to(ROOT)}: application/reference include {name}')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Validated {len(seen)} immutable imports, dependency pin and active-source independence')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
