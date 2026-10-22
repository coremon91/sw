#include "gpu.hpp"
#include "engine.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
static void require(bool b,const char* message) { if(!b)throw std::runtime_error(message); }
static uint8_t luma(sw::FramePtr f,int x,int y) { return f->data[size_t(y)*f->stride+x*2+1]; }
int main(int argc,char** argv) {
    try {
        if(argc>1&&std::string(argv[1])=="--monitor-cadence") {
            sw::Engine engine;sw::Configuration config;engine.start(config);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
            sw::Snapshot initial;
            do {std::this_thread::sleep_for(std::chrono::milliseconds(100));initial=engine.snapshot();}
            while(initial.error.empty()&&initial.ticks<10&&std::chrono::steady_clock::now()<deadline);
            require(initial.running&&initial.error.empty(),"Synthetic engine did not start");
            for(unsigned interval:{1u,2u,4u,1u}) {
                engine.setMonitorInterval(interval);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                const auto before=engine.snapshot();std::this_thread::sleep_for(std::chrono::seconds(2));const auto after=engine.snapshot();
                const auto ticks=after.ticks-before.ticks,frames=after.monitorFrames-before.monitorFrames;
                require(after.running&&after.error.empty()&&ticks>20,"Engine stopped during cadence change");
                require(std::abs(double(frames)-double(ticks)/interval)<=2,"Monitor cadence did not follow selected interval");
                std::cout<<"interval="<<interval<<" ticks="<<ticks<<" monitor_frames="<<frames<<" measured_fps="<<after.monitorFps<<"\n";
            }
            engine.stop();return 0;
        }
        bool uhd=false,engineTest=false,receiveTest=false,aja=false,monitorTest=false;
        if(argc>2&&std::string(argv[1])=="--routing-local") {
            sw::Engine engine;sw::Configuration config;config.synthetic=false;config.mediaPath=argv[2];config.mediaLoop=true;
            engine.start(config);sw::Snapshot state;
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
            do {std::this_thread::sleep_for(std::chrono::milliseconds(100));state=engine.snapshot();}
            while(state.error.empty()&&(state.starting||state.monitors[4].rgba.empty()||!state.signal[3])&&std::chrono::steady_clock::now()<deadline);
            engine.stop();
            std::cerr<<"media="<<state.media.error<<" active="<<state.media.active<<" ready="<<state.media.ready<<" signal="<<state.signal[3]<<" ticks="<<state.ticks<<"\n";
            require(state.running&&state.error.empty(),state.error.empty()?"Optional routing did not start":state.error.c_str());
            require(!state.assigned[0]&&!state.assigned[1]&&!state.assigned[2]&&state.assigned[3],"Disabled inputs incorrectly assigned");
            require(!state.outputAssigned[0]&&!state.outputAssigned[1],"Disabled outputs incorrectly assigned");
            require(state.state.program.background==3&&state.state.preview.background==3,"First assigned input must initialize both buses");
            require(state.signal[3]&&!state.monitors[4].rgba.empty(),"Local-only routing did not render input");
            require(!engine.snapshot().running,"Optional routing failed to stop");
            std::cout<<"Single input / disabled SDI outputs / local monitor start-stop passed\n";return 0;
        }
        if(argc>1&&std::string(argv[1])=="--keyfill") {
            for(auto mode:{sw::Mode::Hd,sw::Mode::Uhd}) {
                const auto f=sw::Format::of(mode);sw::GpuCompositor cg;cg.initialize(f);
                std::array<sw::FramePtr,4> inputs;sw::Switcher routing;uint64_t tick=0;
                auto solid=[&](int source,int y) {
                    auto frame=std::make_shared<sw::Frame>();frame->format=f;frame->stride=f.width*2;frame->data.resize(f.bytes());frame->persistent=true;
                    for(size_t p=0;p<frame->data.size();p+=4){frame->data[p]=frame->data[p+2]=128;frame->data[p+1]=frame->data[p+3]=uint8_t(y);}
                    inputs[source]=frame;
                };
                solid(0,60);solid(1,60);solid(2,180);solid(3,16);
                sw::KeyFill key;key.enabled=true;key.fillSource=2;key.keySource=3;routing.setKeyFill(key);
                auto render=[&]() {sw::GpuResult out;for(int field=0;field<f.ticksPerFrame();++field)out=cg.render(inputs,routing.state(),tick++,true);return out;};
                auto check=[&](int expected,const char* message) {auto out=render();require(std::abs(int(luma(out.output[1],100,100))-expected)<=3,message);require(std::abs(int(luma(out.output[0],100,100))-60)<=3,"CG edit leaked to PGM");};
                check(60,"Black key must reveal background");solid(3,235);check(180,"White key must reveal Fill");
                solid(3,126);check(120,"Grey key must blend per pixel");
                key.invert=true;routing.setKeyFill(key);solid(3,16);check(180,"Inverted black key must reveal Fill");
                key.invert=false;key.opacity=.5f;routing.setKeyFill(key);solid(3,235);check(120,"CG opacity incorrect");
                key.opacity=1;key.premultiplied=true;routing.setKeyFill(key);solid(2,98);solid(3,126);check(120,"Premultiplied Fill must not be multiplied twice");
                key.invert=true;routing.setKeyFill(key);inputs[3].reset();check(60,"Missing inverted key must hide CG");
                solid(3,235);inputs[2].reset();check(60,"Missing Fill must hide CG");
                solid(2,180);key.invert=false;key.premultiplied=false;routing.setKeyFill(key);
                // Spatial matte: left black, right white, on both fields.
                auto matte=std::make_shared<sw::Frame>(*inputs[3]);
                for(int y=0;y<f.height;++y)for(int x=0;x<f.width;x+=2){auto p=size_t(y)*matte->stride+x*2;matte->data[p+1]=matte->data[p+3]=x<f.width/2?16:235;}
                inputs[3]=matte;auto split=render();
                for(int y:{100,101}) {require(std::abs(int(luma(split.output[1],100,y))-60)<=3,"Spatial black matte failed");require(std::abs(int(luma(split.output[1],f.width-100,y))-180)<=3,"Spatial white matte failed");}
                require(!split.monitors[5].rgba.empty(),"CG preview monitor missing");
                solid(3,235);routing.cut();auto cut=render();require(std::abs(int(luma(cut.output[0],100,100))-60)<=3,"CUT unexpectedly enabled CG");cut={};
                routing.setProgramCg(true);auto on=render();require(std::abs(int(luma(on.output[0],100,100))-180)<=3,"Independent CG ON failed");on={};
                solid(0,100);solid(3,126);routing.autoMix(4,f);
                for(int n=0;n<2*f.ticksPerFrame();++n)routing.advance();auto middle=render();
                require(std::abs(int(luma(middle.output[0],100,100))-130)<=4,"CG must remain above the halfway background dissolve");middle={};
                routing.setProgramCg(false);auto off=render();require(std::abs(int(luma(off.output[0],100,100))-80)<=3,"CG OFF during AUTO altered background mix");off={};
                routing.setProgramCg(true);auto again=render();require(std::abs(int(luma(again.output[0],100,100))-130)<=4,"CG ON during AUTO failed");again={};
                for(int n=0;n<2*f.ticksPerFrame();++n)routing.advance();auto mixed=render();
                require(std::abs(int(luma(mixed.output[0],100,100))-140)<=4,"AUTO completion changed CG layer");mixed={};
                routing.cut();auto held=render();require(std::abs(int(luma(held.output[0],100,100))-120)<=4,"CUT failed to retain CG");held={};
                routing.setProgramCg(false);auto clean=render();require(std::abs(int(luma(clean.output[0],100,100))-60)<=3,"CG OFF failed to reveal unchanged background");
                std::cout<<"CG Key/Fill pixel checks passed: "<<f.label()<<"\n";
            }
            return 0;
        }
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
