#pragma once
#include "game.hpp"
namespace hop {
// The simulation keeps its original 280-unit wraparound world. Presentation
// expands it across the entire top screen without changing movement or timing.
constexpr int PLAYFIELD_SCREEN_WIDTH=400;
constexpr float WORLD_TO_SCREEN_X=float(PLAYFIELD_SCREEN_WIDTH)/WORLD_WIDTH;
constexpr int HERO_FEET_X=20,HERO_FEET_Y=40;
struct Surface { uint8_t* pixels=nullptr;int width=0,height=0; };
struct View {
    bool audioAvailable=true,saveFailed=false,quitConfirm=false;
    float animationTime=0,touchAxis=0;bool touchActive=false;
};
void render(const Game& game,const View& view,Surface top,Surface bottom);
}
