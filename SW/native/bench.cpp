#include "gpu.hpp"
#include "engine.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
static void require(bool b,const char* message) { if(!b)throw std::runtime_error(message); }
static uint8_t luma(sw::FramePtr f,int x,int y) { return f->data[size_t(y)*f->stride+x*2+1]; }
int main(int argc,char** argv) {
    try {
        bool uhd=false,engineTest=false,receiveTest=false,aja=false,monitorTest=false;
        for(int i=1;i<argc;++i) { uhd|=std::string(argv[i])=="--uhd";engineTest|=std::string(argv[i])=="--engine";receiveTest|=std::string(argv[i])=="--receive";aja|=std::string(argv[i])=="--receive-aja";monitorTest|=std::string(argv[i])=="--monitor"; }
        receiveTest|=aja;
        auto format=sw::Format::of(uhd?sw::Mode::Uhd:sw::Mode::Hd);
        if(receiveTest) {
            std::vector<std::string> warnings;auto ports=sw::probeReceiveDevices(aja?"aja":"decklink",warnings);
            for(const auto& warning:warnings)std::cerr<<warning<<'\n';
            for(const auto& p:ports)std::cerr<<p.id<<": "<<p.detail<<'\n';
            auto port=std::find_if(ports.begin(),ports.end(),[](const sw::Endpoint& e){return e.input&&e.detail.find("signal=locked")!=std::string::npos;});
            if(port==ports.end())port=std::find_if(ports.begin(),ports.end(),[](const sw::Endpoint& e){return e.input;});require(port!=ports.end(),"No input detected for selected backend");
            sw::Configuration config;config.synthetic=false;config.receiveOnly=true;config.receiveBackend=aja?"aja":"decklink";config.mode=uhd?sw::Mode::Uhd:sw::Mode::Hd;config.inputs[0]=*port;
            // Invalid output backends deliberately detect any accidental attempt to open an output.
            config.outputs[0].backend=config.outputs[1].backend="must-not-open";
            sw::Engine engine;engine.start(config);std::this_thread::sleep_for(std::chrono::seconds(5));auto state=engine.snapshot();engine.stop();
            require(state.running&&state.error.empty(),state.error.empty()?"Receiver failed to start":state.error.c_str());
            require(!state.monitors[0].rgba.empty(),"Receiver must display a monitor even without a signal");
            require(!engine.snapshot().running,"Receiver failed to stop");
            std::cout<<"{\"receive_only_start_stop\":\"passed\",\"endpoint\":\""<<port->id<<"\",\"mode\":\""<<format.label()<<"\",\"valid_frames\":"<<state.io[0].frames<<",\"receive_fps\":"<<state.inputFps[0]<<",\"signal\":"<<(state.signal[0]?"true":"false")<<",\"physical_outputs_opened\":0}\n";return 0;
        }
        if(monitorTest) {
            sw::GpuCompositor monitor;monitor.initialize(format,true);sw::FramePool sourcePool(4);std::array<sw::FramePtr,4> sourceFrames;sw::Switcher state;
            double total=0,worst=0;unsigned count=0;const int samples=180;
            for(int tick=0;tick<samples;++tick) {
                auto begin=std::chrono::steady_clock::now();
                if(tick%format.ticksPerFrame()==0) {auto frame=sourcePool.acquire(format);require(bool(frame),"Source pool exhausted");sw::fillPattern(*frame,0,tick);sourceFrames[0]=frame;}
                auto result=monitor.render(sourceFrames,state.state(),tick,true,false);
                require(!result.output[0]&&!result.output[1],"Monitor benchmark opened output buffers");
                if(!result.monitors[0].rgba.empty())++count;
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();if(tick>=10){total+=ms;worst=std::max(worst,ms);}
            }
            require(count==samples-1,"Monitor pipeline skipped ticks");
            std::cout<<"{\"monitor_only\":\"passed\",\"generated_frames\":"<<count<<",\"mean_ms\":"<<total/(samples-10)<<",\"worst_ms\":"<<worst<<",\"includes_live_upload\":true}\n";return 0;
        }
        if(engineTest) {
            sw::Engine engine;sw::Configuration config;config.mode=uhd?sw::Mode::Uhd:sw::Mode::Hd;engine.start(config);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);sw::Snapshot a;
            do {std::this_thread::sleep_for(std::chrono::milliseconds(50));a=engine.snapshot();}while(a.starting&&a.error.empty()&&std::chrono::steady_clock::now()<deadline);
            require(a.running,a.error.empty()?"Engine failed to start":a.error.c_str());std::this_thread::sleep_for(std::chrono::seconds(2));a=engine.snapshot();
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
            // Shader initialization may outlast the 500 ms signal watchdog.
            for(auto& f:frames)std::const_pointer_cast<sw::Frame>(f)->captured=std::chrono::steady_clock::now();
            sw::GpuResult a,b,c;
            for(int k=0;k<format.ticksPerFrame();++k)a=delayed.render(frames,routing.state(),k,true);
            require(!a.output[0],"Pipelined output must prime before returning a frame");routing.cut();
            for(int k=0;k<format.ticksPerFrame();++k)b=delayed.render(frames,routing.state(),format.ticksPerFrame()+k,false);
            for(int k=0;k<format.ticksPerFrame();++k)c=delayed.render(frames,routing.state(),2*format.ticksPerFrame()+k,true);
            require(b.output[0]&&c.output[0]&&b.output[0]->sequence==0&&c.output[0]->sequence==1,"Pipelined output sequence incorrect");
            require(b.outputState.program.background==0&&c.outputState.program.background==1,"Delayed audio scene must match delayed video");
            require(std::abs(int(luma(b.output[0],100,100))-int(luma(c.output[0],100,100)))>10,"Pipelined CUT must change video on the next complete frame");
            auto monitorOnly=delayed.render(frames,routing.state(),4*format.ticksPerFrame(),true,false);
            require(!monitorOnly.output[0]&&!monitorOnly.output[1],"Receive monitor mode must not produce output buffers");
            require(!monitorOnly.monitors[0].rgba.empty()&&!monitorOnly.monitors[4].rgba.empty(),"Receive monitor mode must still display input and local PGM");
        }
        if(!format.interlaced) {
            // Progressive frames must never move with tick/field parity. A one-pixel
            // checkerboard must remain neutral after 8:1 monitor downsampling.
            auto f=pool.acquire(format);
            for(int y=0;y<format.height;++y)for(int x=0;x<format.width;x+=2) {auto* p=f->data.data()+size_t(y)*f->stride+x*2;p[0]=p[2]=128;p[1]=y%2?235:16;p[3]=y%2?16:235;}
            f->captured=std::chrono::steady_clock::now();frames[0]=f;sw::Switcher stable;
            auto before=gpu.render(frames,stable.state(),100,true,false).monitors[0];
            auto after=gpu.render(frames,stable.state(),101,true,false).monitors[0];
            require(before.rgba==after.rgba,"Progressive monitor shifts with field parity");
            const int grey=after.rgba[(135*480+240)*4];require(grey>=125&&grey<=130,"Monitor minification aliases the checkerboard");
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
