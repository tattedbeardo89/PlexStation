#!/usr/bin/env python3
"""Verify original baseline artifact contents without rebuilding any executable."""
import hashlib
import json
import pathlib
import sys
import zipfile
BASELINE = 'd52e5cd39ad6689f1f917179a6d1761132adde03'
RUN = 39702538057

def verify(root, baseline=BASELINE, run=RUN):
    manifest = json.loads((root / 'debug/build.json').read_text())
    if manifest['commit'] != baseline:
        raise ValueError('Baseline debug commit mismatch')
    report = json.loads((root / 'installation/validation.json').read_text())
    if report.get('application_commit') != baseline or report.get('success') is not True:
        raise ValueError('Baseline installation validation mismatch')
    for name, expected in manifest['files'].items():
        if pathlib.Path(name).name != name:
            raise ValueError('Invalid debug manifest path')
        if hashlib.sha256((root / 'debug' / name).read_bytes()).hexdigest() != expected:
            raise ValueError('Baseline debug hash mismatch: ' + name)
    with zipfile.ZipFile(root / 'installation/PlexStation-PPSA85301.zip') as archive:
        for name, field in [('PPSA85301/eboot.bin', 'installed_eboot_sha256'),
                            ('PPSA85301/sce_module/libc.prx', 'runtime_libc_sha256')]:
            if hashlib.sha256(archive.read(name)).hexdigest() != manifest[field]:
                raise ValueError('Baseline installed binary hash mismatch: ' + name)
    evidence = {'original_run': run, 'original_commit': baseline,
                'rebuilt': False, 'installed_eboot_sha256': manifest['installed_eboot_sha256'],
                'runtime_libc_sha256': manifest['runtime_libc_sha256'],
                'installation_zip_sha256': hashlib.sha256((root / 'installation/PlexStation-PPSA85301.zip').read_bytes()).hexdigest()}
    (root / 'BASELINE.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return evidence

if __name__ == '__main__':
    verify(pathlib.Path(sys.argv[1]), sys.argv[2] if len(sys.argv)>2 else BASELINE, int(sys.argv[3]) if len(sys.argv)>3 else RUN)
    print('Original baseline installation, runtime and matching symbols verified; nothing rebuilt')
