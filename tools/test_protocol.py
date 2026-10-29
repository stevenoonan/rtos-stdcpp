#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Strict shared validation for stdout and debugger-memory test transports."""
import json
from pathlib import Path

CATALOG = Path(__file__).resolve().parents[1] / 'tests/catalog.json'
PREFIX = 'RTOS_STDCPP_REPORT '


def catalog():
    return json.loads(CATALOG.read_text())['cases']


def validate(report, suite):
    entries = catalog()
    suites = list(dict.fromkeys(x['suite'] for x in entries))
    selected = [x for x in entries if x['suite'] == suite]
    def require(condition, message):
        if not condition:
            raise ValueError(message)
    require(bool(selected), 'Unknown or empty test suite')
    require(isinstance(report, dict), 'Report must be an object')
    for key in ['version', 'suite', 'expected', 'passed', 'failed', 'skipped', 'checks', 'complete']:
        require(type(report.get(key)) is int and 0 <= report[key] <= 0xffffffff, 'Invalid report field: ' + key)
    require(report['version'] == 1 and report['suite'] == suites.index(suite), 'Wrong report schema/suite')
    require(report['complete'] == 1, 'Incomplete test run')
    require(report['expected'] == len(selected), 'Wrong expected case count')
    results = report.get('results')
    require(isinstance(results, list) and len(results) == len(selected), 'Missing or extra test cases')
    require(all(isinstance(x, dict) and isinstance(x.get('id'), str) for x in results), 'Invalid result record')
    actual = [x['id'] for x in results]
    require(len(set(actual)) == len(actual) and set(actual) == {x['id'] for x in selected}, 'Missing, duplicate or unknown case IDs')
    by_id = {x['id']: x for x in results}
    counts = {1: 0, 2: 0, 3: 0}
    checks = 0
    enriched = []
    for item in selected:
        result = by_id[item['id']]
        for key in ['state', 'checks', 'failures']:
            require(type(result.get(key)) is int and 0 <= result[key] <= 0xffffffff, 'Invalid case field: ' + key)
        state = result['state']
        require(state in counts, 'Missing/incomplete case: ' + item['id'])
        if 'skip_reason' in item:
            require(state == 3 and result['checks'] == result['failures'] == 0, 'Deferred case executed or falsely reported')
        else:
            require(state != 3, 'Required case skipped: ' + item['id'])
            require(result['checks'] > 0 and result['failures'] <= result['checks'], 'Case executed no checks')
            require((state == 1) == (result['failures'] == 0), 'Inconsistent case failure state')
        counts[state] += 1
        checks += result['checks']
        enriched.append({**result, 'requirements': item['requirements'],
                         **({'skip_reason': item['skip_reason']} if 'skip_reason' in item else {})})
    require((report['passed'], report['failed'], report['skipped'], report['checks']) ==
            (counts[1], counts[2], counts[3], checks), 'Inconsistent summary counts')
    require(counts[2] == 0, 'Test assertions failed')
    return {**report, 'suite_name': suite, 'results': enriched}


def parse(text, suite):
    records = [line[len(PREFIX):] for line in text.splitlines() if line.startswith(PREFIX)]
    if len(records) != 1:
        raise ValueError('Expected exactly one complete structured report')
    return validate(json.loads(records[0]), suite)


def from_words(words, suite):
    entries = catalog()
    if len(words) != 8 + 3 * len(entries):
        raise ValueError('Truncated debugger report')
    report = dict(zip(['version', 'suite', 'expected', 'passed', 'failed', 'skipped', 'checks', 'complete'], words[:8]))
    report['results'] = [dict(id=item['id'], **dict(zip(['state', 'checks', 'failures'], words[8+3*i:11+3*i])))
                         for i, item in enumerate(entries) if item['suite'] == suite]
    return validate(report, suite)
