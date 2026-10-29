#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the C++ catalog from the single test/requirement registry."""
import json
from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:])
data = json.loads(source.read_text())['cases']
ids = [x['id'] for x in data]
if len(ids) != len(set(ids)) or any(not re.fullmatch(r'[a-z][a-z0-9_.]*', x) for x in ids):
    raise ValueError('Invalid or duplicate test IDs')
suites = list(dict.fromkeys(x['suite'] for x in data))
lines = ['// Generated from tests/catalog.json. SPDX-License-Identifier: MIT', '#pragma once',
         '#include <cstdint>', 'namespace rtos_test {', 'enum class Case : uint32_t {']
lines += [f'  {x.replace(".", "_")},' for x in ids]
lines += ['};', 'enum class Suite : uint32_t {'] + [f'  {s},' for s in suites] + ['};',
    'struct Descriptor { const char *id; Suite suite; const char *skip_reason; };',
    'inline constexpr Descriptor catalog[] = {']
for item in data:
    lines.append('  {' + json.dumps(item['id']) + ', Suite::' + item['suite'] + ', ' + json.dumps(item.get('skip_reason','')) + '},')
lines += ['};', f'inline constexpr uint32_t case_count = {len(data)};', '}']
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text('\n'.join(lines)+'\n')
