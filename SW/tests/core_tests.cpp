#include "core.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace sw;
static void require(bool ok,const char* name) { if(!ok) throw std::runtime_error(name); }
int main() try {
    for(auto mode:{Mode::Hd,Mode::Uhd}) {
        auto f=Format::of(mode); Switcher s;
        Dve d; d.enabled=true; d.source=3;
        require(s.setDve(d),"preview DVE edit accepted");
        require(!s.state().program.dve.enabled,"preview DVE must not leak to PGM");
        require(s.preview(2),"select source");
        require(s.autoMix(12,f),"start mix");
        require(!s.cut()&&!s.preview(3),"serialize conflicting transition controls");
        const int ticks=12*f.ticksPerFrame();
        for(int i=0;i<ticks-1;++i) { s.advance(); require(s.state().transitioning,"no early transition completion"); }
        s.advance();
        require(!s.state().transitioning&&s.state().program.background==2,"mix commits target");
        require(s.state().program.dve.enabled&&s.state().preview.background==0,"swap complete scene including DVE");
        require(s.cut()&&s.state().program.background==0,"cut returns old scene");
        uint64_t samples=0;
        for(uint64_t i=0;i<uint64_t(f.timeScale);++i) samples+=audioSamples(i,f);
        require(samples==48000ULL*f.frameDuration,"fractional video rate must not drift audio clock");
        for(uint64_t i=0;i<10000;++i)require(audioSampleTime(i+1,f)-audioSampleTime(i,f)==audioSamples(i,f),"audio timestamp must match video cadence");
        require(recoverOutputFrame(103,100*f.frameDuration,f)==103,"healthy output must retain its schedule");
        require(recoverOutputFrame(50,100*f.frameDuration,f)==104,"late output must recover ahead of hardware time");
        require(recoverOutputFrame(104,101*f.frameDuration,f)==104,"recovery must not repeat on the following frame");
    }
    require(tickTime(60000).count()==1001000000000LL,"exact NTSC clock over 1001 seconds");
    require(Format::of(Mode::Hd).interlaced&&Format::of(Mode::Hd).timeScale==30000,"HD frame rate vs field rate");
    AudioQueue audio;std::vector<int32_t> ramp(4000);for(size_t i=0;i<ramp.size();++i)ramp[i]=int32_t(i+1);
    audio.push(std::vector<int32_t>(ramp.begin(),ramp.begin()+1600));auto silence=audio.take(800);
    require(std::all_of(silence.begin(),silence.end(),[](int32_t x){return x==0;})&&audio.bufferedFrames()==800,"preroll must preserve queued audio");
    audio.push(std::vector<int32_t>(ramp.begin()+1600,ramp.end()));auto first=audio.take(800);auto second=audio.take(800);
    require(std::equal(first.begin(),first.end(),ramp.begin())&&std::equal(second.begin(),second.end(),ramp.begin()+1600),"stereo packets must be continuous");
    auto gap=audio.take(800);require(audio.underruns==1&&audio.bufferedFrames()==400&&gap[0]==0,"short packet must rebuffer without partial consumption");
    audio.push(std::vector<int32_t>(12000,7));require(audio.bufferedFrames()==4800&&audio.overflowFrames==1600,"overflow must be bounded and counted");
    require(audioPeakDb({0,0})==-120.&&audioPeakDb({INT32_MIN})==0.,"audio meter handles silence and signed minimum");
    Dve d; d.cropLeft=.9f; d.cropRight=.8f; d.opacity=std::numeric_limits<float>::quiet_NaN(); d.sanitize();
    require(d.cropLeft+d.cropRight<=.951f&&d.opacity==1,"sanitize invalid crop and NaN");
    FramePool pool(2); auto a=pool.acquire(Format::of(Mode::Hd)); auto b=pool.acquire(Format::of(Mode::Hd));
    require(!pool.acquire(Format::of(Mode::Hd)),"bounded pool cannot overwrite in-flight frames");
    a.reset(); require(bool(pool.acquire(Format::of(Mode::Hd))),"pool reuses released frame");
    std::cout<<"Core timing, scene isolation, transitions, audio cadence and frame ownership passed.\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
