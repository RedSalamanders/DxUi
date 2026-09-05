"""Check authority documents, active plan membership and local Markdown references."""
from pathlib import Path
import re
import hashlib
import json
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
        'Specs/Core/Core_Documentation.md', 'docs/README.md', 'docs/controls.md', 'docs/getting-started.md',
        'docs/hosting.md', 'docs/performance.md', 'docs/gallery/README.md',
    ]
    for name in required:
        path = ROOT / name
        if not path.is_file() or not path.read_text(encoding='utf-8').strip():
            failures.append(f'Missing or empty {name}')
    files = [p for p in ROOT.rglob('*.md') if not any(x in p.relative_to(ROOT).parts for x in ('.build', '.git'))]
    for path in files:
        content = markdown_prose(path.read_text(encoding='utf-8'))
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
    failures.extend(validate_docs(ROOT))
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Validated {len(files)} Markdown files, {len(required)} authority files and {len(expected)} active plans')
    return 0


def markdown_prose(content):
    # C++ lambdas such as [](bool checked) are code, not Markdown links.
    return re.sub(r'^(`{3,}|~{3,})[^\n]*\n.*?^\1[ \t]*$', '', content, flags=re.MULTILINE | re.DOTALL)


def validate_docs(root):
    failures = []
    try:
        if '](docs/README.md)' not in (root / 'README.md').read_text(encoding='utf-8-sig'):
            failures.append('Root README must link docs/README.md')
        catalog = (root / 'include/DxUi/ControlCatalog.h').read_text(encoding='utf-8')
        names = re.search(r'enum class ControlKind[^\{]*\{([^}]+)', catalog).group(1)
        controls = [name.strip() for name in names.split(',') if name.strip()]
        guide = (root / 'docs/controls.md').read_text(encoding='utf-8-sig')
        for name in controls:
            if not re.search(r'^\| ' + re.escape(name) + r' \|', guide, re.MULTILINE):
                failures.append(f'Missing control usage: {name}')
        gallery = root / 'docs/gallery'
        receipt = json.loads((gallery / 'generation.json').read_text(encoding='utf-8-sig'))
        if receipt['controlCount'] != len(controls):
            failures.append('Gallery/catalog control count mismatch')
        entries = receipt['images']
        if len(entries) != 6 or len({e['file'] for e in entries}) != 6:
            failures.append('Gallery requires six distinct images')
        for entry in entries:
            image = (gallery / entry['file']).resolve()
            if image.parent != gallery.resolve() or image.suffix != '.png':
                failures.append('Invalid gallery image path')
                continue
            data = image.read_bytes()
            if not data.startswith(b'\x89PNG\r\n\x1a\n') or hashlib.sha256(data).hexdigest().lower() != entry['sha256'].lower():
                failures.append(f'Gallery hash/format mismatch: {entry["file"]}')
    except (OSError, ValueError, KeyError, AttributeError, TypeError) as error:
        failures.append(f'Invalid documentation/gallery: {error}')
    return failures


if __name__ == '__main__':
    raise SystemExit(main())
