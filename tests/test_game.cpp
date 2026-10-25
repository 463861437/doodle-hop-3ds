#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace hop;
namespace {
unsigned checks=0,updates=0;
void check(bool condition,const char* message){++checks;if(!condition){std::fprintf(stderr,"game FAIL: %s\n",message);std::exit(1);}}
void tick(Game& g,float seconds,float axis=0,bool shoot=false){g.step(seconds,axis,shoot);++updates;}
Game empty(){Game g;g.start(1234);g.launch();g.platforms={};g.enemies={};g.highestPlatform=10000;g.sounds=0;g.dirty=false;return g;}
Platform platform(float x,float y,PlatformKind kind=PlatformKind::Normal){return {true,false,1,x,y,56,x,0,0,0,kind,Pickup::None};}
void sane(const Game& g){
    check(std::isfinite(g.player.x+g.player.y+g.player.vx+g.player.vy+g.camera+g.height),"finite physics");
    check(g.player.x>=0&&g.player.x<WORLD_WIDTH,"player within wrapped width");
    check(g.camera>=0&&g.score<=1000000000u,"bounded camera and score");
    for(const Platform& p:g.platforms)if(p.active){
        check(p.width>0&&std::isfinite(p.x+p.y+p.width+p.phase),"finite platform");
        check(p.x>=0&&p.x<WORLD_WIDTH,"wrapped platform");
        if(p.safe)check(p.kind==PlatformKind::Normal,"main route never crumbles or moves");
    }
}
void landing(){
    Game g=empty();g.platforms[0]=platform(140,100);g.player={140,105,0,-500,0,0,1};
    tick(g,.02f);check(g.player.vy>0&&g.bounces==1,"fast descent lands rather than tunnels");check(g.player.y>=100,"feet cannot pass through solid platform");
    g=empty();g.platforms[0]=platform(140,100);g.player={140,90,0,300,0,0,1};tick(g,.05f);
    check(g.bounces==0&&g.player.y>100,"ascending from beneath passes through platform");
    g=empty();g.platforms[0]=platform(3,100);g.player={277,104,0,-300,0,0,1};tick(g,.02f);
    check(g.bounces==1,"landing crosses world seam");
    g=empty();g.platforms[0]=platform(140,100);g.player={177,104,0,-300,0,0,1};tick(g,.02f);
    check(g.bounces==0,"outside foot overlap must miss platform");
    g=empty();g.platforms[0]=platform(140,100);g.player={174,104,0,-300,0,0,1};tick(g,.02f);
    check(g.bounces==1,"edge contact includes forgiving foot radius");
    g=empty();g.platforms[0]=platform(140,100,PlatformKind::Breakable);g.player={140,104,0,-300,0,0,1};tick(g,.02f);
    check(g.bounces==0&&g.platforms[0].brokenTime>0&&g.player.vy<0,"brittle platform breaks without bouncing");
    g=empty();g.platforms[0]=platform(140,100,PlatformKind::Vanishing);g.player={140,104,0,-300,0,0,1};tick(g,.02f);
    check(g.bounces==1&&g.platforms[0].brokenTime>0,"vanishing platform bounces once");
    g.player={140,104,0,-300,0,0,1};tick(g,.02f);check(g.bounces==1,"vanishing platform cannot bounce again");
    // At a seam, a moving platform advances right while the falling feet do.
    g=empty();g.platforms[0]=platform(278,100,PlatformKind::Moving);g.platforms[0].originX=278;g.platforms[0].amplitude=20;
    g.player={2,104,50,-300,0,0,1};tick(g,.02f,.3f);check(g.bounces==1,"moving platform impact uses wrapped relative motion");
    g=empty();g.player.x=279;g.player.vx=MAX_SPEED;tick(g,.02f,1);check(g.player.x<10,"right edge wraps");
    g=empty();g.player.x=1;g.player.vx=-MAX_SPEED;tick(g,.02f,-1);check(g.player.x>270,"left edge wraps");
}
void pickups(){
    Game g=empty();g.platforms[0]=platform(140,100);g.platforms[0].pickup=Pickup::Spring;
    g.player={140,104,0,-300,0,0,1};tick(g,.02f);check(g.player.vy>490&&(g.sounds&S_POWER),"center spring provides strong launch");
    g=empty();g.platforms[0]=platform(140,100);g.platforms[0].pickup=Pickup::Spring;
    g.player={165,104,0,-300,0,0,1};tick(g,.02f);check(g.player.vy<JUMP_SPEED+1&&g.bounces==1,"spring can be bypassed by landing on platform edge");
    g=empty();g.platforms[0]=platform(140,100);g.platforms[0].pickup=Pickup::Star;
    g.player={140,112,0,0,0,0,1};tick(g,.008f);check(g.stars==1&&g.profile.totalStars==1&&g.platforms[0].pickup==Pickup::None,"star pickup consumed once and persisted");
    const uint32_t score=g.score;tick(g,.008f);check(g.stars==1&&g.score==score,"star cannot award twice");
    for(PlatformKind kind:{PlatformKind::Normal,PlatformKind::Breakable,PlatformKind::Vanishing}){
        g=empty();g.platforms[0]=platform(140,100,kind);g.platforms[0].width=44;g.platforms[0].pickup=Pickup::Star;
        g.player={156.2f,102.2f,-160,-300,0,0,1};tick(g,1.f/120,-1);
        check(g.stars==1&&g.platforms[0].pickup==Pickup::None,"same-tick landing collects star even when its support breaks");
        if(kind!=PlatformKind::Normal)check(g.platforms[0].brokenTime>0,"pickup regression actually breaks support on contact");
    }
    g=empty();g.platforms[0]=platform(140,100,PlatformKind::Breakable);g.platforms[0].pickup=Pickup::Star;g.platforms[0].brokenTime=.2f;
    g.player={140,112,0,0,0,0,1};tick(g,.008f);
    check(g.stars==0&&g.platforms[0].pickup==Pickup::Star,"already-broken platform cannot award leftover pickup");
    g=empty();g.platforms[0]=platform(140,100);g.platforms[0].pickup=Pickup::Jetpack;
    g.player={140,112,0,0,0,0,1};tick(g,.008f);check(g.player.jetTime>1.5f,"jetpack activates on touch");
    const float start=g.player.y;for(int i=0;i<180;++i)tick(g,1.f/120);
    check(g.player.y>start+400&&g.player.jetTime>0,"jetpack carries player upward");
    for(int i=0;i<50;++i)tick(g,1.f/120);check(g.player.jetTime==0&&g.player.vy<290,"jetpack expires into normal gravity");
}
void combat(){
    Game g=empty();g.player={140,104,0,-300,0,0,1};g.enemies[0]={true,140,90,140,0};tick(g,.02f);
    check(!g.enemies[0].active&&g.player.vy>0&&g.scene==Scene::Playing,"descending feet stomp enemy");
    g=empty();g.player={140,90,0,20,0,0,1};g.enemies[0]={true,140,100,140,0};tick(g,.008f);
    check(g.scene==Scene::GameOver&&(g.sounds&S_LOST),"side enemy contact ends run");
    g=empty();g.player={140,90,0,20,1,0,1};g.enemies[0]={true,140,100,140,0};tick(g,.008f);
    check(g.scene==Scene::Playing&&!g.enemies[0].active,"jetpack defeats enemy contact");
    g=empty();g.player={140,18,0,0,0,0,1};g.enemies[0]={true,140,100,140,0};
    for(int i=0;i<30;++i)tick(g,1.f/120,0,true);
    check(!g.enemies[0].active&&g.score>=250,"upward shot defeats enemy");
    int count=0;for(const Bullet& b:g.bullets)count+=b.active;
    check(count<=2,"held shooting obeys cooldown");
}
void lifecycle(){
    Game g;g.profile={123,4000,7,9,false};g.start(0);check(g.scene==Scene::Ready&&g.seed!=0,"zero seed gives valid ready world");
    check(g.profile.bestScore==4000&&g.profile.totalRuns==7&&!g.profile.sound,"restart preserves profile");
    tick(g,1);check(g.player.y==18&&g.profile.totalRuns==7,"ready state does not simulate or count run");
    g.launch();g.launch();check(g.profile.totalRuns==8,"launch counts exactly one run");
    tick(g,.03f);g.pause();const float y=g.player.y,time=g.elapsed;tick(g,.05f,1,true);
    check(g.player.y==y&&g.elapsed==time&&g.scene==Scene::Paused,"pause freezes world");g.resume();tick(g,.01f);check(g.elapsed>time,"resume returns to simulation");
    const float before=g.elapsed;tick(g,-1);tick(g,std::numeric_limits<float>::quiet_NaN());tick(g,std::numeric_limits<float>::infinity());
    check(g.elapsed==before,"invalid dt ignored");tick(g,100);check(g.elapsed-before<.05001f,"long stall clamped to fifty ms");
    tick(g,.01f,std::numeric_limits<float>::quiet_NaN());sane(g);
    g=empty();g.camera=200;g.player.y=170;tick(g,.008f);check(g.scene==Scene::GameOver,"fall below visible area ends run");
    g=empty();g.player.y=400;g.player.vy=0;tick(g,.008f);check(g.camera>270&&g.height>380,"camera follows upward and records height");
    float c=g.camera;g.player.y-=30;tick(g,.008f);check(g.camera==c,"camera never scrolls backward");
    g.finish();check(g.profile.bestHeight>=380&&g.profile.bestScore>0&&g.dirty,"run records score and height for saving");
}
void generated(){
    for(uint32_t seed=1;seed<=40;++seed){
        Game g;g.start(seed);float previousY=18,previousX=WORLD_WIDTH/2;
        for(int cycle=0;cycle<300;++cycle){
            std::vector<Platform> safe;for(const Platform& p:g.platforms)if(p.active&&p.safe&&p.y>previousY)safe.push_back(p);
            std::sort(safe.begin(),safe.end(),[](const Platform& a,const Platform& b){return a.y<b.y;});
            for(const Platform& p:safe){
                const float gap=p.y-previousY,dx=std::abs(wrapDelta(previousX,p.x));
                check(gap>=38&&gap<=50,"generated safe route has bounded vertical gaps");check(dx<=60.001f,"generated safe route stays horizontally reachable");
                const float airtime=(JUMP_SPEED+std::sqrt(JUMP_SPEED*JUMP_SPEED-2*GRAVITY*gap))/GRAVITY;
                check(MAX_SPEED*(airtime-.16f)>dx,"safe route reachable including acceleration allowance");
                check(p.kind==PlatformKind::Normal&&p.brokenTime==0,"generated safe route stays permanent");previousY=p.y;previousX=p.x;
            }
            g.camera+=90;for(Platform& p:g.platforms)if(p.y<g.camera-70)p.active=false;
            for(Enemy& e:g.enemies)if(e.y<g.camera-30)e.active=false;
            g.generate();check(g.highestPlatform>=g.camera+VIEW_HEIGHT+120,"generator keeps ahead of camera");sane(g);
        }
        check(previousY>26000,"generated path verified past 26k altitude");
    }
    Game a,b;a.start(0x1234);b.start(0x1234);
    for(int i=0;i<MAX_PLATFORMS;++i)check(a.platforms[i].x==b.platforms[i].x&&a.platforms[i].y==b.platforms[i].y&&a.platforms[i].kind==b.platforms[i].kind,"same seed reproduces world");
}
void navigation(){
    for(uint32_t seed:{3u,123u,798u,9876u}){
        Game g;g.start(seed);g.launch();
        unsigned flights=0,landingsAfterFlight=0;bool wasFlying=false,flightFinished=false;uint32_t bouncesBefore=0;
        // A proportional steering agent selects reachable permanent platforms
        // from the real generated world. No teleporting or altered physics.
        for(int frame=0;frame<90000&&g.scene==Scene::Playing&&g.height<10000;++frame){
            const float apex=g.player.jetTime>0?g.player.y+80:g.player.y+std::max(0.0f,g.player.vy)*std::max(0.0f,g.player.vy)/(2*GRAVITY);
            const Platform* target=nullptr;
            for(const Platform& p:g.platforms){
                if(!p.active||!p.safe||p.y>apex-1||p.y<g.camera+4)continue;
                const float discriminant=g.player.vy*g.player.vy+2*GRAVITY*(g.player.y-p.y);
                if(g.player.jetTime<=0&&discriminant<0)continue;
                const float time=g.player.jetTime>0?1:(g.player.vy+std::sqrt(discriminant))/GRAVITY;
                if(time<.01f||std::abs(wrapDelta(g.player.x,p.x))>MAX_SPEED*time+p.width*.3f)continue;
                if(!target||p.y>target->y)target=&p;
            }
            const float dx=target?wrapDelta(g.player.x,target->x):0;
            tick(g,1.f/120,std::clamp(dx*.055f,-1.0f,1.0f),true);
            const bool flying=g.player.jetTime>0;
            if(flying&&!wasFlying)++flights;
            if(!flying&&wasFlying){flightFinished=true;bouncesBefore=g.bounces;}
            if(flightFinished&&g.bounces>bouncesBefore){++landingsAfterFlight;flightFinished=false;}
            wasFlying=flying;
            if(frame%120==0)sane(g);
        }
        if(g.height<10000)std::fprintf(stderr,"navigation seed=%u height=%.1f scene=%d bounces=%u\n",seed,g.height,int(g.scene),g.bounces);
        check(g.height>=10000,"real generated run can be navigated beyond 10k altitude");
        check(flights>=3&&landingsAfterFlight>=3,"navigation picks up multiple jetpacks and lands safely afterward");
    }
}
void fuzz(){
    Game g;g.start(14);g.launch();uint32_t state=909;
    for(int i=0;i<120000;++i){
        state=state*1664525u+1013904223u;
        if(g.scene==Scene::GameOver){g.start(state);g.launch();}
        tick(g,(i%13==0)?.05f:1.f/120,float(int((state>>16)%201)-100)/100,(state&31)<3);
        if(i%71==0)sane(g);
    }
}
}
int main(){landing();pickups();combat();lifecycle();generated();navigation();fuzz();std::printf("game: %u assertions, %u real updates passed (collision, wrap, procedural reachability, 10k navigation, powers, combat, persistence state)\n",checks,updates);}
