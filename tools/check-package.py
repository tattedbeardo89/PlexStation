#!/usr/bin/env python3
"""Check an actual built title; metadata checks cannot prove hardware compatibility."""
import json
import pathlib
import sys
import struct
import hashlib
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'dist/PPSA85301')
required = ('eboot.bin', 'sce_sys/param.json', 'sce_sys/icon0.png', 'sce_sys/pic0.dds', 'sce_sys/pic1.dds', 'sce_module/libc.prx',
            'fonts/Inter-Regular.ttf', 'certs/cacert.pem', 'LICENSE', 'THIRD_PARTY.md')
missing = [p for p in required if not (root/p).is_file() or (root/p).stat().st_size == 0]
if missing:
    sys.exit('Incomplete package: ' + ', '.join(missing))
metadata = json.loads((root/'sce_sys/param.json').read_text())
if metadata['titleId'] != 'PPSA85301' or metadata['localizedParameters']['en-US']['titleName'] != 'PlexStation':
    sys.exit('Unexpected application identity')
if metadata['contentId'].split('-')[1].split('_')[0] != metadata['titleId']:
    sys.exit('Content identity mismatch')
if list(root.rglob('plexstation-account.json')):
    sys.exit('Never distribute a package containing account credentials')
icon=(root/'sce_sys/icon0.png').read_bytes()
if icon[:8]!=b'\x89PNG\r\n\x1a\n' or struct.unpack_from('>II',icon,16)!=(512,512):
    sys.exit('Invalid 512x512 PS5 icon')
for name in ('pic0.dds','pic1.dds'):
    data=(root/'sce_sys'/name).read_bytes()
    if data[:4]!=b'DDS ' or len(data)!=128+1920*1080//2 or data[84:88]!=b'DXT1' or struct.unpack_from('<II',data,12)!=(1080,1920):
        sys.exit('Invalid PS5 background dimensions')
expected_icon=pathlib.Path(__file__).resolve().parents[1]/'ps5/sce_sys/icon0.png'
if hashlib.sha256(icon).digest()!=hashlib.sha256(expected_icon.read_bytes()).digest():
    sys.exit('Packaged icon does not match the expected PlexStation icon')
manifest=json.loads((root/'artwork-manifest.json').read_text())
if not manifest['package']['icon_matches_expected']:
    sys.exit('Packaged icon manifest mismatch')
for name in ('action','adventure','animation','comedy','crime','documentary','drama','family','fantasy','horror','mystery','romance','science-fiction','thriller','western','other'):
    data=(root/'artwork/categories'/(name+'.png')).read_bytes()
    if data[:8]!=b'\x89PNG\r\n\x1a\n' or struct.unpack_from('>II',data,16)!=(320,180):
        sys.exit('Invalid category image: '+name)
print('Package files and independent metadata verified; PS5 launch is still unverified')
