#!/usr/bin/env python3
import importlib.util
import pathlib
import stat
import subprocess
import sys
import tempfile
import zipfile
spec=importlib.util.spec_from_file_location('package_title', pathlib.Path(__file__).resolve().parents[1]/'tools/package-title.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as temp:
    root=pathlib.Path(temp)/'PPSA85301'; (root/'sce_module').mkdir(parents=True)
    for name in ('eboot.bin', 'sce_module/libc.prx', 'readme.txt'):
        (root/name).write_text('mode test fixture, not executable'); (root/name).chmod(0o600)
    root.chmod(0o700)
    output=pathlib.Path(temp)/'app.zip'; module.package(root,output)
    with zipfile.ZipFile(output) as archive:
        modes={x.filename:stat.S_IMODE(x.external_attr>>16) for x in archive.infolist()}
    assert modes['PPSA85301/']==0o755
    assert modes['PPSA85301/sce_module/']==0o755
    assert modes['PPSA85301/eboot.bin']==0o755
    assert modes['PPSA85301/sce_module/libc.prx']==0o755
    assert modes['PPSA85301/readme.txt']==0o644
    # Exercise the exact CLI archiver used by the native workflow as well.
    cli_output=pathlib.Path(temp)/'cli.zip'
    subprocess.run([sys.executable,'-m','zipfile','-c',str(cli_output),str(root)],check=True)
    module.verify(root,cli_output)
    bad=pathlib.Path(temp)/'bad.zip'
    with zipfile.ZipFile(bad,'w') as archive:
        archive.writestr('PPSA85301/eboot.bin','bad modes')
        archive.writestr('PPSA85301/sce_module/libc.prx','bad modes')
    try: module.verify(root,bad)
    except ValueError: pass
    else: raise AssertionError('Unsafe ZIP modes accepted')
print('Packaging permission regression passed, including exact workflow ZIP path and rejection gate')
