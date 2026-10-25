#!/usr/bin/env python3
"""Original 1.3 toy-world artwork, modeled and rendered entirely in Blender.

Run with a Python environment containing bpy 4.5 and Pillow:
  python packaging/render-modern-3d.py --only preview --samples 24
  python packaging/render-modern-3d.py --only all --samples 32
All meshes, lights, materials and cameras are built deterministically here.
Render PNGs use straight alpha. Existing runtime headers are not modified.
"""
from pathlib import Path
import argparse
import math
import random
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--only', choices=['preview','hero','forest','backgrounds','platforms','all'], default='all')
parser.add_argument('--samples', type=int, default=32)
parser.add_argument('--width', type=int, default=800)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else None)

import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
from PIL import Image, ImageDraw, ImageFilter

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'art/modern-3d';OUT.mkdir(parents=True,exist_ok=True)
TMP=ROOT/'build-host/modern-art-render';TMP.mkdir(parents=True,exist_ok=True)
MAT={}

def lin(v):
    v=v/255
    return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4

def rgb(s):
    s=s.lstrip('#')
    return tuple(lin(int(s[i:i+2],16)) for i in (0,2,4))

def material(name,color,rough=.4,metal=0,transmission=0,emission=0):
    if name in MAT:return MAT[name]
    m=bpy.data.materials.new(name);m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF');c=rgb(color)
    bs.inputs['Base Color'].default_value=(*c,1)
    bs.inputs['Roughness'].default_value=rough
    bs.inputs['Metallic'].default_value=metal
    if 'Transmission Weight' in bs.inputs:bs.inputs['Transmission Weight'].default_value=transmission
    if 'Coat Weight' in bs.inputs:bs.inputs['Coat Weight'].default_value=.12 if metal<.3 else .28
    if emission:
        bs.inputs['Emission Color'].default_value=(*c,1)
        bs.inputs['Emission Strength'].default_value=emission
    m.diffuse_color=(*c,1);MAT[name]=m
    return m

def mats():
    colors={
      'orange':'#F58C35','orange_light':'#FFB05A','orange_dark':'#C65F29',
      'cream':'#FFF1D9','white':'#FAFEFF','black':'#152630','pink':'#EF9B90',
      'teal':'#16A5B1','teal_light':'#51D7D1','teal_dark':'#167A8A','coral':'#FF726D',
      'leaf':'#22A97B','leaf_light':'#70DB91','leaf_deep':'#087E66','mint':'#B4F2BA',
      'bark':'#A77355','rock':'#7A8E9B','rock_light':'#B0B6AD','rock_dark':'#506F85',
      'gold':'#F4BD55','gold_light':'#FFE19A','purple':'#A179EE','purple_light':'#DABCFB',
      'blue':'#438CDD','blue_light':'#9BE3F2','navy':'#254774','city_cream':'#FFE6CF',
      'city_pink':'#EFA6A1','crystal':'#78CBF5','crystal_lilac':'#B091EE',
      'moon':'#E5EFFC','cloud':'#E8F9F8','rope':'#C3A173',
    }
    for name,color in colors.items():
        material(name,color,rough=.31 if name in ['orange','teal','coral','gold'] else .45,
                 metal=.24 if name in ['gold','gold_light','teal_dark'] else .03)
    material('glass','#8AEAF2',.12,.1,.32)
    material('crystal_glass','#92C7FD',.13,.08,.24)
    material('glow','#FFF1BD',.3,0,0,1.7)
    material('pink_glow','#FFABF5',.35,0,0,.75)

def assign(obj,mat):
    obj.data.materials.append(MAT[mat] if isinstance(mat,str) else mat)
    return obj

def smooth(obj):
    for poly in obj.data.polygons:poly.use_smooth=True
    return obj

def sphere(name,loc,scale,mat,segments=32,rings=20,rot=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,location=loc)
    o=bpy.context.object;o.name=name;o.scale=scale
    if rot:o.rotation_euler=rot
    smooth(o);assign(o,mat);return o

def cube(name,loc,size,mat,bevel=.12,rot=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object;o.name=name;o.dimensions=size
    if rot:o.rotation_euler=rot
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=o.modifiers.new('soft molded edges','BEVEL');mod.width=bevel;mod.segments=3
        mod=o.modifiers.new('weighted corner normals','WEIGHTED_NORMAL');mod.keep_sharp=True
    assign(o,mat);return o

def cone(name,loc,r1,r2,depth,mat,vertices=48,bevel=.05,rot=None):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r1,radius2=r2,depth=depth,location=loc)
    o=bpy.context.object;o.name=name
    if rot:o.rotation_euler=rot
    if bevel:
        mod=o.modifiers.new('soft cone rim','BEVEL');mod.width=bevel;mod.segments=3
        mod=o.modifiers.new('cone normals','WEIGHTED_NORMAL');mod.keep_sharp=True
    assign(o,mat);smooth(o);return o

def cylinder(name,loc,radius,depth,mat,rot=None):
    return cone(name,loc,radius,radius,depth,mat,48,min(radius*.16,.08),rot)

def tube(name,points,radius,mat,cyclic=False):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=16
    curve.bevel_depth=radius;curve.bevel_resolution=3;curve.resolution_u=12
    sp=curve.splines.new('BEZIER');sp.bezier_points.add(len(points)-1)
    for p,co in zip(sp.bezier_points,points):
        p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    sp.use_cyclic_u=cyclic
    o=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(o);assign(o,mat);return o

def ring(name,loc,major,minor,mat,rot=None):
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=12,location=loc,major_radius=major,minor_radius=minor)
    o=bpy.context.object;o.name=name
    if rot:o.rotation_euler=rot
    smooth(o);assign(o,mat);return o

def mesh(name,verts,faces,mat,bevel=0):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    o=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(o)
    assign(o,mat)
    if bevel:
        m=o.modifiers.new('rounded rock facets','BEVEL');m.width=bevel;m.segments=2
        m=o.modifiers.new('weighted rock normals','WEIGHTED_NORMAL')
    return o

