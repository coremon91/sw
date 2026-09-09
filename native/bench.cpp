#include "gpu.hpp"
#include "engine.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
static void require(bool b,const char* message) { if(!b)throw std::runtime_error(message); }
static uint8_t luma(sw::FramePtr f,int x,int y) { return f->data[size_t(y)*f->stride+x*2+1]; }
int main(int argc,char** argv) {
    try {
        bool uhd=false,engineTest=false;
        for(int i=1;i<argc;++i) { uhd|=std::string(argv[i])=="--uhd";engineTest|=std::string(argv[i])=="--engine"; }
        auto format=sw::Format::of(uhd?sw::Mode::Uhd:sw::Mode::Hd);
        if(engineTest) {
            sw::Engine engine;sw::Configuration config;config.mode=uhd?sw::Mode::Uhd:sw::Mode::Hd;engine.start(config);
            std::this_thread::sleep_for(std::chrono::seconds(2));auto a=engine.snapshot();require(a.running,a.error.empty()?"Engine failed to start":a.error.c_str());
            require(!a.monitors[4].rgba.empty()&&!a.monitors[5].rgba.empty(),"Both monitors must render");
            sw::Dve d;d.enabled=true;d.source=3;engine.selectPreview(2);engine.setDve(d);engine.cut();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));auto b=engine.snapshot();require(b.state.program.background==2&&b.state.program.dve.enabled,"UI commands must reach the renderer");
            engine.selectPreview(1);engine.autoMix(15);
            std::this_thread::sleep_for(std::chrono::seconds(6));auto c=engine.snapshot();require(c.running&&c.error.empty(),c.error.empty()?"Engine stopped":c.error.c_str());require(!c.state.transitioning&&c.state.program.background==1,"Automatic transition must finish");
            engine.stop();require(!engine.snapshot().running,"Stop must join the worker");
            std::cout<<"{\"engine\":\"passed\",\"mode\":\""<<format.label()<<"\",\"ticks_in_6_15_seconds\":"<<c.ticks-a.ticks<<",\"overruns\":"<<c.overruns<<",\"last_render_ms\":"<<c.renderMs<<",\"source_ms\":"<<c.sourceMs<<",\"gpu_ms\":"<<c.gpuMs<<"}\n";return 0;
        }
        sw::GpuCompositor gpu;gpu.initialize(format);sw::FramePool pool(8);std::array<sw::FramePtr,4> frames;
        for(int i=0;i<4;++i) { auto f=pool.acquire(format);sw::fillPattern(*f,i,0);frames[i]=f; }
        sw::Switcher switcher;sw::GpuResult result;
        auto renderFrame=[&](uint64_t base) { for(int k=0;k<format.ticksPerFrame();++k)result=gpu.render(frames,switcher.state(),base+k,false); };
        renderFrame(0);require(result.output[0]&&result.output[1],"Both buses must produce a frame");
        require(std::abs(int(luma(result.output[0],100,100))-int(luma(result.output[1],100,100)))>10,"PGM/PVW must be independent");
        const auto backgroundLuma=luma(result.output[1],format.width*27/100,format.height/2);
        sw::Dve dve;dve.enabled=true;dve.source=0;dve.x=dve.y=.25f;dve.width=dve.height=.5f;dve.border=0;switcher.setDve(dve);renderFrame(2);
        require(luma(result.output[0],100,100)>220,"DVE editing must not change PGM");
        require(std::abs(int(luma(result.output[1],format.width*27/100,format.height/2))-235)<4,"DVE must sample the overlay inside its transformed rectangle");
        dve.opacity=0;switcher.setDve(dve);renderFrame(4);
        require(std::abs(int(luma(result.output[1],format.width*27/100,format.height/2))-int(backgroundLuma))<2,"DVE opacity zero must reveal background");
        dve.opacity=1;switcher.setDve(dve);
        switcher.cut();renderFrame(4);require(luma(result.output[0],100,100)<220,"CUT must change PGM");
        // Interlaced output must retain the earlier field when rendering the later field.
        if(format.interlaced) {
            auto f=pool.acquire(format);sw::fillPattern(*f,0,0);
            for(int y=0;y<format.height;++y)for(int x=0;x<format.width;x+=2) { auto* p=f->data.data()+size_t(y)*f->stride+x*2;p[0]=p[2]=128;p[1]=p[3]=y%2?180:40; }
            frames[0]=f;switcher.reset();renderFrame(6);
            require(std::abs(int(luma(result.output[0],100,100))-40)<3,"First field changed");
            require(std::abs(int(luma(result.output[0],100,101))-180)<3,"Second field parity incorrect");
        }
        {
            // The asynchronous engine path must return the preceding frame and scene together.
            sw::GpuCompositor delayed;delayed.initialize(format,true);sw::Switcher routing;
            sw::GpuResult a,b,c;
            for(int k=0;k<format.ticksPerFrame();++k)a=delayed.render(frames,routing.state(),k,true);
            require(!a.output[0],"Pipelined output must prime before returning a frame");routing.cut();
            for(int k=0;k<format.ticksPerFrame();++k)b=delayed.render(frames,routing.state(),format.ticksPerFrame()+k,false);
            for(int k=0;k<format.ticksPerFrame();++k)c=delayed.render(frames,routing.state(),2*format.ticksPerFrame()+k,true);
            require(b.output[0]&&c.output[0]&&b.output[0]->sequence==0&&c.output[0]->sequence==1,"Pipelined output sequence incorrect");
            require(b.outputState.program.background==0&&c.outputState.program.background==1,"Delayed audio scene must match delayed video");
            require(std::abs(int(luma(b.output[0],100,100))-int(luma(c.output[0],100,100)))>10,"Pipelined CUT must change video on the next complete frame");
        }
        const int ticks=120;double worst=0,total=0;int late=0;
        for(int t=0;t<ticks;++t) {
            // Refresh timestamps while retaining fixed source buffers to isolate GPU transfer/composite/readback.
            for(auto& f:frames)std::const_pointer_cast<sw::Frame>(f)->captured=std::chrono::steady_clock::now();
            auto begin=std::chrono::steady_clock::now();result=gpu.render(frames,switcher.state(),uint64_t(t+10),t%4==0);
            double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();total+=ms;worst=std::max(worst,ms);if(ms>16.6834)++late;
        }
        std::cout<<"{\"mode\":\""<<format.label()<<"\",\"gpu\":\""<<gpu.adapter()<<"\",\"pixel_checks\":\"passed\",\"ticks\":"<<ticks<<",\"mean_ms\":"<<total/ticks<<",\"worst_ms\":"<<worst<<",\"over_budget\":"<<late<<"}\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
