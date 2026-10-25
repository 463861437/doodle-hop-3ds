#include <3ds.h>
#include "game.hpp"
#include "controls.hpp"
#include "audio.hpp"
#include "save.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <sys/stat.h>
namespace {
std::atomic<bool> interrupted{false};
void onApt(APT_HookType hook,void*){
    if(hook==APTHOOK_ONSUSPEND||hook==APTHOOK_ONSLEEP||hook==APTHOOK_ONRESTORE||hook==APTHOOK_ONWAKEUP)interrupted.store(true);
}
unsigned keys(u32 native){
    unsigned out=0;
    const struct{u32 native;unsigned mapped;} map[]={
        {KEY_A,hop::K_A},{KEY_B,hop::K_B},{KEY_X,hop::K_X},{KEY_Y,hop::K_Y},
        {KEY_START,hop::K_START},{KEY_SELECT,hop::K_SELECT},{KEY_DLEFT,hop::K_LEFT},
        {KEY_DRIGHT,hop::K_RIGHT},{KEY_DUP,hop::K_UP},{KEY_DDOWN,hop::K_DOWN},{KEY_L,hop::K_L},{KEY_R,hop::K_R}};
    for(const auto& key:map)if(native&key.native)out|=key.mapped;
    return out;
}
}
int main(){
    gfxInitDefault();gfxSet3D(false);
    static hop::Game game;hop::View view;hop::Controller controller;
    controller.nextSeed=static_cast<uint32_t>(svcGetSystemTick())|1u;
    mkdir("sdmc:/3ds",0777);
    hop::SaveStore save("sdmc:/3ds/doodle-hop-3ds");
    view.saveFailed=save.load(game.profile)==hop::LoadStatus::Invalid;
    view.audioAvailable=hop::audioInit();
    aptHookCookie hook{};aptHook(&hook,onApt,nullptr);
    uint64_t previous=svcGetSystemTick();float accumulator=0;bool requireRelease=false;
    constexpr float step=1.0f/120.0f;
    while(aptMainLoop()&&!controller.quit){
        hidScanInput();const u32 held=hidKeysHeld(),down=hidKeysDown();
        const uint64_t now=svcGetSystemTick();
        float dt=now>=previous?std::min(.05f,float(double(now-previous)/SYSCLOCK_ARM11)):0;previous=now;
        if(interrupted.exchange(false)){game.pause();hop::audioStop();accumulator=0;dt=0;requireRelease=true;}
        circlePosition circle{};hidCircleRead(&circle);
        touchPosition touch{};if(held&KEY_TOUCH)hidTouchRead(&touch);
        hop::Input input{keys(down),keys(held),(held&KEY_TOUCH)!=0,(down&KEY_TOUCH)!=0,int(touch.px),int(touch.py),circle.dx/156.0f};
        if(requireRelease){
            input={};
            if(keys(held)==0&&!(held&KEY_TOUCH)&&std::abs(circle.dx)<22)requireRelease=false;
        }
        const auto before=game.scene;const bool confirmBefore=view.quitConfirm;
        controller.update(game,view,input,dt);
        if(controller.quit)break;
        if(before!=game.scene||confirmBefore!=view.quitConfirm){accumulator=0;requireRelease=true;}
        const auto simulated=game.scene;
        if(game.scene==hop::Scene::Playing){
            accumulator+=dt;
            while(accumulator>=step){game.step(step,controller.axis,controller.shoot);accumulator-=step;}
        }else accumulator=0;
        if(game.scene!=simulated){accumulator=0;requireRelease=true;}
        view.animationTime=std::fmod(view.animationTime+dt,10000.0f);
        // Write at menu/pause/death boundaries, never on the active jump path.
        if(game.dirty&&game.scene!=hop::Scene::Playing){game.dirty=false;view.saveFailed=!save.save(game.profile);}
        if(game.scene==hop::Scene::Paused||view.quitConfirm||!game.profile.sound)hop::audioStop();
        else hop::audioPlay(game.sounds,true);
        game.sounds=0;
        hop::render(game,view,{gfxGetFramebuffer(GFX_TOP,GFX_LEFT,nullptr,nullptr),400,240},
                             {gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr),320,240});
        gfxFlushBuffers();gfxScreenSwapBuffers(GFX_TOP,false);gfxScreenSwapBuffers(GFX_BOTTOM,false);gspWaitForVBlank();
    }
    if(game.dirty||view.saveFailed)save.save(game.profile);
    aptUnhook(&hook);hop::audioExit();gfxExit();return 0;
}
