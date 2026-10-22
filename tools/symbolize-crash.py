#!/usr/bin/env python3
"""Symbolize only addresses inside the verified matching ELF's executable ranges."""
import argparse,hashlib,json,pathlib,struct,subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('elf',type=pathlib.Path);p.add_argument('address',type=lambda s:int(s,0))
p.add_argument('--installed-eboot',type=pathlib.Path,help='optional console eboot.bin; verify its hash against this build')
p.add_argument('--load-bias',type=lambda s:int(s,0),default=0,help='verified runtime load address minus ELF VMA, not a guessed base')
a=p.parse_args();data=a.elf.read_bytes()
manifest=a.elf.parent/'build.json'
if not manifest.is_file():raise SystemExit('Matching build.json is required; do not guess the ELF identity')
if manifest.is_file():
    build=json.loads(manifest.read_text())
    if a.installed_eboot and hashlib.sha256(a.installed_eboot.read_bytes()).hexdigest()!=build.get('installed_eboot_sha256'):
        raise SystemExit('Installed eboot does not match this debug build')
    expected=build['files'].get(a.elf.name)
    if expected!=hashlib.sha256(data).hexdigest(): raise SystemExit('ELF checksum does not match debug manifest')
if data[:6]!=b'\x7fELF\x02\x01':raise SystemExit('Expected little-endian ELF64')
off=struct.unpack_from('<Q',data,32)[0];size,count=struct.unpack_from('<HH',data,54)
ranges=[]
for i in range(count):
    kind,flags,_,vma,_,_,memsz,_=struct.unpack_from('<IIQQQQQQ',data,off+i*size)
    if kind==1 and flags&1:ranges.append((vma,vma+memsz))
address=a.address-a.load_bias
if not any(lo<=address<hi for lo,hi in ranges):
    raise SystemExit('Address is outside matching ELF executable ranges; require raw RIP, module/load map or valid application return addresses. Do not treat a fixed-base subtraction as eboot.')
subprocess.run(['llvm-addr2line','-f','-C','-i','-e',str(a.elf),hex(address)],check=True)
