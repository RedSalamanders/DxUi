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
        'Specs/Core/Core_Documentation.md', 'Specs/UI/UI_DesignSystem.md', 'Specs/DesignSystem/README.md', 'docs/README.md', 'docs/controls.md', 'docs/getting-started.md',
        'docs/hosting.md', 'docs/performance.md', 'docs/gallery/README.md', 'docs/samples.md', 'Measurements/README.md',
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
    failures.extend(validate_design_system(ROOT))
    failures.extend(validate_measurements(ROOT))
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Validated {len(files)} Markdown files, {len(required)} authority files and {len(expected)} active plans')
    return 0


def markdown_prose(content):
    # C++ lambdas such as [](bool checked) are code, not Markdown links.
    return re.sub(r'^(`{3,}|~{3,})[^\n]*\n.*?^\1[ \t]*$', '', content, flags=re.MULTILINE | re.DOTALL)


def validate_measurements(root):
    failures = []
    if any((root / 'docs/measurements').rglob('*.json')):
        failures.append('Raw measurements belong in Measurements or their consumer repository, not docs')
    for path in (root / 'Measurements').rglob('*.json'):
        if path.name.endswith('.comparison.json'):
            continue
        try:
            receipt = json.loads(path.read_text(encoding='utf-8-sig'))
            if receipt.get('workloadOwner') != 'DxUi' or not receipt.get('fixture', '').startswith('dxui-'):
                failures.append(f'Non-library measurement: {path.relative_to(root)}')
            inputs = receipt.get('benchmarkInputs', {})
            if not inputs or any(not re.fullmatch(r'[0-9a-fA-F]{64}', value) for value in inputs.values()):
                failures.append(f'Missing fixture input hashes: {path.relative_to(root)}')
            if not (path.parent / 'README.md').is_file():
                failures.append(f'Missing measurement explanation: {path.relative_to(root)}')
        except (OSError, ValueError, TypeError, AttributeError) as error:
            failures.append(f'Invalid measurement {path.relative_to(root)}: {error}')
    return failures


def catalog_controls(root):
    catalog = (root / 'include/DxUi/ControlCatalog.h').read_text(encoding='utf-8')
    names = re.search(r'enum class ControlKind[^\{]*\{([^}]+)', catalog).group(1)
    return [name.strip() for name in names.split(',') if name.strip()]


def validate_design_system(root):
    # Every catalog control has design-system guidelines and a preview; no stale component remains.
    failures = []
    system = root / 'Specs/DesignSystem'
    try:
        controls = catalog_controls(root)
        index = json.loads((system / 'design-system.json').read_text(encoding='utf-8-sig'))
        tokens = json.loads((system / 'tokens.json').read_text(encoding='utf-8-sig'))
        if index.get('layout') != 'files' or not index.get('title'):
            failures.append('Invalid design-system index')
        if not tokens['color']['themes'] or not tokens['color']['tokens']:
            failures.append('Design-system tokens need themes and colors')
        if not (system / 'README.md').read_text(encoding='utf-8').strip():
            failures.append('Empty design-system README')
        components = system / 'components'
        for name in controls:
            guide = components / name / 'README.md'
            preview = components / name / 'preview.html'
            if not guide.is_file() or not preview.is_file():
                failures.append(f'Missing design-system component: {name}')
            elif not preview.read_text(encoding='utf-8').startswith('<!-- @dsCard '):
                failures.append(f'Design-system preview lacks its card marker: {name}')
        known = set(controls) | {'Cover'}
        for folder in components.iterdir():
            if folder.is_dir() and folder.name not in known:
                failures.append(f'Design-system component is not in the catalog: {folder.name}')
    except (OSError, ValueError, KeyError, AttributeError, TypeError) as error:
        failures.append(f'Invalid design system: {error}')
    return failures


def validate_docs(root):
    failures = []
    try:
        if '](docs/README.md)' not in (root / 'README.md').read_text(encoding='utf-8-sig'):
            failures.append('Root README must link docs/README.md')
        controls = catalog_controls(root)
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
