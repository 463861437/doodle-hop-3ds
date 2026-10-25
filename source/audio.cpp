#include "audio.hpp"
#include "game.hpp"
#include <3ds.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace hop {
namespace {
constexpr int RATE=22050, VOICES=4, KINDS=6;
struct Sample { int16_t* data=nullptr;unsigned count=0; };
Sample samples[KINDS];ndspWaveBuf waves[VOICES]{};
bool available=false;
uint64_t lastHit=0;
}
bool audioInit() {
    if(available)return true;
    if(R_FAILED(ndspInit()))return false;
    lastHit=0;std::memset(waves,0,sizeof(waves));
    available=true;ndspSetOutputMode(NDSP_OUTPUT_STEREO);ndspSetMasterVol(0.55f);
    constexpr float duration[KINDS]={0.065f,0.035f,0.24f,0.34f,0.45f,0.16f};
    constexpr float frequency[KINDS]={1174.66f,392,587.33f,246.94f,440,220};
    for(int kind=0;kind<KINDS;++kind) {
        Sample& s=samples[kind];s.count=static_cast<unsigned>(RATE*duration[kind]);
        s.data=static_cast<int16_t*>(linearAlloc(s.count*sizeof(int16_t)));
        if(!s.data){audioExit();return false;}
        float phase=0;
        for(unsigned n=0;n<s.count;++n) {
            const float t=float(n)/s.count;
            float hz=frequency[kind];
            if(kind==2)hz*=1.0f+std::floor(t*3)*0.5f;
            else if(kind==3)hz*=1.0f-0.65f*t;
            else if(kind==4) {constexpr float notes[]={1,1.25f,1.5f,2};hz*=notes[std::min(3,int(t*4))];}
            else if(kind==5)hz*=1+t;
            phase+=6.2831853f*hz/RATE;
            const float envelope=std::min(t*40,1.0f)*(1-t)*(1-t);
            s.data[n]=static_cast<int16_t>((0.72f*std::sin(phase)+0.20f*std::sin(phase*3)+0.08f*std::sin(phase*5))*envelope*7000);
        }
        DSP_FlushDataCache(s.data,s.count*sizeof(int16_t));
    }
    for(int v=0;v<VOICES;++v) {
        ndspChnReset(v);ndspChnSetInterp(v,NDSP_INTERP_LINEAR);
        ndspChnSetRate(v,RATE);ndspChnSetFormat(v,NDSP_FORMAT_MONO_PCM16);
        float mix[12]{};mix[0]=mix[1]=0.6f;ndspChnSetMix(v,mix);
    }
    return true;
}
void audioPlay(unsigned events,bool enabled) {
    if(!available||!enabled||!events)return;
    const int kind=events&S_RECORD?4:events&S_LOST?3:events&S_POWER?2:events&S_START?5:events&S_SHOOT?1:0;
    const uint64_t now=osGetTime();
    if(kind==0&&now-lastHit<45)return;
    for(int v=0;v<VOICES;++v) {
        if(waves[v].status!=NDSP_WBUF_FREE&&waves[v].status!=NDSP_WBUF_DONE)continue;
        ndspChnWaveBufClear(v);std::memset(&waves[v],0,sizeof(waves[v]));
        waves[v].data_pcm16=samples[kind].data;waves[v].nsamples=samples[kind].count;
        ndspChnWaveBufAdd(v,&waves[v]);if(kind==0)lastHit=now;break;
    }
}
void audioStop() {if(available)for(int v=0;v<VOICES;++v)ndspChnWaveBufClear(v);}
void audioExit() {
    if(available){audioStop();ndspExit();available=false;}
    for(Sample& s:samples){if(s.data)linearFree(s.data);s={};}
}
}
