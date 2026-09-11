#include "engine.hpp"
#include <iostream>
#include <thread>
// Local decode/composition check: never opens or modifies an SDI device.
int main(int argc,char** argv)try {
    if(argc<2){std::cerr<<"sw_player_check file [--uhd]\n";return 2;}
    sw::Configuration config;config.mediaPath=argv[1];config.mediaLoop=true;config.mode=argc>2?sw::Mode::Uhd:sw::Mode::Hd;
    sw::Engine engine;engine.setMuted(false);engine.start(config);engine.selectPreview(3);engine.cut();engine.selectPreview(3);
    auto begin=std::chrono::steady_clock::now();
    for(;;) {
        auto s=engine.snapshot();if(!s.error.empty()||!s.media.error.empty())throw std::runtime_error(s.error+s.media.error);
        if(s.running&&s.media.buffered>=6)break;
        if(std::chrono::steady_clock::now()-begin>std::chrono::seconds(30))throw std::runtime_error("Player initialization timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    engine.player().play();std::this_thread::sleep_for(std::chrono::seconds(1));auto before=engine.snapshot();begin=std::chrono::steady_clock::now();
    double peak=-120.;unsigned images=0;
    for(int i=0;i<50;++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));auto s=engine.snapshot();
        if(!s.error.empty()||!s.media.error.empty())throw std::runtime_error(s.error+s.media.error);
        peak=std::max(peak,s.outputAudioPeak[0]);if(!s.monitors[4].rgba.empty()&&s.signal[3])++images;
    }
    auto end=engine.snapshot();const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    engine.player().pause();std::this_thread::sleep_for(std::chrono::milliseconds(150));auto paused=engine.snapshot();engine.stop();
    std::cout<<"{\"tickFps\":"<<(end.ticks-before.ticks)/seconds<<",\"mediaFps\":"<<(end.media.frames-before.media.frames)/seconds
      <<",\"decodeMs\":"<<end.media.decodeMs<<",\"videoMs\":"<<end.media.videoMs<<",\"convertMs\":"<<end.media.convertMs<<",\"audioMs\":"<<end.media.audioMs<<",\"hardware\":"<<end.media.hardware<<",\"playerUnderruns\":"<<end.media.underruns-before.media.underruns<<",\"pgmPeakDb\":"<<peak
      <<",\"pausePeakDb\":"<<paused.outputAudioPeak[0]<<",\"validMonitorChecks\":"<<images<<"}\n";
    return peak>-119.&&paused.outputAudioPeak[0]<=-119.&&images==50?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
