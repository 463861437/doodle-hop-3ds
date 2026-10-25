#include "render.hpp"
#include "sprite_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
// Keep the rasterizer private in production while testing its exact sampling
// against an independent pixel reference in this translation unit.
#include "../source/render.cpp"
using namespace hop;
unsigned checks=0,frames=0;
void check(bool value,const char* what){++checks;if(!value){std::fprintf(stderr,"render FAIL: %s\n",what);std::exit(1);}}
struct Buffer {
    static constexpr size_t GUARD=128;int w,h;std::vector<uint8_t> bytes;
    Buffer(int width,int height):w(width),h(height),bytes(size_t(w)*h*3+2*GUARD,0xD7){}
    Surface surface(){return {bytes.data()+GUARD,w,h};}
    void intact(){check(std::all_of(bytes.begin(),bytes.begin()+GUARD,[](uint8_t x){return x==0xD7;}),"leading framebuffer guard");check(std::all_of(bytes.end()-GUARD,bytes.end(),[](uint8_t x){return x==0xD7;}),"trailing framebuffer guard");}
    uint32_t at(int x,int y)const{size_t p=GUARD+(size_t(x)*h+h-1-y)*3;return (uint32_t(bytes[p+2])<<16)|(uint32_t(bytes[p+1])<<8)|bytes[p];}
};
void frame(const Game& g,const View& v,Buffer& a,Buffer& b){render(g,v,a.surface(),b.surface());++frames;a.intact();b.intact();}
unsigned differences(const Buffer& a,const Buffer& b,int x,int y,int w,int h){
    unsigned changed=0;
    for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)changed+=a.at(px,py)!=b.at(px,py);
    return changed;
}
std::array<int,4> differenceBounds(const Buffer& a,const Buffer& b,int y0,int y1){
    std::array<int,4> out{{a.w,-1,a.h,-1}};
    for(int x=0;x<a.w;++x)for(int y=y0;y<y1;++y)if(a.at(x,y)!=b.at(x,y)){
        out[0]=std::min(out[0],x);out[1]=std::max(out[1],x);
        out[2]=std::min(out[2],y);out[3]=std::max(out[3],y);
    }
    return out;
}
std::vector<uint8_t> sceneEdges(const Buffer& b){
    auto luma=[](uint32_t c){return int(((c>>16)&255)*3+((c>>8)&255)*6+(c&255))/10;};
    std::vector<uint8_t> edges;
    for(int y=30;y<238;y+=2)for(int x=2;x<398;x+=2){
        const int dx=luma(b.at(x+2,y))-luma(b.at(x-2,y));
        const int dy=luma(b.at(x,y+2))-luma(b.at(x,y-2));
        edges.push_back(uint8_t(std::abs(dx)+std::abs(dy)>24));
    }
    return edges;
}
void referenceRegion(Buffer& b,int x,int y,const hopArt::Sprite& art,int sx,int sy,int sw,int sh,int w,int h,bool mirror,uint8_t opacity,int clipLeft,int clipRight);
void checkWorldProjection(){
    Game empty;empty.scene=Scene::Playing;empty.player.y=-1000;
    View v;Buffer background(400,240),bottom(320,240),actual(400,240),seam(400,240);
    frame(empty,v,background,bottom);
    auto object=[&](int kind,float x){
        Game g=empty;
        if(kind==0){g.player.x=x;g.player.y=100;}
        if(kind==1)g.enemies[0]={true,x,100,x,0};
        if(kind==2)g.bullets[0]={true,x,100};
        if(kind==3)g.particles[0]={true,x,100,0,0,1,0xFF00FF};
        if(kind==4)g.platforms[0]={true,false,1,x,100,52,x,0,0,0,PlatformKind::Normal,Pickup::Star};
        return g;
    };
    // These values are deliberately independent of screenX/WORLD_TO_SCREEN_X:
    // each quarter of the original 280-unit world spans exactly 100 pixels.
    for(int kind=0;kind<5;++kind){
        for(int quarter=1;quarter<=3;++quarter){
            const Game g=object(kind,float(70*quarter));frame(g,v,actual,bottom);
            const auto bounds=differenceBounds(actual,background,80,175);
            check(bounds[1]>=bounds[0],"each projected world object remains visible");
            if(kind==0||kind==1){
                // The model's tail and backpack make its alpha bounds
                // asymmetric. Verify the actual foot/centre anchor pixel for
                // pixel, without assuming that its silhouette is symmetric.
                const auto& art=kind==0?hopArt::hero_right:hopArt::enemy;
                const int x=100*quarter-(kind==0?20:art.width/2),y=140-(kind==0?40:art.height/2);
                Buffer expected=background;
                referenceRegion(expected,x,y,art,0,0,art.width,art.height,art.width,art.height,false,255,0,400);
                check(actual.bytes==expected.bytes,"projected sprite exactly matches independently composited foot/centre anchor");
            }else check(std::abs((bounds[0]+bounds[1])*.5f-100*quarter)<=3,
                       "projectile, particle and platform use the same full-width world projection");
        }
        frame(object(kind,0),v,actual,bottom);frame(object(kind,280),v,seam,bottom);
        check(actual.bytes==seam.bytes,"every world object uses a 400-pixel wrap seam");
    }
    for(float width:{52.f,58.f,68.f}){
        Game g=object(4,140);g.platforms[0].width=width;g.platforms[0].pickup=Pickup::None;
        frame(g,v,actual,bottom);const auto bounds=differenceBounds(actual,background,140,164);
        const float expected=width*400.f/280.f;
        check(std::abs(float(bounds[1]-bounds[0]+1)-expected)<=4,
              "visible platform width follows the projected physics width");
    }
}
void checkHudMetrics(){
    Game baseline;baseline.scene=Scene::Playing;baseline.player.y=-1000;
    baseline.height=123;baseline.stars=7;baseline.score=456;
    View v;Buffer reference(400,240),referenceBottom(320,240),actual(400,240),bottom(320,240);
    frame(baseline,v,reference,referenceBottom);
    constexpr int badgeX[]={8,140,272};
    for(int metric=0;metric<3;++metric){
        Game changed=baseline;
        if(metric==0)changed.height=1789;
        if(metric==1)changed.stars=std::numeric_limits<uint32_t>::max();
        if(metric==2)changed.score=std::numeric_limits<uint32_t>::max();
        frame(changed,v,actual,bottom);
        check(differences(actual,reference,badgeX[metric],4,120,24)>4,"each live metric updates its compact HUD badge");
        for(int other=0;other<3;++other)if(other!=metric)
            check(differences(actual,reference,badgeX[other],4,120,24)==0,"large HUD numbers cannot spill into another badge");
        check(bottom.bytes==referenceBottom.bytes,"live metrics do not alter the fixed playing controls");
    }
    Game record=baseline;record.profile.bestHeight=1789;record.profile.totalStars=200;record.profile.sound=false;
    frame(record,v,actual,bottom);
    check(bottom.bytes==referenceBottom.bytes,"record and sound fields absent from playing controls do not change its background");
    for(int x:badgeX){
        Game behind=baseline;behind.player.x=float(x+60)*280.f/400.f;behind.player.y=212;
        frame(behind,v,actual,bottom);unsigned opaque=0,overwritten=0;
        for(int py=0;py<24;++py)for(int px=0;px<120;++px)
            if(hopArt::hud_plate.rgba[(size_t(py)*120+px)*4+3]==255){
                ++opaque;overwritten+=actual.at(x+px,4+py)!=reference.at(x+px,4+py);
            }
        check(opaque>100&&overwritten==0,"opaque HUD artwork remains above a hero passing behind its badge");
    }
}
void checkLandingEffects(){
    Game g;g.scene=Scene::Playing;g.player.x=140;g.player.y=100;
    View v;v.animationTime=.5f;
    Buffer baseline(400,240),actual(400,240),seam(400,240),bottom(320,240);
    frame(g,v,baseline,bottom);g.player.squash=.8f;frame(g,v,actual,bottom);
    check(differences(actual,baseline,170,140,60,12)>8,"landing pulse is visible at the physical foot position");
    g.player.x=0;frame(g,v,actual,bottom);g.player.x=280;frame(g,v,seam,bottom);
    check(actual.bytes==seam.bytes,"landing pulse and character wrap together at the world seam");
    g.player.x=140;
    for(float strength:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        g.player.squash=strength;frame(g,v,actual,bottom);
        check(actual.bytes==baseline.bytes,"expired or non-finite landing pulse leaves no stale pixels");
    }
}
double luminance(uint32_t color){
    auto linear=[](unsigned channel){const double s=double(channel)/255;return s<=.04045?s/12.92:std::pow((s+.055)/1.055,2.4);};
    return .2126*linear((color>>16)&255)+.7152*linear((color>>8)&255)+.0722*linear(color&255);
}
void checkTextContrast(const Buffer& frame,int x,int y,const char* label,int scale,double minimum){
    // Measure the rendered letters against their nearest non-letter pixels.
    // No palette or background colour is prescribed, so a new illustration
    // cannot silently make an otherwise unchanged label unreadable.
    std::vector<uint8_t> mask(size_t(frame.w)*frame.h);
    const char* input=label;int cursor=x;
    while(*input){
        const auto* letter=hop::glyph(hop::decode(input));if(!letter)continue;
        for(int gy=0;gy<14;++gy)for(int gx=0;gx<letter->width;++gx){
            const uint8_t alpha=letter->alpha[gy*14+gx];if(!alpha)continue;
            for(int dy=0;dy<scale;++dy)for(int dx=0;dx<scale;++dx){
                const int px=cursor+gx*scale+dx,py=y+gy*scale+dy;
                if(px>=0&&px<frame.w&&py>=0&&py<frame.h)mask[size_t(py)*frame.w+px]=alpha>=250?2:1;
            }
        }
        cursor+=letter->width*scale;
    }
    double lowest=100;unsigned samples=0;
    for(int py=0;py<frame.h;++py)for(int px=0;px<frame.w;++px)if(mask[size_t(py)*frame.w+px]==2){
        bool found=false;double local=100;
        for(int radius=1;radius<=4&&!found;++radius)for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
            if(std::abs(dx)+std::abs(dy)!=radius)continue;
            const int nx=px+dx,ny=py+dy;
            if(nx<0||nx>=frame.w||ny<0||ny>=frame.h||mask[size_t(ny)*frame.w+nx])continue;
            const double a=luminance(frame.at(px,py)),b=luminance(frame.at(nx,ny));
            local=std::min(local,(std::max(a,b)+.05)/(std::min(a,b)+.05));found=true;
        }
        if(found){lowest=std::min(lowest,local);++samples;}
    }
    if(samples<=20||lowest<minimum)std::fprintf(stderr,"text contrast '%s': %.3f, samples=%u, required=%.1f\n",label,lowest,samples,minimum);
    check(samples>20&&lowest>=minimum,"small-screen labels retain local contrast on the actual rendered background");
}
void checkUiReadability(){
    Game g;View v;Buffer top(400,240),bottom(320,240);
    g.scene=Scene::Help;frame(g,v,top,bottom);
    checkTextContrast(top,22,14,"漫游指南",2,3.0);
    checkTextContrast(top,24,49,"认清落脚点，探索更高处。",1,4.5);
    checkTextContrast(top,52,215,"星星加分，弹簧与喷气助你高飞。",1,4.5);
    g.scene=Scene::Playing;frame(g,v,top,bottom);
    checkTextContrast(bottom,236+(68-hop::width("长按"))/2,136,"长按",1,4.5);
}
void referenceRegion(Buffer& b,int x,int y,const hopArt::Sprite& art,int sx,int sy,int sw,int sh,int w,int h,bool mirror,uint8_t opacity,int clipLeft,int clipRight){
    for(int py=0;py<h;++py)for(int px=0;px<w;++px){
        const int outX=x+px,outY=y+py;
        if(outX<clipLeft||outX>=clipRight||outY<0||outY>=b.h)continue;
        const int sourceX=sx+(mirror?w-1-px:px)*sw/w,sourceY=sy+py*sh/h;
        const uint8_t* rgba=art.rgba+(size_t(sourceY)*art.width+sourceX)*4;
        const unsigned alpha=(unsigned(rgba[3])*opacity+127)/255;
        auto* pixel=b.bytes.data()+Buffer::GUARD+(size_t(outX)*b.h+b.h-1-outY)*3;
        for(int channel=0;channel<3;++channel)
            pixel[channel]=uint8_t((unsigned(rgba[2-channel])*alpha+unsigned(pixel[channel])*(255-alpha)+127)/255);
    }
}
void checkScaling(){
    Buffer actual(400,240),expected(400,240);
    auto region=[&](const hopArt::Sprite& art,int sx,int sy,int sw,int sh,int w,int h){
        for(const auto clip:std::array<std::array<int,2>,2>{{{{0,400}},{{73,315}}}})
        for(bool mirror:{false,true})for(int x:{clip[0]-11,160,clip[1]-14})for(int y:{-9,100,232})for(uint8_t alpha:{uint8_t(255),uint8_t(93)}){
            // A nonuniform background also checks blending when source pixels
            // are transparent, and makes wrong edge samples observable.
            for(size_t i=Buffer::GUARD;i<actual.bytes.size()-Buffer::GUARD;++i)actual.bytes[i]=uint8_t((i*53+i/17)%256);
            expected.bytes=actual.bytes;
            hop::Canvas canvas(actual.surface());canvas.clip(clip[0],clip[1]);
            if(sx==0&&sy==0&&sw==art.width&&sh==art.height)canvas.scaled(x,y,art,w,h,mirror,alpha);
            else canvas.spriteRegion(x,y,art,sx,sy,sw,sh,w,h,mirror,alpha);
            referenceRegion(expected,x,y,art,sx,sy,sw,sh,w,h,mirror,alpha,clip[0],clip[1]);
            check(actual.bytes==expected.bytes,"DDA scaling matches independent sampling/blending for mirror, clipping and crop");
        }
    };
    for(int w:{74,82,83,97}){
        const auto& art=hopArt::platform_grass;region(art,0,0,art.width,art.height,w,art.height);
        const auto& split=hopArt::platform_breakable;
        region(split,0,0,split.width/2,split.height,w/2,split.height);
        region(split,split.width/2,0,split.width-split.width/2,split.height,w-w/2,split.height);
    }
    for(int h:{10,13,17,24}){
        const auto& art=hopArt::jet_flame;region(art,0,0,art.width,art.height,art.width,h);
    }
    const auto& native=hopArt::hero_right;
    region(native,0,0,native.width,native.height,native.width,native.height);
    // Nine-slice UI panels also crop vertically; platform fragments
    // alone only exercise a horizontal source offset.
    const auto& panel=hopArt::ui_panel;
    region(panel,14,14,panel.width-28,panel.height-28,101,37);
    region(panel,panel.width-14,panel.height-14,14,14,14,14);
    actual.intact();expected.intact();
}
int main(){
    checkScaling();
    checkWorldProjection();
    checkHudMetrics();
    checkLandingEffects();
    checkUiReadability();
    Game g;View v;Buffer top(400,240),bottom(320,240),alternate(400,240),alternateBottom(320,240);
    // Sprite opacity, not its transparent bounding box, defines the visible
    // foot. Every pose shares y=40, with antialiasing/shadow permitted below it.
    for(const hopArt::Sprite* art:{&hopArt::hero_right,&hopArt::hero_left,&hopArt::hero_rise,&hopArt::hero_fall,&hopArt::hero_blink,&hopArt::hero_jet}){
        check(art->width==40&&art->height==44,"hero poses preserve the physical foot anchor dimensions");
        int lastSolid=-1;unsigned lowestOpacity=0;
        for(int y=0;y<art->height;++y)for(int x=0;x<art->width;++x){
            const unsigned alpha=art->rgba[(size_t(y)*art->width+x)*4+3];
            if(alpha>=128)lastSolid=y;
            if(y>=42)lowestOpacity=std::max(lowestOpacity,alpha);
        }
        check(lastSolid>=39&&lastSolid<=40&&lowestOpacity<64,"opaque feet meet y40 with only soft fringe below");
    }
    for(const hopArt::Sprite* art:{&hopArt::platform_grass,&hopArt::platform_moving,&hopArt::platform_breakable,&hopArt::platform_vanishing,
                                 &hopArt::platform_grass_town,&hopArt::platform_moving_town,&hopArt::platform_breakable_town,&hopArt::platform_vanishing_town,
                                 &hopArt::platform_grass_crystal,&hopArt::platform_moving_crystal,&hopArt::platform_breakable_crystal,&hopArt::platform_vanishing_crystal}){
        check(art->width==80&&art->height==24,"all biome platforms share their landing geometry dimensions");
        check(art->rgba[(art->width/2)*4+3]>=128,"platform top has no transparent margin above landing line");
    }
    for(auto scene:{Scene::Title,Scene::Ready,Scene::Playing,Scene::Paused,Scene::GameOver,Scene::Help}){
        g.scene=scene;
        for(int i=0;i<MAX_PLATFORMS;++i){auto& p=g.platforms[i];p.active=true;p.x=float(i*53%280);p.y=float(i*33-90);p.width=54;p.kind=static_cast<PlatformKind>(i%4);p.pickup=static_cast<Pickup>(i%4);}
        for(int flags=0;flags<8;++flags){v.quitConfirm=(flags&1)!=0;v.saveFailed=(flags&2)!=0;v.audioAvailable=(flags&4)==0;g.player.jetTime=flags%2?1.2f:0;frame(g,v,top,bottom);}
    }
    v={};g.scene=Scene::Playing;g.platforms={};g.enemies={};g.bullets={};g.particles={};g.player={};g.player.y=100;
    // Moving only the hero leaves the same world behind it. Compare against
    // that background instead of relying on any particular palette or sprite.
    for(bool flying:{false,true}){
        g.player.jetTime=flying?1.3f:0;g.player.x=140;frame(g,v,alternate,alternateBottom);
        g.player.x=0;frame(g,v,top,bottom);
        check(differences(top,alternate,0,92,25,75)>20&&differences(top,alternate,375,92,25,75)>20,
              "wrapped hero visibly occupies both edges of the playfield");
        check(differences(top,alternate,0,0,400,28)==0,
              "hero and jet flame below the HUD leave its top 28 pixels unchanged");
        Buffer seam(400,240),seamBottom(320,240);g.player.x=WORLD_WIDTH;frame(g,v,seam,seamBottom);
        check(top.bytes==seam.bytes,"left and right world seam positions render identically");
    }
    // Animation uses the same physical footprint; rising, falling and a blink
    // must each produce a different image without affecting the HUD.
    g.player.x=140;g.player.jetTime=0;g.player.vy=0;v.animationTime=1;
    frame(g,v,alternate,alternateBottom);
    for(const auto& state:std::array<std::array<float,3>,4>{{{{180,1,0}},{{-180,1,0}},{{0,3.9f,0}},{{180,1,1.2f}}}}){
        g.player.vy=state[0];v.animationTime=state[1];g.player.jetTime=state[2];frame(g,v,top,bottom);
        check(differences(top,alternate,176,92,48,75)>8,"rise, fall, blink and jet poses differ from resting hero");
    }
    g.player.vy=180;g.player.jetTime=0;v.animationTime=1;g.player.facing=1;frame(g,v,alternate,alternateBottom);
    g.player.facing=-1;frame(g,v,top,bottom);
    check(differences(top,alternate,176,92,48,75)>8,"direction changes character pose or orientation");
    // Isolate the maximum-altitude theme selector from camera/parallax and HUD
    // numbers. A day-only background must not pass merely because height text
    // or grid scrolling changed.
    std::array<std::vector<uint8_t>,3> themes;
    std::array<std::vector<uint8_t>,3> themeEdges;
    const float heights[]={0,3000,6000};
    g.camera=0;g.player.y=100;
    for(int i=0;i<3;++i){
        g.height=heights[i];frame(g,v,top,bottom);
        for(int x=0;x<400;++x)for(int y=28;y<240;++y){
            const uint32_t pixel=top.at(x,y);
            for(unsigned shift:{0u,8u,16u})themes[i].push_back(uint8_t(pixel>>shift));
        }
        themeEdges[i]=sceneEdges(top);
    }
    check(themes[0]!=themes[1]&&themes[1]!=themes[2]&&themes[0]!=themes[2],"three altitude themes produce distinct frames");
    for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){
        unsigned either=0,shared=0;
        for(size_t i=0;i<themeEdges[a].size();++i){either+=themeEdges[a][i]||themeEdges[b][i];shared+=themeEdges[a][i]&&themeEdges[b][i];}
        check(either>100&&shared*100<either*85,"biomes have different edge arrangements beyond a palette change");
    }
    // Theme boundaries must use the same rendering path in either physical
    // framebuffer; transitions cannot leave pixels from the previous theme.
    for(float h:{1999.f,2000.f,4999.f,5000.f}){
        g.height=h;g.camera=h;g.player.y=h+100;frame(g,v,top,bottom);frame(g,v,alternate,alternateBottom);
        check(top.bytes==alternate.bytes&&bottom.bytes==alternateBottom.bytes,"theme transition fully replaces both framebuffers");
    }
    g={};g.scene=Scene::Playing;g.player.y=-1000;v={};frame(g,v,alternate,alternateBottom);
    for(float biomeHeight:{0.f,3000.f,6000.f}){
        g.height=biomeHeight;g.platforms={};frame(g,v,alternate,alternateBottom);
        for(auto kind:{PlatformKind::Normal,PlatformKind::Moving,PlatformKind::Breakable,PlatformKind::Vanishing}){
            g.platforms[0]={true,false,1,140,100,56,140,0,0,0,kind,Pickup::None};frame(g,v,top,bottom);
            check(differences(top,alternate,192,140,16,1)>8,"visible platform surface is drawn exactly at physics landing Y");
            check(differences(top,alternate,160,139,80,1)==0,"platform graphic does not protrude above its collision line");
        }
    }
    // Full redraw must reproduce the same state in a different physical buffer.
    frame(g,v,alternate,alternateBottom);check(top.bytes==alternate.bytes&&bottom.bytes==alternateBottom.bytes,"alternate framebuffer receives complete deterministic output");
    Buffer direct(321,240);
    auto compareCache=[&]{
        render(g,v,{},bottom.surface());render(g,v,{},direct.surface());
        check(std::equal(bottom.bytes.begin()+Buffer::GUARD,bottom.bytes.end()-Buffer::GUARD,
                         direct.bytes.begin()+Buffer::GUARD),"UI cache matches independently redrawn state");
        std::fill(alternateBottom.bytes.begin()+Buffer::GUARD,alternateBottom.bytes.end()-Buffer::GUARD,0x39);
        render(g,v,{},alternateBottom.surface());
        check(bottom.bytes==alternateBottom.bytes,"cache hit restores another physical framebuffer");
        bottom.intact();alternateBottom.intact();direct.intact();
    };
    for(auto scene:{Scene::Title,Scene::Ready,Scene::Playing,Scene::Paused,Scene::GameOver,Scene::Help}){
        g.scene=scene;compareCache();g.profile.bestHeight+=200;compareCache();g.profile.totalStars+=10;compareCache();
        g.profile.sound=!g.profile.sound;compareCache();v.saveFailed=!v.saveFailed;compareCache();
        v.audioAvailable=!v.audioAvailable;compareCache();v.quitConfirm=!v.quitConfirm;compareCache();
    }
    g.scene=Scene::Playing;v={};v.touchActive=true;
    // Crossing the highlight threshold can leave the rounded knob coordinate
    // unchanged. The highlight bits therefore need independent cache identity.
    for(float axis:{-1.f,-.5f,0.f,.5f,1.f,.089f,.091f,-.089f,-.091f}){v.touchAxis=axis;compareCache();}
    v.touchActive=false;compareCache();
    v.quitConfirm=true;render(g,v,{},bottom.surface());const auto quitPixels=bottom.bytes;
    for(float axis:{-1.f,0.f,1.f}){
        v.touchActive=true;v.touchAxis=axis;compareCache();
        check(bottom.bytes==quitPixels,"touch overlay cannot change an open quit confirmation");
    }
    v.quitConfirm=false;v.touchActive=false;compareCache();
    for(float h:{0.f,3000.f,6000.f}){g.height=h;g.camera=h;g.player.y=h+100;compareCache();}
    v.touchActive=true;v.touchAxis=std::numeric_limits<float>::quiet_NaN();compareCache();
    v.touchAxis=std::numeric_limits<float>::infinity();compareCache();v={};
    g.profile.bestHeight=g.profile.bestScore=g.profile.totalStars=std::numeric_limits<uint32_t>::max();g.height=1000000;g.score=std::numeric_limits<uint32_t>::max();g.camera=1000000;g.player.y=1000110;g.player.x=279;frame(g,v,top,bottom);
    g.player.x=std::numeric_limits<float>::quiet_NaN();g.player.vy=std::numeric_limits<float>::infinity();g.camera=std::numeric_limits<float>::infinity();g.height=0;
    v.animationTime=std::numeric_limits<float>::quiet_NaN();frame(g,v,top,bottom);
    g.platforms[0]={true,false,1,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),54,0,0,0,0,PlatformKind::Normal,Pickup::Star};
    g.enemies[0]={true,std::numeric_limits<float>::quiet_NaN(),100,0,0};
    g.bullets[0]={true,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
    g.particles[0]={true,std::numeric_limits<float>::quiet_NaN(),100,0,0,.2f,0xFFFFFF};frame(g,v,top,bottom);
    for(const auto& size:std::array<std::array<int,2>,7>{{{{1,1}},{{3,2}},{{17,31}},{{93,71}},{{400,1}},{{1,240}},{{0,0}}}}){
        Buffer small(size[0],size[1]),smallBottom(size[0],size[1]);
        for(auto scene:{Scene::Title,Scene::Playing,Scene::Help}){g.scene=scene;frame(g,v,small,smallBottom);}
    }
    Buffer small(93,71),smallBottom(83,51);frame(g,v,small,smallBottom);render(g,v,{},{});
    std::printf("render: %u full/partial frames, %u checks passed (projection, sprite sampling, biome geometry, effects, text contrast, cache and buffer guards)\n",frames,checks);
}
