#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace hop {
namespace {
constexpr float PI=3.14159265358979323846f;
constexpr uint32_t MAX_SCORE=1000000000u;
float wrap(float x){x=std::fmod(x,WORLD_WIDTH);return x<0?x+WORLD_WIDTH:x;}
uint32_t addScore(uint32_t value,uint32_t increment){return value>MAX_SCORE-increment?MAX_SCORE:value+increment;}
void burst(Game& g,float x,float y,uint32_t color,int count=7){
    for(Particle& p:g.particles){
        if(p.active)continue;
        const float angle=float(g.random()%6284)*.001f,speed=24+float(g.random()%42);
        p={true,x,y,std::cos(angle)*speed,std::sin(angle)*speed,.34f+float(g.random()%20)*.01f,color};
        if(--count==0)break;
    }
}
void record(Game& g){
    const uint32_t h=static_cast<uint32_t>(std::clamp(g.height,0.0f,float(MAX_SCORE)));
    if(h>g.profile.bestHeight){g.profile.bestHeight=h;g.dirty=true;}
    if(g.score>g.profile.bestScore){
        if(!g.newRecord&&g.profile.bestScore>0)g.sounds|=S_RECORD;
        g.newRecord=true;g.profile.bestScore=g.score;g.dirty=true;
    }
}
void updateScore(Game& g){
    const float h=std::max(g.height,std::max(0.0f,g.player.y-18));
    const uint32_t before=static_cast<uint32_t>(std::min(g.height,float(MAX_SCORE/10)));
    const uint32_t after=static_cast<uint32_t>(std::min(h,float(MAX_SCORE/10)));
    g.height=h;g.score=addScore(g.score,(after-before)*10);record(g);
}
void tick(Game& g,float dt,float axis,bool shoot){
    g.elapsed=std::fmod(g.elapsed+dt,10000.0f);
    g.shootCooldown=std::max(0.0f,g.shootCooldown-dt);
    g.player.squash=std::max(0.0f,g.player.squash-dt*6);
    float oldPlatformX[MAX_PLATFORMS];
    bool pickupEligible[MAX_PLATFORMS];
    for(int i=0;i<MAX_PLATFORMS;++i){
        Platform& p=g.platforms[i];oldPlatformX[i]=p.x;
        pickupEligible[i]=p.active&&p.brokenTime<=0;
        if(!p.active)continue;
        if(p.brokenTime>0){p.brokenTime=std::max(0.0f,p.brokenTime-dt);if(p.brokenTime<=0)p.active=false;}
        if(p.kind==PlatformKind::Moving){p.phase=std::fmod(p.phase+dt*1.35f,2*PI);p.x=wrap(p.originX+std::sin(p.phase)*p.amplitude);}
        if(p.y<g.camera-70)p.active=false;
    }
    for(Enemy& enemy:g.enemies){
        if(!enemy.active)continue;
        enemy.phase=std::fmod(enemy.phase+dt*1.4f,2*PI);
        enemy.x=wrap(enemy.originX+std::sin(enemy.phase)*14);
        if(enemy.y<g.camera-30)enemy.active=false;
    }
    for(Particle& p:g.particles){
        if(!p.active)continue;
        p.life-=dt;if(p.life<=0){p.active=false;continue;}
        p.x=wrap(p.x+p.vx*dt);p.y+=p.vy*dt;p.vy-=90*dt;
    }
    const float oldX=g.player.x,oldY=g.player.y;
    const float desired=axis*MAX_SPEED,change=1000*dt;
    g.player.vx+=std::clamp(desired-g.player.vx,-change,change);
    if(std::abs(axis)>.08f)g.player.facing=axis<0?-1:1;
    const float travel=g.player.vx*dt;
    g.player.x=wrap(oldX+travel);
    const bool flying=g.player.jetTime>0;
    if(flying){g.player.jetTime=std::max(0.0f,g.player.jetTime-dt);g.player.vy=290;}
    else g.player.vy=std::max(-580.0f,g.player.vy-GRAVITY*dt);
    g.player.y+=g.player.vy*dt;

    // Find the first descending contact. Test X at impact time against the
    // moving platform's position, including the horizontal seam of the world.
    if(!flying&&g.player.vy<0&&g.player.y<oldY){
        for(int pass=0;pass<MAX_PLATFORMS;++pass){
            int best=-1;float impact=2;
            for(int i=0;i<MAX_PLATFORMS;++i){
                const Platform& p=g.platforms[i];
                if(!p.active||p.brokenTime>0||oldY<p.y-.001f||g.player.y>p.y)continue;
                const float t=std::clamp((oldY-p.y)/(oldY-g.player.y),0.0f,1.0f);
                const float px=wrap(oldX+travel*t),platformX=wrap(oldPlatformX[i]+wrapDelta(oldPlatformX[i],p.x)*t);
                if(t<impact&&std::abs(wrapDelta(px,platformX))<=p.width*.5f+PLAYER_RADIUS*.70f){best=i;impact=t;}
            }
            if(best<0)break;
            Platform& p=g.platforms[best];
            if(p.kind==PlatformKind::Breakable){
                p.brokenTime=.32f;burst(g,p.x,p.y,0xBC805C);continue;
            }
            g.player.y=p.y;g.player.vy=JUMP_SPEED;g.player.squash=1;
            ++g.bounces;g.sounds|=S_BOUNCE;
            if(p.pickup==Pickup::Spring&&std::abs(wrapDelta(wrap(oldX+travel*impact),p.x))<=15){
                g.player.vy=530;g.sounds|=S_POWER;burst(g,p.x,p.y+8,0xF4B34B);
            }
            if(p.kind==PlatformKind::Vanishing){p.brokenTime=.20f;burst(g,p.x,p.y,0xB6CEDC,5);}
            break;
        }
    }

    for(int i=0;i<MAX_PLATFORMS;++i){
        Platform& p=g.platforms[i];
        // Landing can crumble a platform this tick; its previously valid
        // pickup is still collectible at that same contact. Earlier debris is
        // ineligible, so it cannot award an item on a later pass.
        if(!p.active||!pickupEligible[i]||(p.pickup!=Pickup::Jetpack&&p.pickup!=Pickup::Star))continue;
        const float py=p.y+17;
        const float lo=std::min(oldY,g.player.y),hi=std::max(oldY,g.player.y)+PLAYER_HEIGHT;
        if(py<lo-5||py>hi+5||std::abs(wrapDelta(g.player.x,p.x))>16)continue;
        if(p.pickup==Pickup::Star){
            ++g.stars;g.profile.totalStars=addScore(g.profile.totalStars,1);g.score=addScore(g.score,100);
            g.dirty=true;burst(g,p.x,py,0xE6AE36);
        }else{g.player.jetTime=1.6f;g.player.vy=290;burst(g,p.x,py,0xF18A53,10);}
        p.pickup=Pickup::None;g.sounds|=S_POWER;
    }
    if(shoot&&g.shootCooldown<=0){
        for(Bullet& b:g.bullets)if(!b.active){b={true,g.player.x,g.player.y+PLAYER_HEIGHT};g.shootCooldown=.24f;g.sounds|=S_SHOOT;break;}
    }
    for(Bullet& b:g.bullets){
        if(!b.active)continue;
        const float before=b.y;b.y+=460*dt;
        if(b.y>g.camera+VIEW_HEIGHT+45){b.active=false;continue;}
        for(Enemy& e:g.enemies){
            if(!e.active||std::abs(wrapDelta(b.x,e.x))>13)continue;
            if(before<=e.y+12&&b.y>=e.y-12){
                b.active=false;e.active=false;g.score=addScore(g.score,250);g.sounds|=S_POWER;burst(g,e.x,e.y,0x9C8AD4,10);break;
            }
        }
    }
    for(Enemy& e:g.enemies){
        if(!e.active||std::abs(wrapDelta(g.player.x,e.x))>PLAYER_RADIUS+10)continue;
        const float top=e.y+10;
        if(oldY>=top-.001f&&g.player.y<=top&&g.player.vy<0){
            e.active=false;g.player.y=top;g.player.vy=JUMP_SPEED;g.player.squash=1;
            g.score=addScore(g.score,250);g.sounds|=S_BOUNCE;burst(g,e.x,e.y,0x9C8AD4,10);
        }else if(g.player.y<e.y+10&&g.player.y+PLAYER_HEIGHT>e.y-10){
            if(flying||g.player.jetTime>0){e.active=false;g.score=addScore(g.score,250);burst(g,e.x,e.y,0x9C8AD4,10);}
            else{g.finish();break;}
        }
    }
    updateScore(g);
    g.camera=std::max(g.camera,g.player.y-125.0f);
    if(g.player.y+PLAYER_HEIGHT<g.camera){g.finish();return;}
    g.generate();
}
}

