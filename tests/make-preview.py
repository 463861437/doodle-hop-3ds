#!/usr/bin/env python3
"""Render original game screens through production C++ drawing code."""
import os,subprocess,tempfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='doodle-hop-preview-') as tmp:
 out=Path(tmp);binary=out/'preview'
 subprocess.run([os.environ.get('HOST_CXX','c++'),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'include'),str(root/'tests/preview.cpp'),str(root/'source/render.cpp'),'-o',str(binary)],check=True)
 screens=[]
 for state in range(13):
  prefix=out/str(state);subprocess.run([str(binary),str(state),str(prefix)],check=True)
  canvas=Image.new('RGB',(400,492),'#172F43')
  with Image.open(str(prefix)+'-top.ppm') as im:canvas.paste(im,(0,0))
  with Image.open(str(prefix)+'-bottom.ppm') as im:canvas.paste(im,(40,252))
  screens.append(canvas)
 screens[2].resize((800,984),Image.Resampling.NEAREST).save(root/'preview.png')
 screens[0].resize((800,984),Image.Resampling.NEAREST).save(root/'title-preview.png')
 sheet=Image.new('RGB',(1600,984),'#172F43')
 for i,s in enumerate(screens[:8]):sheet.paste(s,((i%4)*400,(i//4)*492))
 sheet.save(root/'screens.png')
 screens[8].save(root/'wrap-preview.png')
 themes=Image.new('RGB',(1200,264),'#172F43')
 for i,state in enumerate([2,9,10]):
  themes.paste(screens[state].crop((0,0,400,240)),(i*400,24))
  ImageDraw.Draw(themes).text((i*400+16,6),['01 / EMERALD ISLES','02 / CORAL SKY CITY','03 / AURORA CRYSTALS'][i],fill='#F6D891')
 themes.save(root/'themes-preview.png')
 poses=Image.new('RGB',(1600,240),'#172F43')
 for i,state in enumerate([2,11,12,3]):poses.paste(screens[state].crop((0,0,400,240)),(i*400,0))
 poses.save(root/'poses-preview.png')
 showcase=Image.new('RGB',(800,520),'#172F43')
 for i,state in enumerate([0,2]):
  showcase.paste(screens[state],(400*i,28))
  ImageDraw.Draw(showcase).text((16+400*i,8),['1.3 / MODERN 3D ART','1.3 / FULL-SCREEN GAMEPLAY'][i],fill='#F6D891')
 showcase.save(root/'redesign-preview.png')
 old=root/'docs/history/v1.2.0-preview.png'
 if old.exists():
  compare=Image.new('RGB',(800,520),'#172F43')
  with Image.open(old) as prior:compare.paste(prior.resize((400,492),Image.Resampling.NEAREST),(0,28))
  compare.paste(screens[2],(400,28))
  draw=ImageDraw.Draw(compare)
  draw.text((16,8),'BEFORE / 1.2.0',fill='#F6D891')
  draw.text((416,8),'REDESIGNED / 1.3.0',fill='#F6D891')
  compare.save(root/'docs/visual-comparison.png')
print('Saved production-rendered title, game, menus, wrap, themes, poses and comparison previews.')
