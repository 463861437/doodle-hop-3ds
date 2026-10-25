#pragma once
#include "render.hpp"
namespace hop {
enum Key : unsigned { K_A=1,K_B=2,K_X=4,K_Y=8,K_START=16,K_SELECT=32,K_LEFT=64,K_RIGHT=128,K_UP=256,K_DOWN=512,K_L=1024,K_R=2048 };
struct Input { unsigned pressed=0,held=0;bool touching=false,touchDown=false;int touchX=0,touchY=0;float circleX=0; };
// Touch areas (all bottom screen): main 24,62,272,46; help 24,116,130,34;
// sound 166,116,130,34; play steering 12,66,212,150; shoot 232,66,76,94;
// pause 232,172,76,44; quit confirmation left 24,146,130,42 / right 166,146,130,42.
struct Controller {
    bool quit=false,shoot=false;float axis=0;uint32_t nextSeed=1;
    void update(Game& game,View& view,const Input& input,float seconds);
};
}
