#include "controls.hpp"
#include "save.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#include <unistd.h>
using namespace hop;
static int checks=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"platform failure "<<__LINE__<<": "<<#x<<'\n';std::abort();}++checks;}while(0)
static Input press(unsigned k){Input i;i.pressed=i.held=k;return i;}
static Input touch(int x,int y,bool down=true){Input i;i.touching=true;i.touchDown=down;i.touchX=x;i.touchY=y;return i;}
static std::vector<unsigned char> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::filesystem::path& p,const std::vector<unsigned char>& b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));}
static void set32(std::vector<unsigned char>& b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=static_cast<unsigned char>(x>>(8*i));}
static void checksum(std::vector<unsigned char>& b){uint32_t n=2166136261u;for(size_t i=0;i<32;++i){n^=b[i];n*=16777619u;}set32(b,32,n);}
int main(){
    Game g;View v;Controller c;c.nextSeed=123;
    c.update(g,v,press(K_Y),.016f);CHECK(g.scene==Scene::Help);
    c.update(g,v,touch(100,130),.016f);CHECK(g.scene==Scene::Help);
    c.update(g,v,press(K_B),.016f);CHECK(g.scene==Scene::Title);
    c.update(g,v,touch(200,130),.016f);CHECK(!g.profile.sound&&g.dirty);
    c.update(g,v,touch(100,80),.016f);CHECK(g.scene==Scene::Ready&&c.nextSeed==124);
    c.update(g,v,press(K_A),.016f);CHECK(g.scene==Scene::Playing&&!c.shoot);
    Input i;i.circleX=.1f;c.update(g,v,i,.016f);CHECK(c.axis==0);
    i.circleX=.57f;c.update(g,v,i,.016f);CHECK(c.axis>.49f&&c.axis<.51f);
    i.circleX=std::numeric_limits<float>::quiet_NaN();c.update(g,v,i,.016f);CHECK(c.axis==0);
    i.circleX=1;i.held=K_LEFT;c.update(g,v,i,.016f);CHECK(c.axis==-1);
    i.held=K_LEFT|K_RIGHT;c.update(g,v,i,.016f);CHECK(c.axis==0);
    i.held=K_R;c.update(g,v,i,.016f);CHECK(c.axis==1);
    c.update(g,v,touch(12,90,false),.016f);CHECK(c.axis==-1&&v.touchActive&&v.touchAxis==-1);
    c.update(g,v,touch(223,90,false),.016f);CHECK(c.axis==1);
    c.update(g,v,touch(118,100,false),.016f);CHECK(c.axis==0&&v.touchActive);
    c.update(g,v,touch(224,90,false),.016f);CHECK(!v.touchActive&&c.axis==0);
    c.update(g,v,touch(250,100,false),.016f);CHECK(c.shoot&&!v.touchActive);
    i={};i.held=K_A;c.update(g,v,i,.016f);CHECK(c.shoot);
    i.held=K_UP;c.update(g,v,i,.016f);CHECK(c.shoot);
    c.update(g,v,touch(250,180),.016f);CHECK(g.scene==Scene::Paused&&!c.shoot);
    c.update(g,v,touch(100,80),.016f);CHECK(g.scene==Scene::Playing);
    c.update(g,v,press(K_SELECT),.016f);CHECK(v.quitConfirm&&g.scene==Scene::Paused&&!c.quit);
    c.update(g,v,touch(180,160),.016f);CHECK(!v.quitConfirm&&!c.quit&&g.scene==Scene::Paused);
    c.update(g,v,press(K_B),.016f);CHECK(g.scene==Scene::Title);
    c.update(g,v,touch(100,80,false),.016f);CHECK(g.scene==Scene::Title);
    c.update(g,v,touch(24,62),.016f);CHECK(g.scene==Scene::Ready);
    c.update(g,v,touch(24,116),.016f);CHECK(g.scene==Scene::Title);
    g.scene=Scene::GameOver;c.update(g,v,press(K_A),.016f);CHECK(g.scene==Scene::Ready);
    c.update(g,v,press(K_SELECT),.016f);CHECK(v.quitConfirm);
    c.update(g,v,touch(30,150),.016f);CHECK(c.quit);

    char temporary[]="/tmp/doodle-hop-save-XXXXXX";CHECK(mkdtemp(temporary)!=nullptr);
    const std::filesystem::path root=temporary,path=root/"profile";
    Profile p;p.bestHeight=1234;p.bestScore=4321;p.totalRuns=42;p.totalStars=77;p.sound=false;
    SaveStore store(path.string());Profile out;
    CHECK(store.load(out)==LoadStatus::Missing);CHECK(out.bestHeight==0&&out.sound);
    CHECK(store.save(p));const auto first=read(path/"progress0.bin");CHECK(first.size()==36);
    p.bestHeight=2000;p.totalRuns=43;CHECK(store.save(p));const auto second=read(path/"progress1.bin");
    SaveStore fresh(path.string());CHECK(fresh.load(out)==LoadStatus::Loaded);
    CHECK(out.bestHeight==2000&&out.bestScore==4321&&out.totalRuns==43&&out.totalStars==77&&!out.sound);
    for(int broken=0;broken<5;++broken){
        auto bytes=second;
        if(broken==0)bytes[0]^=1;
        if(broken==1)bytes.resize(20);
        if(broken==2)bytes.push_back(0);
        if(broken==3)bytes[16]^=1;
        if(broken==4){set32(bytes,28,3);checksum(bytes);}
        write(path/"progress1.bin",bytes);
        SaveStore recover(path.string());CHECK(recover.load(out)==LoadStatus::Recovered);CHECK(out.bestHeight==1234&&out.totalRuns==42);
    }
    write(path/"progress0.bin",{});out.bestScore=987;
    SaveStore invalid(path.string());CHECK(invalid.load(out)==LoadStatus::Invalid);CHECK(out.bestScore==987);
    auto old=first,newer=second;set32(old,8,0xFFFFFFFFu);checksum(old);set32(newer,8,0);checksum(newer);
    write(path/"progress0.bin",old);write(path/"progress1.bin",newer);
    SaveStore wrapped(path.string());CHECK(wrapped.load(out)==LoadStatus::Loaded&&out.bestHeight==2000);
    CHECK(wrapped.save(out));CHECK(read(path/"progress0.bin")[8]==1);
    // A blocked temporary path must not corrupt the older valid slot.
    std::filesystem::create_directory(path/"progress1.bin.tmp");
    CHECK(!wrapped.save(p));SaveStore afterFail(path.string());CHECK(afterFail.load(out)==LoadStatus::Loaded&&out.bestHeight==2000);
    const auto nondir=root/"plain-file";write(nondir,{1,2,3});SaveStore badDir(nondir.string());CHECK(!badDir.save(p));
    std::filesystem::remove_all(root);
    std::cout<<checks<<" controls and save checks passed\n";
}
