#include "media.hpp"
#include "media_pixels.hpp"
#include <iostream>
#include <cmath>
#include <functional>
#include <stdexcept>
using namespace sw;
void require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
void wait(MediaPlayer& p,const std::function<bool(MediaStatus)>& condition) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
    while(std::chrono::steady_clock::now()<deadline) {
        auto s=p.status();if(!s.error.empty())throw std::runtime_error(s.error);
        if(condition(s))return;std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Player operation timed out.");
}
int main(int argc,char** argv)try {
    if(argc!=2)return 2;
    // Compare SIMD plus scalar tail against independently calculated samples.
    for(bool wide:{false,true})for(bool planar:{false,true})for(bool half:{false,true})for(bool interlaced:{false,true}) {
        constexpr int w=18,h=8,stride=64;const int shift=planar?2:8;
        std::vector<uint8_t> y(stride*h),u(stride*h),v(stride*h),packed(w*h*2);
        for(int row=0;row<h;++row)for(int x=0;x<w;++x) {
            const int yy=16+x+row,uu=90+row,vv=150+row;
            if(wide) {reinterpret_cast<uint16_t*>(y.data()+row*stride)[x]=uint16_t(yy<<shift);reinterpret_cast<uint16_t*>(u.data()+row*stride)[x]=uint16_t((planar?uu:(x%2?vv:uu))<<shift);reinterpret_cast<uint16_t*>(v.data()+row*stride)[x]=uint16_t(vv<<shift);}
            else {y[row*stride+x]=uint8_t(yy);u[row*stride+x]=uint8_t(planar?uu:(x%2?vv:uu));v[row*stride+x]=uint8_t(vv);}
        }
        const uint8_t* planes[]={y.data(),u.data(),v.data()};const int lines[]={stride,stride,stride};
        packFileYuv(planes,lines,packed.data(),w*2,w,h,planar,half,wide,shift,interlaced);
        for(int row=0;row<h;++row)for(int x=0;x<w;x+=2) {
            const int cy=half?(interlaced?(row/4)*2+row%2:row/2):row;const int offset=(row*w+x)*2;
            require(packed[offset]==90+cy&&packed[offset+1]==16+x+row&&packed[offset+2]==150+cy&&packed[offset+3]==17+x+row,"YUV packing/channel/field ordering error");
        }
    }
    const Format f{320,180,false,1001,60000};MediaPlayer p;
    p.open(argv[1],f);wait(p,[](auto s){return s.buffered>=8;});
    auto cue=p.pull(800);require(cue.video&&cue.video->persistent,"CUE image missing");
    require(audioPeakDb(cue.audio)==-120.,"CUE leaked audio");
    p.play();std::vector<int32_t> audio;uint64_t previous=0;
    for(uint64_t i=0;i<90;++i) {
        wait(p,[](auto s){return s.buffered>0;});auto v=p.pull(audioSamples(i,f));
        require(v.video&&v.video->data.size()==f.bytes(),"Invalid decoded image");
        require(i==0||v.video->sequence==previous+1,"Decoded frame missing or repeated");previous=v.video->sequence;
        audio.insert(audio.end(),v.audio.begin(),v.audio.end());
    }
    require(audioPeakDb(audio)>-30.,"Audio missing");
    int crossings[2]={};for(size_t i=2;i<audio.size();i+=2)for(int c=0;c<2;++c)if(audio[i-2+c]<=0&&audio[i+c]>0)++crossings[c];
    require(crossings[0]>550&&crossings[0]<750&&crossings[1]>1200&&crossings[1]<1400,"CH1/CH2 mono stream routing or sample rate incorrect");
    p.pause();auto hold=p.pull(801);const auto position=p.status().position;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));auto paused=p.pull(800);
    require(hold.video==paused.video&&position==p.status().position,"Paused position moved");
    require(audioPeakDb(paused.audio)==-120.,"Pause leaked old audio");
    p.seek(.6);wait(p,[](auto s){return s.ready;});auto seek=p.pull(800);
    require(seek.video&&seek.video->sequence>=34&&seek.video->sequence<=37,"Seek did not reach requested frame");
    require(audioPeakDb(seek.audio)==-120.,"Paused seek leaked audio");
    p.cue();wait(p,[](auto s){return s.ready;});auto start=p.pull(800);require(start.video&&start.video->sequence==0,"CUE did not return to beginning");
    p.seek(2.8);p.play();wait(p,[](auto s){return s.ready;});
    for(int i=0;i<50&&!p.status().eof;++i){p.pull(800);std::this_thread::sleep_for(std::chrono::milliseconds(3));}
    wait(p,[](auto s){return s.eof;});require(audioPeakDb(p.pull(800).audio)==-120.,"EOF leaked audio");
    p.setLoop(true);p.play();wait(p,[](auto s){return s.ready&&s.buffered>0;});require(p.pull(800).video->sequence<3,"PLAY at EOF did not restart");
    p.seek(2.8);wait(p,[](auto s){return s.ready;});bool wrapped=false;
    for(int i=0;i<100&&!wrapped;++i){auto sample=p.pull(800);wrapped=sample.video&&sample.video->sequence<3;std::this_thread::sleep_for(std::chrono::milliseconds(3));}
    require(wrapped&&p.status().playing,"Loop failed");p.close();
    p.open(argv[1],Format::of(Mode::Uhd));
    for(int i=0;i<500&&p.status().error.empty();++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(!p.status().error.empty(),"Mismatched format accepted");p.close();
    bool rejected=false;try{p.open("missing-media-file.mxf",f);}catch(...){rejected=true;}require(rejected,"Missing media accepted");
    std::cout<<"PASS: decode/B-frame drain, rational cadence, two mono audio streams, CUE, pause, seek, EOF, loop, invalid input.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
