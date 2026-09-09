#include "engine.hpp"
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <cmath>
#include <limits>
namespace sw {
Engine::~Engine() { stop(); }
void Engine::start(Configuration config) {
    stop();
    if(!config.synthetic) validateRouting(config.inputs,config.outputs,config.mode);
    { std::lock_guard lock(mutex_); quit_=false;format_=Format::of(config.mode);switcher_.reset();snapshot_={};snapshot_.starting=true;snapshot_.synthetic=config.synthetic; }
    worker_=std::thread([this,config] { run(config); });
}
void Engine::stop() {
    { std::lock_guard lock(mutex_);quit_=true; }wake_.notify_all();
    if(worker_.joinable()) worker_.join();
}
Snapshot Engine::snapshot() const { std::lock_guard lock(mutex_);return snapshot_; }
void Engine::selectPreview(int i) { std::lock_guard lock(mutex_);switcher_.preview(i); }
void Engine::setDve(Dve d) { std::lock_guard lock(mutex_);switcher_.setDve(d); }
void Engine::cut() { std::lock_guard lock(mutex_);switcher_.cut(); }
void Engine::autoMix(unsigned n) { std::lock_guard lock(mutex_);switcher_.autoMix(n,format_); }
void Engine::setMuted(bool b) { std::lock_guard lock(mutex_);muted_=b; }
void Engine::run(Configuration config) {
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    timeBeginPeriod(1);
    // Capture callbacks finish before these queues are destroyed.
    std::array<Mailbox,4> mail;
    std::mutex audioMutex;
    std::array<std::deque<int32_t>,4> audio;
    std::array<std::unique_ptr<Input>,4> inputs;
    std::array<std::unique_ptr<Output>,2> outputs;
    const auto format=Format::of(config.mode);
    try {
        if(FAILED(com)) throw std::runtime_error("Unable to initialize COM on video thread.");
        GpuCompositor gpu;gpu.initialize(format,true);
        if(!config.synthetic) {
            for(int i=0;i<4;++i) {
                inputs[i]=makeInput(config.inputs[i]);
                inputs[i]->start(config.inputs[i],format,[&,i](FramePtr f) {
                    { std::lock_guard lock(audioMutex);auto& q=audio[i];
                      q.insert(q.end(),f->audio.begin(),f->audio.end());
                      // Keep at most 100 ms. An unlocked source may drift; expose underruns.
                      while(q.size()>9600) { q.pop_front();q.pop_front(); } }
                    mail[i].publish(std::move(f));
                });
            }
            for(int i=0;i<2;++i) { outputs[i]=makeOutput(config.outputs[i]);outputs[i]->start(config.outputs[i],format); }
        }
        { std::lock_guard lock(mutex_);snapshot_.starting=false;snapshot_.running=true;snapshot_.adapter=gpu.adapter(); }
        FramePool patterns{12};
        std::array<FramePtr,4> frames;
        uint64_t tick=0,overruns=0,audioUnderruns=0;
        auto base=std::chrono::steady_clock::now();
        while(true) {
            RenderState state;bool muted;
            { std::lock_guard lock(mutex_);if(quit_) break;state=switcher_.state();muted=muted_; }
            auto begin=std::chrono::steady_clock::now();
            for(int i=0;i<4;++i) {
                if(config.synthetic) {
                    if(tick%format.ticksPerFrame()==0) {
                        auto f=patterns.acquire(format);if(!f) throw std::runtime_error("Test source pool exhausted");
                        fillPattern(*f,i,tick);frames[i]=f;
                    }
                } else frames[i]=mail[i].latest();
            }
            const bool show=tick%4==0;
            const auto gpuBegin=std::chrono::steady_clock::now();
            auto result=gpu.render(frames,state,tick,show);
            const auto gpuEnd=std::chrono::steady_clock::now();
            if(result.output[0]) {
                const auto count=audioSamples(result.output[0]->sequence,format);
                const auto& audioState=result.outputState;
                std::array<std::vector<int32_t>,4> samples;
                { std::lock_guard lock(audioMutex);
                  for(int i=0;i<4;++i) {
                    samples[i].resize(size_t(count)*2,0);auto& q=audio[i];
                    size_t n=std::min(q.size(),samples[i].size());n-=n%2;
                    for(size_t k=0;k<n;++k) { samples[i][k]=q.front();q.pop_front(); }
                    if(!config.synthetic&&n<samples[i].size()&&frames[i]) ++audioUnderruns;
                  } }
                for(int bus=0;bus<2;++bus) {
                    // The pool created this mutable object; nobody else has received it yet.
                    auto f=std::const_pointer_cast<Frame>(result.output[bus]);f->audio.resize(size_t(count)*2,0);
                    if(!muted) for(size_t k=0;k<f->audio.size();++k) {
                        double sample=0;
                        if(bus==0&&audioState.transitioning) sample=(1.-audioState.mix)*samples[audioState.transitionFrom.background][k]+audioState.mix*double(samples[audioState.transitionTo.background][k]);
                        else sample=samples[bus==0?audioState.program.background:audioState.preview.background][k];
                        f->audio[k]=int32_t(std::clamp(sample,double(INT32_MIN),double(INT32_MAX)));
                    }
                    if(outputs[bus]) outputs[bus]->submit(f);
                }
            }
            std::array<IoStats,6> stats;
            for(int i=0;i<6;++i) {
                if(i<4&&inputs[i]) stats[i]=inputs[i]->stats();
                if(i>=4&&outputs[i-4]) stats[i]=outputs[i-4]->stats();
                if(!stats[i].error.empty()) throw std::runtime_error(stats[i].error);
            }
            const auto end=std::chrono::steady_clock::now();
            auto next=base+tickTime(tick+1);
            if(end>next) { ++overruns;base+=end-next;next=end; }
            { std::unique_lock lock(mutex_);
              snapshot_.state=state;snapshot_.ticks=tick+1;snapshot_.overruns=overruns;snapshot_.audioUnderruns=audioUnderruns;
              snapshot_.renderMs=std::chrono::duration<double,std::milli>(end-begin).count();snapshot_.io=stats;
              snapshot_.sourceMs=std::chrono::duration<double,std::milli>(gpuBegin-begin).count();snapshot_.gpuMs=std::chrono::duration<double,std::milli>(gpuEnd-gpuBegin).count();
              for(int i=0;i<4;++i) snapshot_.signal[i]=frames[i]&&end-frames[i]->captured<std::chrono::milliseconds(500);
              if(show) snapshot_.monitors=std::move(result.monitors);
              switcher_.advance();wake_.wait_until(lock,next,[&]{return quit_;}); }
            ++tick;
        }
    } catch(const std::exception& e) { std::lock_guard lock(mutex_);snapshot_.error=e.what(); }
    for(auto& in:inputs) if(in) in->stop();
    for(auto& out:outputs) if(out) out->stop();
    inputs={};outputs={};
    timeEndPeriod(1);if(SUCCEEDED(com)) CoUninitialize();
    std::lock_guard lock(mutex_);snapshot_.running=snapshot_.starting=false;
}
}
