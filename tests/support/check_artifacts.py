#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check upload policy against successful, failed and interrupted matrix fixtures."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from collect_ci_artifacts import collect


def elf(kind=2):
    header = bytearray(18)
    header[:6] = b'\x7fELF\x01\x01'
    header[16:18] = kind.to_bytes(2, 'little')
    return header


class ArtifactPolicy(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.results = self.base / 'results'
        self.results.mkdir()
        self.output = self.base / 'selected'

    def write(self, relative, content):
        path = self.results / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content)
        return path

    def summary(self, status='passed', checks=None):
        self.write('results.json', json.dumps({'status': status, 'checks': checks or []}))

    def build(self, name):
        self.write(name + '/CMakeCache.txt', 'configuration')
        self.write(name + '/compile_commands.json', '[]')
        self.write(name + '/conformance.json', '{"status":"passed"}')
        self.write(name + '/junit.xml', '<testsuite tests="1" failures="0"/>')
        self.write(name + '/Testing/Temporary/LastTest.log', 'test output')
        self.write(name + '/CMakeFiles/CMakeConfigureLog.yaml', 'configure diagnostics')
        self.write(name + '/CMakeFiles/test.o', bytes(elf(1)))
        self.write(name + '/relocatable-without-extension', bytes(elf(1)))
        self.write(name + '/firmware.elf', bytes(elf()))
        self.write(name + '/firmware.map', 'map')
        self.write(name + '/tests/unit/native_test', bytes(elf(3)))
        self.write(name + '/build.ninja', 'build system')
        return self.results / name

    def test_success_keeps_reports_without_build_trees(self):
        self.build('good')
        self.summary()
        self.write('good-build.log', 'compiler output')
        self.write('library/source.json', '{"copied":"source"}')
        self.write('dependencies/kernel/compile.log', 'dependency')
        result = collect(self.results, self.output, 'success')
        self.assertEqual([], result['debugging'])
        self.assertEqual({'results.json', 'good-build.log', 'good/conformance.json',
                          'good/junit.xml', 'good/Testing/Temporary/LastTest.log'},
                         {entry['path'] for entry in result['reports']})
        self.assertFalse((self.output / 'debugging').exists())

    def test_failure_keeps_debugging_for_failed_configuration_only(self):
        bad = self.build('bad')
        self.build('good')
        self.summary('failed', [{'status': 'failed', 'command': ['ctest', '--test-dir', str(bad)]}])
        result = collect(self.results, self.output, 'failure')
        self.assertEqual(['bad'], result['failed_configurations'])
        self.assertEqual({'bad/CMakeCache.txt', 'bad/compile_commands.json', 'bad/firmware.elf',
                          'bad/firmware.map', 'bad/tests/unit/native_test',
                          'bad/CMakeFiles/CMakeConfigureLog.yaml'},
                         {entry['path'] for entry in result['debugging']})

    def test_expected_rejections_do_not_create_debugging(self):
        build = self.build('reject-language')
        self.summary(checks=[{'status': 'passed', 'returncode': 1, 'expected_rejection': 'language',
                              'command': ['cmake', '-B', str(build)]}])
        result = collect(self.results, self.output, 'success')
        self.assertEqual([], result['debugging'])

    def test_junit_and_per_image_failure_detection(self):
        self.build('junit-failed')
        self.build('image-failed')
        self.summary()
        self.write('junit-failed/junit.xml', '<testsuite failures="1"><testcase><failure/></testcase></testsuite>')
        self.write('image-failed/conformance.json', '{"status":"failed"}')
        result = collect(self.results, self.output, 'failure')
        self.assertEqual(['image-failed', 'junit-failed'], result['failed_configurations'])

    def test_ctest_timeout_without_junit(self):
        self.build('hung')
        self.summary('running')
        self.write('hung/Testing/Temporary/LastTestsFailed.log', '1:qualification')
        result = collect(self.results, self.output, 'cancelled')
        self.assertEqual(['hung'], result['failed_configurations'])

    def test_interrupted_configuration_uses_stage_log(self):
        self.build('good')
        self.build('bad-wrap')
        self.summary('running')
        self.write('bad-wrap-configure.log', 'incomplete configure')
        result = collect(self.results, self.output, 'cancelled')
        self.assertEqual(['bad-wrap'], result['failed_configurations'])

    def test_symlinks_are_not_uploaded(self):
        self.build('bad')
        self.summary('failed', [{'status': 'failed', 'command': ['cmake', '--build', str(self.results / 'bad')]}])
        outside = self.base / 'outside.json'
        outside.write_text('private')
        (self.results / 'private.json').symlink_to(outside)
        (self.results / 'bad/escape.map').symlink_to(outside)
        (self.results / 'bad/escape').symlink_to(self.base, target_is_directory=True)
        result = collect(self.results, self.output, 'failure')
        self.assertFalse(any('private' in e['path'] or 'escape' in e['path']
                             for kind in ['reports', 'debugging'] for e in result[kind]))

    def test_missing_results_and_invalid_json_keep_a_manifest(self):
        result = collect(self.base / 'absent', self.output, 'failure')
        self.assertFalse(result['results_present'])
        self.assertTrue((self.output / 'reports/artifact-manifest.json').is_file())
        self.write('results.json', '{truncated')
        result = collect(self.results, self.base / 'invalid', 'failure')
        self.assertTrue(result['warnings'])
        self.assertTrue((self.base / 'invalid/reports/results.json').is_file())

    def test_overlapping_or_nonempty_outputs_are_rejected(self):
        for output in [self.results, self.base, self.results / 'inside']:
            with self.assertRaises(ValueError):
                collect(self.results, output)
        self.output.mkdir()
        (self.output / 'keep').write_text('keep')
        with self.assertRaises(ValueError):
            collect(self.results, self.output)

    def test_cli_emits_debug_output_for_composite_action(self):
        build = self.build('bad')
        self.summary('failed', [{'status': 'failed', 'command': ['cmake', '--build', str(build)]}])
        github_output = self.base / 'github-output'
        script = Path(__file__).resolve().parents[2] / 'tools/collect_ci_artifacts.py'
        subprocess.run([sys.executable, str(script), '--results', str(self.results),
                        '--output', str(self.output), '--job-status', 'failure',
                        '--github-output', str(github_output)], check=True, capture_output=True)
        self.assertEqual('has_debugging=true\n', github_output.read_text())


if __name__ == '__main__':
    unittest.main()
