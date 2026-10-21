#!/usr/bin/env python3
"""Normalize homebrew modes, then explicitly preserve them in a Unix-mode ZIP."""
import argparse
import pathlib
import stat
import zipfile

def executable(path, root):
    return path.relative_to(root).as_posix() in ('eboot.bin', 'sce_module/libc.prx')

def normalize(root):
    for path in [root, *sorted(root.rglob('*'))]:
        if path.is_symlink():
            raise ValueError(f'Symlink in installation: {path}')
        path.chmod(0o755 if path.is_dir() or executable(path, root) else 0o644)

def package(root, output):
    normalize(root)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in [root, *sorted(root.rglob('*'))]:
            archive.write(path, path.relative_to(root.parent).as_posix())
    verify(root, output)

def verify(root, output):
    with zipfile.ZipFile(output) as archive:
        names = set(archive.namelist())
        for required in ('eboot.bin', 'sce_module/libc.prx'):
            if root.name+'/'+required not in names:
                raise ValueError(f'ZIP lacks required executable: {required}')
        for entry in archive.infolist():
            expected = 0o755 if entry.is_dir() or entry.filename in (
                root.name+'/eboot.bin', root.name+'/sce_module/libc.prx') else 0o644
            if entry.create_system != 3 or stat.S_IMODE(entry.external_attr >> 16) != expected:
                raise ValueError(f'ZIP lost required Unix mode: {entry.filename}')
    print('Installation ZIP verified: directories/executables 755, read-only files 644')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('root', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path, nargs='?')
    args = parser.parse_args()
    if args.verify_only: verify(args.root, args.output)
    elif args.output: package(args.root, args.output)
    else: normalize(args.root)
