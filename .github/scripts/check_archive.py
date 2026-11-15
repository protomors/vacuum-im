#!/usr/bin/env python3
"""Check the actual extracted archive, with development SDKs hidden on Windows."""
import argparse
import contextlib
import glob
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import time
import zipfile


@contextlib.contextmanager
def hidden_sdks(paths):
    moved = []
    try:
        for value in paths:
            source = Path(value).resolve()
            destination = source.with_name(source.name + '.archive-check-hidden')
            if not source.is_dir() or destination.exists():
                raise RuntimeError(f'Cannot temporarily hide SDK {source}')
            source.rename(destination)
            moved.append((source, destination))
        yield
    finally:
        for source, destination in reversed(moved):
            destination.rename(source)


def run(args, env):
    result = subprocess.run([str(x) for x in args], env=env, capture_output=True,
                            text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(f'{args[0]} failed ({result.returncode}):\n'
                           f'{result.stdout}\n{result.stderr}')
    return result.stdout


def check(archive, workspace):
    extracted = workspace / 'extracted'
    extracted.mkdir()
    if archive.suffix == '.zip':
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(extracted)
    else:
        with tarfile.open(archive) as contents:
            contents.extractall(extracted, filter='data')
    metadata = list(extracted.rglob('build-info.json'))
    if len(metadata) != 1:
        raise RuntimeError('Expected exactly one build-info.json in the archive')
    info = json.loads(metadata[0].read_text())
    if info['configuration'] != 'Release':
        raise RuntimeError('Expected a Release build')
    windows = info['system'] == 'Windows'
    # Find the installation prefix independently of CPack's optional outer folder.
    root = metadata[0].parent if windows else metadata[0].parents[3]
    if windows:
        root = root.parent  # documentation lives in the application's directory
    executable = root / info['executable']
    plugins = [root / info['plugins_directory'] / name for name in info['plugins']]
    for path in [executable, root / info['library'], *plugins]:
        if not path.is_file():
            raise RuntimeError(f'Archive is missing {path.relative_to(root)}')
    if not plugins:
        raise RuntimeError('Archive contains no application plugins')
    if (root / 'include').exists() or (executable.parent / 'sdk').exists():
        raise RuntimeError('Application archive unexpectedly includes the SDK')
    if any('adiummessagestyle' in p.name for p in plugins):
        raise RuntimeError('Disabled Adium plugin was included')
    if not any('voicecalls' in p.name for p in plugins):
        raise RuntimeError('Release archive is missing voice calls')

    env = os.environ.copy()
    for key in list(env):
        if key.startswith(('QT_', 'GST_', 'GSTREAMER_')) or key in ('GIO_EXTRA_MODULES', 'SSL_CERT_FILE'):
            env.pop(key)
    env.update(QT_QPA_PLATFORM='windows' if windows else 'offscreen',
               XDG_CONFIG_HOME=str(workspace / 'config'),
               XDG_CACHE_HOME=str(workspace / 'cache'),
               XDG_DATA_HOME=str(workspace / 'data'))
    if windows:
        system_root = Path(env['SystemRoot'])
        env['PATH'] = os.pathsep.join([str(system_root / 'System32'), str(system_root)])
        for name in ['msvcp140.dll', 'vcruntime140.dll', 'Qt6Core.dll',
                     'qt-plugins/platforms/qwindows.dll',
                     'qt-plugins/sqldrivers/qsqlite.dll']:
            if not (executable.parent / name).exists():
                raise RuntimeError(f'Missing Windows runtime file {name}')
        sql_drivers = {p.name.lower() for p in
                       (executable.parent / 'qt-plugins/sqldrivers').glob('*.dll')}
        if sql_drivers != {'qsqlite.dll'}:
            raise RuntimeError(f'Unexpected SQL drivers in Windows archive: {sorted(sql_drivers)}')
        env['GST_PLUGIN_SYSTEM_PATH_1_0'] = str(executable.parent / 'gstreamer-1.0')
        env['GST_PLUGIN_SCANNER_1_0'] = str(executable.parent / 'gst-plugin-scanner.exe')
        env['GST_PLUGIN_PATH_1_0'] = ''
        env['GST_REGISTRY_1_0'] = str(workspace / 'gst-registry.bin')
        env['GIO_EXTRA_MODULES'] = str(executable.parent / 'gio-modules')
        env['SSL_CERT_FILE'] = str(executable.parent / 'certificates/ca-certificates.crt')
        for name in ('gio-modules/gioopenssl.dll', 'certificates/ca-certificates.crt'):
            if not (executable.parent / name).is_file():
                raise RuntimeError(f'Missing voice-call TLS runtime file {name}')
        inspector = executable.parent / 'gst-inspect-1.0.exe'
        required = ('webrtcbin nicesrc nicesink dtlssrtpenc dtlssrtpdec srtpenc srtpdec '
                    'rtpbin opusenc opusdec rtpopuspay rtpopusdepay audioconvert '
                    'audioresample volume queue capsfilter autoaudiosrc autoaudiosink '
                    'fakesink wasapi2src wasapi2sink webrtcdsp webrtcechoprobe').split()
        for element in required:
            run([inspector, '--exists', element], env)
        run([executable.parent / 'gst-launch-1.0.exe', '-q', 'audiotestsrc',
             'num-buffers=20', '!', 'audioconvert', '!', 'audioresample', '!',
             'opusenc', '!', 'rtpopuspay', '!', 'rtpopusdepay', '!', 'opusdec',
             '!', 'fakesink'], env)
        # The application must initialize its bundled GStreamer paths itself.
        for key in ('GST_PLUGIN_SYSTEM_PATH_1_0', 'GST_PLUGIN_SCANNER_1_0',
                    'GIO_EXTRA_MODULES', 'SSL_CERT_FILE'):
            env.pop(key)
    else:
        if list(root.rglob('libQt6*.so*')) or list(root.rglob('libgstreamer*.so*')):
            raise RuntimeError('Linux archive unexpectedly bundles system Qt/GStreamer')
        requirements = metadata[0].with_name('RUNTIME-REQUIREMENTS.txt')
        if not requirements.is_file():
            raise RuntimeError('Linux archive has no runtime requirements manifest')
        env.pop('LD_LIBRARY_PATH', None)
        for binary in [executable, *plugins]:
            output = run(['ldd', binary], env)
            if 'not found' in output:
                raise RuntimeError(f'Unresolved libraries for {binary}:\n{output}')
    profile = workspace / 'profile'
    profile.mkdir()
    log_path = workspace / 'startup.log'
    with log_path.open('w') as log:
        process = subprocess.Popen([str(executable), '-h', str(profile)], env=env,
                                   cwd=workspace, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 30
            while True:
                logs = '\n'.join(p.read_text(errors='replace') for p in profile.rglob('*.log'))
                if process.poll() is not None:
                    raise RuntimeError(f'Application exited during startup ({process.returncode}):\n'
                                       f'{log_path.read_text(errors="replace")}\n{logs}')
                if 'Application started' in logs:
                    break
                if time.monotonic() > deadline:
                    raise RuntimeError(f'Application did not finish startup:\n{logs}')
                time.sleep(.1)
            for message in ('Failed to load plugin ', 'Failed to initialize plugin', 'Failed to start plugin'):
                if message in logs:
                    raise RuntimeError(f'Plugin startup error:\n{logs}')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    print(f'{archive.name}: checked {len(plugins)} plugins, runtime dependencies and startup')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', help='Archive filename or a glob matching exactly one archive')
    parser.add_argument('--hide-sdk', action='append', default=[])
    args = parser.parse_args()
    matches = glob.glob(args.archive)
    if len(matches) != 1:
        parser.error('archive must match exactly one file')
    archive = Path(matches[0]).resolve()
    with hidden_sdks(args.hide_sdk), tempfile.TemporaryDirectory(prefix='vacuum-archive-') as work:
        check(archive, Path(work))


if __name__ == '__main__':
    main()
