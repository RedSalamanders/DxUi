"""Exercise owned-source evolution and dependency boundaries in isolated fixtures."""
from pathlib import Path
import contextlib
import hashlib
import importlib.util
import io
import json
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'Tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ValidatorTests(unittest.TestCase):
    def setUp(self):
        scratch = (ROOT / '.build' / 'tooling-fixtures').resolve()
        scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.root = Path(self.temporary.name).resolve()
        assert self.root.is_relative_to(scratch)

    def tearDown(self):
        self.temporary.cleanup()

    def put(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value, encoding='utf-8', newline='\n')

    def run_tool(self, tool):
        module = load_tool(tool)
        module.ROOT = self.root
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return module.main()

    def dependency_fixture(self):
        revision = 'a' * 40
        self.put('vcpkg.json', json.dumps({'builtin-baseline': revision}))
        self.put('vcpkg-tool.json', json.dumps({'commit': revision}))
        self.put('src/Controls/Control.cpp', 'original\n')
        self.put('Specs/Done/SourceImport/source-origin.json', json.dumps({'schemaVersion': 2, 'commit': revision, 'files': [{
            'source': 'Common/DxUi/Control.cpp',
            'currentPath': 'src/Controls/Control.cpp',
            'disposition': 'owned',
            'originalSha256': hashlib.sha256(b'original\n').hexdigest(),
            'originalBytes': 9,
        }]}))
        self.put('Specs/Done/SourceImport/pending-dependencies.json', json.dumps({'schemaVersion': 1, 'files': []}))

    def change_origin(self, **changes):
        manifest = json.loads((self.root / 'Specs/Done/SourceImport/source-origin.json').read_text())
        manifest['files'][0].update(changes)
        self.put('Specs/Done/SourceImport/source-origin.json', json.dumps(manifest))

    def test_owned_source_can_evolve_without_rewriting_historical_hash(self):
        self.dependency_fixture()
        self.put('src/Controls/Control.cpp', '// independent improvements\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 0)

    def test_new_owned_source_does_not_require_fake_historical_origin(self):
        self.dependency_fixture()
        self.put('src/New.cpp', '#include <cstdint>\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 0)

    def test_missing_owned_source_is_rejected(self):
        self.dependency_fixture()
        self.change_origin(currentPath='src/missing.cpp')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_parent_path_cannot_escape_owned_tree(self):
        self.dependency_fixture()
        self.change_origin(currentPath='../elsewhere.cpp')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_developer_settings_cannot_be_owned_source(self):
        self.dependency_fixture()
        self.put('src/Controls/project.user', 'settings\n')
        self.change_origin(currentPath='src/Controls/project.user')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_duplicate_original_source_tree_is_rejected(self):
        self.dependency_fixture()
        self.put('upstream/original.cpp', 'duplicate\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_application_debt_cannot_be_reintroduced(self):
        self.dependency_fixture()
        self.put('src/Controls/Control.cpp', '#include "Helpers.h"\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)
        self.put('Specs/Done/SourceImport/pending-dependencies.json', json.dumps({'schemaVersion': 1, 'files': [{
            'path': 'src/Controls/Control.cpp', 'includes': ['Helpers.h'],
        }]}))
        self.assertEqual(self.run_tool('validate_dependencies'), 1)
        self.put('src/Controls/Control.cpp', '#include <cstdint>\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_supported_source_cannot_waive_application_dependency(self):
        self.dependency_fixture()
        self.put('src/Foundation/one.cpp', '#include "Helpers.h"\n')
        self.put('Specs/Done/SourceImport/pending-dependencies.json', json.dumps({'schemaVersion': 1, 'files': [{
            'path': 'src/Foundation/one.cpp', 'includes': ['Helpers.h'],
        }]}))
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_independent_controls_can_enter_single_supported_build(self):
        self.dependency_fixture()
        self.put('src/Bad.vcxproj', '<Project><ClCompile Include="Controls/Control.cpp" /></Project>')
        self.assertEqual(self.run_tool('validate_dependencies'), 0)

    def test_owned_source_can_use_other_owned_headers(self):
        self.dependency_fixture()
        self.put('src/Controls/Control.h', '#pragma once\n')
        self.put('src/Foundation/one.cpp', '#include "../Controls/Control.h"\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 0)

    def test_old_application_namespace_is_rejected(self):
        self.dependency_fixture()
        self.put('src/Controls/Control.cpp', 'namespace RedSalamander::DxUi {}\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_invalid_skill_metadata_is_rejected(self):
        self.put('.agents/skills/example/SKILL.md', '---\nname: wrong\ndescription: Useful task.\n---\nDo work.\n')
        self.assertEqual(self.run_tool('validate_skills'), 1)

    def test_valid_skill_metadata(self):
        self.put('.agents/skills/example/SKILL.md', '---\nname: example\ndescription: Useful task.\n---\nDo work.\n')
        self.assertEqual(self.run_tool('validate_skills'), 0)


if __name__ == '__main__':
    unittest.main()
