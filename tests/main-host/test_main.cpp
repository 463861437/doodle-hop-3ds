#include <3ds.h>
#include "game.hpp"
#include "render.hpp"
#include "save.hpp"
#include "audio.hpp"
#include <cmath>
#include <iostream>
#include <string>
#include <sys/stat.h>
int hopProgramMain();
static int checks=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"main failure "<<__LINE__<<": "<<#x<<'\n';std::abort();}++checks;}while(0)
struct Snapshot{hop::Scene scene;float elapsed,x;bool confirm;unsigned bullets;int saves;bool saveFailed;};
static std::vector<Snapshot> snapshots;
static std::string scenario;
extern "C" int hop_test_mkdir(const char*,mode_t){return 0;}
namespace hop {
LoadStatus SaveStore::load(Profile&){sequence_=0;active_=-1;return LoadStatus::Missing;}
bool SaveStore::save(const Profile&){++mainMock::saves;return scenario!="save-retry"||mainMock::saves>1;}
bool audioInit(){++mainMock::audioStarts;mainMock::audioLive=scenario!="audio-fail";return mainMock::audioLive;}
void audioPlay(unsigned,bool){}
void audioStop(){++mainMock::audioStops;}
void audioExit(){mainMock::audioLive=false;++mainMock::audioExits;}
void render(const Game& g,const View& v,Surface top,Surface bottom){
    CHECK(top.pixels==gfxGetFramebuffer(GFX_TOP,GFX_LEFT,nullptr,nullptr));
    CHECK(bottom.pixels==gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr));
    CHECK(top.width==400&&bottom.width==320&&top.height==240&&bottom.height==240);
    unsigned bullets=0;for(const auto& b:g.bullets)bullets+=b.active;
    snapshots.push_back({g.scene,g.elapsed,g.player.x,v.quitConfirm,bullets,mainMock::saves,v.saveFailed});
    if(scenario=="save"&&mainMock::frame==3)const_cast<Game&>(g).dirty=true;
    // Put the player below the screen after a real launched frame. The next
    // actual physics step must trigger GameOver, exercising the main loop's
    // simulation-originated transition rather than a mocked menu transition.
    if((scenario=="death-held-key"||scenario=="death-held-touch")&&mainMock::frame==3)
        const_cast<Game&>(g).player.y=g.camera-PLAYER_HEIGHT-10;
    CHECK(std::isfinite(v.animationTime)&&v.animationTime>=0);
}
}
static mainMock::Frame press(u32 key){return {key,key};}
static mainMock::Frame held(u32 key){return {key,0};}
static mainMock::Frame touch(int x,int y,bool down=true){return {KEY_TOUCH,down?KEY_TOUCH:0,x,y};}
int main(int argc,char** argv){
    CHECK(argc==2);scenario=argv[1];using hop::Scene;using mainMock::frames;
    if(scenario=="keys")frames={press(KEY_A),held(KEY_A),{},press(KEY_A),held(KEY_A),{},held(KEY_A),press(KEY_B),{},press(KEY_A)};
    else if(scenario=="touch")frames={touch(100,80),touch(100,80,false),{},touch(100,80),touch(250,100,false),{},touch(250,100),touch(250,180),{},touch(100,80)};
    else if(scenario=="home"||scenario=="sleep"){
        const unsigned hooks=scenario=="home"?(1u<<APTHOOK_ONSUSPEND)|(1u<<APTHOOK_ONRESTORE):(1u<<APTHOOK_ONSLEEP)|(1u<<APTHOOK_ONWAKEUP);
        frames={press(KEY_A),{},press(KEY_A),{}, {KEY_A,KEY_A,0,0,10000,hooks},held(KEY_A),{},press(KEY_A)};
    }else if(scenario=="dt")frames={press(KEY_A),{},press(KEY_A),{0,0,0,0,1000},{0,0,0,0,0},{0,0,0,0,1}};
    else if(scenario=="quit")frames={press(KEY_SELECT),press(KEY_A),{},press(KEY_A)};
    else if(scenario=="save")frames={press(KEY_A),{},press(KEY_A),{}, {},press(KEY_B)};
    else if(scenario=="save-retry")frames={press(KEY_X),{},press(KEY_SELECT),{},press(KEY_A)};
    else if(scenario=="stick-gate")frames={press(KEY_A),{0,0,0,0,16,0,156},press(KEY_A),{},press(KEY_A),{0,0,0,0,16,0,156},{},{0,0,0,0,16,0,156}};
    else if(scenario=="audio-fail")frames={press(KEY_A),{},press(KEY_A),{},press(KEY_B)};
    else if(scenario=="death-held-key")frames={press(KEY_A),{},press(KEY_A),{},held(KEY_A),held(KEY_A),held(KEY_A),{},press(KEY_A)};
    else if(scenario=="death-held-touch"){
        // While the firing finger is still held, pressing A must not restart.
        const mainMock::Frame touchAndA{KEY_TOUCH|KEY_A,KEY_A,250,100};
        frames={press(KEY_A),{},press(KEY_A),{},touch(250,100),touchAndA,touch(100,80,false),{},touch(100,80)};
    }
    else CHECK(false);
    CHECK(hopProgramMain()==0);CHECK(mainMock::gfxStarts==1&&mainMock::gfxStops==1&&!mainMock::threeD);
    CHECK(mainMock::audioStarts==1&&mainMock::audioExits==1&&!mainMock::audioLive);
    CHECK(mainMock::unhooks==1&&!mainMock::hook);
    CHECK(mainMock::swaps==int(snapshots.size())&&mainMock::bottomSwaps==mainMock::swaps);
    if(scenario=="keys"){
        CHECK(snapshots[0].scene==Scene::Ready&&snapshots[1].scene==Scene::Ready);
        CHECK(snapshots[3].scene==Scene::Playing&&snapshots[4].bullets==0);
        CHECK(snapshots[6].bullets>0&&snapshots[7].scene==Scene::Paused&&snapshots[9].scene==Scene::Playing);
    }else if(scenario=="touch"){
        CHECK(snapshots[0].scene==Scene::Ready&&snapshots[3].scene==Scene::Playing);
        CHECK(snapshots[4].bullets==0&&snapshots[6].bullets>0);
        CHECK(snapshots[7].scene==Scene::Paused&&snapshots[9].scene==Scene::Playing);
    }else if(scenario=="home"||scenario=="sleep"){
        CHECK(snapshots[3].scene==Scene::Playing);
        for(int i=4;i<7;++i)CHECK(snapshots[i].scene==Scene::Paused&&snapshots[i].elapsed==snapshots[3].elapsed);
        CHECK(snapshots[7].scene==Scene::Playing&&snapshots[7].elapsed-snapshots[6].elapsed<.017f);
        CHECK(mainMock::audioStops>=4);
    }else if(scenario=="dt"){
        const float elapsed=snapshots[3].elapsed-snapshots[2].elapsed;
        CHECK(elapsed>.04f&&elapsed<.051f);CHECK(snapshots[4].elapsed==snapshots[3].elapsed);
    }else if(scenario=="quit"){
        CHECK(snapshots.size()==3&&snapshots[0].confirm&&snapshots[1].confirm);
    }else if(scenario=="save"){
        CHECK(snapshots[3].saves==snapshots[4].saves);CHECK(snapshots[5].saves>snapshots[4].saves);
    }else if(scenario=="save-retry"){
        CHECK(snapshots[0].saveFailed);CHECK(mainMock::saves>=2);
    }else if(scenario=="stick-gate"){
        CHECK(snapshots[2].scene==Scene::Ready&&snapshots[4].scene==Scene::Playing);
        CHECK(snapshots[5].x==snapshots[4].x&&snapshots[7].x>snapshots[6].x);
    }else if(scenario=="audio-fail")CHECK(snapshots[2].scene==Scene::Playing);
    else if(scenario=="death-held-key"||scenario=="death-held-touch"){
        CHECK(snapshots[3].scene==Scene::Playing);
        for(int i=4;i<=7;++i)CHECK(snapshots[i].scene==Scene::GameOver);
        CHECK(snapshots[8].scene==Scene::Ready);
        CHECK(snapshots[5].elapsed==snapshots[4].elapsed&&snapshots[7].elapsed==snapshots[4].elapsed);
    }
    std::cout<<scenario<<": "<<checks<<" actual main-loop checks passed\n";
}
