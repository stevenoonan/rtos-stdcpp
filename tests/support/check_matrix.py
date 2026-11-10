#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Negative controls for shared matrix evidence and offline input verification."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import matrix_support as matrix


class MatrixControls(unittest.TestCase):
    def test_success_and_failure_are_recorded(self):
        with tempfile.TemporaryDirectory() as directory:
            results = matrix.MatrixResults(Path(directory))
            results.run('success', [sys.executable, '-c', 'print("ok")'], 'fixture')
            with self.assertRaises(RuntimeError) as failure:
                results.run('failure', [sys.executable, '-c', 'raise SystemExit(7)'], 'fixture')
            self.assertEqual(1, results.finish(failure.exception))
            report = json.loads((Path(directory) / 'results.json').read_text())
            self.assertEqual('failed', report['status'])
            self.assertEqual([0, 7], [check['returncode'] for check in report['checks']])
            self.assertEqual(['passed', 'failed'], [check['status'] for check in report['checks']])

    def test_timeout_is_recorded_as_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            results = matrix.MatrixResults(Path(directory))
            with patch.object(matrix.subprocess, 'run', side_effect=subprocess.TimeoutExpired('fixture', 240)):
                with self.assertRaises(RuntimeError):
                    results.run('timeout', ['fixture'], 'fixture')
            self.assertTrue(results.report['checks'][0]['timed_out'])
            self.assertEqual('failed', results.report['checks'][0]['status'])

    def test_existing_evidence_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / 'keep.txt'
            marker.write_text('keep')
            with self.assertRaises(ValueError):
                matrix.MatrixResults(Path(directory))
            self.assertEqual('keep', marker.read_text())
        with self.assertRaises(ValueError):
            matrix.MatrixResults(matrix.ROOT)

    def test_wrong_qemu_is_rejected_before_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'root/usr/bin/qemu-system-arm'
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b'wrong emulator')
            with patch.object(matrix.subprocess, 'check_output') as execute:
                with self.assertRaisesRegex(RuntimeError, 'QEMU'):
                    matrix.verify_qemu(Path(directory))
                execute.assert_not_called()

    def test_wrong_compiler_and_kernel_are_rejected(self):
        with patch.object(matrix.subprocess, 'check_output', return_value='wrong'):
            with self.assertRaisesRegex(RuntimeError, 'Compiler'):
                matrix.verify_compiler(Path('/tmp'))
            for backend in ['FreeRTOS', 'ThreadX']:
                with self.assertRaisesRegex(RuntimeError, backend):
                    matrix.verify_kernel(Path('/tmp'), backend)


if __name__ == '__main__':
    unittest.main()