def area(name,loc,power,size,color,target=(0,0,2)):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;data.color=rgb(color)
    o=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(o);o.location=loc
    o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()

def reset(w,h,ortho=7,target=(0,0,2),camera=(5,-12,5),large=False,night=False):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    MAT.clear();mats()
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=args.samples
    s.cycles.use_denoising=True;s.cycles.use_adaptive_sampling=True;s.cycles.adaptive_threshold=.025
    s.cycles.max_bounces=6;s.cycles.diffuse_bounces=3;s.cycles.glossy_bounces=3;s.cycles.transmission_bounces=4
    s.render.resolution_x=w;s.render.resolution_y=h;s.render.resolution_percentage=100
    s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.film_transparent=True
    s.render.image_settings.color_depth='8';s.render.threads_mode='AUTO'
    s.view_settings.view_transform='AgX'
    try:s.view_settings.look='AgX - Medium High Contrast'
    except TypeError:pass
    s.view_settings.exposure=-.25 if not night else .15
    world=bpy.data.worlds.new('soft studio hemisphere');world.use_nodes=True;s.world=world
    world.node_tree.nodes['Background'].inputs['Color'].default_value=(*rgb('#B7D2E9' if not night else '#6574B6'),1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value=.34 if not night else .28
    factor=3 if large else 1
    area('warm key softbox',(-5,-6,10),650*factor,7,'#FFF2D9',target)
    area('cool blue fill',(6,-3,5),260*factor,6,'#B3E6FF',target)
    area('rim softbox',(3,6,8),950*factor,5,'#BFFFE3' if not night else '#C6AEFF',target)
    data=bpy.data.cameras.new('orthographic art camera');o=bpy.data.objects.new('orthographic art camera',data)
    bpy.context.collection.objects.link(o);o.location=camera;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
    data.type='ORTHO';data.ortho_scale=ortho;data.lens=50;s.camera=o
    s.render.filepath=str(TMP/'_render.png')
    return s

def render(name):
    s=bpy.context.scene;path=TMP/name;s.render.filepath=str(path)
    bpy.ops.render.render(write_still=True)
    return Image.open(path).convert('RGBA')

def hero(pose='idle'):
    # Fox proportions are designed for a readable 40px head-and-body silhouette.
    # No hat, feather or cape: this is a modern soft-vinyl explorer mascot.
    sphere('big curved orange tail',(-1.02,.38,1.38),(.64,.48,1.04),'orange',rot=(0,-.7,-.08))
    sphere('cream tail tip',(-1.58,.34,1.98),(.40,.42,.52),'cream',rot=(0,-.55,-.08))
    sphere('orange lower torso',(0,0,1.21),(.61,.44,.70),'orange')
    for x in [-.43,.43]:
        sphere('small orange shin',(x,-.04,.68),(.22,.26,.44),'orange')
        cube('white chunky sneaker',(x,-.28,.29),(.64,.98,.42),'white',.18)
        cube('cyan sneaker sole',(x,-.29,.135),(.68,1.01,.16),'teal_light',.07)
        cube('sneaker toe highlight',(x,-.72,.32),(.49,.18,.23),'cream',.08)
        for yy in [-.42,-.28]:tube('white sneaker lace',[(x-.17,yy,.505),(x,yy-.03,.53),(x+.17,yy,.505)],.025,'white')
    sphere('rounded teal jacket',(0,0,1.69),(.70,.53,.83),'teal')
    sphere('white chest bib',(0,-.50,1.85),(.37,.12,.48),'cream')
    # Open hem and asymmetric panel seams read as clothing, not flat body color.
    tube('zip seam',[(0,-.585,1.2),(0,-.627,1.5),(0,-.62,1.9)],.018,'teal_dark')
    cube('brass zip tab',(.045,-.65,1.65),(.08,.045,.12),'gold',.025)
    for x in [-.44,.44]:cube('jacket pocket',(x,-.42,1.34),(.23,.1,.23),'teal_dark',.055)
    ring('padded hoodie collar',(0,-.015,2.22),.56,.10,'teal_light')
    # Backpack sits off the body and catches a cool rim light.
    cube('lightweight backpack',(0,.54,1.72),(1.02,.44,1.15),'teal_dark',.22)
    cube('backpack padded lid',(0,.79,2.01),(.9,.13,.41),'teal_light',.10)
    for x in [-.45,.45]:tube('backpack strap',[(x,.46,2.13),(x,-.28,2.13),(x,-.46,1.51)],.065,'teal_light')
    if pose=='jet':
        for x in [-.62,.62]:
            cylinder('rocket pack capsule',(x,.53,1.48),.20,1.09,'teal_light')
            cone('rocket copper exhaust',(x,.53,.90),.14,.20,.24,'gold')
            ring('rocket glow collar',(x,.53,1.77),.20,.035,'glow')
    # Sleeve and paw gesture varies while the feet stay at the same model anchor.
    for side in [-1,1]:
        if pose in ['rise','jet']:
            elbow=(side*.93,-.08,2.12);hand=(side*1.14,-.20,2.48)
        elif pose=='fall':
            elbow=(side*.98,-.10,1.96);hand=(side*1.28,-.22,2.06)
        else:
            elbow=(side*.82,-.10,1.70);hand=(side*.89,-.26,1.34 if side<0 else 1.79)
        tube('rounded sleeve',[(side*.54,0,2.07),elbow,hand],.22,'teal')
        sphere('cream sleeve cuff',hand,(.25,.25,.20),'teal_light')
        sphere('soft orange paw',(hand[0],hand[1]-.025,hand[2]-.08),(.27,.25,.25),'orange_light')
    # Oversized fox head has pointed cheek silhouette but smooth soft-toy edges.
    sphere('fox head',(0,-.08,3.15),(1.08,.74,.91),'orange')
    for side in [-1,1]:
        sphere('soft pointed ear',(side*.73,-.015,3.95),(.38,.30,.62),'orange',rot=(0,side*.25,side*.05))
        sphere('cream inner ear',(side*.74,-.265,3.98),(.23,.075,.40),'cream',rot=(0,side*.25,side*.05))
        sphere('ear warm center',(side*.745,-.322,3.98),(.13,.036,.26),'pink',rot=(0,side*.25,side*.05))
        sphere('cream fox cheek',(side*.43,-.64,2.94),(.57,.31,.37),'cream',rot=(0,side*-.17,side*-.05))
        sphere('outer cheek tuft',(side*.84,-.43,3.02),(.34,.25,.25),'cream',rot=(0,side*.3,0))
    sphere('chin',(0,-.52,2.72),(.56,.30,.26),'cream')
    # Eyes retain large white catchlights after 4x downsampling.
    for side in [-1,1]:
        ex=side*.47
        if pose=='blink':
            tube('closed happy eyelid',[(ex-.15,-.785,3.30),(ex,-.805,3.25),(ex+.15,-.785,3.30)],.045,'black')
        else:
            sphere('dark glossy eye',(ex,-.74,3.35),(.18,.10,.24),'black')
            sphere('large white eye highlight',(ex-.045,-.833,3.425),(.058,.025,.070),'white',20,12)
            sphere('tiny eye glint',(ex+.055,-.830,3.28),(.025,.018,.030),'blue_light',16,10)
        tube('fox brow',[(ex-.13,-.733,3.63),(ex+.08,-.75,3.65)],.035,'orange_dark')
    sphere('rounded fox nose',(0,-.98,2.99),(.17,.15,.125),'black')
    sphere('nose glint',(-.045,-1.105,3.035),(.037,.014,.021),'white',16,10)
    tube('gentle smile left',[(0,-.915,2.91),(-.05,-.926,2.81),(-.21,-.90,2.80)],.018,'orange_dark')
    tube('gentle smile right',[(0,-.915,2.91),(.05,-.926,2.81),(.21,-.90,2.80)],.018,'orange_dark')
    for side in [-1,1]:sphere('peach cheek blush',(side*.72,-.725,3.025),(.11,.027,.07),'pink')
    # Three overlapping soft forelocks and a coral technical neck scarf.
    for x,z,rz in [(-.23,3.89,-.2),(0,3.99,0),(.21,3.91,.25)]:
        sphere('fox forehead tuft',(x,-.38,z),(.16,.21,.28),'orange_light',rot=(0,rz,0))
    tube('coral scarf collar',[(-.56,-.16,2.39),(-.39,-.51,2.35),(0,-.62,2.32),(.43,-.49,2.36),(.55,-.13,2.39)],.13,'coral')
    sphere('scarf knot',(-.34,-.55,2.25),(.19,.15,.19),'coral')
    end=(-1.16,.09,2.37 if pose in ['rise','jet'] else 2.05)
    tube('short windblown scarf',[(-.37,-.45,2.27),(-.73,-.20,2.17),end],.13,'coral')
    cube('scarf edge seam',end,(.10,.26,.12),'gold_light',.045)

def alpha_anchor(im,width,height,bottom,center):
    # Normalize each pose around the projected foot center, not the tail's bbox.
    alpha=im.getchannel('A');bbox=alpha.point(lambda v:255 if v>8 else 0).getbbox()
    if not bbox:raise ValueError('Empty 3D render')
    ax=center[0]*im.width;left=ax-bbox[0];right=bbox[2]-ax
    scale=min((width/2-3)/max(left,right),(bottom-3)/(bbox[3]-bbox[1]))
    crop=im.crop(bbox);size=(max(1,round(crop.width*scale)),max(1,round(crop.height*scale)))
    crop=crop.resize(size,Image.Resampling.LANCZOS)
    dest=Image.new('RGBA',(width,height));x=round(width/2-(ax-bbox[0])*scale);y=bottom-size[1]
    dest.alpha_composite(crop,(x,y))
    # Strong alpha reaches bottom-1; the remaining border is genuinely transparent.
    return dest

def render_hero(pose='idle',title=False):
    w,h=(720,840) if title else (320,352)
    reset(w,h,ortho=6.2,target=(-.1,0,2.1),camera=(-4.5,-13,5.0))
    hero(pose)
    bpy.context.scene.view_settings.exposure=.08
    bpy.data.objects['warm key softbox'].data.energy=800
    bpy.data.objects['cool blue fill'].data.energy=380
    s=bpy.context.scene;anchor=world_to_camera_view(s,s.camera,Vector((0,-.28,.25)))
    raw=render('_hero_render.png')
    name='title_hero' if title else 'hero_right' if pose=='idle' else 'hero_'+pose
    out=alpha_anchor(raw,480 if title else 160,560 if title else 176,528 if title else 160,(anchor.x,anchor.y))
    out.save(OUT/(name+'.png'));print('READY',name,flush=True)

# ----- World-building vocabulary: all scenery below consists of actual meshes. -----
def island(name,loc,radius=2.2,theme='forest'):
    x,y,z=loc;n=20;rng=random.Random(name)
    rings=[(radius,.0),(radius*.94,-.30),(radius*.78,-.92),(radius*.36,-1.9),(radius*.17,-2.2)]
    verts=[]
    for k,(r,zz) in enumerate(rings):
        for i in range(n):
            a=i*math.tau/n;pert=1+rng.uniform(-.09,.09)
            verts.append((x+math.cos(a)*r*pert,y+math.sin(a)*r*pert,z+zz+rng.uniform(-.06,.06)))
    faces=[tuple(range(n-1,-1,-1))]
    for k in range(len(rings)-1):
        for i in range(n):faces.append((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i))
    faces.append(tuple((len(rings)-1)*n+i for i in range(n)))
    rockmat='rock_dark' if theme=='crystal' else 'city_pink' if theme=='town' else 'rock'
    o=mesh(name+' tapered floating rock',verts,faces,rockmat,.10)
    o.data.materials.append(MAT['rock_light' if theme=='forest' else 'purple' if theme=='crystal' else 'coral'])
    for i,p in enumerate(o.data.polygons):
        if i%7==0:p.material_index=1
    top='leaf' if theme=='forest' else 'city_cream' if theme=='town' else 'navy'
    sphere(name+' soft turf',(x,y,z+.06),(radius*.98,radius*.95,.22),top)
    if theme=='forest':sphere(name+' sunlit turf',(x-.12,y-.07,z+.16),(radius*.83,radius*.78,.15),'leaf_light')
    return loc

def tree(loc,size=1,variant=0):
    x,y,z=loc
    tube('smooth branching tree',[(x,y,z),(x+.06*size,y,z+.9*size),(x-.1*size,y+.03*size,z+1.85*size)],.13*size,'bark')
    for side in [-1,1]:tube('branch',[(x,y,z+1.15*size),(x+side*.42*size,y,z+1.75*size)],.075*size,'bark')
    for dx,dy,dz,sc,mat in [(-.48,0,1.92,.62,'leaf'),(.38,.10,2.0,.70,'leaf_light'),(0,.03,2.51,.72,'leaf'),(-.07,-.27,2.1,.67,'leaf_light')]:
        sphere('puffy emerald tree crown',(x+dx*size,y+dy*size,z+dz*size),(.72*sc*size,.70*sc*size,.80*sc*size),mat)
    for dx,dz in [(-.52,2.0),(.42,2.30),(.04,2.72)]:sphere('tree leaf highlight',(x+dx*size,y-.43*size,z+dz*size),(.18*size,.07*size,.13*size),'mint')

def mushroom(loc,size=1,color='coral'):
    x,y,z=loc
    cylinder('soft mushroom stem',(x,y,z+.24*size),.12*size,.46*size,'cream')
    sphere('mushroom cap',(x,y,z+.52*size),(.48*size,.44*size,.25*size),color)
    for dx,dy in [(-.15,-.22),(.19,-.10),(-.07,.13)]:sphere('mushroom pale spot',(x+dx*size,y+dy*size,z+.70*size),(.065*size,.065*size,.025*size),'cream',20,12)

def bubbleplant(loc,size=1):
    x,y,z=loc
    for dx,dy,dz,r in [(-.20,.05,.7,.18),(.16,.08,.95,.24),(0,-.12,.48,.17)]:
        tube('bubble plant stem',[(x,y,z),(x+dx*size,y+dy*size,z+dz*size)],.025*size,'leaf_deep')
        sphere('glossy turquoise berry',(x+dx*size,y+dy*size,z+dz*size),(r*size,r*size,r*size),'teal_light')
        sphere('berry catchlight',(x+dx*size-r*.3*size,y+dy*size-r*.80*size,z+(dz+r*.3)*size),(.04*size,.025*size,.04*size),'white',16,10)

def grass(loc,size=1):
    x,y,z=loc
    for side in [-1,0,1]:sphere('rounded grass leaf',(x+side*.10*size,y,z+.21*size),(.07*size,.055*size,.30*size),'leaf_deep',20,12,rot=(0,side*.35,0))

def lighthouse(loc,size=1):
    x,y,z=loc
    cone('lighthouse body',(x,y,z+1.03*size),.48*size,.32*size,2.0*size,'cream')
    for zz in [.35,1.15]:ring('lighthouse teal band',(x,y,z+zz*size),(.45 if zz<.5 else .39)*size,.06*size,'teal')
    cylinder('lantern balcony',(x,y,z+2.06*size),.63*size,.14*size,'teal_dark')
    cylinder('glazed lantern',(x,y,z+2.39*size),.36*size,.55*size,'glass')
    sphere('warm lighthouse bulb',(x,y,z+2.39*size),(.16*size,.16*size,.23*size),'glow')
    for i in range(6):
        t=math.tau*i/6;cylinder('lantern frame',(x+math.cos(t)*.35*size,y+math.sin(t)*.35*size,z+2.39*size),.025*size,.59*size,'white')
    cone('coral lighthouse roof',(x,y,z+2.83*size),.60*size,.07*size,.47*size,'coral')
    sphere('lighthouse finial',(x,y,z+3.10*size),(.07*size,.07*size,.07*size),'gold')
    for zz in [.72,1.48]:cube('dark arched tower window',(x,y-.41*size,z+zz*size),(.20*size,.06*size,.30*size),'teal_dark',.08*size)

def bridge(a,b,width=.58):
    a=Vector(a);b=Vector(b);delta=b-a;side=Vector((-delta.y,delta.x,0)).normalized()*width/2
    for i in range(14):
        t=i/13;p=a.lerp(b,t);p.z-=math.sin(math.pi*t)*.3
        angle=math.atan2(delta.y,delta.x)
        cube('bridge warm plank',p,(delta.length/12,width,.07),'rope',.025,rot=(0,0,angle))
    for sgn in [-1,1]:
        pts=[]
        for i in range(7):
            t=i/6;p=a.lerp(b,t)+side*sgn;p.z+=.45-math.sin(math.pi*t)*.3;pts.append(tuple(p))
        tube('suspended bridge rope',pts,.025,'cream')
        for t in [0,.25,.5,.75,1]:
            p=a.lerp(b,t)+side*sgn;p.z-=math.sin(math.pi*t)*.3
            tube('bridge post',[tuple(p),tuple(p+Vector((0,0,.47)))],.022,'bark')

def waterfall(loc,width=.32,height=1.7):
    x,y,z=loc
    verts=[]
    for i in range(9):
        t=i/8
        for side in [-1,1]:verts.append((x+side*width*(1+.3*t),y-.14*math.sin(t*math.pi*.7),z-height*t))
    faces=[(i*2,i*2+1,i*2+3,i*2+2) for i in range(8)]
    mesh('silky waterfall',verts,faces,'blue_light')
    for dx in [-.11,0,.12]:tube('waterfall white stream',[(x+dx,y-.01,z),(x+dx,y-.11,z-height*.55),(x+dx,y-.10,z-height)],.012,'white')
    for dx in [-.23,0,.23]:sphere('waterfall spray',(x+dx,y-.08,z-height),(.12,.08,.07),'white',20,12)

def cloud_cluster(loc,size=1):
    x,y,z=loc
    for dx,dy,dz,rr in [(-.8,0,0,.7),(0,0,.17,1),(.8,.1,0,.72),(.25,-.25,-.1,.85)]:sphere('soft volume cloud',(x+dx*size,y+dy*size,z+dz*size),(rr*size,.53*size,.36*size),'cloud')

def forest_scene(title=False):
    reset(args.width,round(args.width*.6),ortho=15.6,target=(0,.1,1.25),camera=(0,-16,11),large=True)
    # Large foreground islands sit at the edges; open sky occupies the jump field.
    island('left forest island',(-5.35,0,.05),2.45)
    island('right forest island',(5.25,.9,.15),2.55)
    island('tiny distant island',(-2.65,4.8,.55),1.0)
    tree((-6.0,.35,.34),1.30);tree((-4.13,.80,.33),.92)
    tree((6.15,1.5,.43),1.25);tree((4.35,1.30,.40),.72)
    lighthouse((4.70,.05,.44),.76)
    lighthouse((-2.65,4.9,.79),.39)
    bridge((-4.8,1.4,.45),(-2.95,4.1,.80),.49)
    for x,y,z,sz in [(-6.2,-.85,.33,.8),(-4.8,-1.08,.33,.62),(-3.8,-.3,.35,.43),(6.0,-.35,.4,.80),(4.0,.0,.41,.47),(5.2,-.75,.41,.62)]:mushroom((x,y,z),sz,'coral' if x<0 else 'purple')
    for loc,sz in [((-3.82,.15,.39),.85),((-6.45,-.45,.35),.82),((6.30,.1,.43),.90),((3.72,1.3,.45),.73)]:bubbleplant(loc,sz)
    for x,y,z in [(-4.25,-.8,.38),(-6.4,.3,.4),(-4.7,.2,.42),(4.12,-.45,.44),(5.8,-.6,.41),(6.35,1.8,.42)]:grass((x,y,z),.82)
    tube('winding island creek',[(5.4,2.5,.42),(5.8,1.8,.48),(5.5,1.0,.47),(5.95,.15,.48),(5.9,-1.30,.38)],.16,'blue_light')
    tube('creek shimmering center',[(5.4,2.5,.55),(5.8,1.8,.59),(5.5,1.0,.59),(5.95,.15,.60),(5.9,-1.30,.49)],.035,'white')
    waterfall((5.9,-1.30,.41),.17,1.75)
    cloud_cluster((-6.8,3.7,3.45),.78);cloud_cluster((5.2,5,4.0),.68)
    cloud_cluster((-1.6,7.1,-.2),.52)
    # A small genuinely modeled island in the far atmosphere adds gentle depth.
    material('far mist teal','#A0D8D0',.65)
    material('far mist rock','#96BDC9',.65)
    sphere('distant island turf',(.75,8.9,-1.10),(.72,.53,.12),'far mist teal')
    cone('distant island rock',(.75,8.9,-1.48),.10,.63,.70,'far mist rock',10,.05)
    sphere('tiny distant crown',(.8,9.0,-.70),(.27,.24,.32),'far mist teal')
    cylinder('tiny distant trunk',(.8,9.0,-.98),.035,.35,'far mist rock')
    # Floating bubbles and tiny warm lamps add volume without filling the center.
    for x,y,z,r in [(-5.8,-.4,4.1,.16),(6.5,.3,3.7,.19),(-3.7,3.3,2.6,.13)]:sphere('floating pearl',(x,y,z),(r,r,r),'glass')
    return render_background('bg_title' if title else 'bg_day',('#BDEBFF','#E2FFF2') if title else ('#A9DEF5','#D8FFF0'))

def dome_house(loc,size=1,mat='coral'):
    x,y,z=loc
    cylinder('cream round sky house',(x,y,z+.57*size),.58*size,1.1*size,'city_cream')
    sphere('rounded colorful roof',(x,y,z+1.13*size),(.70*size,.70*size,.40*size),mat)
    ring('roof rim',(x,y,z+1.08*size),.64*size,.045*size,'gold_light')
    cube('house cyan glass door',(x,y-.575*size,z+.44*size),(.26*size,.08*size,.63*size),'teal_dark',.12*size)
    for dx in [-.36,.36]:
        sphere('round house window',(x+dx*size,y-.45*size,z+.75*size),(.14*size,.045*size,.18*size),'glass')
        ring('window brass trim',(x+dx*size,y-.475*size,z+.75*size),.16*size,.025*size,'gold_light',rot=(math.pi/2,0,0))

def glass_tower(loc,size=1):
    x,y,z=loc
    cylinder('tower base',(x,y,z+.08*size),.62*size,.18*size,'gold_light')
    cone('modern glass tower',(x,y,z+1.65*size),.50*size,.34*size,3.15*size,'glass')
    for zz in [.35,.8,1.25,1.70,2.15,2.60,3.05]:ring('white tower story rim',(x,y,z+zz*size),(.50-zz*.052)*size,.035*size,'white')
    for a in [0,math.pi*.5,math.pi,math.pi*1.5]:tube('tower curved rib',[(x+math.cos(a)*.5*size,y+math.sin(a)*.5*size,z+.13*size),(x+math.cos(a)*.34*size,y+math.sin(a)*.34*size,z+3.2*size)],.027*size,'teal_dark')
    sphere('tower coral dome',(x,y,z+3.18*size),(.42*size,.42*size,.25*size),'coral')
    cylinder('thin tower antenna',(x,y,z+3.57*size),.025*size,.5*size,'gold_light')
    sphere('beacon',(x,y,z+3.85*size),(.08*size,.08*size,.08*size),'glow')

def windmill(loc,size=1):
    x,y,z=loc
    cone('windmill white tower',(x,y,z+.8*size),.36*size,.22*size,1.6*size,'cream')
    sphere('windmill coral cap',(x,y,z+1.55*size),(.38*size,.38*size,.25*size),'coral')
    hub=(x,y-.32*size,z+1.47*size)
    sphere('gold windmill hub',hub,(.14*size,.15*size,.14*size),'gold')
    for a in [.35,.35+math.pi/2,.35+math.pi,.35+math.pi*1.5]:
        dx=math.cos(a);dz=math.sin(a)
        o=cube('rounded windmill blade',(x+dx*.55*size,y-.39*size,z+1.47*size+dz*.55*size),(.8*size,.08*size,.18*size),'white',.07*size,rot=(0,-a,0))

def spaceship(loc,size=1):
    x,y,z=loc
    sphere('tiny airship hull',(x,y,z),(.70*size,.30*size,.23*size),'white')
    sphere('airship glass canopy',(x-.05*size,y-.10*size,z+.15*size),(.35*size,.23*size,.20*size),'glass')
    cube('airship wing',(x,y,z),(.73*size,1.16*size,.07*size),'coral',.05*size)
    for yy in [-.46,.46]:cylinder('airship engine',(x-.23*size,y+yy*size,z),.12*size,.36*size,'teal',rot=(0,math.pi/2,0))

def town_scene():
    reset(args.width,round(args.width*.6),ortho=15.6,target=(0,.1,1.25),camera=(0,-16,11),large=True)
    island('left sky city',(-5.3,.15,.15),2.5,'town')
    island('right sky city',(5.0,.7,.15),2.5,'town')
    island('distant roof garden',(1.9,5.0,.50),.90,'town')
    glass_tower((-5.8,.6,.42),.9)
    dome_house((-4.5,-.55,.42),.83,'coral');dome_house((-6.6,-.75,.43),.63,'teal')
    dome_house((4.25,-.25,.43),.86,'coral');dome_house((6.0,.6,.43),.76,'purple')
    glass_tower((5.4,1.4,.43),.68);windmill((6.6,-.5,.41),.8)
    dome_house((1.85,5,.75),.52,'teal')
    # A suspended monorail bends around the outside city rim, never crosses center.
    pts=[(-7.3,-.30,.88),(-6.6,-1.4,.82),(-4.5,-1.5,.82),(-3.1,-.3,1.03),(-3.25,1.65,1.19),(-4.5,2.3,1.2)]
    tube('elevated sky rail',pts,.08,'white')
    tube('gold monorail guide',[(x,y,z+.07) for x,y,z in pts],.025,'gold')
    for x,y,z in [pts[1],pts[2],pts[4]]:cylinder('sky rail support',(x,y,z-.37),.075,.75,'city_cream')
    cube('rounded monorail pod',(-4.8,-1.5,1.05),(.95,.40,.43),'teal',.17)
    cube('monorail panoramic glass',(-4.8,-1.71,1.12),(.62,.04,.19),'blue_light',.07)
    spaceship((3.9,2.9,3.35),.70)
    cloud_cluster((-4.0,5.6,4.35),.85);cloud_cluster((6.0,5,3.75),.75)
    # Discrete planter globes repeat the toy architecture's material palette.
    for x,y,z in [(-3.75,.55,.47),(-6.85,.8,.45),(3.55,.55,.47),(5.7,-1.0,.43)]:
        cylinder('coral city planter',(x,y,z+.12),.22,.27,'city_pink');sphere('round topiary',(x,y,z+.46),(.30,.27,.32),'leaf_light')
    return render_background('bg_dusk',('#B6B5F0','#FFD7C6'))

def crystal(loc,size=1,mat='crystal_glass',angle=0):
    x,y,z=loc;r=.40*size;h=1.8*size;n=6
    verts=[]
    for zz,rr in [(0,r*.80),(h*.68,r),(h*.81,r*.75)]:
        for i in range(n):
            a=i*math.tau/n+angle;verts.append((x+math.cos(a)*rr,y+math.sin(a)*rr,z+zz))
    verts.append((x+r*.10,y-r*.05,z+h))
    faces=[tuple(range(n-1,-1,-1))]
    for k in range(2):
        for i in range(n):faces.append((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i))
    for i in range(n):faces.append((2*n+i,2*n+(i+1)%n,18))
    mesh('large faceted crystal',verts,faces,mat,.025*size)

def crystal_scene():
    reset(args.width,round(args.width*.6),ortho=15.6,target=(0,.1,1.25),camera=(0,-16,11),large=True,night=True)
    island('left crystal grove',(-5.35,.05,.15),2.55,'crystal')
    island('right crystal grove',(5.1,1.0,.20),2.55,'crystal')
    # Distant monumental ring is a real vertical torus with inset glowing traces.
    ring('moon ring ruin',(-5.5,.9,2.30),1.42,.18,'purple_light',rot=(math.pi/2,0,.15))
    ring('luminous ring inlay',(-5.5,.69,2.30),1.41,.045,'glow',rot=(math.pi/2,0,.15))
    for x,y,z,sz,mat in [(-6.5,-.5,.4,1.1,'crystal'),(-4.55,-.6,.4,.87,'crystal_glass'),(-5.15,.55,.4,1.32,'crystal_lilac'),(-3.65,.5,.4,.58,'crystal'),
                         (4.8,1.2,.48,1.6,'crystal_glass'),(6.15,.30,.48,1.16,'crystal_lilac'),(4,-.15,.45,.88,'crystal'),(6.7,1.4,.44,.63,'crystal')]:crystal((x,y,z),sz,mat,angle=.2)
    # Futuristic broken pillars and rounded ruins read as architecture, not blocks.
    for x,y,z,h in [(-6.9,.8,.42,1.6),(6.7,1.8,.46,2.0),(3.6,1.5,.47,1.3)]:
        cylinder('fluted ruin pillar',(x,y,z+h/2),.19,h,'blue_light')
        for zz in [z+.12,z+h-.12]:cylinder('pillar molded capital',(x,y,zz),.29,.15,'purple_light')
        for a in [0,math.pi/2,math.pi,math.pi*1.5]:tube('pillar flute',[(x+math.cos(a)*.18,y+math.sin(a)*.18,z+.2),(x+math.cos(a)*.18,y+math.sin(a)*.18,z+h-.2)],.02,'white')
    for x,y,z in [(-4.4,-1,.45),(-6.4,.5,.45),(5.8,-.5,.47),(3.7,.6,.48)]:
        for side in [-1,0,1]:
            tube('violet coral stem',[(x,y,z),(x+side*.16,y,z+.45)],.035,'purple')
            sphere('glowing crystal flower',(x+side*.16,y,z+.49),(.10,.10,.13),'pink_glow')
    sphere('moon small planet',(1.5,5.2,1.8),(.84,.84,.84),'moon')
    for dx,dy,dz,rr in [(-.27,-.72,.19,.16),(.23,-.76,-.07,.22),(-.13,-.78,-.38,.13)]:sphere('moon raised crater',(1.5+dx,5.2+dy,1.8+dz),(rr,.06,rr),'blue_light')
    cloud_cluster((-1.4,8.0,.0),.66)
    return render_background('bg_night',('#222D68','#627DB4'),night=True)

def sky(size,colors,night=False):
    w,h=size;im=Image.new('RGB',size);d=ImageDraw.Draw(im)
    c=[tuple(int(v.lstrip('#')[i:i+2],16) for i in (0,2,4)) for v in colors]
    for y in range(h):
        t=y/max(h-1,1);co=tuple(round(a+(b-a)*t) for a,b in zip(*c));d.line((0,y,w,y),fill=co)
    gl=Image.new('RGBA',size);g=ImageDraw.Draw(gl)
    if night:
        # Aurora light sheets are only atmospheric compositing; all terrain is 3D.
        for off,col in [(0,(106,235,222,100)),(35,(174,127,255,90))]:
            pts=[(0,h*.28+off),(w*.2,h*.12+off),(w*.47,h*.18+off),(w*.7,h*.05+off),(w,h*.13+off)]
            g.line(pts,fill=col,width=round(h*.075),joint='curve')
        gl=gl.filter(ImageFilter.GaussianBlur(h*.028))
        im=Image.alpha_composite(im.convert('RGBA'),gl)
        d=ImageDraw.Draw(im)
        rng=random.Random(1301)
        for n in range(38):
            x=rng.randrange(w);y=rng.randrange(round(h*.70));r=1 if n%5 else 2
            d.ellipse((x-r,y-r,x+r,y+r),fill=(215,234,255,150))
    else:
        g.ellipse((w*.23,-h*.42,w*.86,h*.70),fill=(255,255,232,125))
        im=Image.alpha_composite(im.convert('RGBA'),gl.filter(ImageFilter.GaussianBlur(w*.12)))
    return im.convert('RGBA')

def render_background(name,colors,night=False):
    raw=render('_'+name+'_objects.png')
    result=Image.alpha_composite(sky(raw.size,colors,night),raw).convert('RGB')
    result.save(OUT/(name+'.png'));print('READY',name,flush=True);return result

# ----- Beveled material platforms, always cropped to a firm landing edge. -----
def platform_scene(kind,theme):
    # A shallow camera angle keeps the top plane narrow. Front fill makes these
    # small game pieces luminous rather than black-rimmed miniature slabs.
    reset(640,240,ortho=9.3,target=(0,0,-.28),camera=(0,-14,2.15))
    bpy.context.scene.view_settings.exposure=.08
    area('broad platform front fill',(0,-7,3.0),1000,7,'#EDFFFF',(0,0,-.25))
    area('low platform reflector',(-3,-4,-1.8),300,5,'#CCEFFA',(0,0,-.4))
    for n,c in [('platform_mint','#17B995'),('platform_pearl','#F3EFE2'),
                ('platform_stone','#B6D4C9'),('platform_gold','#FFD884'),
                ('platform_clay','#E26C55'),('platform_clay_light','#FFA78C'),
                ('platform_crack','#754B62'),('platform_ice','#9BE4FA'),
                ('platform_lilac','#ABA5F2'),('platform_blue','#73B8EF')]:
        material(n,c,.35,.06)

    def rounded_xy(name,loc,size,mat,radius=.45,soft=.065):
        # Explicit rounded XY outline prevents thin-height bevel clamping from
        # turning a long platform into a rectangle with barely visible corners.
        x,y,z=loc;w,d,h=size;r=min(radius,w/2,d/2)
        outline=[]
        for cx,cy,angle in [(w/2-r,d/2-r,0),(-w/2+r,d/2-r,90),
                            (-w/2+r,-d/2+r,180),(w/2-r,-d/2+r,270)]:
            for i in range(9):
                a=math.radians(angle+i*90/8)
                outline.append((x+cx+math.cos(a)*r,y+cy+math.sin(a)*r))
        n=len(outline);verts=[(xx,yy,z+zz) for zz in [-h/2,h/2] for xx,yy in outline]
        faces=[tuple(range(n-1,-1,-1)),tuple(n+i for i in range(n))]
        faces.extend((i,(i+1)%n,n+(i+1)%n,n+i) for i in range(n))
        return mesh(name,verts,faces,mat,soft)

    if kind=='grass':
        face='platform_mint' if theme=='forest' else 'teal_light' if theme=='town' else 'platform_ice'
        body='platform_stone' if theme=='forest' else 'platform_pearl' if theme=='town' else 'platform_blue'
        rounded_xy('soft colorful landing cushion',(0,0,-.14),(8,1.05,.36),face,.50,.085)
        rounded_xy('bright thin landing rim',(0,-.015,.025),(7.82,1.02,.075),
                   'mint' if theme=='forest' else 'platform_ice',.48,.025)
        rounded_xy('lightweight rounded underside',(0,.035,-.41),(7.45,.90,.28),body,.43,.07)
        if theme=='forest':
            for x,y,sz in [(-2.65,.02,.48),(-.95,.05,.56),(.9,.05,.52),(2.6,.02,.46)]:
                sphere('pale rounded island pebble',(x,y,-.59),(sz,.40,.27),'platform_stone')
            for x in [-2.8,2.8]:
                tube('small hanging vine',[(x,-.26,-.35),(x-.12,-.32,-.74)],.028,'leaf')
                sphere('fresh emerald pendant leaf',(x-.06,-.37,-.60),(.17,.07,.23),'leaf_light',rot=(0,.55,0))
            for x in [-1.8,1.8]:sphere('tiny coral flower',(x,-.50,-.10),(.065,.04,.065),'coral')
        elif theme=='town':
            for x in [-2.65,2.65]:
                ring('rounded ivory suspension loop',(x,-.1,-.57),.21,.075,'platform_gold',rot=(math.pi/2,0,0))
            rounded_xy('bright cyan inset',(0,-.46,-.37),(2.8,.08,.12),'teal_light',.035,.025)
        else:
            for x,sz in [(-2.6,.43),(-.9,.55),(.9,.55),(2.6,.43)]:
                cone('luminous downward ice facet',(x,.02,-.67),.04,.33,sz,'platform_ice',6,.028)
    elif kind=='moving':
        rounded_xy('warm white hover shell',(0,0,-.18),(8,1.04,.40),'platform_pearl',.50,.085)
        rounded_xy('pale gold landing trim',(0,-.015,.025),(7.80,1.02,.09),'platform_gold',.48,.028)
        rounded_xy('shallow ivory underbody',(0,.06,-.43),(6.85,.83,.20),'cream',.40,.06)
        for x in [-2.75,2.75]:
            cylinder('round mint propulsion housing',(x,-.55,-.54),.29,.28,'platform_mint',rot=(math.pi/2,0,0))
            cylinder('bright aqua engine inset',(x,-.71,-.54),.21,.075,'teal_light',rot=(math.pi/2,0,0))
            cylinder('warm white engine core',(x,-.76,-.54),.10,.035,'white',rot=(math.pi/2,0,0))
        for x in [-.83,.83]:
            tube('subtle gold direction chevron',[(x+.16,-.54,-.19),(x-.04,-.55,-.32),(x+.16,-.54,-.44)],.045,'platform_gold')
    elif kind=='breakable':
        rounded_xy('warm coral fracture slab',(0,0,-.17),(8,1.03,.40),'platform_clay',.50,.085)
        rounded_xy('terracotta lit landing edge',(0,-.015,.025),(7.80,1.0,.085),'platform_clay_light',.48,.025)
        for x,z,rot in [(-2.8,-.49,-.07),(-1.0,-.53,.05),(1.0,-.49,-.04),(2.8,-.51,.07)]:
            cube('soft broken terracotta piece',(x,.02,z),(1.69,.82,.35),'platform_clay_light',.15,rot=(0,rot,0))
        # A broad high-contrast zig-zag is readable at native 80px width.
        tube('deep continuous fracture',[(.48,-.54,.065),(.05,-.55,-.10),(.54,-.55,-.28),(.10,-.55,-.49),(.29,-.55,-.70)],.070,'platform_crack')
        tube('large fracture fork',[(.07,-.55,-.10),(-.50,-.55,-.25),(-.78,-.55,-.44)],.044,'platform_crack')
    else:
        rounded_xy('soft purple energy capsule',(0,0,-.18),(8,1.03,.40),'platform_lilac',.50,.09)
        rounded_xy('icy lavender landing trim',(0,-.015,.025),(7.82,1.0,.09),'purple_light',.48,.025)
        rounded_xy('blue luminous lower shell',(0,.04,-.43),(7.10,.85,.22),'platform_blue',.41,.065)
        tube('thin pearl energy filament',[(-3.25,-.53,-.29),(-1.1,-.55,-.34),(1.1,-.55,-.29),(3.25,-.53,-.34)],.028,'white')
        for x in [-2.6,0,2.6]:sphere('gentle suspension light',(x,-.04,-.62),(.13,.13,.13),'pink_glow')
    if kind!='grass':
        if theme=='forest':
            for x in [-3.25,3.25]:
                tube('forest theme tendril',[(x,0,-.31),(x-.10,-.10,-.62)],.022,'leaf')
                sphere('small fresh leaf',(x-.07,-.14,-.54),(.13,.05,.18),'leaf_light',rot=(0,.55,0))
        elif theme=='town':
            for x in [-3.3,3.3]:sphere('ivory city side rivet',(x,-.47,-.22),(.065,.03,.065),'white')
        else:
            for x in [-1.7,1.7]:cone('ice theme hanging gem',(x,.12,-.57),.025,.12,.30,'platform_ice',6,.02)
    raw=render('_platform_render.png')
    bbox=raw.getchannel('A').point(lambda v:255 if v>8 else 0).getbbox();raw=raw.crop(bbox)
    out=Image.new('RGBA',(320,96))
    # Preserve the camera's proportions: a fixed-height stretch made circular
    # thrusters oval and inflated a thin shell into a heavy rectangular block.
    scale=min(316/raw.width,92/raw.height)
    thumb=raw.resize((round(raw.width*scale),round(raw.height*scale)),Image.Resampling.LANCZOS)
    out.alpha_composite(thumb,((320-thumb.width)//2,0))
    # The collision edge is an explicit bright, thin rim. Rounded outside ends
    # remain transparent while the gameplay's central contact line is at Y=0.
    pix=out.load();fallback=next(pix[160,y] for y in range(12) if pix[160,y][3]>=128)
    for x in range(24,296):
        sample=next((pix[x,y] for y in range(12) if pix[x,y][3]>=128),fallback)
        for y in range(3):pix[x,y]=(*sample[:3],255)
    name='platform_'+kind+('' if theme=='forest' else '_'+theme)
    out.save(OUT/(name+'.png'));print('READY',name,flush=True)

if args.only in ['preview','hero','all']:
    poses=['idle'] if args.only=='preview' else ['idle','rise','fall','blink','jet']
    for pose in poses:render_hero(pose)
    if args.only!='preview':render_hero('idle',True)
if args.only in ['preview','forest','backgrounds','all']:
    forest_scene(False)
    if args.only!='preview':forest_scene(True)
if args.only in ['backgrounds','all']:
    town_scene();crystal_scene()
if args.only in ['platforms','all']:
    for theme in ['forest','town','crystal']:
        for kind in ['grass','moving','breakable','vanishing']:platform_scene(kind,theme)
print('Completed original 3D art:',OUT,flush=True)
