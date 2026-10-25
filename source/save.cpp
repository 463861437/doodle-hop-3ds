#include "save.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
namespace hop {
namespace {
constexpr size_t SIZE=36;
using Bytes=std::array<unsigned char,SIZE>;
void put(Bytes& b,size_t off,uint32_t n){for(unsigned i=0;i<4;++i)b[off+i]=static_cast<unsigned char>(n>>(i*8));}
uint32_t get(const Bytes& b,size_t off){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(b[off+i])<<(i*8);return n;}
uint32_t checksum(const Bytes& b){uint32_t v=2166136261u;for(size_t i=0;i<SIZE-4;++i){v^=b[i];v*=16777619u;}return v;}
int readSlot(const std::string& path,Profile& profile,uint32_t& sequence){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f)return errno==ENOENT?0:-1;
    Bytes b{};const size_t read=std::fread(b.data(),1,b.size(),f);
    const int tail=std::fgetc(f);const bool failed=std::ferror(f)!=0;const int closed=std::fclose(f);
    if(read!=SIZE||tail!=EOF||failed||closed||std::memcmp(b.data(),"DH3DS001",8)||get(b,32)!=checksum(b)||get(b,28)>1)return -1;
    Profile decoded;decoded.bestHeight=get(b,12);decoded.bestScore=get(b,16);
    decoded.totalRuns=get(b,20);decoded.totalStars=get(b,24);decoded.sound=get(b,28)!=0;
    profile=decoded;sequence=get(b,8);return 1;
}
bool directory(const std::string& path){
    if(mkdir(path.c_str(),0777)==0)return true;
    struct stat s{};return errno==EEXIST&&stat(path.c_str(),&s)==0&&S_ISDIR(s.st_mode);
}
}
LoadStatus SaveStore::load(Profile& p){
    Profile candidates[2];uint32_t seq[2]{};int states[2];
    for(int i=0;i<2;++i)states[i]=readSlot(directory_+"/progress"+char('0'+i)+".bin",candidates[i],seq[i]);
    active_=-1;sequence_=0;if(states[0]==1)active_=0;
    const uint32_t distance=seq[1]-seq[0];
    if(states[1]==1&&(active_<0||(distance&&distance<0x80000000u)))active_=1;
    if(active_>=0){p=candidates[active_];sequence_=seq[active_];return states[1-active_]<0?LoadStatus::Recovered:LoadStatus::Loaded;}
    return states[0]<0||states[1]<0?LoadStatus::Invalid:LoadStatus::Missing;
}
bool SaveStore::save(const Profile& p){
    if(!directory(directory_))return false;
    Bytes b{};std::memcpy(b.data(),"DH3DS001",8);put(b,8,sequence_+1);
    put(b,12,p.bestHeight);put(b,16,p.bestScore);put(b,20,p.totalRuns);put(b,24,p.totalStars);put(b,28,p.sound?1u:0u);put(b,32,checksum(b));
    const int next=active_==0?1:0;
    const std::string path=directory_+"/progress"+char('0'+next)+".bin",temp=path+".tmp";
    FILE* f=std::fopen(temp.c_str(),"wb");if(!f)return false;
    const bool written=std::fwrite(b.data(),1,b.size(),f)==b.size(),flushed=std::fflush(f)==0;
    const bool closed=std::fclose(f)==0;
    if(!written||!flushed||!closed){std::remove(temp.c_str());return false;}
    // The other complete slot survives replacement even if power is lost.
    if(std::remove(path.c_str())!=0&&errno!=ENOENT){std::remove(temp.c_str());return false;}
    if(std::rename(temp.c_str(),path.c_str())!=0){std::remove(temp.c_str());return false;}
    ++sequence_;active_=next;return true;
}
}
