"""Check all live native projects/solution mappings; execution still requires native build/test evidence."""
import argparse
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

MATRIX = {f'{configuration}|{platform}' for configuration in ('Debug', 'Release', 'ASan Debug')
          for platform in ('x64', 'ARM64')}


def validate_project(path):
    configurations = []
    for item in ET.parse(path).getroot().iter():
        if item.tag.rsplit('}', 1)[-1] == 'ProjectConfiguration':
            configurations.append(item.attrib.get('Include', ''))
    errors = [f'{path}: missing {name}' for name in sorted(MATRIX - set(configurations))]
    if len(configurations) != len(set(configurations)):
        errors.append(f'{path}: duplicate project configuration')
    return errors


def validate_solution(path):
    content = path.read_text(encoding='utf-8-sig')
    projects = re.findall(r'^Project\("\{[^}]+\}"\) = "[^"]+", "([^"]+\.vcxproj)", "(\{[^}]+\})"', content, re.M)
    sections = re.findall(r'GlobalSection\(SolutionConfigurationPlatforms\) = preSolution(.*?)EndGlobalSection', content, re.S)
    declared = set(re.findall(r'^\s*(.+?)\s*=.*$', sections[0], re.M)) if sections else set()
    errors = [f'{path}: missing solution configuration {name}' for name in sorted(MATRIX - declared)]
    for name, guid in projects:
        for configuration in sorted(MATRIX):
            for kind in ('ActiveCfg', 'Build.0'):
                key = re.escape(f'{guid}.{configuration}.{kind}')
                values = re.findall(r'^\s*' + key + r'\s*=\s*(.*?)\s*$', content, re.M)
                if values != [configuration]:
                    errors.append(f'{path}: {name} {configuration}.{kind} maps to {values}, expected itself')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    paths = subprocess.check_output(['git', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard', '-z',
                                     '--', '*.vcxproj', '*.sln']).decode('utf-8').split('\0')
    paths = sorted({p for p in paths if p and not p.startswith('Specs/Plans/Done/')})
    errors = []
    for relative in paths:
        path = root / relative
        errors.extend(validate_project(path) if path.suffix == '.vcxproj' else validate_solution(path))
    if not paths:
        errors.append('No live native projects found.')
    for error in errors:
        print(error)
    print(f'{len(paths)} native project/solution files; {len(errors)} six-configuration mapping errors. '
          'Build definitions do not establish instrumentation or native runtime qualification.')
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())
