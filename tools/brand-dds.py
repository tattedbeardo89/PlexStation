#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert committed RGB8 PNG artwork to legacy uncompressed RGBA DDS, stdlib only."""
import struct, sys, zlib
from pathlib import Path

def rgba_png(path):
    data=Path(path).read_bytes()
    if data[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('PNG signature')
    at=8;compressed=bytearray();width=height=0
    while at<len(data):
        size=struct.unpack_from('>I',data,at)[0];kind=data[at+4:at+8];body=data[at+8:at+8+size]
        if zlib.crc32(kind+body)!=struct.unpack_from('>I',data,at+8+size)[0]:raise ValueError('PNG checksum')
        if kind==b'IHDR':
            width,height,depth,color,compression,filtering,interlace=struct.unpack('>IIBBBBB',body)
            if (depth,color,compression,filtering,interlace)!=(8,2,0,0,0) or (width,height)!=(1920,1080):raise ValueError('Expected 1920x1080 RGB8 PNG')
        elif kind==b'IDAT':compressed.extend(body)
        elif kind==b'IEND':break
        at+=12+size
    stride=width*3;raw=zlib.decompress(compressed)
    if len(raw)!=(stride+1)*height:raise ValueError('PNG decoded size')
    previous=bytearray(stride);pixels=bytearray()
    for y in range(height):
        start=y*(stride+1);mode=raw[start];row=bytearray(raw[start+1:start+1+stride])
        if mode>4:raise ValueError('PNG filter')
        for x in range(stride):
            left=row[x-3] if x>=3 else 0;above=previous[x];corner=previous[x-3] if x>=3 else 0
            if mode==1:predict=left
            elif mode==2:predict=above
            elif mode==3:predict=(left+above)//2
            elif mode==4:
                p=left+above-corner;a,b,c=abs(p-left),abs(p-above),abs(p-corner)
                predict=left if a<=b and a<=c else above if b<=c else corner
            else:predict=0
            row[x]=(row[x]+predict)&255
        for x in range(0,stride,3):pixels.extend(row[x:x+3]);pixels.append(255)
        previous=row
    return width,height,pixels

def convert(source,destination):
    width,height,pixels=rgba_png(source)
    header=[124,0x100f,height,width,width*4,0,0]+[0]*11+[32,0x41,0,32,0xff,0xff00,0xff0000,0xff000000,0x1000,0,0,0,0]
    Path(destination).write_bytes(b'DDS '+struct.pack('<31I',*header)+pixels)

if __name__=='__main__':
    convert(sys.argv[1],sys.argv[2])
