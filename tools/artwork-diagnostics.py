#!/usr/bin/env python3
"""Read-only title-specific artwork comparison; optional explicit staged-icon refresh.
Never reads accounts, saves, system databases or launcher caches.
"""
import argparse,hashlib,json,os,pathlib,stat,struct,tempfile
TITLE='PPSA85301'
FILES=['sce_sys/icon0.png','sce_sys/pic0.dds','sce_sys/pic1.dds']
def info(path):
 if not path.is_file():return {'state':'not-provided-or-missing'}
 data=path.read_bytes();out={'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
 if data[:8]==b'\x89PNG\r\n\x1a\n' and len(data)>=24:out['dimensions']=list(struct.unpack('>II',data[16:24]))
 return out
def title(root):
 metadata=json.loads((root/'sce_sys/param.json').read_text())
 if metadata.get('titleId')!=TITLE or metadata.get('localizedParameters',{}).get('en-US',{}).get('titleName')!='PlexStation':raise ValueError('Refusing a different title identity/name')
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--expected-root',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[1]);parser.add_argument('--package',type=pathlib.Path);parser.add_argument('--installed',type=pathlib.Path);parser.add_argument('--registered-icon',type=pathlib.Path)
 parser.add_argument('--refresh-staged-icon',type=pathlib.Path,metavar='STAGED_TITLE_ROOT');parser.add_argument('--confirm-title')
 args=parser.parse_args();expected=args.expected_root/'ps5'
 report={'title_id':TITLE,'read_only':args.refresh_staged_icon is None,'expected':{name:info(expected/name) for name in FILES},'physical_registration':'Not inspected; no PS5 cache diagnosis inferred'}
 for key,root in [('package',args.package),('installed',args.installed)]:
  if root is not None:
   title(root);report[key]={name:info(root/name) for name in FILES}
   report[key]['icon_matches_expected']=report[key]['sce_sys/icon0.png'].get('sha256')==report['expected']['sce_sys/icon0.png'].get('sha256')
 if args.registered_icon:report['registered_icon']=info(args.registered_icon)
 if args.package:
  report['category_art']={p.name:info(p) for p in sorted((args.package/'artwork/categories').glob('*.png'))}
 if args.refresh_staged_icon:
  if args.confirm_title!=TITLE:raise ValueError('Explicit --confirm-title PPSA85301 is required for staged artwork writes')
  root=args.refresh_staged_icon;title(root);target=root/'sce_sys/icon0.png';source=expected/'sce_sys/icon0.png'
  if root.is_symlink() or target.is_symlink() or (root/'sce_sys').is_symlink():raise ValueError('Refusing a linked staging destination')
  data=source.read_bytes()
  if data[:8]!=b'\x89PNG\r\n\x1a\n' or struct.unpack('>II',data[16:24])!=(512,512):raise ValueError('Expected icon is not a 512-square PNG')
  if target.exists():
   old=target.read_bytes();backup=target.with_name('icon0.backup-'+hashlib.sha256(old).hexdigest()[:16]+'.png')
   if not backup.exists():
    with backup.open('xb') as stream:stream.write(old)
  fd,name=tempfile.mkstemp(prefix='.plexstation-icon-',dir=target.parent)
  try:
   with os.fdopen(fd,'wb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno());os.fchmod(stream.fileno(),stat.S_IRUSR|stat.S_IWUSR|stat.S_IRGRP|stat.S_IROTH)
   os.replace(name,target)
  finally:
   if os.path.exists(name):os.unlink(name)
  report['refresh']={'scope':'staged icon only','sha256':hashlib.sha256(data).hexdigest(),'registration_or_cache_changed':False}
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
