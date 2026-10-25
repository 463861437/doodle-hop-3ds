#include "render.hpp"
#include "font_data.hpp"
#include "sprite_data.hpp"
#include "background_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace hop {
namespace {
using Color=uint32_t;
constexpr Color INK=0x253950,WHITE=0xF8FCFF,MUTED=0xBCD1DF,TEAL=0x53D5C5,GOLD=0xFFD38B;
constexpr Color EDGE=0xAAC2D2,SHADOW=0x172A43,CREAM=0xF2F7FB;
struct Rect{int x,y,w,h;};
Color blend(Color a,Color b,unsigned t){
    const unsigned u=255-t;
    return ((((a>>16)&255)*u+((b>>16)&255)*t)/255<<16)|
           ((((a>>8)&255)*u+((b>>8)&255)*t)/255<<8)|((a&255)*u+(b&255)*t)/255;
}
class Canvas {
    Surface s;int left=0,right=0;
public:
    explicit Canvas(Surface surface):s(surface),right(std::max(0,surface.width)){}
    void clip(int l,int r){left=std::max(0,l);right=std::min(s.width,r);}
    void pixel(int x,int y,Color c,uint8_t alpha=255){
        if(!s.pixels||x<left||x>=right||y<0||y>=s.height||!alpha)return;
        uint8_t* p=s.pixels+(size_t(x)*s.height+s.height-1-y)*3;
        const uint8_t rgb[]={uint8_t(c),uint8_t(c>>8),uint8_t(c>>16)};
        if(alpha==255){std::memcpy(p,rgb,3);return;}
        for(int k=0;k<3;++k)p[k]=uint8_t((rgb[k]*alpha+p[k]*(255-alpha)+127)/255);
    }
    void rect(int x,int y,int w,int h,Color color){
        if(!s.pixels||w<=0||h<=0)return;
        const int x0=std::max(left,x),x1=std::min(right,x+w),y0=std::max(0,y),y1=std::min(s.height,y+h);
        if(x0>=x1||y0>=y1)return;
        for(int px=x0;px<x1;++px){
            uint8_t* p=s.pixels+(size_t(px)*s.height+s.height-y1)*3;
            for(int py=y0;py<y1;++py){p[0]=uint8_t(color);p[1]=uint8_t(color>>8);p[2]=uint8_t(color>>16);p+=3;}
        }
    }
    void background(const hopArt::Background& art){
        if(!s.pixels||s.width<=0||s.height<=0)return;
        if(s.width==art.width&&s.height==art.height){std::memcpy(s.pixels,art.bgr,size_t(s.width)*s.height*3);return;}
        rect(0,0,s.width,s.height,SHADOW);
        const int w=std::min(s.width,art.width),h=std::min(s.height,art.height);
        for(int x=0;x<w;++x){
            if(s.height==art.height)std::memcpy(s.pixels+size_t(x)*s.height*3,art.bgr+size_t(x)*art.height*3,size_t(h)*3);
            else for(int y=0;y<h;++y){
                const uint8_t* p=art.bgr+(size_t(x)*art.height+art.height-1-y)*3;
                pixel(x,y,(Color(p[2])<<16)|(Color(p[1])<<8)|p[0]);
            }
        }
    }
    void line(int x,int y,int ex,int ey,Color color,uint8_t alpha=255){
        const int dx=std::abs(ex-x),sx=x<ex?1:-1,dy=-std::abs(ey-y),sy=y<ey?1:-1;int error=dx+dy;
        for(int limit=0;limit<2048;++limit){pixel(x,y,color,alpha);if(x==ex&&y==ey)break;
            const int e=2*error;if(e>=dy){error+=dy;x+=sx;}if(e<=dx){error+=dx;y+=sy;}}
    }
    void round(Rect r,int radius,Color top,Color bottom){
        if(r.w<=0||r.h<=0)return;
        radius=std::clamp(radius,0,std::min(r.w,r.h)/2);
        for(int row=0;row<r.h;++row){
            const int dy=row<radius?radius-1-row:row>=r.h-radius?row-(r.h-radius):0;
            const int inset=dy?radius-int(std::sqrt(float(radius*radius-dy*dy))):0;
            rect(r.x+inset,r.y+row,r.w-2*inset,1,blend(top,bottom,unsigned(row)*255/std::max(1,r.h-1)));
        }
    }
    void round(Rect r,int radius,Color color){round(r,radius,color,color);}
    void disc(int cx,int cy,int radius,Color color){
        if(radius<1)return;
        for(int y=-radius;y<=radius;++y){const int half=int(std::sqrt(float(radius*radius-y*y)));rect(cx-half,cy+y,half*2+1,1,color);}
    }
    void panel(Rect r,Color=CREAM,Color=EDGE){
        // Preserve the rounded card edges and soft shadow while its centre
        // stretches. Menus are cached, so this costs nothing while steering.
        const auto& art=hopArt::ui_panel;const int edge=std::min(14,std::min(r.w,r.h)/2);
        const int sx[]={0,edge,art.width-edge},sy[]={0,edge,art.height-edge};
        const int sw[]={edge,art.width-2*edge,edge},sh[]={edge,art.height-2*edge,edge};
        const int dx[]={r.x,r.x+edge,r.x+r.w-edge},dy[]={r.y,r.y+edge,r.y+r.h-edge};
        const int dw[]={edge,r.w-2*edge,edge},dh[]={edge,r.h-2*edge,edge};
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)
            spriteRegion(dx[col],dy[row],art,sx[col],sy[row],sw[col],sh[row],dw[col],dh[row]);
    }
    void spriteRegion(int x,int y,const hopArt::Sprite& art,int srcX,int srcY,int srcW,int srcH,int w,int h,bool mirror=false,uint8_t alpha=255){
        if(!alpha||w<=0||h<=0||srcW<=0||srcH<=0||y+h<=0||y>=s.height||x+w<=left||x>=right||!s.pixels)return;
        const int dx0=std::max(0,left-x),dx1=std::min(w,right-x),dy0=std::max(0,-y),dy1=std::min(h,s.height-y);
        // Exact nearest-neighbour sampling, including a clipped/mirrored source
        // region. Advance the quotient and remainder instead of dividing once
        // per pixel: ARM11 has no hardware integer division.
        const int firstNumerator=(mirror?w-1-dx0:dx0)*srcW;
        const int firstSource=srcX+firstNumerator/w,firstRemainder=firstNumerator%w;
        const int sourceStep=mirror?-(srcW/w):srcW/w;
        const int remainderStep=mirror?-(srcW%w):srcW%w;
        for(int dy=dy0;dy<dy1;++dy){const int sy=srcY+dy*srcH/h;
            int sx=firstSource,remainder=firstRemainder;
            for(int dx=dx0;dx<dx1;++dx){
                const uint8_t* p=art.rgba+(size_t(sy)*art.width+sx)*4;
                if(p[3])pixel(x+dx,y+dy,(Color(p[0])<<16)|(Color(p[1])<<8)|p[2],uint8_t((unsigned(p[3])*alpha+127)/255));
                sx+=sourceStep;remainder+=remainderStep;
                if(remainder>=w){remainder-=w;++sx;}
                else if(remainder<0){remainder+=w;--sx;}
            }
        }
    }
    void sprite(int x,int y,const hopArt::Sprite& art,bool mirror=false,uint8_t alpha=255){
        if(!alpha||y+art.height<=0||y>=s.height||x+art.width<=left||x>=right||!s.pixels)return;
        const int dx0=std::max(0,left-x),dx1=std::min(art.width,right-x),dy0=std::max(0,-y),dy1=std::min(art.height,s.height-y);
        // Most artwork is drawn at native size. Keep integer division out of
        // this hot path on the original 3DS CPU.
        for(int dy=dy0;dy<dy1;++dy)for(int dx=dx0;dx<dx1;++dx){
            const uint8_t* p=art.rgba+(size_t(dy)*art.width+(mirror?art.width-1-dx:dx))*4;
            if(p[3])pixel(x+dx,y+dy,(Color(p[0])<<16)|(Color(p[1])<<8)|p[2],uint8_t((unsigned(p[3])*alpha+127)/255));
        }
    }
    void scaled(int x,int y,const hopArt::Sprite& art,int w,int h,bool mirror=false,uint8_t alpha=255){
        if(w==art.width&&h==art.height)sprite(x,y,art,mirror,alpha);
        else spriteRegion(x,y,art,0,0,art.width,art.height,w,h,mirror,alpha);
    }
    void veil(Rect r,Color color,uint8_t alpha){
        for(int x=std::max(left,r.x);x<std::min(right,r.x+r.w);++x)
            for(int y=std::max(0,r.y);y<std::min(s.height,r.y+r.h);++y)pixel(x,y,color,alpha);
    }
};
uint32_t decode(const char*& p){
    const uint8_t first=uint8_t(*p++);if(first<128)return first;
    unsigned count=0;uint32_t cp=0,min=0;
    if(first>=0xC2&&first<=0xDF){count=1;cp=first&31;min=0x80;}
    else if(first>=0xE0&&first<=0xEF){count=2;cp=first&15;min=0x800;}
    else if(first>=0xF0&&first<=0xF4){count=3;cp=first&7;min=0x10000;}else return '?';
    for(unsigned i=0;i<count;++i){const uint8_t v=uint8_t(*p);if((v&0xC0)!=0x80)return '?';cp=(cp<<6)|(v&63);++p;}
    return cp<min||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)?'?':cp;
}
const FontGlyph* glyph(uint32_t cp){
    size_t a=0,b=FONT_COUNT;while(a<b){const size_t m=a+(b-a)/2;if(FONT_GLYPHS[m].codepoint<cp)a=m+1;else b=m;}
    if(a<FONT_COUNT&&FONT_GLYPHS[a].codepoint==cp)return &FONT_GLYPHS[a];
    return cp=='?'?nullptr:glyph('?');
}
int width(const char* str,int scale=1){int n=0;while(*str){const auto* g=glyph(decode(str));if(g)n+=g->width*scale;}return n;}
void text(Canvas& c,int x,int y,const char* str,Color color=INK,int maxWidth=400,int scale=1){
    const int end=x+maxWidth;
    while(*str&&x<end){const auto* g=glyph(decode(str));if(!g)continue;if(x+g->width*scale>end)break;
        for(int gy=0;gy<14;++gy)for(int gx=0;gx<g->width;++gx){const uint8_t alpha=g->alpha[gy*14+gx];if(!alpha)continue;
            for(int sy=0;sy<scale;++sy)for(int sx=0;sx<scale;++sx)c.pixel(x+gx*scale+sx,y+gy*scale+sy,color,alpha);}
        x+=g->width*scale;
    }
}
void centered(Canvas& c,Rect r,const char* str,Color color=INK,int scale=1){text(c,r.x+std::max(0,(r.w-width(str,scale))/2),r.y+(r.h-14*scale)/2,str,color,r.w,scale);}
void formatNumber(char* out,size_t size,uint32_t value){
    if(value>=1000000000u)std::snprintf(out,size,"%lu.%luB",static_cast<unsigned long>(value/1000000000u),static_cast<unsigned long>(value%1000000000u/100000000u));
    else if(value>=1000000u)std::snprintf(out,size,"%lu.%luM",static_cast<unsigned long>(value/1000000u),static_cast<unsigned long>(value%1000000u/100000u));
    else std::snprintf(out,size,"%lu",static_cast<unsigned long>(value));
}
void number(Canvas& c,Rect r,uint32_t value,Color color=WHITE){char str[24];formatNumber(str,sizeof(str),value);centered(c,r,str,color);}
void button(Canvas& c,Rect r,const char* label,Color=0,Color=0,Color labelColor=INK){
    const auto& art=r.w>200?hopArt::ui_primary:hopArt::ui_secondary;
    if(r.w==art.width&&r.h==art.height)c.sprite(r.x,r.y,art);
    else c.scaled(r.x,r.y,art,r.w,r.h);
    centered(c,r,label,labelColor);
}
int coord(float f){return std::isfinite(f)?int(std::clamp(f,-1000.f,1000.f)):0;}
int screenX(float x){return std::isfinite(x)?coord(std::clamp(x,-1000.f,1000.f)*WORLD_TO_SCREEN_X):0;}
int screenY(float y,float camera){return coord(VIEW_HEIGHT-(y-camera));}
uint32_t altitude(float y){return std::isfinite(y)?uint32_t(std::clamp(y,0.f,1000000000.f)):0;}
float safeTime(float t){return std::isfinite(t)?std::fmod(std::max(t,0.f),10000.f):0;}
int scroll(float camera,float factor,int period){return std::isfinite(camera)?int(std::fmod(std::max(0.f,camera)*factor,float(period))):0;}
int regionFor(const Game& g){return altitude(g.height)<2000?0:altitude(g.height)<5000?1:2;}
const hopArt::Sprite& platformArt(PlatformKind kind,int region=0){
    if(region==1){
        if(kind==PlatformKind::Moving)return hopArt::platform_moving_town;
        if(kind==PlatformKind::Breakable)return hopArt::platform_breakable_town;
        if(kind==PlatformKind::Vanishing)return hopArt::platform_vanishing_town;
        return hopArt::platform_grass_town;
    }
    if(region==2){
        if(kind==PlatformKind::Moving)return hopArt::platform_moving_crystal;
        if(kind==PlatformKind::Breakable)return hopArt::platform_breakable_crystal;
        if(kind==PlatformKind::Vanishing)return hopArt::platform_vanishing_crystal;
        return hopArt::platform_grass_crystal;
    }
    if(kind==PlatformKind::Moving)return hopArt::platform_moving;
    if(kind==PlatformKind::Breakable)return hopArt::platform_breakable;
    if(kind==PlatformKind::Vanishing)return hopArt::platform_vanishing;
    return hopArt::platform_grass;
}
void platform(Canvas& c,int x,int y,int w,PlatformKind kind,float brokenTime=0,int region=0){
    const auto& art=platformArt(kind,region);if(y< -art.height-20||y>250)return;
    if(brokenTime>0){
        const float duration=kind==PlatformKind::Breakable?.32f:.20f;
        const float remain=std::clamp(brokenTime/duration,0.f,1.f);
        const int drift=coord((1-remain)*15),dx=coord((1-remain)*7);
        const uint8_t opacity=uint8_t(remain*255);
        c.spriteRegion(x-w/2-dx,y+drift,art,0,0,art.width/2,art.height,w/2,art.height,false,opacity);
        c.spriteRegion(x+dx,y+drift+2,art,art.width/2,0,art.width-art.width/2,art.height,w-w/2,art.height,false,opacity);
    }else c.scaled(x-w/2,y,art,w,art.height);
}
void pickup(Canvas& c,int x,int y,Pickup kind,float time){
    if(kind==Pickup::Spring)c.sprite(x-hopArt::spring.width/2,y-hopArt::spring.height,hopArt::spring);
    else if(kind==Pickup::Star){
        c.sprite(x-hopArt::star.width/2,y-17-hopArt::star.height/2,hopArt::star);
        const float raw=std::fmod(time*2+x*.013f,3.f),phase=raw<0?raw+3:raw;
        if(phase<.7f)c.sprite(x+4,y-32,hopArt::spark,false,uint8_t(175*(1-phase/.7f)));
    }else if(kind==Pickup::Jetpack)c.sprite(x-hopArt::jetpack.width/2,y-17-hopArt::jetpack.height/2,hopArt::jetpack);
}
void scenery(Canvas& c,const Game& g,const View& v){
    const int region=regionFor(g);
    c.background(region==0?hopArt::bg_day:region==1?hopArt::bg_dusk:hopArt::bg_night);c.clip(0,PLAYFIELD_SCREEN_WIDTH);
    const auto& decor=region==0?hopArt::decor_forest:region==1?hopArt::decor_town:hopArt::decor_crystal;
    const int drift=scroll(g.camera,.11f,400);
    c.sprite(-58,(80+drift)%400-140,decor,false,210);
    c.sprite(365,(292+drift)%400-140,decor,true,190);
    const float time=safeTime(v.animationTime);
    const auto& mote=region==0?hopArt::firefly:region==1?hopArt::gear:hopArt::crystal_spark;
    for(int i=0;i<4;++i){
        const int x=24+(i*107)%355+int(std::sin(time*.4f+i*1.7f)*6);
        const int y=(43+i*59+scroll(g.camera,.16f,280))%280-20;
        const uint8_t alpha=uint8_t(75+35*(1+std::sin(time*1.2f+i)));
        c.sprite(x,y,mote,false,alpha);
    }
    if(region==0){
        const int y=(26+scroll(g.camera,.23f,300)+int(std::fmod(time*5,300.f)))%300-20;
        c.sprite(62+int(std::sin(time*.6f)*9),y,hopArt::leaf,false,150);
        c.sprite(319+int(std::sin(time*.5f+2)*9),(y+147)%300-20,hopArt::leaf,true,130);
    }
}
void topHud(Canvas& c,const Game& g){
    c.clip(0,PLAYFIELD_SCREEN_WIDTH);
    for(int x:{8,140,272})c.sprite(x,4,hopArt::hud_plate);
    text(c,17,9,"高度",MUTED,30);number(c,{48,6,72,20},altitude(g.height),WHITE);
    c.sprite(147,5,hopArt::star);number(c,{173,6,77,20},g.stars,GOLD);
    text(c,281,9,"得分",MUTED,30);number(c,{311,6,73,20},g.score,WHITE);
    const int region=regionFor(g);
    c.scaled(310,218,hopArt::hud_plate,82,18);
    centered(c,{314,219,74,16},region==0?"森境 · 01":region==1?"云城 · 02":"晶谷 · 03",WHITE);
    if(std::isfinite(g.player.jetTime)&&g.player.jetTime>0){
        c.round({10,220,58,9},4,0x293A31);
        c.rect(13,223,int(52*std::clamp(g.player.jetTime/1.6f,0.f,1.f)),3,TEAL);
    }
}
void landingPulse(Canvas& c,int x,int y,float squash){
    if(!std::isfinite(squash)||squash<=0||x+26<0||x-26>=PLAYFIELD_SCREEN_WIDTH)return;
    const float strength=std::clamp(squash,0.f,1.f);
    const int rx=12+int((1-strength)*14),ry=2+int((1-strength)*3);
    const uint8_t alpha=uint8_t(strength*145);
    // A small twelve-segment ring adds impact feedback without a fullscreen
    // effect, runtime allocation, or per-pixel trigonometry.
    constexpr int dx[]={1000,866,500,0,-500,-866,-1000,-866,-500,0,500,866,1000};
    constexpr int dy[]={0,500,866,1000,866,500,0,-500,-866,-1000,-866,-500,0};
    for(int i=0;i<12;++i)c.line(x+dx[i]*rx/1000,y+dy[i]*ry/1000,
        x+dx[i+1]*rx/1000,y+dy[i+1]*ry/1000,0xB8F7EA,alpha);
}
void playingTop(Canvas& c,const Game& g,const View& v){
    scenery(c,g,v);const float time=safeTime(v.animationTime);const int region=regionFor(g);
    for(const Platform& p:g.platforms)if(p.active&&std::isfinite(p.x)&&std::isfinite(p.y)){
        const int x=screenX(p.x),y=screenY(p.y,g.camera),w=std::clamp(screenX(p.width),8,PLAYFIELD_SCREEN_WIDTH);
        for(int shift:{-PLAYFIELD_SCREEN_WIDTH,0,PLAYFIELD_SCREEN_WIDTH})if(x+shift+w/2>=0&&x+shift-w/2<PLAYFIELD_SCREEN_WIDTH){
            platform(c,x+shift,y,w,p.kind,p.brokenTime,region);if(p.brokenTime<=0)pickup(c,x+shift,y,p.pickup,time);
        }
    }
    for(const Enemy& e:g.enemies)if(e.active&&std::isfinite(e.x)&&std::isfinite(e.y)){
        const int x=screenX(e.x),y=screenY(e.y,g.camera);
        const auto& art=region==0?hopArt::enemy:region==1?hopArt::enemy_town:hopArt::enemy_crystal;
        for(int shift:{-PLAYFIELD_SCREEN_WIDTH,0,PLAYFIELD_SCREEN_WIDTH})c.sprite(x+shift-art.width/2,y-art.height/2,art);
    }
    for(const Bullet& b:g.bullets)if(b.active&&std::isfinite(b.x)&&std::isfinite(b.y)){
        const int x=screenX(b.x),y=screenY(b.y,g.camera);
        for(int shift:{-PLAYFIELD_SCREEN_WIDTH,0,PLAYFIELD_SCREEN_WIDTH}){c.round({x+shift-2,y-5,4,9},2,GOLD);c.rect(x+shift-1,y-4,2,4,WHITE);}
    }
    for(const Particle& p:g.particles)if(p.active&&std::isfinite(p.life)&&p.life>0){
        const uint8_t alpha=uint8_t(std::clamp(p.life*400,0.f,255.f));const int x=screenX(p.x),y=screenY(p.y,g.camera);
        for(int shift:{-PLAYFIELD_SCREEN_WIDTH,0,PLAYFIELD_SCREEN_WIDTH}){
            c.line(x+shift-1,y,x+shift+1,y,p.color,alpha);c.line(x+shift,y-1,x+shift,y+1,p.color,alpha);
        }
    }
    const int px=screenX(g.player.x),py=screenY(g.player.y,g.camera);
    const bool left=g.player.facing<0;const hopArt::Sprite* art=nullptr;bool mirror=left;
    if(g.player.jetTime>0)art=&hopArt::hero_jet;
    else if(g.player.vy>60)art=&hopArt::hero_rise;
    else if(g.player.vy< -60)art=&hopArt::hero_fall;
    else if(std::fmod(time,4.f)>3.8f)art=&hopArt::hero_blink;
    else {art=left?&hopArt::hero_left:&hopArt::hero_right;mirror=false;}
    for(int shift:{-PLAYFIELD_SCREEN_WIDTH,0,PLAYFIELD_SCREEN_WIDTH}){
        landingPulse(c,px+shift,py+2,g.player.squash);
        if(g.player.jetTime>0){
            const int flameHeight=21+int(std::sin(time*30)*4);c.scaled(px+shift-8,py-5,hopArt::jet_flame,16,flameHeight);
            const float trail=std::fmod(time*36,18.f);
            c.sprite(px+shift-8,py+17+int(trail),hopArt::spark,false,uint8_t(100*(1-trail/18)));
        }
        c.sprite(px+shift-HERO_FEET_X,py-HERO_FEET_Y,*art,mirror);
    }
    topHud(c,g);
    if(g.scene==Scene::Ready){
        c.panel({106,38,188,39});centered(c,{108,41,184,30},"准备好，向上出发！",INK);
    }else if(g.scene==Scene::Paused){
        c.veil({0,0,400,240},0x102E43,135);c.panel({102,76,196,88});
        centered(c,{108,87,184,24},"旅途暂停",INK);c.line(126,118,274,118,0xD4E3ED);
        centered(c,{107,128,186,23},"按 A / 下屏继续",0x526D84);
    }else if(g.scene==Scene::GameOver){
        c.veil({0,0,400,240},0x102E43,140);c.panel({91,47,218,145});
        c.sprite(190,55,hopArt::star);centered(c,{100,82,200,23},g.newRecord?"新的最高纪录！":"冒险日志",INK);
        centered(c,{105,112,90,16},"本次高度",0x526D84);centered(c,{205,112,90,16},"本次得分",0x526D84);
        number(c,{104,134,92,20},altitude(g.height),INK);number(c,{204,134,92,20},g.score,INK);
        centered(c,{101,165,198,17},"下一跃，再高一点",0x526D84);
    }
}
void titleTop(Canvas& c,const Game& g,const View& v){
    c.background(hopArt::bg_title);const float time=safeTime(v.animationTime);
    c.sprite(20,27,hopArt::title_logo);
    const int bob=int(std::sin(time*1.8f)*3);
    c.sprite(267,88+bob,hopArt::title_hero);
    c.sprite(262,103,hopArt::spark,false,150);c.sprite(374,205,hopArt::spark,false,130);
    c.scaled(24,7,hopArt::hud_plate,132,18);centered(c,{27,8,126,16},"DREAM HOP",GOLD);
    c.panel({20,108,232,48});
    text(c,31,115,"轻轻一跃，发现新世界。",INK,210);
    text(c,31,136,"翡翠森境 · 云城 · 极光晶谷",0x526D84,210);
    c.panel({24,174,224,40});
    c.sprite(35,182,hopArt::star);text(c,65,187,"最高高度",0x526D84,73);number(c,{146,183,88,22},g.profile.bestHeight,INK);
    c.scaled(24,221,hopArt::hud_plate,154,17);
    centered(c,{28,221,146,17},"DOODLE HOP / 3DS",WHITE);
}
void helpTop(Canvas& c){
    c.background(hopArt::bg_title);
    c.round({12,7,376,63},13,0x213B54);
    text(c,22,14,"漫游指南",WHITE,260,2);text(c,24,49,"认清落脚点，探索更高处。",0xD6E9E6,345);
    struct Entry{PlatformKind kind;const char* title;const char* desc;};
    const Entry entries[]={{PlatformKind::Normal,"苔台","稳定落脚，自动起跳"},{PlatformKind::Moving,"浮台","左右移动，提前瞄准"},{PlatformKind::Breakable,"碎岩","一踩就碎，不会起跳"},{PlatformKind::Vanishing,"幻台","只能借力一次"}};
    for(int i=0;i<4;++i){const int y=78+i*33;c.panel({20,y-2,360,29});
        platform(c,54,y+1,49,entries[i].kind);text(c,94,y+5,entries[i].title,INK,36);text(c,141,y+5,entries[i].desc,0x526D84,231);}
    c.round({16,210,368,28},11,0x213B54);
    c.sprite(24,214,hopArt::star);text(c,52,215,"星星加分，弹簧与喷气助你高飞。",WHITE,325);
}
void footer(Canvas& c,const View& v,const char* hint){
    c.round({8,222,304,16},6,0x172C43);
    centered(c,{10,222,300,16},v.saveFailed?"纪录未能保存，请检查 SD 卡":!v.audioAvailable?"当前无声运行":hint,v.saveFailed?0xFFD69B:0xBCD1DF);
}
void heading(Canvas& c,const char* title,const char* subtitle){
    text(c,17,9,title,WHITE,285);text(c,18,32,subtitle,0xBBD0DF,286);c.rect(17,26,31,2,GOLD);
}
void touchOutline(Canvas& c,Rect r){
    const Color gold=0x32B7AE;const int x=r.x,y=r.y,w=r.w-1,h=r.h-1,k=9;
    c.line(x+k,y,x+w-k,y,gold);c.line(x+k,y+h,x+w-k,y+h,gold);
    c.line(x,y+k,x,y+h-k,gold);c.line(x+w,y+k,x+w,y+h-k,gold);
    c.line(x,y+k,x+k,y,gold);c.line(x+w-k,y,x+w,y+k,gold);
    c.line(x,y+h-k,x+k,y+h,gold);c.line(x+w-k,y+h,x+w,y+h-k,gold);
}
void bottom(Canvas& c,const Game& g,const View& v){
    c.background(g.scene==Scene::Playing?hopArt::bg_controls:hopArt::bg_bottom);
    if(g.scene==Scene::Playing){
        heading(c,"冒险控制台","左右寻找落脚点，按住 A 发射");
        centered(c,{25,77,186,20},"滑动控制方向",0x526D84);
        centered(c,{30,178,72,18},"向左",0x526D84);centered(c,{134,178,72,18},"向右",0x526D84);
        centered(c,{236,112,68,20},"发射 A",INK);centered(c,{236,135,68,16},"长按",INK);
        centered(c,{232,172,76,44},"暂停",INK);footer(c,v,"左右边缘互通，下一跃有新风景");
    }else if(g.scene==Scene::Help){
        heading(c,"冒险准备","自动弹跳，控制方向，寻找下一座岛");
        button(c,{24,62,272,46},"准备好了，返回");
        c.panel({22,118,276,94});
        text(c,33,127,"方向键 / 摇杆 / 触屏：移动",INK,256);
        text(c,33,149,"A / 上键：发射    START / B：暂停",INK,256);
        text(c,33,171,"踩怪头也能借力，别从侧面撞上。",0x526D84,256);
        text(c,33,193,"SELECT：退出并保存纪录",0x526D84,256);footer(c,v,"穿过浮岛，探索极光与云城");
    }else{
        const bool ready=g.scene==Scene::Ready,paused=g.scene==Scene::Paused,over=g.scene==Scene::GameOver;
        heading(c,ready?"整装待发":paused?"稍作休息":over?"挑战完成":"幻境漫游",ready?"自动起跳，只需控制左右与发射":paused?"按 A 继续，或返回标题":over?"再出发，去看更高处的风景":"向上探索，遇见三个缤纷世界");
        button(c,{24,62,272,46},ready?"出发！ A":paused?"继续冒险 A":over?"再来一次 A":"开始冒险 A");
        button(c,{24,116,130,34},g.scene==Scene::Title?"怎么玩 Y":"返回标题 B");
        button(c,{166,116,130,34},g.profile.sound?"音效：开 X":"音效：关 X");
        c.panel({24,164,130,50});c.panel({166,164,130,50});
        c.sprite(34,177,hopArt::star);text(c,61,170,"累计星星",0x526D84,83);number(c,{58,188,87,20},g.profile.totalStars,INK);
        text(c,179,170,"最高高度",0x526D84,105);number(c,{173,188,115,20},g.profile.bestHeight,INK);
        footer(c,v,ready?"瞄准落脚点，别掉出屏幕下方":paused?"按 A 或 START 继续":"SELECT 退出并保存纪录");
    }
    if(v.quitConfirm){
        c.veil({0,0,320,240},0x102F44,180);c.panel({15,46,290,154});
        centered(c,{25,63,270,27},"结束这次冒险？",INK);centered(c,{25,98,270,28},"最高纪录会自动保存",0x526D84);
        button(c,{24,146,130,42},"退出 A");button(c,{166,146,130,42},"取消 B");
    }
}
void touchOverlay(Canvas& c,const Game& g,const View& v){
    if(g.scene!=Scene::Playing||v.quitConfirm||!v.touchActive)return;
    const float axis=std::isfinite(v.touchAxis)?std::clamp(v.touchAxis,-1.f,1.f):0;
    if(axis<-.09f)touchOutline(c,{27,107,78,65});
    else if(axis>.09f)touchOutline(c,{131,107,78,65});
    c.disc(coord(118+axis*75),204,4,0x20A69E);
}
}
void render(const Game& game,const View& view,Surface top,Surface lower){
    Canvas a(top),b(lower);
    if(game.scene==Scene::Title)titleTop(a,game,view);
    else if(game.scene==Scene::Help)helpTop(a);
    else playingTop(a,game,view);
    if(view.quitConfirm&&game.scene==Scene::Playing)a.veil({0,0,400,240},0x102E43,120);
    static std::array<uint8_t,320*240*3> bottomPixels{};
    static std::array<uint32_t,5> previous{};static bool valid=false;
    const bool menuStats=game.scene!=Scene::Playing;
    const std::array<uint32_t,5> key={{static_cast<uint32_t>(game.scene),menuStats?game.profile.bestHeight:0u,
        menuStats?game.profile.totalStars:0u,menuStats&&game.profile.sound?1u:0u,
        (view.quitConfirm?1u:0u)|(view.audioAvailable?2u:0u)|(view.saveFailed?4u:0u)}};
    const bool cacheable=lower.pixels&&lower.width==320&&lower.height==240;
    // Cache only the static controls. A drag changes two small visual elements,
    // so restore the base before drawing them on either physical framebuffer.
    if(cacheable&&valid&&key==previous)std::memcpy(lower.pixels,bottomPixels.data(),bottomPixels.size());
    else{bottom(b,game,view);if(cacheable){std::memcpy(bottomPixels.data(),lower.pixels,bottomPixels.size());previous=key;valid=true;}}
    touchOverlay(b,game,view);
}
}
