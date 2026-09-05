"""Validate repo-local skill metadata without depending on a user's Codex installation."""
from pathlib import Path
import re
import sys

try:
    import yaml
except ImportError:
    raise SystemExit('Install validator dependencies: py -3 -m pip install -r Tools/requirements-validation.txt')

ROOT = Path(__file__).resolve().parents[1]


def main():
    failures = []
    skills = sorted((ROOT / '.agents/skills').iterdir())
    for folder in skills:
        if not folder.is_dir():
            continue
        path = folder / 'SKILL.md'
        if not path.is_file():
            failures.append(f'{folder.name}: missing SKILL.md')
            continue
        text = path.read_text(encoding='utf-8')
        parts = text.split('---', 2)
        try:
            metadata = yaml.safe_load(parts[1]) if len(parts) == 3 and not parts[0].strip() else None
        except yaml.YAMLError as error:
            failures.append(f'{folder.name}: {error}')
            continue
        if not isinstance(metadata, dict):
            failures.append(f'{folder.name}: missing YAML front matter')
            continue
        name = metadata.get('name', '')
        description = metadata.get('description', '')
        if name != folder.name or not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', name) or len(name) > 64:
            failures.append(f'{folder.name}: invalid name')
        if not isinstance(description, str) or not description.strip() or len(description) > 1024:
            failures.append(f'{folder.name}: invalid description')
        if not parts[2].strip():
            failures.append(f'{folder.name}: empty instructions')
        print(f'Validated {folder.name}')
    if not skills:
        failures.append('No skills found')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
