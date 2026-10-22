#!/usr/bin/env python3
"""Retain exact matching native ELF/map independently of installation files."""
import hashlib, json, pathlib, shutil, subprocess


def write_json_atomic(path, value):
    # Container-created validation.json is root-owned. The runner owns its
    # parent directory, so publish a new file without opening the old one to write.
    temporary = path.with_suffix(path.suffix + '.workflow-tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def collect(root):
    work = root / '.ci-cache/vlc/build-title'
    out = root / 'artifacts/debug'
    out.mkdir(parents=True, exist_ok=True)
    manifest = {
        'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'hardware_verified': False, 'files': {},
    }
    for name in ('llvm-pie.elf', 'eboot.elf', 'plexstation.map', 'elf-layout.txt', 'symbols.txt', 'link.log'):
        source = work / name
        if not source.is_file() or (name != 'link.log' and not source.stat().st_size):
            raise RuntimeError('Missing debug output: ' + name)
        shutil.copy2(source, out / name)
        manifest['files'][name] = hashlib.sha256(source.read_bytes()).hexdigest()
    exe = root / 'dist/PPSA85301/eboot.bin'
    runtime = root / 'dist/PPSA85301/sce_module/libc.prx'
    manifest['runtime_libc_sha256'] = hashlib.sha256(runtime.read_bytes()).hexdigest()
    manifest['installed_eboot_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
    write_json_atomic(out / 'build.json', manifest)
    shutil.copy2(root / 'docs/CRASH-DIAGNOSTICS.md', out / 'SYMBOLIZATION.md')
    shutil.copy2(root / 'tools/symbolize-crash.py', out / 'symbolize-crash.py')
    report_path = root / 'artifacts/validation.json'
    if report_path.is_file():
        report = json.loads(report_path.read_text())
        report['host_tests'] = 'passed required workflow regression gate; see artifacts/logs/host-*.log'
        report['hardware_verification'] = 'NOT TESTED; live Plex Home permissions, library crash resolution and playback remain pending'
        report['debug_manifest'] = 'debug/build.json'
        write_json_atomic(report_path, report)
    print('Matching unstripped ELF, converted ELF, linker map, symbols and binary identities retained')


if __name__ == '__main__':
    root = pathlib.Path(__file__).resolve().parents[1]
    try:
        collect(root)
    except Exception as error:
        detail = str(error).replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
        print('::error title=Native debug collection::' + detail)
        report_path = root / 'artifacts/validation.json'
        if report_path.is_file():
            report = json.loads(report_path.read_text())
            report.update(success=False, last_stage='debug-collection', reason='Matching debug artifact collection failed')
            write_json_atomic(report_path, report)
        raise
