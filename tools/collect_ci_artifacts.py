#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage small CI reports and debugging files from failed configurations only."""
import argparse
import json
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET


REPORT_LOGS = {'LastTest.log', 'LastTestsFailed.log', 'LastTestsDisabled.log'}
CONFIGURATION_FILES = {'CMakeCache.txt', 'compile_commands.json'}
CMAKE_LOGS = {'CMakeConfigureLog.yaml', 'CMakeOutput.log', 'CMakeError.log'}
EXCLUDED_DIRECTORIES = {'library', 'dependencies', 'CMakeFiles', 'Testing', '__pycache__'}


def regular_files(directory):
    """Walk generated files without following symlinks or copied source trees."""
    if not directory.is_dir() or directory.is_symlink():
        return
    for path in sorted(directory.iterdir()):
        if path.is_symlink():
            continue
        if path.is_file():
            yield path
        elif path.is_dir() and path.name not in EXCLUDED_DIRECTORIES:
            yield from regular_files(path)


def executable(path):
    if path.suffix in {'.o', '.obj', '.a', '.so'}:
        return False
    with path.open('rb') as stream:
        header = stream.read(18)
    if len(header) != 18 or header[:4] != b'\x7fELF' or header[5] not in {1, 2}:
        return False
    # ET_EXEC/ET_DYN include bare-metal images and native PIE executables;
    # relocatable objects (ET_REL) and core files are not upload candidates.
    endian = 'little' if header[5] == 1 else 'big'
    return int.from_bytes(header[16:18], endian) in {2, 3}


def collect(results, output, job_status='unknown'):
    results = results.resolve()
    output = output.resolve()
    if output == results or output in results.parents or results in output.parents:
        raise ValueError('Artifact output must be separate from the result tree')
    if output.exists() and any(output.iterdir()):
        raise ValueError('Use an empty artifact output directory')
    output.mkdir(parents=True, exist_ok=True)
    reports, debugging, failed = set(), set(), set()
    warnings = []
    builds = {}
    if results.is_dir():
        for path in sorted(results.iterdir()):
            if path.is_symlink():
                continue
            if path.is_file() and path.suffix in {'.json', '.log', '.xml'} and path.name not in CONFIGURATION_FILES:
                reports.add(path)
            if (path.is_dir() and path.name not in EXCLUDED_DIRECTORIES
                    and any((path / name).exists() for name in ['CMakeCache.txt', 'CMakeFiles', 'Testing'])):
                builds[path.name] = path

    def read_json(path):
        try:
            value = json.loads(path.read_text())
            if not isinstance(value, dict):
                raise ValueError('Expected an object')
            return value
        except (OSError, ValueError) as error:
            warnings.append(str(path.relative_to(results)) + ': ' + str(error))
            return {}

    def command_build(command):
        for argument in command:
            if not isinstance(argument, str):
                continue
            path = Path(argument)
            if not path.is_absolute():
                path = results / path
            try:
                parts = path.resolve().relative_to(results).parts
            except ValueError:
                continue
            if parts and parts[0] in builds:
                return parts[0]
        return None

    summary_path = results / 'results.json'
    summary = read_json(summary_path) if summary_path.is_file() and not summary_path.is_symlink() else {}
    for check in summary.get('checks', []):
        # Expected configure/compile rejections have status=passed despite a
        # nonzero process status. They must never produce failure bundles.
        if check.get('status') == 'failed':
            build = command_build(check.get('command', []))
            if build:
                failed.add(build)

    for name, build in builds.items():
        for path in sorted(build.iterdir()):
            if path.is_symlink() or not path.is_file():
                continue
            if path.suffix == '.json' and path.name not in CONFIGURATION_FILES:
                reports.add(path)
                if read_json(path).get('status') == 'failed':
                    failed.add(name)
            elif path.name == 'junit.xml':
                reports.add(path)
                try:
                    root = ET.parse(path).getroot()
                    if (any(int(node.get(key, '0')) for node in root.iter() for key in ['failures', 'errors'])
                            or any(node.tag in {'failure', 'error'} for node in root.iter())):
                        failed.add(name)
                except (ET.ParseError, ValueError) as error:
                    warnings.append(str(path.relative_to(results)) + ': ' + str(error))
                    failed.add(name)
        temporary = build / 'Testing' / 'Temporary'
        if not (build / 'Testing').is_symlink() and not temporary.is_symlink():
            for filename in REPORT_LOGS:
                path = temporary / filename
                if path.is_file() and not path.is_symlink():
                    reports.add(path)
                    if filename == 'LastTestsFailed.log' and path.stat().st_size:
                        failed.add(name)

    # A killed test/build can leave no final report. Choose the last active
    # configuration using its stage log, never every completed configuration.
    incomplete = summary.get('status') in {'running', 'failed'} or (not summary and job_status != 'success')
    if incomplete and not failed and builds:
        stage_logs = sorted((path for path in reports if path.parent == results and path.suffix == '.log'),
                            key=lambda path: path.stat().st_mtime, reverse=True)
        for path in stage_logs:
            matches = [name for name in builds if path.name.startswith(name + '-')]
            if matches:
                failed.add(max(matches, key=len))
                break
        if not failed:
            for check in reversed(summary.get('checks', [])):
                build = command_build(check.get('command', []))
                if build:
                    failed.add(build)
                    break

    for name in sorted(failed):
        build = builds[name]
        for path in regular_files(build):
            if path.name in CONFIGURATION_FILES or path.suffix == '.map' or executable(path):
                debugging.add(path)
        cmake = build / 'CMakeFiles'
        if not cmake.is_symlink():
            for filename in CMAKE_LOGS:
                path = cmake / filename
                if path.is_file() and not path.is_symlink():
                    debugging.add(path)

    manifest = {'job_status': job_status, 'results_present': results.is_dir(),
                'failed_configurations': sorted(failed), 'warnings': warnings,
                'reports': [], 'debugging': []}
    for kind, paths in [('reports', reports), ('debugging', debugging)]:
        for path in sorted(paths):
            relative = path.relative_to(results)
            destination = output / kind / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, destination)
            manifest[kind].append({'path': relative.as_posix(), 'bytes': destination.stat().st_size})
    manifest['report_bytes'] = sum(item['bytes'] for item in manifest['reports'])
    manifest['debugging_bytes'] = sum(item['bytes'] for item in manifest['debugging'])
    report_directory = output / 'reports'
    report_directory.mkdir(exist_ok=True)
    (report_directory / 'artifact-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--job-status', choices=['success', 'failure', 'cancelled', 'unknown'], default='unknown')
    parser.add_argument('--github-output', type=Path, help='Optional composite-action output file')
    args = parser.parse_args()
    manifest = collect(args.results, args.output, args.job_status)
    if args.github_output:
        with args.github_output.open('a') as stream:
            stream.write('has_debugging=' + str(bool(manifest['debugging'])).lower() + '\n')
    print(f"Reports: {manifest['report_bytes']} bytes; failure debugging: {manifest['debugging_bytes']} bytes")


if __name__ == '__main__':
    main()
