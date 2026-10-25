#!/usr/bin/env python3
import ftplib
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('artwork_report', ROOT/'tools/ps5-artwork-report.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

class ReadOnlyFTP:
    def __init__(self):
        self.commands = []
    def retrlines(self, command, callback):
        self.commands.append(command)
        for name in ('PPSA85301_000', 'PPSA85301_001', 'PPSA85300_000', 'unrelated'):
            callback(name)
    def retrbinary(self, command, callback):
        self.commands.append(command)
        path = command[5:]
        if path == '/app0/sce_sys/param.json':
            callback(b'{"titleId":"PPSA85300"}')
        elif path.endswith('/param.json'):
            callback(b'{"titleId":"PPSA85301"}')
        elif path.endswith('/icon0.png'):
            callback(b'registered-old' if '/appmeta/' in path else b'packaged')
        else:
            raise ftplib.error_perm('550 missing')

ftp=ReadOnlyFTP()
roots=tool.candidates(ftp)
assert ('mounted','/mnt/sandbox/PPSA85301_001/app0') in roots
assert all('PPSA85300' not in path for _,path in roots)
observations=tool.inspect(ftp,roots)
assert all(command.startswith(('NLST ','RETR ')) for command in ftp.commands)
assert 'RETR /app0/sce_sys/icon0.png' not in ftp.commands
diagnosis=tool.diagnose(observations,hashlib.sha256(b'packaged').hexdigest())
assert len(diagnosis['mismatches'])>=2 and all('/appmeta/PPSA85301' in path for path in diagnosis['mismatches'])
assert 'no cause established' in tool.diagnose([{'path':'/icon0.png','state':'read','sha256':'same'}],'same')['finding']
assert tool.EXPECTED_ICON_SHA256==hashlib.sha256((ROOT/'ps5/sce_sys/icon0.png').read_bytes()).hexdigest()
try:
    tool.candidates(ftp,['/user/appmeta/PPSA85300'])
    raise AssertionError('Foreign title root accepted')
except ValueError:
    pass
with tempfile.TemporaryDirectory() as temporary:
    path=pathlib.Path(temporary)/'install.zip'
    with zipfile.ZipFile(path,'w') as archive:
        archive.writestr('PPSA85301/sce_sys/param.json',json.dumps({'titleId':'PPSA85301'}))
        archive.writestr('PPSA85301/sce_sys/icon0.png',b'packaged')
        archive.writestr('PPSA85301/plexstation-account.json','must-not-read')
    assert set(tool.package_files(path))=={'sce_sys/param.json','sce_sys/icon0.png'}
print('Windows-compatible FTP artwork report: read-only command allowlist, exact title discovery, duplicate mounts, foreign-title rejection, hashes and bounded package reads passed')
