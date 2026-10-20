#!/usr/bin/env python3
"""Check an actual built title; metadata checks cannot prove hardware compatibility."""
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'dist/PPSA85301')
required = ('eboot.bin', 'sce_sys/param.json', 'sce_sys/icon0.png', 'sce_module/libc.prx',
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
print('Package files and independent metadata verified; PS5 launch is still unverified')
