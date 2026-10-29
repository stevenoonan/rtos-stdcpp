#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prove that target/report/timeout faults cannot produce a successful run."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import resource
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('protocol', ROOT / 'tools/test_protocol.py')
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixture', required=True, type=Path)
args = parser.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix='rtos-stdcpp-runner-') as temporary:
    observed = []
    for mode in ['pass', 'fail', 'repeat-fail', 'timeout', 'crash', 'exit', 'exit-after-report', 'incomplete', 'missing', 'wrong-count']:
        output = Path(temporary) / (mode + '.json')
        command = [sys.executable, str(ROOT / 'tools/run_tests.py'), '--suite', 'fixture',
                   '--layer', 'emulator', '--timeout', '0.2' if mode == 'timeout' else '5',
                   '--output', str(output), '--', str(args.fixture), mode]
        result = subprocess.run(command, capture_output=True, text=True, timeout=8)
        evidence = json.loads(output.read_text())
        expected = mode == 'pass'
        if (result.returncode == 0) != expected or (evidence['status'] == 'passed') != expected:
            raise RuntimeError('Broken process/report gate: ' + mode + '\n' + result.stdout + result.stderr)
        reasons = {'fail': 'Test assertions failed', 'repeat-fail': 'Test assertions failed',
                   'timeout': 'timed out', 'crash': 'exited unsuccessfully',
                   'exit': 'exactly one complete structured report', 'exit-after-report': 'exited unsuccessfully',
                   'incomplete': 'Incomplete test run', 'missing': 'Missing/incomplete case',
                   'wrong-count': 'Inconsistent summary counts'}
        if not expected and reasons[mode] not in evidence.get('error', ''):
            raise RuntimeError('Negative control rejected for the wrong reason: ' + mode)
        if mode == 'pass':
            valid = evidence['report']
        observed.append({'mode': mode, 'expected_success': expected, 'returncode': result.returncode,
                         'rejection': evidence.get('error')})
    # Required-case omission/skip/duplicates and forged aggregate counts are
    # checked independently of process exit, including optimized Python.
    invalid = []
    for mode in ['duplicate', 'skip-required', 'no-checks', 'bad-type', 'missing-record', 'unknown-id']:
        report = copy.deepcopy(valid)
        if mode == 'duplicate': report['results'].append(copy.deepcopy(report['results'][0]))
        if mode == 'skip-required': report['results'][0]['state'] = 3
        if mode == 'no-checks': report['results'][0]['checks'] = 0
        if mode == 'bad-type': report['passed'] = True
        if mode == 'missing-record': report['results'] = []
        if mode == 'unknown-id': report['results'][0]['id'] = 'unknown.case'
        try:
            protocol.validate(report, 'fixture')
        except ValueError:
            invalid.append(mode)
        else:
            raise RuntimeError('Invalid result accepted: ' + mode)
    for text in ['OK\n', 'RTOS_STDCPP_REPORT {', protocol.PREFIX + json.dumps(valid) + '\n' + protocol.PREFIX + json.dumps(valid)]:
        try:
            protocol.parse(text, 'fixture')
        except ValueError:
            pass
        else:
            raise RuntimeError('Incomplete/ambiguous console accepted')
    entries = protocol.catalog()
    words = [valid[key] for key in ['version', 'suite', 'expected', 'passed', 'failed', 'skipped', 'checks', 'complete']]
    results = {x['id']: x for x in valid['results']}
    for entry in entries:
        item = results.get(entry['id'], {'state': 0, 'checks': 0, 'failures': 0})
        words += [item[key] for key in ['state', 'checks', 'failures']]
    protocol.from_words(words, 'fixture')
    try:
        protocol.from_words(words[:-1], 'fixture')
    except ValueError:
        pass
    else:
        raise RuntimeError('Truncated debugger memory report accepted')
    print(json.dumps({'status': 'passed' , 'process_controls': observed, 'invalid_report_controls': invalid,
                      'console_controls': ['OK-only', 'truncated-json', 'duplicate-final-report']}, indent=2))
