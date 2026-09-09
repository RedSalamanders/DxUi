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
        self.put('Specs/Done/SourceImport/test-port.json', json.dumps(self.manifest))

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

    def policy_fixture(self):
        self.fixture()
        self.manifest['tests'][0].update(status='excluded', reason='Source-text implementation assertion')
        self.save()
        self.policy = {'schemaVersion': 1, 'dispositions': [{
            'originFile': 'Tests/Controls/Example.cpp', 'originTest': 'TestToggle',
            'decision': 'runtime-restored', 'rationale': 'Behavior replaces spelling',
            'runtimeCases': [{'file': 'Tests/Controls/Example.cpp', 'test': 'TestToggle'}]}]}
        self.save_policy()

    def save_policy(self):
        self.put('Specs/Testing/SourcePolicyDispositions.json', json.dumps(self.policy))

    def test_current_runtime_replacement_passes(self):
        self.policy_fixture()
        self.assertEqual(self.run_tool('validate_test_port'), 0)

    def test_missing_current_policy_fails(self):
        self.policy_fixture()
        self.policy['dispositions'] = []
        self.save_policy()
        self.assertEqual(self.run_tool('validate_test_port'), 1)

    def test_deleted_replacement_fails(self):
        self.policy_fixture()
        self.put('Tests/Controls/Example.cpp', '')
        self.assertEqual(self.run_tool('validate_test_port'), 1)

    def test_duplicate_policy_fails(self):
        self.policy_fixture()
        self.policy['dispositions'] *= 2
        self.save_policy()
        self.assertEqual(self.run_tool('validate_test_port'), 1)
