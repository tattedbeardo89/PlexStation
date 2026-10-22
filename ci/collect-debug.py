#!/usr/bin/env python3
"""Retain exact matching native ELF/map independently of installation files."""
import hashlib, json, pathlib, shutil, subprocess
root=pathlib.Path(__file__).resolve().parents[1]
work=root/'.ci-cache/vlc/build-title'
out=root/'artifacts/debug'; out.mkdir(parents=True,exist_ok=True)
manifest={'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
          'hardware_verified':False,'files':{}}
for name in ('llvm-pie.elf','eboot.elf','plexstation.map','elf-layout.txt','symbols.txt','link.log'):
    source=work/name
    if not source.is_file() or (name!='link.log' and not source.stat().st_size):
        print('::error title=Debug artifact collection::Missing debug output: '+name)
        raise SystemExit('Missing debug output: '+name)
    shutil.copy2(source,out/name)
    manifest['files'][name]=hashlib.sha256(source.read_bytes()).hexdigest()
exe=root/'dist/PPSA85301/eboot.bin'
manifest['runtime_libc_sha256']=hashlib.sha256((root/'dist/PPSA85301/sce_module/libc.prx').read_bytes()).hexdigest()
manifest['installed_eboot_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
(out/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
shutil.copy2(root/'docs/CRASH-DIAGNOSTICS.md',out/'SYMBOLIZATION.md')
shutil.copy2(root/'tools/symbolize-crash.py',out/'symbolize-crash.py')
print('Matching unstripped ELF, converted ELF, linker map, symbols and binary identities retained')

report_path=root/'artifacts/validation.json'
if report_path.is_file():
    report=json.loads(report_path.read_text())
    report['host_tests']='passed required workflow regression gate; see artifacts/logs/host-*.log'
    report['hardware_verification']='NOT TESTED; live Plex Home permissions, library crash resolution and playback remain pending'
    report['debug_manifest']='debug/build.json'
    report_path.write_text(json.dumps(report,indent=2)+'\n')
