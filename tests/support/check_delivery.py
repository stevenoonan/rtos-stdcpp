#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prove delivery rejects missing policy, changed inputs and archived references."""
import hashlib
import base64
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import zipfile
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='rtos-delivery-controls-') as directory:
    temporary = Path(directory); source = temporary / 'source'; source.mkdir()
    licenses = source / 'LICENSES'; licenses.mkdir()
    payload = source / 'sample.txt'; payload.write_text('clean fixture')
    inventory = {'retained_files': [{'path': 'sample.txt', 'sha256': hashlib.sha256(payload.read_bytes()).hexdigest()}]}
    (licenses / 'inventory.json').write_text(json.dumps(inventory))
    def run(name, path, expected, policy='["consumer_fixture"]', inventoried=False, encoded=False):
        env = os.environ.copy(); env['RTOS_STDCPP_FORBIDDEN_TOKENS'] = policy
        if encoded:
            env.pop('RTOS_STDCPP_FORBIDDEN_TOKENS', None)
            env['RTOS_STDCPP_FORBIDDEN_TOKENS_B64'] = base64.b64encode(policy.encode()).decode()
        command = [sys.executable, ROOT / 'tools/check_delivery.py', '--path', path, '--output', temporary / (name + '.json')]
        if inventoried:
            command.append('--inventory')
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=5)
        if (result.returncode == 0) != expected:
            raise RuntimeError(name + ': ' + result.stdout + result.stderr)
        print(name + ': passed')
    run('clean-source', source, True, inventoried=True)
    run('encoded-policy', source, True, inventoried=True, encoded=True)
    run('missing-policy', source, False, policy='null')
    run('empty-policy', source, False, policy='[]')
    run('malformed-policy', source, False, policy='broken')
    payload.write_text('modified')
    run('changed-source', source, False, inventoried=True)
    payload.write_text('CONSUMER_FIXTURE')
    run('case-insensitive-content', source, False)
    payload.unlink(); (source / 'consumer_fixture.txt').write_text('clean')
    run('filename', source, False)
    archive = temporary / 'payload.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('sample.txt', 'CONSUMER_FIXTURE')
    run('compressed-zip-content', archive, False)
    nested = temporary / 'nested.zip'
    with zipfile.ZipFile(nested, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('payload.zip', archive.read_bytes())
    run('nested-archive', nested, False)
    archive = temporary / 'payload.tar.gz'
    with tarfile.open(archive, 'w:gz') as tar:
        data = b'CONSUMER_FIXTURE'; item = tarfile.TarInfo('sample.txt'); item.size = len(data)
        tar.addfile(item, io.BytesIO(data))
    run('compressed-tar-content', archive, False)
    run('missing-artifacts', temporary / 'missing', False)
