#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Gate inventoried source and publishable artifacts using external name policy."""
import argparse
import base64
import binascii
import hashlib
import io
import json
import os
from pathlib import Path
import tarfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--path', required=True, type=Path)
    parser.add_argument('--inventory', action='store_true', help='Require complete source/license inventory')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    report = {'status': 'failed', 'files_checked': 0, 'violations': []}
    try:
        value = os.environ.get('RTOS_STDCPP_FORBIDDEN_TOKENS')
        if value is None:
            value = base64.b64decode(os.environ.get('RTOS_STDCPP_FORBIDDEN_TOKENS_B64', ''), validate=True).decode()
        raw = json.loads(value)
        if not isinstance(raw, list) or not raw or any(not isinstance(x, str) or not x.strip() for x in raw):
            raise ValueError('Supply a nonempty external policy JSON list through RTOS_STDCPP_FORBIDDEN_TOKENS or its base64 counterpart')
        tokens = [x.lower().encode() for x in raw]
        def scan(name, data):
            report['files_checked'] += 1
            if any(x in name.lower().encode() or x in data.lower() for x in tokens):
                # Never echo a forbidden identifier into a publishable report.
                redacted = name
                for token in raw:
                    import re
                    redacted = re.sub(re.escape(token), '<redacted>', redacted, flags=re.IGNORECASE)
                report['violations'].append(redacted)
        root = args.path.resolve()
        if not root.exists():
            raise ValueError('Required source/artifact path is missing')
        files = sorted(p for p in root.rglob('*') if p.is_file() and '.git' not in p.relative_to(root).parts) if root.is_dir() else [root]
        if not files:
            raise ValueError('No files to audit')
        if args.inventory:
            inventory = json.loads((root / 'LICENSES/inventory.json').read_text())
            selected = {x['path']: x for x in inventory['retained_files']}
            actual = {p.relative_to(root).as_posix() for p in files}
            if actual != set(selected) | {'LICENSES/inventory.json'} or len(selected) != len(inventory['retained_files']):
                raise ValueError('Source tree differs from the reviewed license inventory')
            for name, item in selected.items():
                if hashlib.sha256((root / name).read_bytes()).hexdigest() != item['sha256']:
                    raise ValueError('Inventoried source bytes changed')
            report['license_inventory_verified'] = True
        def contents(name, data, depth=0):
            scan(name, data)
            is_zip = zipfile.is_zipfile(io.BytesIO(data))
            is_tar = name.endswith(('.tar', '.tar.gz', '.tgz', '.tar.xz'))
            if (is_zip or is_tar) and depth >= 4:
                raise ValueError('Archive nesting exceeds audit depth')
            if is_zip:
                with zipfile.ZipFile(io.BytesIO(data)) as archive:
                    for item in archive.infolist():
                        contents(name + '/' + item.filename, archive.read(item), depth + 1)
            elif is_tar:
                with tarfile.open(fileobj=io.BytesIO(data)) as archive:
                    for item in archive:
                        stream = archive.extractfile(item) if item.isfile() else None
                        contents(name + '/' + item.name, stream.read() if stream else item.linkname.encode(), depth + 1)
        for path in files:
            name = path.relative_to(root).as_posix() if root.is_dir() else path.name
            contents(name, path.read_bytes())
        if report['violations']:
            raise ValueError('External consumer-independence policy rejected delivery files')
        report['status'] = 'passed'
    except (OSError, ValueError, KeyError, binascii.Error, tarfile.TarError, zipfile.BadZipFile) as error:
        report['error'] = str(error)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return int(report['status'] != 'passed')

if __name__ == '__main__':
    raise SystemExit(main())
