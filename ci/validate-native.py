#!/usr/bin/env python3
"""Fail closed on missing/placeholder native output; this is not a PS5 launch test."""
import pathlib
import struct
import subprocess
import sys
root, elf, tool = map(pathlib.Path, sys.argv[1:])
eboot = root/'eboot.bin'
if not elf.is_file() or elf.stat().st_size < 1024*1024:
    sys.exit('Missing or implausibly small VLC application ELF')
header = elf.read_bytes()[:64]
if header[:4] != b'\x7fELF' or header[4:6] != b'\x02\x01' or struct.unpack_from('<H', header, 18)[0] != 62:
    sys.exit('Application must be a 64-bit little-endian x86-64 ELF')
if not eboot.is_file() or eboot.stat().st_size < 1024*1024:
    sys.exit('Missing or implausibly small eboot.bin')
if struct.unpack('<I', eboot.read_bytes()[:4])[0] != 0x1D3D154F:
    sys.exit('eboot.bin is not the required signed SELF container')
subprocess.run([str(tool), 'self', '--inspect', '--file', str(eboot)], check=True)
if b'Video memory output' not in elf.read_bytes():
    sys.exit('Static VLC video output plugins are absent')
if not (root/'imports.txt').is_file() or not (root/'imports.txt').read_text().strip():
    sys.exit('Missing runtime imports report')
for path in root.rglob('*'):
    if path.is_symlink():
        sys.exit(f'Installation folder must be self-contained: {path}')
    if path.name in ('plexstation-account.json', 'credentials.json') or path.suffix == '.log':
        sys.exit(f'Runtime user data in package: {path}')
print('Native ELF, signed SELF and static VLC plugin/package gates passed. Hardware untested.')
