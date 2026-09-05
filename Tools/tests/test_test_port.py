import test_validation as helpers
import json
import unittest


class TestPortTests(unittest.TestCase):
    setUp = helpers.ValidatorTests.setUp
    tearDown = helpers.ValidatorTests.tearDown
    put = helpers.ValidatorTests.put
    run_tool = helpers.ValidatorTests.run_tool
    def fixture(self):
        self.put('Tests/Controls/Example.cpp', 'void TestToggle() {}\n')
        self.manifest = {'schemaVersion': 1, 'originCommit': 'a'*40, 'originCaseCount': 1, 'tests': [
            {'file': 'Tests/Controls/Example.cpp', 'test': 'TestToggle', 'status': 'ported', 'reason': 'Runtime behavior'}]}
        self.save()

    def save(self):
        self.put('provenance/test-port.json', json.dumps(self.manifest))

    def test_retained_case_is_accounted(self):
        self.fixture()
        self.assertEqual(self.run_tool('validate_test_port'), 0)

    def test_silent_case_deletion_fails(self):
        self.fixture()
        self.put('Tests/Controls/Example.cpp', '')
        self.assertEqual(self.run_tool('validate_test_port'), 1)

    def test_discarding_disposition_fails(self):
        self.fixture()
        self.manifest['tests'] = []
        self.save()
        self.assertEqual(self.run_tool('validate_test_port'), 1)

    def test_explicit_rename_retains_origin(self):
        self.fixture()
        self.put('Tests/Controls/Example.cpp', 'void TestNeutralToggle() {}\n')
        self.manifest['tests'][0]['currentTest'] = 'TestNeutralToggle'
        self.save()
        self.assertEqual(self.run_tool('validate_test_port'), 0)

    def test_exclusion_needs_reason(self):
        self.fixture()
        self.manifest['tests'][0].update(status='excluded', reason='')
        self.save()
        self.assertEqual(self.run_tool('validate_test_port'), 1)
