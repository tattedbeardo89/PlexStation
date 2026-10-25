#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only PlexStation FTP artwork inspection. Python 3 standard library, Windows supported.
No FTP writes, registration changes, database reads, uninstall, or cache removal.
"""
import argparse
import ftplib
import getpass
import hashlib
import io
import json
import pathlib
import posixpath
import re
import struct
import zipfile

TITLE = 'PPSA85301'
EXPECTED_ICON_SHA256 = 'd6b3eaac084816c97312bdc36a29b31493efb37fad43616b43d7a4dc9a1d14ec'
ART = ('sce_sys/param.json', 'sce_sys/icon0.png', 'sce_sys/pic0.dds', 'sce_sys/pic1.dds')
LIMIT = 12 * 1024 * 1024


def describe(data):
    result = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    if data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) >= 26:
        result.update(format='PNG', dimensions=list(struct.unpack('>II', data[16:24])),
                      bit_depth=data[24], color_type=data[25])
    return result


def package_files(path):
    """Read the normal GitHub outer ZIP or inner install ZIP without extraction."""
    if path.is_dir():
        root = path / TITLE if (path / TITLE).is_dir() else path
        for name in ART:
            if (root / name).is_file() and (root / name).stat().st_size > LIMIT:
                raise ValueError('Oversized artwork')
        return {name: (root / name).read_bytes() for name in ART if (root / name).is_file()}
    with zipfile.ZipFile(path) as archive:
        nested = next((n for n in archive.namelist() if n.endswith('PlexStation-PPSA85301.zip')), None)
        if nested:
            item = archive.getinfo(nested)
            if item.file_size > 128 * 1024 * 1024:
                raise ValueError('Oversized installation archive')
            return zip_art(zipfile.ZipFile(io.BytesIO(archive.read(nested))))
        return zip_art(archive)


def zip_art(archive):
    result = {}
    for name in ART:
        for candidate in (TITLE + '/' + name, name):
            if candidate in archive.namelist():
                if archive.getinfo(candidate).file_size > LIMIT:
                    raise ValueError('Oversized artwork')
                result[name] = archive.read(candidate)
                break
    return result


def read_ftp(ftp, path):
    data = bytearray()
    def receive(block):
        if len(data) + len(block) > LIMIT:
            raise ValueError('Artwork exceeds read limit; connection closed')
        data.extend(block)
    ftp.retrbinary('RETR ' + path, receive)
    return bytes(data)


def candidates(ftp, extra_roots=()):
    roots = [('mounted', '/app0'), ('staged', '/data/homebrew/' + TITLE),
             ('staged', '/data/etaHEN/homebrew/' + TITLE),
             ('registered', '/user/appmeta/' + TITLE),
             ('registered', '/system_data/priv/appmeta/' + TITLE)]
    # Bound both parent listings. Only a matching title name is inspected further.
    for parent in ('/mnt/sandbox', '/data/homebrew', '/user/appmeta', '/system_data/priv/appmeta'):
        names = []
        def line(value):
            if len(names) >= 2048:
                raise ValueError('Discovery listing limit exceeded')
            names.append(value)
        try:
            ftp.retrlines('NLST ' + parent, line)
        except ftplib.error_perm:
            continue
        for value in names:
            name = posixpath.basename(value.rstrip('/'))
            if re.fullmatch(TITLE + r'(?:_[0-9]{3})?', name):
                root = parent + '/' + name
                kind = 'registered' if 'appmeta' in parent else 'staged'
                roots.append(('mounted', root + '/app0') if parent == '/mnt/sandbox' else (kind, root))
    for root in extra_roots:
        if '\r' in root or '\n' in root or '..' in root.split('/') or not any(re.fullmatch(TITLE + r'(?:_[0-9]{3})?', part) for part in root.split('/')):
            raise ValueError('Additional roots must contain the exact PlexStation title ID')
        roots.append(('explicit', root.rstrip('/')))
    return list(dict.fromkeys(roots))


def inspect(ftp, roots):
    observations = []
    for kind, root in roots[:32]:
        # appmeta stores icon0.png directly; app roots use sce_sys/icon0.png.
        names = ('param.json', 'icon0.png', 'pic0.dds', 'pic1.dds') if kind == 'registered' else ART
        for name in names:
            path = root + '/' + name
            try:
                data = read_ftp(ftp, path)
            except ftplib.error_perm:
                observations.append({'kind': kind, 'path': path, 'state': 'missing-or-denied'})
                if root == '/app0' and name.endswith('param.json'):
                    break
                continue
            if name.endswith('param.json'):
                try:
                    metadata = json.loads(data)
                except (ValueError, UnicodeDecodeError):
                    observations.append({'kind': kind, 'path': path, 'state': 'invalid-title-metadata'})
                    break
                if not isinstance(metadata, dict) or metadata.get('titleId') != TITLE:
                    observations.append({'kind': kind, 'path': path, 'state': 'different-title-not-inspected'})
                    break
            observations.append({'kind': kind, 'path': path, 'state': 'read', **describe(data)})
    return observations


def diagnose(observations, expected):
    icons = [o for o in observations if o['path'].endswith('/icon0.png') and o['state'] == 'read']
    differing = [o for o in icons if o['sha256'] != expected]
    if differing:
        return {'finding': 'Different artwork bytes found; paths identify the stale/mismatching sources, not an automatic cache diagnosis.',
                'mismatches': [o['path'] for o in differing]}
    if icons:
        return {'finding': 'All readable title icons match expected bytes. A stale shell image could be registration/cache/another launcher source; no cause established without further console evidence.'}
    return {'finding': 'No title icon was readable. FTP visibility or launcher paths are insufficient to establish a cause.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='192.168.7.9')
    parser.add_argument('--port', type=int, default=2120)
    parser.add_argument('--user', help='Optional FTP username; password is prompted privately and never reported')
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--package', type=pathlib.Path)
    parser.add_argument('--root', action='append', default=[], help='Additional explicit title root containing PPSA85301')
    parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path('PlexStation-artwork-report.json'))
    args = parser.parse_args()
    report = {'title_id': TITLE, 'read_only': True, 'expected_icon_sha256': EXPECTED_ICON_SHA256,
              'registration_database_read': False, 'hardware_artwork_fixed': False, 'observations': []}
    if args.package:
        files = package_files(args.package)
        metadata = json.loads(files.get('sce_sys/param.json', b'{}'))
        if not isinstance(metadata, dict) or metadata.get('titleId') != TITLE:
            parser.error('Package identity is not PPSA85301')
        report['package'] = {name: describe(data) for name, data in files.items()}
        packaged = files.get('sce_sys/icon0.png')
        report['packaged_icon_matches_expected'] = bool(packaged) and describe(packaged)['sha256'] == EXPECTED_ICON_SHA256
    password = getpass.getpass('FTP password (not stored): ') if args.user else 'anonymous@'
    try:
        with ftplib.FTP() as ftp:
            ftp.connect(args.host, args.port, timeout=max(1, min(args.timeout, 30)))
            ftp.login(args.user or 'anonymous', password)
            report['observations'] = inspect(ftp, candidates(ftp, args.root))
    except (OSError, EOFError, ftplib.Error, ValueError):
        # Do not copy server replies/exception text into reports: they may contain secrets.
        report['connection_or_read_error'] = 'FTP unavailable, access denied, or bounded read failed. No console writes were attempted.'
    report['diagnosis'] = diagnose(report['observations'], EXPECTED_ICON_SHA256)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print('Read-only report written to ' + str(args.output))


if __name__ == '__main__':
    main()
