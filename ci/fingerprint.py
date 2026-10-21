#!/usr/bin/env python3
"""Content key for compiled dependencies; no application code in this cache."""
import hashlib
import pathlib
root = pathlib.Path(__file__).resolve().parents[1]
paths = [root/'ci/build-lock.json', root/'ci/Dockerfile.ps5', root/'ci/build-native.sh',
         root/'ci/fingerprint.py', root/'ps5/deps/build-deps.sh', root/'ps5/configure-vlc.sh',
         root/'ps5/ps5-cross.ini', root/'ps5/pkg-config', root/'ps5/radv-patches.sh']
paths += [p for p in (root/'patches').rglob('*.patch') if p.is_file()]
paths += [p for p in (root/'ps5/compat').rglob('*') if p.is_file()]
h = hashlib.sha256()
for path in sorted(paths):
    h.update(str(path.relative_to(root)).encode()+b'\0'+path.read_bytes()+b'\0')
print(h.hexdigest())
