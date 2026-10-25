#include "controls.hpp"
#include <algorithm>
#include <cmath>
namespace hop {
namespace {
bool inside(const Input& i,int x,int y,int w,int h){return i.touchX>=x&&i.touchX<x+w&&i.touchY>=y&&i.touchY<y+h;}
bool tap(const Input& i,int x,int y,int w,int h){return i.touching&&i.touchDown&&inside(i,x,y,w,h);}
}
void Controller::update(Game& g,View& v,const Input& i,float seconds){
    (void)seconds;
    axis=0;shoot=false;v.touchActive=false;v.touchAxis=0;
    if(v.quitConfirm){
        if((i.pressed&K_A)||tap(i,24,146,130,42))quit=true;
        else if((i.pressed&(K_B|K_SELECT))||tap(i,166,146,130,42))v.quitConfirm=false;
        return;
    }
    if(i.pressed&K_SELECT){g.pause();v.quitConfirm=true;return;}
    const bool main=(i.pressed&(K_A|K_START))||tap(i,24,62,272,46);
    const bool secondary=tap(i,24,116,130,34);
    if(g.scene==Scene::Help){
        if(main||(i.pressed&(K_B|K_Y)))g.scene=Scene::Title;
        return;
    }
    if((i.pressed&K_X)||(g.scene!=Scene::Playing&&tap(i,166,116,130,34))){
        g.profile.sound=!g.profile.sound;g.dirty=true;
    }
    if(g.scene==Scene::Title){
        if(main){g.start(nextSeed++);return;}
        if((i.pressed&K_Y)||secondary)g.scene=Scene::Help;
        return;
    }
    if(g.scene==Scene::Ready){
        if(main){g.launch();return;}
        if((i.pressed&K_B)||secondary)g.scene=Scene::Title;
        return;
    }
    if(g.scene==Scene::Paused||g.scene==Scene::GameOver){
        if(main){if(g.scene==Scene::Paused)g.resume();else g.start(nextSeed++);return;}
        if((i.pressed&K_B)||secondary)g.scene=Scene::Title;
        return;
    }
    if(g.scene!=Scene::Playing)return;
    if((i.pressed&(K_START|K_B))||tap(i,232,172,76,44)){g.pause();return;}
    const float stick=std::isfinite(i.circleX)?std::clamp(i.circleX,-1.0f,1.0f):0;
    if(std::abs(stick)>.14f)axis=std::copysign((std::abs(stick)-.14f)/.86f,stick);
    const bool left=(i.held&(K_LEFT|K_L))!=0,right=(i.held&(K_RIGHT|K_R))!=0;
    if(left||right)axis=left==right?0:(left?-1.0f:1.0f);
    if(i.touching&&inside(i,12,66,212,150)){
        // Absolute touch steering: center is neutral and the pad edges are full
        // speed, so no calibration or finger-motion history is required.
        const float touch=std::clamp((i.touchX-118)/94.0f,-1.0f,1.0f);
        axis=std::abs(touch)<.09f?0:touch;v.touchActive=true;v.touchAxis=axis;
    }
    shoot=(i.held&(K_A|K_UP))||(i.touching&&inside(i,232,66,76,94));
}
}
