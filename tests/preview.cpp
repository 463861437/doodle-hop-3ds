#include "render.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace hop;
void ppm(const std::string& path,Surface s){std::ofstream out(path,std::ios::binary);out<<"P6\n"<<s.width<<" "<<s.height<<"\n255\n";for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x){const size_t p=(size_t(x)*s.height+s.height-y-1)*3;const char rgb[]={char(s.pixels[p+2]),char(s.pixels[p+1]),char(s.pixels[p])};out.write(rgb,3);}}
int main(int argc,char** argv){
    if(argc!=3)return 2;const int mode=std::atoi(argv[1]);Game g;View v;
    g.profile.bestHeight=1860;g.profile.bestScore=28600;g.profile.totalStars=138;g.profile.totalRuns=24;
    g.camera=480;g.height=620;g.score=7900;g.stars=7;g.bounces=13;g.player.x=136;g.player.y=591;g.player.vy=180;g.scene=Scene::Playing;v.animationTime=1.1f;
    const float xs[]={130,70,198,113,221,168,45,258};
    for(int i=0;i<8;++i){auto& p=g.platforms[i];p.active=true;p.x=xs[i];p.y=493+i*32;p.width=52+(i%2)*6;p.kind=i==2?PlatformKind::Moving:i==5?PlatformKind::Breakable:i==7?PlatformKind::Vanishing:PlatformKind::Normal;p.safe=p.kind==PlatformKind::Normal;p.pickup=i==3?Pickup::Star:i==6?Pickup::Jetpack:i==1?Pickup::Spring:Pickup::None;}
    g.enemies[0]={true,218,678,218,0};g.bullets[0]={true,136,642};
    if(mode==0)g.scene=Scene::Title;
    if(mode==1){g.scene=Scene::Ready;g.camera=0;g.height=0;g.score=0;g.stars=0;g.player.y=18;g.player.x=140;for(int i=0;i<8;++i)g.platforms[i].y=18+i*36;g.platforms[0].x=140;g.enemies[0].active=false;g.bullets[0].active=false;}
    if(mode==3){g.player.jetTime=1.25f;g.particles[0]={true,130,580,0,0,.5f,0xF4B45B};}
    if(mode==4)g.scene=Scene::Paused;
    if(mode==5){g.scene=Scene::GameOver;g.newRecord=true;g.height=2160;g.score=28600;}
    if(mode==6)g.scene=Scene::Help;
    if(mode==7){g.scene=Scene::Paused;v.quitConfirm=true;}
    if(mode==8){g.player.x=2;g.player.y=620;g.enemies[0].x=279;g.platforms[2].x=278;g.platforms[2].y=586;}
    if(mode==9||mode==10){
        const float lift=mode==9?2300.f:5400.f;
        g.camera+=lift;g.height+=lift;g.player.y+=lift;
        g.score+=static_cast<uint32_t>(lift)*10;
        for(auto& p:g.platforms)if(p.active)p.y+=lift;
        for(auto& e:g.enemies)if(e.active)e.y+=lift;
        for(auto& b:g.bullets)if(b.active)b.y+=lift;
    }
    if(mode==11)g.player.vy=-170;
    if(mode==12){g.player.vy=0;v.animationTime=3.95f;}
    std::vector<uint8_t> a(400*240*3),b(320*240*3);Surface top{a.data(),400,240},lower{b.data(),320,240};render(g,v,top,lower);
    ppm(std::string(argv[2])+"-top.ppm",top);ppm(std::string(argv[2])+"-bottom.ppm",lower);
    std::printf("Rendered staged state %d (desktop reference, not a console capture)\n",mode);
}
