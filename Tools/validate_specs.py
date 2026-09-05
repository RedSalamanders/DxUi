"""Check authority documents, active plan membership and local Markdown references."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]


def main():
    failures = []
    required = [
        'AGENTS.md', 'Specs/README.md', 'Specs/Core/Core_Architecture.md',
        'Specs/Core/Core_PerformanceAndResources.md', 'Specs/Build/Build_ToolchainAndConsumption.md',
        'Specs/UI/UI_ControlsAndLayout.md', 'Specs/UI/UI_ThemeAndTypography.md',
        'Specs/UI/UI_InputAndAccessibility.md', 'Specs/Rendering/Rendering_EmbeddedD3D11.md',
        'Specs/Rendering/Rendering_Win32Host.md', 'Specs/Testing/Testing_Validation.md',
    ]
    for name in required:
        path = ROOT / name
        if not path.is_file() or not path.read_text(encoding='utf-8').strip():
            failures.append(f'Missing or empty {name}')
    files = [p for p in ROOT.rglob('*.md') if not any(x in p.relative_to(ROOT).parts for x in ('.build', '.git'))]
    for path in files:
        content = path.read_text(encoding='utf-8')
        for target in re.findall(r'\]\(([^)]+)\)', content):
            if re.match(r'^[a-z]+://|^#', target):
                continue
            target = unquote(target.split('#', 1)[0].strip('<>'))
            if not (path.parent / target).exists():
                failures.append(f'{path.relative_to(ROOT)}: broken reference {target}')
    active = ROOT / 'Specs/Plans/WIP'
    index = (active / 'README.md').read_text(encoding='utf-8')
    indexed = re.findall(r'\]\(([^)]+\.md)\)', index)
    expected = {p.name for p in active.glob('*.md') if p.name != 'README.md'}
    if set(indexed) != expected or len(indexed) != len(set(indexed)):
        failures.append('Every direct WIP plan must appear exactly once in its index')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Validated {len(files)} Markdown files, {len(required)} authority files and {len(expected)} active plans')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
