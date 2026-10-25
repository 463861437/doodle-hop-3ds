#!/usr/bin/env python3
"""Package original offline 3D artwork and supersampled modern UI for 3DS.

The rendered source PNGs are included. Rebuilding the game only needs these
headers; recreating artwork additionally uses render-modern-3d.py / bpy 4.5.3.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import wave
from PIL import Image, ImageDraw, ImageFilter, ImageOps
from modern_ui import Paint, S, build_ui

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'art/generated'
SOURCE = ROOT / 'art/modern-3d'


def glow(p, xy, radius, color, alpha=95):
    layer = Image.new('RGBA', p.im.size)
    x, y = xy
    ImageDraw.Draw(layer).ellipse(Paint.box((x-radius, y-radius, x+radius, y+radius)),
                                 fill=(*color, alpha))
    p.im = Image.alpha_composite(p.im, layer.filter(ImageFilter.GaussianBlur(radius*S*.45)))


def polygon(p, points, fill):
    ImageDraw.Draw(p.im).polygon([(round(x*S),round(y*S)) for x,y in points],fill=fill)


def gradient_shape(p, points, top, bottom):
    mask = Image.new('L', p.im.size)
    ImageDraw.Draw(mask).polygon([(round(x*S),round(y*S)) for x,y in points],fill=255)
    face = Paint(p.w,p.h);face.gradient((0,0,p.w,p.h),0,top,bottom)
    p.im = Image.composite(face.im,p.im,mask)


def sphere(p, box, base, highlight):
    # Small companion icons share the rounded forms of the offline 3D artwork.
    p.ellipse(box,base)
    x0,y0,x1,y1=box;w,h=x1-x0,y1-y0
    p.ellipse((x0+w*.13,y0+h*.08,x0+w*.78,y0+h*.68),highlight)
    p.ellipse((x0+w*.23,y0+h*.14,x0+w*.42,y0+h*.29),'#F5FFFF')


def utility_art():
    result={}
    p=Paint(22,22);glow(p,(11,11),8,(255,213,104),65)
    points=[(11+math.sin(i*math.pi/5)*(9 if i%2==0 else 4.2),
             10-math.cos(i*math.pi/5)*(9 if i%2==0 else 4.2)) for i in range(10)]
    polygon(p,[(x,y+1.8) for x,y in points],'#CA8B35')
    gradient_shape(p,points,'#FFF2A3','#F4B34D')
    polygon(p,[(11,2),(10.2,9),(3,7.4),(7.3,7.1)],'#FFFBE2')
    p.ellipse((9.1,8.2,11,10),'#FFFFFF');result['star']=p.finish()

    p=Paint(22,20);p.shadow((2,15,20,18),2,70,1)
    p.gradient((2,15,20,19),2,'#BEEDE8','#4B8E9D')
    points=[(6,16),(16,12),(6,9),(16,5),(7,3)]
    p.line(points,'#397887',2.4)
    p.line([(x-.35,y-.6) for x,y in points],'#BAEEE7',1)
    p.gradient((1,0,21,5),2,'#FFF1B0','#EAB15A')
    p.line([(4,1),(18,1)],'#FFFFE4',.7);result['spring']=p.finish()

    p=Paint(24,30)
    for x in [2,15]:
        p.gradient((x,4,x+7,25),3,'#BCF5E7','#32969F')
        p.gradient((x+.6,23,x+6.4,28),2,'#FFB59D','#C75F7C')
        p.line([(x+1.5,7),(x+1.5,17)],'#F1FFFD',1)
    p.gradient((8,8,16,23),2,'#EBF7FA','#759CAE')
    p.round((9,11,15,17),1,'#2B5873');p.line([(10,13),(14,13)],'#82F0D4',1)
    p.line([(6,5),(17,5)],'#C8EAF0',1.5);result['jetpack']=p.finish()

    for name,mode in [('enemy','bee'),('enemy_town','bot'),('enemy_crystal','gem')]:
        p=Paint(36,32)
        if mode=='bee':
            sphere(p,(6,1,17,15),'#84C8DE','#D9F9FF')
            sphere(p,(19,0,31,15),'#72BDD9','#CBF5FD')
            sphere(p,(4,10,32,29),'#DC9351','#F6CA72')
            for x in [12,19]:p.line([(x,11),(x-1,27)],'#736574',2.8)
            p.ellipse((22,15,31,25),'#FFE3A1');eye_x=[25]
        elif mode=='bot':
            p.line([(18,5),(18,9)],'#73B5CC',2);p.ellipse((16,2,20,6),'#FF9A95')
            p.gradient((4,9,32,28),9,'#90EEE0','#369CAC')
            p.round((8,13,29,24),5,'#244762');p.round((9,14,28,23),4,'#355E7D')
            p.line([(10,10),(26,10)],'#D9FFF3',.8);eye_x=[14,24]
            p.ellipse((1,17,6,25),'#D4F2F3');p.ellipse((31,17,35,25),'#D4F2F3')
        else:
            glow(p,(18,17),11,(154,150,255),70)
            gradient_shape(p,[(18,3),(31,10),(30,24),(19,30),(5,23),(4,12)],'#E6D8FF','#797ED0')
            polygon(p,[(18,3),(18,29),(5,23),(4,12)],'#A9ABF0')
            polygon(p,[(5,11),(0,6),(1,21),(6,24)],'#BAE5F5')
            polygon(p,[(29,10),(36,5),(34,23),(29,25)],'#9CD0F0')
            p.line([(18,4),(28,11)],'#F5F0FF',1);eye_x=[12,23]
        for x in eye_x:
            p.ellipse((x-1.6,16,x+1.6,21),'#25465C' if mode!='bot' else '#A2FFDD')
            p.ellipse((x-1,16,x-.1,17.2),'#FFFFFF')
        result[name]=p.finish()

    p=Paint(16,16);glow(p,(8,8),6,(196,255,244),100)
    polygon(p,[(8,1),(9.3,6.6),(15,8),(9.3,9.3),(8,15),(6.7,9.3),(1,8),(6.7,6.6)],'#F1FFFA')
    p.ellipse((6.8,6.8,9.2,9.2),'#FFFFFF');result['spark']=p.finish()
    p=Paint(16,28);glow(p,(8,12),7,(255,173,134),90)
    gradient_shape(p,[(3,0),(13,0),(14,8),(11,18),(8,27),(4,18),(2,8)],'#FFEBC3','#F28894')
    gradient_shape(p,[(6,0),(11,0),(11,8),(8,22),(5,10)],'#FFFFFF','#7DE7E2')
    result['jet_flame']=p.finish()

    p=Paint(8,8);gradient_shape(p,[(1,6),(2,2),(7,0),(6,5),(1,6)],'#CBF599','#43BCA4')
    p.line([(2,5),(6,1)],'#EBFFE0',.6);result['leaf']=p.finish()
    p=Paint(8,8);glow(p,(4,4),3,(190,255,226),140);p.ellipse((3,3,5,5),'#F7FFE8');result['firefly']=p.finish()
    p=Paint(16,16);sphere(p,(3,3,13,13),'#BA8A67','#FFE2A6')
    for i in range(6):
        a=i*math.tau/6;x=8+math.sin(a)*6;y=8+math.cos(a)*6
        p.ellipse((x-1,y-1,x+1,y+1),'#EFCB93')
    p.ellipse((6,6,10,10),'#7C7390');result['gear']=p.finish()
    p=Paint(12,16);glow(p,(6,8),5,(158,201,255),70)
    gradient_shape(p,[(6,1),(11,6),(8,15),(2,13),(1,5)],'#E0FCFF','#918CE3')
    polygon(p,[(6,1),(6,14),(2,13),(1,5)],'#B8D9FB');result['crystal_spark']=p.finish()

    for theme in ['forest','town','crystal']:
        p=Paint(96,180)
        for i in range(5):
            x=55+math.sin(i*1.2)*13;y=12+i*35
            if theme=='forest':
                p.line([(x,y),(x+5,y+19)],'#58BFA7',.5)
                polygon(p,[(x,y+4),(x-8,y+1),(x-3,y+10),(x+2,y+12)],'#79D8B1')
                glow(p,(x+4,y+15),3,(195,255,213),55)
            elif theme=='town':
                if i:p.line([(x-3,y-21),(x,y)],'#A8CCD5',.5)
                sphere(p,(x-3,y,x+3,y+6),'#DA8F9B','#FFCCAD')
            else:
                glow(p,(x,y),5,(144,206,255),65)
                gradient_shape(p,[(x,y-5),(x+3,y),(x,y+6),(x-3,y)],'#DEF9FF','#9298E6')
        result['decor_'+theme]=p.finish()
    return result


def load_render(name, size, mode='RGBA'):
    path=SOURCE/(name+'.png')
    if not path.is_file():
        raise FileNotFoundError(f'Missing 3D render {path}; run packaging/render-modern-3d.py --only all')
    with Image.open(path) as image:
        image=image.convert('RGBA').resize(size,Image.Resampling.LANCZOS)
    if mode=='RGB':
        background=Image.new('RGBA',size,'#A9D8E8');background.alpha_composite(image)
        return background.convert('RGB')
    return image


def atomic_text(path,text):
    temp=path.with_suffix(path.suffix+'.tmp');temp.write_text(text);temp.replace(path)


def export_headers(sprites,backgrounds):
    lines=['// Original offline 3D/modern UI pixels. GPL-3.0-or-later.',
           '#pragma once','#include <cstdint>','namespace hopArt {',
           'struct Sprite { int width,height;const uint8_t* rgba; };']
    for name,image in sprites.items():
        raw=image.convert('RGBA').tobytes();lines.append(f'static const uint8_t {name}_data[] = {{')
        lines.extend(','.join(map(str,raw[i:i+72]))+',' for i in range(0,len(raw),72))
        lines.extend(['};',f'static const Sprite {name}={{{image.width},{image.height},{name}_data}};'])
    lines.append('}')
    atomic_text(ROOT/'include/sprite_data.hpp','\n'.join(lines)+'\n')
    lines=['// Original pre-rendered backgrounds, native 3DS column BGR8.',
           '#pragma once','#include <cstdint>','namespace hopArt {',
           'struct Background { int width,height;const uint8_t* bgr; };']
    for name,image in backgrounds.items():
        pixels=image.convert('RGB').load();raw=bytearray()
        for x in range(image.width):
            for y in reversed(range(image.height)):
                r,g,b=pixels[x,y];raw.extend((b,g,r))
        lines.append(f'static const uint8_t {name}_data[] = {{')
        lines.extend(','.join(map(str,raw[i:i+72]))+',' for i in range(0,len(raw),72))
        lines.extend(['};',f'static const Background {name}={{{image.width},{image.height},{name}_data}};'])
    lines.append('}')
    atomic_text(ROOT/'include/background_data.hpp','\n'.join(lines)+'\n')


def presentation(sprites,backgrounds,font_path):
    from PIL import ImageFont
    # Native-size inputs and high resolution raw 3D hero preserve HOME detail.
    icon=Paint(48,48);icon.gradient((0,0,48,48),0,'#40C8C8','#BBE8FA')
    model=load_render('title_hero',(44,51))
    icon.im.alpha_composite(model.resize((176,204),Image.Resampling.LANCZOS),(8,-12))
    icon.finish('RGB').save(ROOT/'packaging/icon.png')
    banner=Image.open(SOURCE/'bg_title.png').convert('RGBA').resize((1024,512),Image.Resampling.LANCZOS)
    banner.alpha_composite(load_render('title_hero',(352,412)),(16,40))
    banner.alpha_composite(sprites['title_logo'].resize((640,172),Image.Resampling.LANCZOS),(360,80))
    draw=ImageDraw.Draw(banner);font=ImageFont.truetype(str(font_path),35)
    draw.rounded_rectangle((402,296,972,365),radius=20,fill=(26,49,76,230))
    draw.text((437,310),'DREAM HOP / 3DS',font=font,fill='#E7FFFC')
    banner.resize((256,128),Image.Resampling.LANCZOS).convert('RGB').save(ROOT/'packaging/banner.png')
    with wave.open(str(ROOT/'packaging/silence.wav'),'wb') as stream:
        stream.setparams((1,2,22050,0,'NONE','not compressed'));stream.writeframes(bytes(4410))


def contacts(sprites,backgrounds):
    sheet=Image.new('RGB',(960,math.ceil(len(sprites)/6)*112),'#DFEAF3');draw=ImageDraw.Draw(sheet)
    for i,(name,image) in enumerate(sprites.items()):
        x=(i%6)*160;y=(i//6)*112;scale=min(2,148/image.width,86/image.height)
        shown=image.resize((round(image.width*scale),round(image.height*scale)),Image.Resampling.NEAREST)
        sheet.paste(shown,(x+(160-shown.width)//2,y+3),shown)
        draw.text((x+3,y+94),name,fill='#24435B')
    sheet.save(OUT/'assetcontactsheet.png')
    sheet=Image.new('RGB',(800,720),'#20354B')
    for i,image in enumerate(backgrounds.values()):sheet.paste(image,((i%2)*400,(i//2)*240))
    sheet.save(OUT/'backgroundcontactsheet.png')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('font',type=Path)
    args=parser.parse_args();sprites={};backgrounds={}
    # Load every required render before publishing a header, so an interrupted
    # Blender batch cannot silently make a partly old, partly new resource set.
    for name in ['hero_right','hero_rise','hero_fall','hero_blink','hero_jet']:
        sprites[name]=load_render(name,(40,44))
    sprites['hero_left']=ImageOps.mirror(sprites['hero_right'])
    sprites['title_hero']=load_render('title_hero',(120,140))
    for suffix in ['', '_town','_crystal']:
        for kind in ['grass','moving','breakable','vanishing']:
            name='platform_'+kind+suffix;sprites[name]=load_render(name,(80,24))
    for name in ['bg_day','bg_dusk','bg_night','bg_title']:backgrounds[name]=load_render(name,(400,240),'RGB')
    sprites.update(utility_art())
    for name,image in build_ui(args.font).items():
        (backgrounds if name.startswith('bg_') else sprites)[name]=image
    OUT.mkdir(parents=True,exist_ok=True)
    for name,image in {**sprites,**backgrounds}.items():image.save(OUT/(name+'.png'))
    export_headers(sprites,backgrounds);presentation(sprites,backgrounds,args.font);contacts(sprites,backgrounds)
    manifest={'style':'modern pre-rendered 3D cartoon','render_tool':'Blender bpy 4.5.3 LTS',
              'sprite_count':len(sprites),'background_count':len(backgrounds),
              'sprite_bytes':sum(im.width*im.height*4 for im in sprites.values()),
              'background_bytes':sum(im.width*im.height*3 for im in backgrounds.values()),
              'generated':{name:{'width':im.width,'height':im.height,
                          'png_sha256':hashlib.sha256((OUT/(name+'.png')).read_bytes()).hexdigest()}
                           for name,im in {**sprites,**backgrounds}.items()}}
    atomic_text(ROOT/'packaging/ASSET-MANIFEST.json',json.dumps(manifest,indent=2)+'\n')
    print('Generated',len(sprites),'sprites and',len(backgrounds),'backgrounds;',
          manifest['sprite_bytes']+manifest['background_bytes'],'packed pixel bytes.')


if __name__=='__main__':main()
