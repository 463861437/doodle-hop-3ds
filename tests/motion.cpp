#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
using namespace hop;
int main(int argc,char** argv){
    if(argc!=2)return 2;
    Game game;View view;game.start(3);game.launch();
    std::vector<uint8_t> pixels(400*240*3);Surface screen{pixels.data(),400,240};
    for(int frame=0;frame<240;++frame){
        for(int step=0;step<4;++step){
            const float upward=std::max(0.0f,game.player.vy);
            const float apex=game.player.jetTime>0?game.player.y+80:game.player.y+upward*upward/(2*GRAVITY);
            const Platform* target=nullptr;
            for(const Platform& p:game.platforms){
                if(!p.active||!p.safe||p.y>apex-1||p.y<game.camera+4)continue;
                const float discriminant=game.player.vy*game.player.vy+2*GRAVITY*(game.player.y-p.y);
                if(game.player.jetTime<=0&&discriminant<0)continue;
                const float time=game.player.jetTime>0?1:(game.player.vy+std::sqrt(discriminant))/GRAVITY;
                if(time<.01f||std::abs(wrapDelta(game.player.x,p.x))>MAX_SPEED*time+p.width*.3f)continue;
                if(!target||p.y>target->y)target=&p;
            }
            const float axis=target?std::clamp(wrapDelta(game.player.x,target->x)*.055f,-1.0f,1.0f):0;
            game.step(1.0f/120,axis,true);view.animationTime+=1.0f/120;
        }
        render(game,view,screen,{});
        std::ofstream out(std::string(argv[1])+"/"+std::to_string(frame)+".ppm",std::ios::binary);
        out<<"P6\n400 240\n255\n";
        for(int y=0;y<240;++y)for(int x=0;x<400;++x){const size_t i=(size_t(x)*240+239-y)*3;
            const char rgb[]={char(pixels[i+2]),char(pixels[i+1]),char(pixels[i])};out.write(rgb,3);}
    }
    std::printf("Rendered 240 actual simulation frames: height %.1f, bounces %u, stars %u, scene %d\n",game.height,game.bounces,game.stars,int(game.scene));
    return game.scene==Scene::Playing?0:1;
}
