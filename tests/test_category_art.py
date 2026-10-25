#!/usr/bin/env python3
import hashlib,json,pathlib,shutil,struct,subprocess,tempfile,zlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 root=pathlib.Path(tmp);a=root/'a';b=root/'b'
 for out in [a,b]:subprocess.run(['python3',str(ROOT/'tools/make-category-art.py'),'--output',str(out)],check=True,capture_output=True)
 images=list(a.glob('*.png'));assert len(images)==16
 assert len({hashlib.sha256(p.read_bytes()).hexdigest() for p in images})==16
 for path in images:
  data=path.read_bytes();assert data==(b/path.name).read_bytes() and struct.unpack('>II',data[16:24])==(320,180)
 stage=root/'PPSA85301';(stage/'sce_sys').mkdir(parents=True);shutil.copy(ROOT/'ps5/sce_sys/param.json',stage/'sce_sys/param.json');shutil.copy(ROOT/'ps5/sce_sys/icon0.png',stage/'sce_sys/icon0.png');(stage/'prefs.txt').write_text('preserve')
 def run(*args,check=True):return subprocess.run(['python3',str(ROOT/'tools/artwork-diagnostics.py'),*map(str,args)],capture_output=True,check=check)
 before={str(p.relative_to(stage)):p.read_bytes() for p in stage.rglob('*') if p.is_file()}
 report=json.loads(run('--package',stage).stdout);assert report['read_only'] and report['package']['icon_matches_expected']
 assert before=={str(p.relative_to(stage)):p.read_bytes() for p in stage.rglob('*') if p.is_file()}
 assert run('--refresh-staged-icon',stage,check=False).returncode!=0
 (stage/'sce_sys/icon0.png').write_bytes(before['sce_sys/icon0.png']+b'old-staged-copy')
 run('--refresh-staged-icon',stage,'--confirm-title','PPSA85301');assert (stage/'prefs.txt').read_text()=='preserve';assert list((stage/'sce_sys').glob('icon0.backup-*.png'))
 assert json.loads(run('--package',stage).stdout)['package']['icon_matches_expected']
 (stage/'sce_sys/param.json').write_text(json.dumps({'titleId':'PPSA85300'}));assert run('--refresh-staged-icon',stage,'--confirm-title','PPSA85301',check=False).returncode!=0
print('Category SVG/PNG reproducibility and read-only/title-scoped icon diagnostics/refresh passed')
