"""Exercise artifact collection with isolated test fixtures, never deployable binaries."""
import hashlib, importlib.util, json, pathlib, subprocess, tempfile
repo=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('collect_debug',repo/'ci/collect-debug.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='plex-debug-collector-fixture-') as directory:
    root=pathlib.Path(directory)
    for command in (['init','-q'],['-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','--allow-empty','-qm','fixture']):
        subprocess.run(['git','-C',str(root),*command],check=True)
    work=root/'.ci-cache/vlc/build-title';work.mkdir(parents=True)
    for name in ('llvm-pie.elf','eboot.elf','plexstation.map','elf-layout.txt','symbols.txt','link.log'):
        (work/name).write_text('debug collector test fixture, not executable' if name!='link.log' else '')
    exe=root/'dist/PPSA85301/eboot.bin';exe.parent.mkdir(parents=True);exe.write_bytes(b'collector test fixture, not executable')
    runtime=exe.parent/'sce_module/libc.prx';runtime.parent.mkdir();runtime.write_bytes(b'fixture runtime')
    (root/'docs').mkdir();(root/'docs/CRASH-DIAGNOSTICS.md').write_text('fixture instructions')
    (root/'tools').mkdir();(root/'tools/symbolize-crash.py').write_text('# fixture')
    report=root/'artifacts/validation.json';report.parent.mkdir();report.write_text('{"success":true}')
    report.chmod(0o444);original=report.stat().st_ino
    module.collect(root)
    data=json.loads((root/'artifacts/debug/build.json').read_text())
    assert data['installed_eboot_sha256']==hashlib.sha256(exe.read_bytes()).hexdigest()
    assert data['files']['link.log']==hashlib.sha256(b'').hexdigest()
    assert report.stat().st_ino!=original and json.loads(report.read_text())['debug_manifest']=='debug/build.json'
    (work/'plexstation.map').unlink()
    try:module.collect(root)
    except RuntimeError as error:assert 'plexstation.map' in str(error)
    else:raise AssertionError('Missing required map was accepted')
print('Debug collector passed: empty successful link log, checksums, required files, atomic read-only report replacement')
