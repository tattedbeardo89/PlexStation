#!/usr/bin/env python3
"""Inspect only immutable matching build artifacts; runtime identity is separately required."""
import hashlib,json,pathlib,struct,subprocess,sys,zipfile
root=pathlib.Path(sys.argv[1]);debug=root/'debug';commit='f511344616040a64054f1b45584bdab9a5cc67c7';rip=0x4f93d7
manifest=json.loads((debug/'build.json').read_text());assert manifest['commit']==commit
for name,expected in manifest['files'].items():
    assert pathlib.Path(name).name==name
    assert hashlib.sha256((debug/name).read_bytes()).hexdigest()==expected
with zipfile.ZipFile(root/'installation/PlexStation-PPSA85301.zip') as z:
    for name,key in [('PPSA85301/eboot.bin','installed_eboot_sha256'),('PPSA85301/sce_module/libc.prx','runtime_libc_sha256')]:
        assert hashlib.sha256(z.read(name)).hexdigest()==manifest[key]
lines=['Matching build '+commit,'Original run 39797264696','Reported raw RIP '+hex(rip),
       'Packaged eboot SHA256 '+manifest['installed_eboot_sha256'],
       'Runtime libc SHA256 '+manifest['runtime_libc_sha256'],
       'Console executable hash/load map not supplied; all candidate mappings remain conditional.']
def run(args):
    result=subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=False)
    return result.stdout
for name in ['llvm-pie.elf','eboot.elf']:
    elf=debug/name;data=elf.read_bytes();assert data[:6]==b'\x7fELF\x02\x01'
    off=struct.unpack_from('<Q',data,32)[0];size,count=struct.unpack_from('<HH',data,54)
    ranges=[]
    for i in range(count):
        kind,flags,_,vma,_,_,memsz,_=struct.unpack_from('<IIQQQQQQ',data,off+i*size)
        if kind==1 and flags&1:ranges.append((vma,vma+memsz))
    lines.append(name+' executable PT_LOAD ranges: '+repr([(hex(a),hex(b)) for a,b in ranges]))
    for bias in [0,0x400000]:
        address=rip-bias
        if not any(a<=address<b for a,b in ranges):continue
        lines.append('CONDITIONAL mapping '+name+' bias='+hex(bias)+' ELF address='+hex(address))
        if name=='llvm-pie.elf':
            lines.append(run(['python3','tools/symbolize-crash.py',str(elf),hex(rip),'--load-bias',hex(bias)]))
        lines.append(run(['llvm-objdump','--disassemble','--demangle','--line-numbers','--start-address='+hex(max(0,address-48)),'--stop-address='+hex(address+64),str(elf)]))
(root/'fault-report.txt').write_text('\n'.join(lines)+'\n');print('\n'.join(lines))
