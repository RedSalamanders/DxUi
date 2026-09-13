import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('matrix', Path(__file__).resolve().parents[1] / 'validate_build_matrix.py')
matrix = importlib.util.module_from_spec(spec)
spec.loader.exec_module(matrix)


class BuildMatrixTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_every_project_requires_real_configuration_entries(self):
        path = self.root / 'library.vcxproj'
        def write(configurations):
            path.write_text('<Project><ItemGroup>' + ''.join(f'<ProjectConfiguration Include="{c}" />' for c in configurations)
                            + '</ItemGroup></Project>', encoding='utf-8')
        write(sorted(matrix.MATRIX))
        self.assertEqual(matrix.validate_project(path), [])
        write(sorted(matrix.MATRIX - {'ASan Debug|ARM64'}))
        self.assertTrue(any('ASan Debug|ARM64' in e for e in matrix.validate_project(path)))
        write(sorted(matrix.MATRIX) + ['Debug|x64'])
        self.assertTrue(any('duplicate' in e for e in matrix.validate_project(path)))

    def test_silent_debug_fallback_and_missing_build_are_rejected(self):
        path = self.root / 'consumer.sln'
        configurations = sorted(matrix.MATRIX)
        header = 'Project("{TYPE}") = "app", "app.vcxproj", "{APP}"\nEndProject\n'
        header += 'GlobalSection(SolutionConfigurationPlatforms) = preSolution\n'
        header += ''.join(f'  {c} = {c}\n' for c in configurations) + 'EndGlobalSection\n'
        mappings = ''.join(f'  {{APP}}.{c}.{kind} = {c}\n' for c in configurations for kind in ('ActiveCfg', 'Build.0'))
        path.write_text(header + mappings, encoding='utf-8')
        self.assertEqual(matrix.validate_solution(path), [])
        path.write_text((header + mappings).replace('ASan Debug|ARM64.ActiveCfg = ASan Debug|ARM64',
                                                  'ASan Debug|ARM64.ActiveCfg = Debug|ARM64'), encoding='utf-8')
        self.assertTrue(any('maps to' in e for e in matrix.validate_solution(path)))
        path.write_text((header + mappings).replace('  {APP}.Release|ARM64.Build.0 = Release|ARM64\n', ''), encoding='utf-8')
        self.assertTrue(any('Build.0' in e for e in matrix.validate_solution(path)))
