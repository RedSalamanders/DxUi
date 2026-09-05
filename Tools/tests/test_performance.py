import copy
import unittest
from test_validation import load_tool

tool = load_tool('compare_performance')


class PerformanceTests(unittest.TestCase):
    def setUp(self):
        self.before = {key: 'fixture' for key in tool.IDENTITY}
        self.before.update(roundCount=5, framesPerRound=40, hiddenPreparations=0, hiddenComposites=0,
                           sourceCommit='a', sourceFingerprint='a', executableSha256='a', benchmarkSha256='a')
        metrics = {key: 100 for key in tool.METRICS}
        metrics['composeAllocations'] = 0
        self.before['scenarios'] = [{'name': name, 'rounds': [dict(metrics) for _ in range(5)]} for name in ('clean', 'dirty')]
        self.after = copy.deepcopy(self.before)

    def change(self, metric, value):
        for row in self.after['scenarios'][1]['rounds']:
            row[metric] = value

    def test_identical_and_new_source_are_comparable(self):
        self.after['sourceCommit'] = 'b'
        self.after['sourceFingerprint'] = 'b'
        self.assertEqual(tool.compare(self.before, self.after)['status'], 'within-noise-budget')

    def test_fps_regression_requires_advice(self):
        self.change('fps', 80)
        self.assertEqual(tool.compare(self.before, self.after)['status'], 'advice-required')

    def test_memory_regression_requires_advice(self):
        self.change('privateBytes', 104)
        self.assertEqual(tool.compare(self.before, self.after)['status'], 'advice-required')

    def test_surface_growth_has_no_tolerance(self):
        self.change('surfaceBytes', 101)
        self.assertEqual(tool.compare(self.before, self.after)['status'], 'advice-required')

    def test_improvement_passes(self):
        self.change('fps', 120)
        self.assertEqual(tool.compare(self.before, self.after)['status'], 'within-noise-budget')

    def test_mismatched_fixture_rejected(self):
        for key in ('machine', 'configuration', 'compiler', 'benchmarkSha256'):
            candidate = copy.deepcopy(self.after)
            candidate[key] = 'different'
            with self.subTest(key=key), self.assertRaises(ValueError):
                tool.compare(self.before, candidate)

    def test_missing_rounds_rejected(self):
        self.after['scenarios'][0]['rounds'].pop()
        with self.assertRaises(ValueError):
            tool.compare(self.before, self.after)

    def test_missing_or_nonfinite_metric_rejected(self):
        for value in (None, float('nan'), float('inf'), -1):
            self.change('fps', value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                tool.compare(self.before, self.after)

    def test_allocating_composition_rejected(self):
        self.change('composeAllocations', 1)
        with self.assertRaises(ValueError):
            tool.compare(self.before, self.after)

    def test_hidden_work_rejected(self):
        self.after['hiddenPreparations'] = 1
        with self.assertRaises(ValueError):
            tool.compare(self.before, self.after)
