#pragma once
#include <array>
#include <cstdint>
namespace hop {
constexpr float WORLD_WIDTH=280.0f,VIEW_HEIGHT=240.0f;
constexpr float PLAYER_RADIUS=10.0f,PLAYER_HEIGHT=23.0f;
constexpr float GRAVITY=780.0f,JUMP_SPEED=360.0f,MAX_SPEED=160.0f;
constexpr int MAX_PLATFORMS=40,MAX_ENEMIES=8,MAX_BULLETS=12,MAX_PARTICLES=48;
enum class Scene { Title,Ready,Playing,Paused,GameOver,Help };
enum class PlatformKind { Normal,Moving,Breakable,Vanishing };
enum class Pickup { None,Spring,Jetpack,Star };
enum Sound : unsigned { S_BOUNCE=1,S_SHOOT=2,S_POWER=4,S_LOST=8,S_RECORD=16,S_START=32 };
struct Profile { uint32_t bestHeight=0,bestScore=0,totalRuns=0,totalStars=0;bool sound=true; };
// Y is altitude, increasing upward. Player Y is the sole of the feet.
struct Player { float x=WORLD_WIDTH/2,y=18,vx=0,vy=0,jetTime=0,squash=0;int facing=1; };
struct Platform {
    bool active=false,safe=false;uint32_t id=0;
    float x=0,y=0,width=56,originX=0,phase=0,amplitude=0,brokenTime=0;
    PlatformKind kind=PlatformKind::Normal;Pickup pickup=Pickup::None;
};
struct Enemy { bool active=false;float x=0,y=0,originX=0,phase=0; };
struct Bullet { bool active=false;float x=0,y=0; };
struct Particle { bool active=false;float x=0,y=0,vx=0,vy=0,life=0;uint32_t color=0; };
struct Game {
    Scene scene=Scene::Title;Profile profile;Player player;
    std::array<Platform,MAX_PLATFORMS> platforms{};
    std::array<Enemy,MAX_ENEMIES> enemies{};
    std::array<Bullet,MAX_BULLETS> bullets{};
    std::array<Particle,MAX_PARTICLES> particles{};
    float camera=0,height=0,elapsed=0,shootCooldown=0,highestPlatform=0,lastSafeX=WORLD_WIDTH/2;
    uint32_t score=0,stars=0,bounces=0,seed=1,nextId=1;
    bool dirty=false,newRecord=false;unsigned sounds=0;
    void start(uint32_t runSeed);
    void launch();void pause();void resume();void finish();
    void step(float seconds,float axis,bool shoot);
    void generate();
    uint32_t random();
};
// Shortest horizontal displacement, matching wrap-around rendering/collisions.
float wrapDelta(float from,float to);
}