float wrapDelta(float from,float to){
    float d=std::fmod(to-from,WORLD_WIDTH);
    if(d>WORLD_WIDTH*.5f)d-=WORLD_WIDTH;
    if(d< -WORLD_WIDTH*.5f)d+=WORLD_WIDTH;
    return d;
}
uint32_t Game::random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
void Game::start(uint32_t runSeed){
    player={};platforms={};enemies={};bullets={};particles={};
    camera=height=elapsed=shootCooldown=0;highestPlatform=18;lastSafeX=WORLD_WIDTH/2;
    score=stars=bounces=0;seed=runSeed?runSeed:0xD00D1E5u;nextId=2;newRecord=false;sounds=0;
    platforms[0]={true,true,1,WORLD_WIDTH/2,18,74,WORLD_WIDTH/2,0,0,0,PlatformKind::Normal,Pickup::None};
    scene=Scene::Ready;generate();
}
void Game::launch(){if(scene==Scene::Ready){scene=Scene::Playing;player.vy=JUMP_SPEED;profile.totalRuns=addScore(profile.totalRuns,1);dirty=true;sounds|=S_START;}}
void Game::pause(){if(scene==Scene::Playing)scene=Scene::Paused;}
void Game::resume(){if(scene==Scene::Paused)scene=Scene::Playing;}
void Game::finish(){if(scene==Scene::Playing){scene=Scene::GameOver;player.jetTime=0;sounds|=S_LOST;record(*this);dirty=true;}}
void Game::step(float seconds,float axis,bool shoot){
    if(scene!=Scene::Playing||!std::isfinite(seconds)||seconds<=0)return;
    if(!std::isfinite(axis))axis=0;
    axis=std::clamp(axis,-1.0f,1.0f);
    float remaining=std::min(seconds,.05f);
    while(remaining>1e-7f&&scene==Scene::Playing){const float dt=std::min(remaining,1.0f/120.0f);tick(*this,dt,axis,shoot);remaining-=dt;}
}
void Game::generate(){
    const float ceiling=camera+VIEW_HEIGHT+120;
    while(highestPlatform<ceiling){
        Platform* safe=nullptr;for(Platform& p:platforms)if(!p.active){safe=&p;break;}
        if(!safe)break;
        const float gap=38+float(random()%13),shift=float(int(random()%121)-60);
        highestPlatform+=gap;lastSafeX=wrap(lastSafeX+shift);
        const uint32_t id=nextId++;
        const float width=height>2400?52:58;
        Pickup pickup=id%19==0?Pickup::Jetpack:id%8==0?Pickup::Spring:id%3==0?Pickup::Star:Pickup::None;
        *safe={true,true,id,lastSafeX,highestPlatform,width,lastSafeX,0,0,0,PlatformKind::Normal,pickup};
        // Optional side routes never replace the static, reachable main chain.
        if(id%2==0){
            for(Platform& p:platforms)if(!p.active){
                const float x=wrap(lastSafeX+105+float(random()%30));
                const auto kind=static_cast<PlatformKind>(1+random()%3);
                const float phase=float(random()%6284)*.001f,amplitude=kind==PlatformKind::Moving?20.0f:0;
                p={true,false,nextId++,wrap(x+std::sin(phase)*amplitude),highestPlatform+8,44,x,phase,amplitude,0,kind,Pickup::Star};break;
            }
        }
        if(highestPlatform>600&&id%11==0){
            for(Enemy& e:enemies)if(!e.active){
                const float origin=wrap(lastSafeX+120),phase=float(random()%6284)*.001f;
                e={true,wrap(origin+std::sin(phase)*14),highestPlatform+13,origin,phase};break;
            }
        }
    }
}
}
