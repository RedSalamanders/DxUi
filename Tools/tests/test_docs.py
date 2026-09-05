import hashlib
import json
import unittest
import test_validation as helpers

load_tool = helpers.load_tool


class DocumentationTests(unittest.TestCase):
    setUp = helpers.ValidatorTests.setUp
    tearDown = helpers.ValidatorTests.tearDown
    put = helpers.ValidatorTests.put
    def fixture(self):
        self.put('README.md', '[Docs](docs/README.md)')
        self.put('include/DxUi/ControlCatalog.h', 'enum class ControlKind { Label, Button };')
        self.put('docs/controls.md', '| Label | Caption |\n| Button | Action |\n')
        image = b'\x89PNG\r\n\x1a\nfixture'
        folder = self.root / 'docs/gallery'
        folder.mkdir(parents=True)
        entries = []
        for i in range(6):
            name = f'{i}.png'
            (folder / name).write_bytes(image)
            entries.append({'file': name, 'sha256': hashlib.sha256(image).hexdigest()})
        self.put('docs/gallery/generation.json', json.dumps({'controlCount': 2, 'images': entries}))

    def test_complete_docs(self):
        self.fixture()
        self.assertEqual(load_tool('validate_specs').validate_docs(self.root), [])

    def test_missing_usage_fails(self):
        self.fixture()
        self.put('docs/controls.md', '| Label | Caption |\n')
        self.assertIn('Missing control usage: Button', load_tool('validate_specs').validate_docs(self.root))

    def test_changed_gallery_bytes_fail(self):
        self.fixture()
        self.put('docs/gallery/0.png', 'invalid image')
        self.assertTrue(load_tool('validate_specs').validate_docs(self.root))

    def test_lambdas_are_not_links(self):
        text = '```cpp\nx.SetOnClick([](bool checked) {});\n```\n[Docs](docs/README.md)'
        result = load_tool('validate_specs').markdown_prose(text)
        self.assertNotIn('checked', result)
        self.assertIn('[Docs]', result)

    def test_raw_measurements_cannot_return_to_docs(self):
        self.put('docs/measurements/application/run.json', '{}')
        self.assertTrue(load_tool('validate_specs').validate_measurements(self.root))

    def test_measurements_require_library_ownership_and_explanation(self):
        receipt = {'workloadOwner': 'DxUi', 'fixture': 'dxui-complex-ui-v2', 'benchmarkInputs': {'scene.h': 'a' * 64}}
        self.put('Measurements/example/run.json', json.dumps(receipt))
        tool = load_tool('validate_specs')
        self.assertTrue(tool.validate_measurements(self.root))
        self.put('Measurements/example/README.md', 'Independent synthetic scene; offscreen only.')
        self.assertEqual(tool.validate_measurements(self.root), [])
        receipt['workloadOwner'] = 'Application'
        self.put('Measurements/example/run.json', json.dumps(receipt))
        self.assertTrue(tool.validate_measurements(self.root))
