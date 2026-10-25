#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reproducible original SVG category masters -> native PNG; Python stdlib only."""
import argparse, pathlib, struct, zlib, xml.etree.ElementTree as ET
ROOT=pathlib.Path(__file__).resolve().parents[1]
NAMES=['action','adventure','animation','comedy','crime','documentary','drama','family','fantasy','horror','mystery','romance','science-fiction','thriller','western','other']
MOTIFS=[
 '<polygon points="175,28 119,94 155,94 143,151 203,77 166,77"/>',
 '<polygon points="74,140 130,51 157,96 181,65 245,140"/><polygon points="116,74 130,51 144,74" fill="#f9fafb"/>',
 '<polygon points="160,30 175,71 220,73 185,100 196,145 160,118 124,145 135,100 100,73 145,71"/>',
 '<circle cx="128" cy="86" r="43"/><circle cx="192" cy="102" r="39" fill="#3b82f6"/><rect x="108" y="83" width="40" height="8" fill="#111827"/><rect x="177" y="96" width="30" height="8" fill="#111827"/>',
 '<circle cx="146" cy="79" r="40"/><circle cx="146" cy="79" r="27" fill="#111827"/><polygon points="171,107 184,94 220,132 207,145"/>',
 '<rect x="95" y="43" width="130" height="94"/><rect x="120" y="59" width="80" height="62" fill="#111827"/><polygon points="149,68 184,90 149,112" fill="#f9fafb"/>',
 '<polygon points="105,43 161,55 159,120 133,145 107,118"/><polygon points="161,55 215,43 213,118 187,145 161,120" fill="#3b82f6"/><rect x="119" y="80" width="20" height="8" fill="#111827"/><rect x="181" y="80" width="20" height="8" fill="#111827"/>',
 '<polygon points="80,83 160,25 240,83 224,83 224,145 96,145 96,83"/><rect x="145" y="98" width="30" height="47" fill="#111827"/>',
 '<polygon points="113,143 102,132 189,45 200,56"/><polygon points="195,25 202,44 222,46 206,58 211,79 194,66 177,79 183,58 167,46 187,44" fill="#f9fafb"/>',
 '<circle cx="160" cy="76" r="43"/><polygon points="117,75 203,75 203,142 182,130 160,145 138,130 117,142"/><circle cx="145" cy="79" r="7" fill="#111827"/><circle cx="175" cy="79" r="7" fill="#111827"/>',
 '<circle cx="160" cy="70" r="42"/><circle cx="160" cy="70" r="24" fill="#111827"/><polygon points="160,69 192,55 196,79 170,101 170,116 151,116 151,93 178,71"/><circle cx="160" cy="138" r="10"/>',
 '<polygon points="160,144 92,82 92,55 112,34 138,34 160,55 182,34 208,34 228,55 228,82"/>',
 '<circle cx="160" cy="88" r="49"/><polygon points="77,114 238,49 246,65 85,130" fill="#f9fafb"/><polygon points="80,115 157,84 167,98 88,130" fill="#3b82f6"/>',
 '<polygon points="64,92 108,92 127,44 155,137 180,62 197,92 256,92 256,106 187,106 180,96 154,164 126,87 118,106 64,106"/>',
 '<circle cx="198" cy="55" r="25"/><polygon points="62,143 114,53 155,143 173,107 188,143 258,143 258,157 62,157"/><rect x="209" y="90" width="12" height="53"/>',
 '<polygon points="110,34 178,34 218,74 218,106 178,146 144,146 144,160 110,160"/><polygon points="144,62 194,90 144,118" fill="#f9fafb"/>']
def masters():
 folder=ROOT/'assets/categories';folder.mkdir(exist_ok=True)
 for name,motif in zip(NAMES,MOTIFS):
  text=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 180" width="320" height="180"><title>PlexStation {name}</title><rect width="320" height="180" fill="#111827"/><polygon points="0,180 160,0 320,0 320,180" fill="#1f2937"/><g fill="#818cf8">{motif}</g></svg>\n'
  (folder/(name+'.svg')).write_text(text)
def render(path):
 pixels=bytearray(b'\x11\x18\x27'*(320*180))
 def paint(node,inherited='#818cf8'):
  fill=node.get('fill',inherited);tag=node.tag.rsplit('}',1)[-1]
  if tag=='g':
   for child in node:paint(child,fill)
   return
  color=bytes.fromhex(fill.removeprefix('#'))
  if tag=='polygon':
   pts=[tuple(map(float,p.split(','))) for p in node.get('points').split()]
   def inside(x,y):
    odd=False;j=len(pts)-1
    for i,(a,b) in enumerate(pts):
     c,d=pts[j]
     if (b>y)!=(d>y) and x<(c-a)*(y-b)/(d-b)+a:odd=not odd
     j=i
    return odd
   left,right=int(min(p[0] for p in pts)),int(max(p[0] for p in pts))+1;top,bottom=int(min(p[1] for p in pts)),int(max(p[1] for p in pts))+1
  elif tag=='circle':
   cx,cy,r=map(float,[node.get('cx'),node.get('cy'),node.get('r')]);left,right=int(cx-r),int(cx+r)+1;top,bottom=int(cy-r),int(cy+r)+1
   def inside(x,y):return (x-cx)**2+(y-cy)**2<=r*r
  elif tag=='rect':
   left,top=int(node.get('x','0')),int(node.get('y','0'));right,bottom=left+int(node.get('width')),top+int(node.get('height'))
   def inside(x,y):return True
  else:return
  for y in range(max(0,top),min(180,bottom)):
   for x in range(max(0,left),min(320,right)):
    if inside(x+.5,y+.5):pixels[(y*320+x)*3:(y*320+x)*3+3]=color
 for node in ET.parse(path).getroot():paint(node)
 def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
 data=b''.join(b'\0'+pixels[y*960:(y+1)*960] for y in range(180))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',320,180,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b'')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--masters',action='store_true');parser.add_argument('--output',type=pathlib.Path,default=ROOT/'build/category-art');args=parser.parse_args()
 if args.masters:masters()
 args.output.mkdir(parents=True,exist_ok=True)
 for name in NAMES:(args.output/(name+'.png')).write_bytes(render(ROOT/'assets/categories'/(name+'.svg')))
 print('Generated 16 original category images, 320x180, reproducible SVG -> PNG')
if __name__=='__main__':main()
