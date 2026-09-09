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
    }
    require(tickTime(60000).count()==1001000000000LL,"exact NTSC clock over 1001 seconds");
    require(Format::of(Mode::Hd).interlaced&&Format::of(Mode::Hd).timeScale==30000,"HD frame rate vs field rate");
    Dve d; d.cropLeft=.9f; d.cropRight=.8f; d.opacity=std::numeric_limits<float>::quiet_NaN(); d.sanitize();
    require(d.cropLeft+d.cropRight<=.951f&&d.opacity==1,"sanitize invalid crop and NaN");
    FramePool pool(2); auto a=pool.acquire(Format::of(Mode::Hd)); auto b=pool.acquire(Format::of(Mode::Hd));
    require(!pool.acquire(Format::of(Mode::Hd)),"bounded pool cannot overwrite in-flight frames");
    a.reset(); require(bool(pool.acquire(Format::of(Mode::Hd))),"pool reuses released frame");
    std::cout<<"Core timing, scene isolation, transitions, audio cadence and frame ownership passed.\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
