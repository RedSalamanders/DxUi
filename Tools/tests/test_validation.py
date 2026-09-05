"""Exercise corrupted imports and malformed metadata in isolated repository-owned fixtures."""
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
        self.put('upstream/RedSalamander/source.cpp', 'original\n')
        self.put('provenance/source-import.json', json.dumps({'commit': revision, 'files': [{
            'importedPath': 'upstream/RedSalamander/source.cpp',
            'sha256': hashlib.sha256(b'original\n').hexdigest(),
        }]}))

    def test_clean_import_and_active_source(self):
        self.dependency_fixture()
        self.put('src/one.cpp', '#include <cstdint>\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 0)

    def test_tampered_reference_is_rejected(self):
        self.dependency_fixture()
        self.put('upstream/RedSalamander/source.cpp', 'changed\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_uninventoried_reference_is_rejected(self):
        self.dependency_fixture()
        self.put('upstream/RedSalamander/extra.cpp', 'extra\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_application_dependency_is_rejected(self):
        self.dependency_fixture()
        self.put('src/one.cpp', '#include "RedSalamander/Helpers.h"\n')
        self.assertEqual(self.run_tool('validate_dependencies'), 1)

    def test_invalid_skill_metadata_is_rejected(self):
        self.put('.agents/skills/example/SKILL.md', '---\nname: wrong\ndescription: Useful task.\n---\nDo work.\n')
        self.assertEqual(self.run_tool('validate_skills'), 1)

    def test_valid_skill_metadata(self):
        self.put('.agents/skills/example/SKILL.md', '---\nname: example\ndescription: Useful task.\n---\nDo work.\n')
        self.assertEqual(self.run_tool('validate_skills'), 0)


if __name__ == '__main__':
    unittest.main()
