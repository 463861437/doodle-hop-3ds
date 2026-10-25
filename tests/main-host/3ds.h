#pragma once
#include <cassert>
#include <cstdint>
#include <vector>
#define mkdir hop_test_mkdir
using u32=uint32_t;
constexpr uint64_t SYSCLOCK_ARM11=1000000;
enum:u32{KEY_A=1u<<0,KEY_B=1u<<1,KEY_X=1u<<2,KEY_Y=1u<<3,KEY_START=1u<<4,KEY_SELECT=1u<<5,
    KEY_DLEFT=1u<<6,KEY_DRIGHT=1u<<7,KEY_DUP=1u<<8,KEY_DDOWN=1u<<9,KEY_L=1u<<10,KEY_R=1u<<11,KEY_TOUCH=1u<<12};
enum APT_HookType{APTHOOK_ONSUSPEND,APTHOOK_ONSLEEP,APTHOOK_ONRESTORE,APTHOOK_ONWAKEUP};
struct aptHookCookie{void(*callback)(APT_HookType,void*)=nullptr;void* parameter=nullptr;};
struct circlePosition{int16_t dx=0,dy=0;};struct touchPosition{uint16_t px=0,py=0;};
enum{GFX_TOP,GFX_BOTTOM,GFX_LEFT,GFX_RIGHT};
namespace mainMock {
struct Frame{u32 held=0,down=0;int x=0,y=0;uint64_t milliseconds=16;unsigned hooks=0;int axis=0;};
inline std::vector<Frame> frames;
inline int frame=-1,gfxStarts=0,gfxStops=0,unhooks=0,swaps=0,bottomSwaps=0,audioStarts=0,audioExits=0,audioStops=0,saves=0;
inline uint64_t clock=1000;
inline bool threeD=true,audioLive=false;
inline aptHookCookie* hook=nullptr;
inline uint8_t top[2][1]{},bottom[2][1]{};
inline int back=0,lastTop=-1,lastBottom=-1;
inline Frame current(){assert(frame>=0&&frame<int(frames.size()));return frames[frame];}
}
inline void gfxInitDefault(){++mainMock::gfxStarts;}
inline void gfxSet3D(bool enable){mainMock::threeD=enable;}
inline void gfxExit(){assert(!mainMock::audioLive);++mainMock::gfxStops;}
inline uint8_t* gfxGetFramebuffer(int screen,int side,void*,void*){
    assert(side==GFX_LEFT);return screen==GFX_TOP?mainMock::top[mainMock::back]:mainMock::bottom[mainMock::back];
}
inline void gfxFlushBuffers(){}
inline void gfxScreenSwapBuffers(int screen,int stereo){
    assert(!stereo&&!mainMock::threeD);
    if(screen==GFX_TOP){assert(mainMock::lastTop!=mainMock::frame);mainMock::lastTop=mainMock::frame;++mainMock::swaps;}
    else{assert(mainMock::lastBottom!=mainMock::frame);mainMock::lastBottom=mainMock::frame;++mainMock::bottomSwaps;}
}
inline void gspWaitForVBlank(){assert(mainMock::lastTop==mainMock::frame&&mainMock::lastBottom==mainMock::frame);mainMock::back^=1;}
inline uint64_t svcGetSystemTick(){return mainMock::clock*1000;}
inline void aptHook(aptHookCookie* cookie,void(*cb)(APT_HookType,void*),void* parameter){cookie->callback=cb;cookie->parameter=parameter;mainMock::hook=cookie;}
inline void aptUnhook(aptHookCookie* cookie){assert(mainMock::hook==cookie);mainMock::hook=nullptr;++mainMock::unhooks;}
inline bool aptMainLoop(){
    if(++mainMock::frame>=int(mainMock::frames.size()))return false;
    const auto f=mainMock::current();mainMock::clock+=f.milliseconds;
    for(unsigned h=0;h<4;++h)if(f.hooks&(1u<<h))mainMock::hook->callback(static_cast<APT_HookType>(h),mainMock::hook->parameter);
    return true;
}
inline void hidScanInput(){}inline u32 hidKeysHeld(){return mainMock::current().held;}inline u32 hidKeysDown(){return mainMock::current().down;}
inline void hidCircleRead(circlePosition* p){*p={int16_t(mainMock::current().axis),0};}
inline void hidTouchRead(touchPosition* p){const auto f=mainMock::current();*p={uint16_t(f.x),uint16_t(f.y)};}
