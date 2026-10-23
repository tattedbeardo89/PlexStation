#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render the original vector master; no network or proprietary logo assets."""
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
master=ET.parse(root/'assets/plexstation.svg').getroot()
def symbol(size):
    factor=size/512
    image=Image.new('RGBA',(size,size),'#111827');draw=ImageDraw.Draw(image)
    for node in master:
        if node.tag.endswith('polygon'):
            points=[tuple(float(v)*factor for v in point.split(',')) for point in node.attrib['points'].split()]
            draw.polygon(points,fill=node.attrib['fill'])
    return image
icon=symbol(2048).resize((512,512),Image.Resampling.LANCZOS)
output=root/'ps5/sce_sys';icon.convert('RGB').save(output/'icon0.png')
symbol(1024).resize((512,512),Image.Resampling.LANCZOS).save(root/'assets/vlc-logo.png')
font=ImageFont.truetype(str(root/'assets/fonts/Inter-SemiBold.ttf'),88)
for stem,dds in [('background-source','pic0'),('launch-background-source','pic1')]:
    image=Image.new('RGBA',(1920,1080),'#111827');draw=ImageDraw.Draw(image)
    draw.polygon([(1000,0),(1920,0),(1920,1080),(1560,1080),(720,240)],fill='#1F2937')
    badge=symbol(1024).resize((420,420),Image.Resampling.LANCZOS);image.alpha_composite(badge,(750,215))
    draw.text((960,715),'PlexStation',font=font,fill='#F9FAFB',anchor='mm')
    image.convert('RGB').save(output/(stem+'.png'))
    image.save(output/(dds+'.dds'))
print('Generated original icon0.png 512x512; pic0.dds/pic1.dds 1920x1080; in-app symbol')
