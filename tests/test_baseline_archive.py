#!/usr/bin/env python3
import hashlib, importlib.util, json, pathlib, tempfile, zipfile
root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('archive_baseline', root / 'tools/archive-baseline.py')
archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archive)
with tempfile.TemporaryDirectory() as tmp:
    p = pathlib.Path(tmp)
    (p / 'installation').mkdir(); (p / 'debug').mkdir()
    (p / 'debug/llvm-pie.elf').write_bytes(b'matching ELF fixture')
    manifest = {'commit': archive.BASELINE,
                'files': {'llvm-pie.elf': hashlib.sha256(b'matching ELF fixture').hexdigest()},
                'installed_eboot_sha256': hashlib.sha256(b'original executable').hexdigest(),
                'runtime_libc_sha256': hashlib.sha256(b'original runtime').hexdigest()}
    (p / 'debug/build.json').write_text(json.dumps(manifest))
    (p / 'installation/validation.json').write_text(json.dumps({'application_commit': archive.BASELINE, 'success': True}))
    with zipfile.ZipFile(p / 'installation/PlexStation-PPSA85301.zip', 'w') as z:
        z.writestr('PPSA85301/eboot.bin', b'original executable')
        z.writestr('PPSA85301/sce_module/libc.prx', b'original runtime')
    assert archive.verify(p)['rebuilt'] is False
    (p / 'debug/llvm-pie.elf').write_bytes(b'other build')
    try:
        archive.verify(p)
    except ValueError:
        pass
    else:
        raise AssertionError('Mismatched symbols were accepted')
print('Baseline archive verifies exact executable/runtime/symbol identity and rejects substituted symbols')
