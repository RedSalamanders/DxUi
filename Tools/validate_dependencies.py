"""Validate source ownership and dependency boundaries without freezing editable source."""
from pathlib import Path, PurePosixPath
import json
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PENDING_ROOTS = ()


def owned_path(relative):
    parsed = PurePosixPath(relative)
    if not relative or '\\' in relative or parsed.is_absolute() or '..' in parsed.parts:
        return None
    if (relative != '.clang-format' and parsed.parts[0] not in ('src', 'include', 'Tests', 'Samples')) or parsed.suffix.lower() in ('.user', '.suo'):
        return None
    resolved = (ROOT / relative).resolve()
    return resolved if resolved.is_relative_to(ROOT.resolve()) else None


def pending_path(path):
    return any(path.is_relative_to((ROOT / folder).resolve()) for folder in PENDING_ROOTS)


def main():
    failures = []
    manifest = json.loads((ROOT / 'vcpkg.json').read_text(encoding='utf-8'))
    tool = json.loads((ROOT / 'vcpkg-tool.json').read_text(encoding='utf-8'))
    if manifest['builtin-baseline'] != tool['commit'] or not re.fullmatch('[0-9a-f]{40}', tool['commit']):
        failures.append('vcpkg baseline/tool revision mismatch')
    provenance = json.loads((ROOT / 'Specs/Done/SourceImport/source-origin.json').read_text(encoding='utf-8'))
    if provenance.get('schemaVersion') != 2 or not re.fullmatch('[0-9a-f]{40}', provenance['commit']):
        failures.append('Source origin needs schema 2 and an exact historical commit')
    origins = set()
    destinations = set()
    for entry in provenance['files']:
        original = entry['source']
        if original in origins or not re.fullmatch('[0-9a-f]{64}', entry.get('originalSha256', '')):
            failures.append(f'Invalid or duplicate source origin: {original}')
        origins.add(original)
        if not isinstance(entry.get('originalBytes'), int) or entry['originalBytes'] < 0:
            failures.append(f'Invalid historical size: {original}')
        if entry.get('disposition') == 'retired':
            if not entry.get('reason') or 'currentPath' in entry:
                failures.append(f'Retired source requires a reason and no current path: {original}')
            continue
        relative = entry.get('currentPath', '')
        path = owned_path(relative)
        if entry.get('disposition') not in ('owned', 'consolidated') or path is None or not path.is_file():
            failures.append(f'Missing or invalid owned source: {relative}')
        if relative in destinations:
            failures.append(f'Duplicate current source path: {relative}')
        destinations.add(relative)
    if (ROOT / 'upstream').exists():
        failures.append('DxUi owns its source; remove the duplicate upstream tree')

    debt = json.loads((ROOT / 'Specs/Done/SourceImport/pending-dependencies.json').read_text(encoding='utf-8'))
    allowed = {}
    if debt.get('schemaVersion') != 1:
        failures.append('Pending dependency inventory needs schema 1')
    for entry in debt['files']:
        relative = entry['path']
        path = owned_path(relative)
        includes = entry['includes']
        if path is None or not pending_path(path) or not path.is_file() or relative in allowed:
            failures.append(f'Invalid or duplicate pending dependency path: {relative}')
        if not includes or sorted(set(includes)) != includes:
            failures.append(f'Pending dependencies must be sorted and unique: {relative}')
        allowed[relative] = set(includes)
    observed = {}
    for folder in ('src', 'include', 'Tests', 'Samples'):
        for path in (ROOT / folder).rglob('*'):
            if path.suffix.lower() in ('.user', '.suo'):
                failures.append(f'Developer-local settings in owned source: {path.relative_to(ROOT)}')
            if path.suffix not in ('.h', '.cpp'):
                continue
            text = path.read_text(encoding='utf-8-sig')
            relative = path.relative_to(ROOT).as_posix()
            if 'RedSalamander::DxUi' in text:
                failures.append(f'Use the owned DxUi namespace: {relative}')
            missing = set()
            for opener, name in re.findall(r'#include\s*([<"])([^">]+)[">]', text):
                if opener == '"':
                    candidates = [(path.parent / name).resolve(), (ROOT / 'include' / name).resolve()]
                    resolved = next((p for p in candidates if p.is_file()), None)
                    if resolved is None:
                        missing.add(name)
                    elif not resolved.is_relative_to(ROOT.resolve()):
                        failures.append(f'Include escapes this repository: {relative}: {name}')
                    elif not pending_path(path.resolve()) and pending_path(resolved):
                        failures.append(f'Supported source includes pending controls: {relative}: {name}')
                if not pending_path(path.resolve()) and (name == 'Helpers.h' or any(x in name.lower() for x in ('upstream', 'redsalamander', 'redxe', 'pluginterfaces/viewer'))):
                    failures.append(f'Application include in supported source: {relative}: {name}')
            if missing:
                observed[relative] = missing
    if observed != allowed:
        for path in sorted(set(observed) | set(allowed)):
            if observed.get(path, set()) != allowed.get(path, set()):
                failures.append(f'Unresolved include inventory mismatch: {path}: actual={sorted(observed.get(path, set()))}; recorded={sorted(allowed.get(path, set()))}')
    # Compilable controls are normal source. Reject missing/escaping build inputs and split static libraries.
    libraries = []
    for folder in ('src', 'Tests', 'Samples'):
        for project in (ROOT / folder).rglob('*.vcxproj'):
            tree = ET.parse(project)
            if any(item.tag.rsplit('}', 1)[-1] == 'ConfigurationType' and item.text == 'StaticLibrary' for item in tree.iter()):
                libraries.append(project)
            for item in tree.iter():
                if item.tag.rsplit('}', 1)[-1] != 'ClCompile' or 'Include' not in item.attrib:
                    continue
                path = (project.parent / item.attrib['Include'].replace('\\', '/')).resolve()
                if not path.is_relative_to(ROOT.resolve()) or not path.is_file():
                    failures.append(f'Invalid build source: {project.relative_to(ROOT)}: {item.attrib["Include"]}')
    if len(libraries) > 1:
        failures.append('DxUi ships one static library; split targets are not supported')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Validated {len(origins)} historical origins, {len(destinations)} owned paths, {len(allowed)} pending dependency records and supported-source independence')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
