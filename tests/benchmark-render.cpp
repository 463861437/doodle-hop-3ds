#include "render.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace hop;
#ifndef HOP_BENCHMARK_LABEL
#define HOP_BENCHMARK_LABEL "current"
#endif
namespace {
volatile uint32_t observed=0;
enum class Mode {Still,Touch,Climb};
Game fixture(float altitude){
    Game g;g.scene=Scene::Playing;g.height=altitude+620;g.camera=altitude+480;
    g.profile.bestHeight=8900;g.profile.bestScore=120000;g.profile.totalStars=107;
    g.player={136,altitude+591,100,180,0,0,1};g.score=7900;g.stars=7;
    constexpr float xs[]={130,70,198,113,221,168,45,258};
    for(int i=0;i<8;++i){
        auto& p=g.platforms[i];p.active=true;p.x=xs[i];p.y=altitude+493+i*32;p.width=52+(i%2)*6;
        p.kind=static_cast<PlatformKind>(i%4);p.pickup=static_cast<Pickup>(i%4);
    }
    g.enemies[0]={true,218,altitude+678,218,0};g.enemies[1]={true,79,altitude+576,79,0};
    g.bullets[0]={true,136,altitude+642};g.bullets[1]={true,136,altitude+705};
    for(int i=0;i<10;++i)g.particles[i]={true,float(110+i*4),altitude+570+i*2,0,0,.2f+.025f*i,0xD79A4B};
    return g;
}
void run(float altitude,Mode mode){
    constexpr int ROUNDS=5,FRAMES=200,WARM=40;
    Game g=fixture(altitude);View v;float previousRise=0;
    std::vector<uint8_t> top[2]={std::vector<uint8_t>(400*240*3),std::vector<uint8_t>(400*240*3)};
    std::vector<uint8_t> lower[2]={std::vector<uint8_t>(320*240*3),std::vector<uint8_t>(320*240*3)};
    auto draw=[&](int frame){
        const bool touching=mode!=Mode::Still;
        v.animationTime=float(frame)/60;v.touchActive=touching;v.touchAxis=touching?float(frame%31-15)/15:0;
        // The third workload changes records as well as current HUD values.
        // This exposes accidental full bottom-screen redraws while climbing.
        if(mode==Mode::Climb){
            const float rise=float(frame)*.45f,dy=rise-previousRise;previousRise=rise;
            g.height=altitude+620+rise;g.camera=altitude+480+rise;g.player.y=altitude+591+rise;
            g.profile.bestHeight=uint32_t(g.height);g.profile.bestScore=g.score=7900+uint32_t(frame)*10;
            g.stars=7+uint32_t(frame)/40;g.profile.totalStars=107+g.stars;
            for(auto& p:g.platforms)if(p.active)p.y+=dy;
            for(auto& e:g.enemies)if(e.active)e.y+=dy;
            for(auto& b:g.bullets)if(b.active)b.y+=dy;
            for(auto& p:g.particles)if(p.active)p.y+=dy;
        }
        g.player.x=136+std::sin(float(frame)*.07f)*28;g.player.vy=(frame%60)<30?180:-180;
        g.player.jetTime=frame%120<20?1.2f:0;
        g.player.squash=frame%60<8?1.f-float(frame%60)/8.f:0.f;
        const int back=frame&1;
        render(g,v,{top[back].data(),400,240},{lower[back].data(),320,240});
    };
    for(int i=0;i<WARM;++i)draw(i);
    std::array<double,ROUNDS> microseconds{};
    for(int r=0;r<ROUNDS;++r){
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<FRAMES;++i)draw(WARM+r*FRAMES+i);
        microseconds[r]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/FRAMES;
        observed=observed+top[0][123456]+lower[1][12345];
    }
    std::sort(microseconds.begin(),microseconds.end());
    const char* workload=mode==Mode::Still?"stable-bottom":mode==Mode::Touch?"touch-moving":"climbing-touch";
    std::printf("%s region=%d workload=%s median=%.3f us/frame range=%.3f..%.3f (%d x %d frames, warmup=%d)\n",
        HOP_BENCHMARK_LABEL,altitude<2000?1:altitude<5000?2:3,workload,
        microseconds[ROUNDS/2],microseconds.front(),microseconds.back(),ROUNDS,FRAMES,WARM);
}
}
int main(){
    std::puts("Desktop CPU rendering benchmark only; excludes 3DS GPU/cache services, simulation and VBlank. Not console FPS.");
    for(float altitude:{0.f,3000.f,6000.f})for(Mode mode:{Mode::Still,Mode::Touch,Mode::Climb})run(altitude,mode);
    std::printf("observable checksum: %u\n",unsigned(observed));
}
