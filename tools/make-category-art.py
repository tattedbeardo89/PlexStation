#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Text-only SVG templates; checked-in PNGs keep native builds dependency-free.
Use --masters to regenerate standard artwork (requires Pillow only when authoring),
or --label for any Unicode label. Unknown runtime genres use native typography.
"""
import argparse, hashlib, pathlib, shutil, xml.sax.saxutils
ROOT=pathlib.Path(__file__).resolve().parents[1]
NAMES=['action','adventure','animation','comedy','crime','documentary','drama','family','fantasy','horror','mystery','romance','science-fiction','thriller','western','other']
def label_for(name):return 'SCI-FI' if name=='science-fiction' else name.replace('-',' ').upper()
def svg(label):
 safe=xml.sax.saxutils.escape(label);size=max(16,min(34,450/max(1,len(label))))
 return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 180" width="320" height="180"><title>{safe}</title><rect width="320" height="180" fill="#111827"/><rect x="24" y="145" width="272" height="2" fill="#6366f1"/><text x="160" y="92" text-anchor="middle" dominant-baseline="middle" font-family="Inter, sans-serif" font-weight="600" font-size="{size:.2f}" fill="#f9fafb">{safe}</text></svg>\n'
def raster(label,path):
 from PIL import Image,ImageDraw,ImageFont
 factor=3;im=Image.new('RGB',(960,540),'#111827');draw=ImageDraw.Draw(im)
 size=34*factor
 while size>16*factor:
  font=ImageFont.truetype(str(ROOT/'assets/fonts/Inter-SemiBold.ttf'),size)
  if draw.textbbox((0,0),label,font=font)[2]<=272*factor:break
  size-=1
 draw.line([(24*factor,145*factor),(296*factor,145*factor)],fill='#6366f1',width=2*factor)
 draw.text((160*factor,90*factor),label,font=font,fill='#f9fafb',anchor='mm')
 im.resize((320,180),Image.Resampling.LANCZOS).save(path,compress_level=9)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--masters',action='store_true');parser.add_argument('--label');parser.add_argument('--output',type=pathlib.Path,default=ROOT/'build/category-art');args=parser.parse_args()
 args.output.mkdir(parents=True,exist_ok=True)
 if args.masters:
  for name in NAMES:
   label=label_for(name);(ROOT/'assets/categories'/(name+'.svg')).write_text(svg(label),encoding='utf-8');raster(label,ROOT/'assets/categories'/(name+'.png'))
 if args.label is not None:
  label=args.label.strip();name=hashlib.sha256(label.encode()).hexdigest()[:16]
  (args.output/(name+'.svg')).write_text(svg(label),encoding='utf-8');raster(label,args.output/(name+'.png'))
 else:
  for name in NAMES:shutil.copyfile(ROOT/'assets/categories'/(name+'.png'),args.output/(name+'.png'))
 print('Generated text-only category artwork, 320x180; no genre pictograms')
if __name__=='__main__':main()
