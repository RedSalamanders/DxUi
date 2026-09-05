"""Validate inherited-test accounting, not control implementation spelling or historical file hashes."""
from pathlib import Path, PurePosixPath
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    data = json.loads((ROOT / 'provenance/test-port.json').read_text(encoding='utf-8'))
    errors, seen = [], set()
    cases = data['tests']
    if data.get('schemaVersion') != 1 or data.get('originCaseCount') != len(cases):
        errors.append('Every inherited case requires a disposition; origin count differs')
    if not re.fullmatch('[0-9a-f]{40}', data.get('originCommit', '')):
        errors.append('Historical test origin must be an exact commit')
    for case in cases:
        relative = PurePosixPath(case['file'])
        identity = (case['file'], case['test'])
        if identity in seen:
            errors.append(f'Duplicate test origin: {identity}')
        seen.add(identity)
        path = (ROOT / relative).resolve()
        if relative.is_absolute() or '..' in relative.parts or not path.is_relative_to((ROOT / 'Tests/Controls').resolve()) or not path.is_file():
            errors.append(f'Invalid owned test source: {relative}')
            continue
        if case['status'] not in ('ported', 'excluded') or not case.get('reason'):
            errors.append(f'Test needs status and rationale: {identity}')
        if case['status'] == 'ported':
            name = case.get('currentTest', case['test'])
            if not re.search(r'\bvoid\s+' + re.escape(name) + r'\s*\(\s*\)', path.read_text(encoding='utf-8-sig')):
                errors.append(f'Retained case is missing: {identity} -> {name}')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    retained = sum(c['status'] == 'ported' for c in cases)
    print(f'Accounted for {len(cases)} inherited cases: {retained} retained, {len(cases)-retained} excluded with reasons')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
