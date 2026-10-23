#include "engine.hpp"
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <avrt.h>
#include <cmath>
#include <limits>
namespace sw {
Engine::Engine() : stopEvent_(CreateEventW(nullptr,TRUE,FALSE,nullptr)) {}
Engine::~Engine() { stop();if(stopEvent_)CloseHandle(stopEvent_); }
void Engine::start(Configuration config) {
    stop();
    if(!config.mediaPath.empty()) {
        if(config.receiveOnly)throw std::runtime_error("Internal player is unavailable in card receive-only mode.");
        config.inputs[3]=mediaEndpoint();
    }else if(config.inputs[3].backend=="media")throw std::runtime_error("Select a player file before starting.");
    if(config.receiveOnly) {
        if(config.synthetic) throw std::runtime_error("Receive-only mode cannot use synthetic inputs.");
        validateReceiveRouting(config.inputs,config.mode,config.receiveBackend);
    } else if(!config.synthetic) validateRouting(config.inputs,config.outputs,config.mode);
    { std::lock_guard lock(mutex_); quit_=false;if(stopEvent_)ResetEvent(stopEvent_);format_=Format::of(config.mode);switcher_.reset();snapshot_={};snapshot_.starting=true;snapshot_.synthetic=config.synthetic;snapshot_.receiveOnly=config.receiveOnly;
      for(int i=0;i<4;++i)snapshot_.assigned[i]=config.synthetic||!config.inputs[i].id.empty();
      for(int i=0;i<2;++i)snapshot_.outputAssigned[i]=!config.synthetic&&!config.receiveOnly&&!config.outputs[i].id.empty();
      if(!config.synthetic)for(int i=0;i<4;++i)if(snapshot_.assigned[i]) {switcher_.preview(i);switcher_.cut();switcher_.preview(i);break;}
      snapshot_.state=switcher_.state(); }
    worker_=std::thread([this,config] { run(config); });
}
void Engine::stop() {
    { std::lock_guard lock(mutex_);quit_=true;if(stopEvent_)SetEvent(stopEvent_); }wake_.notify_all();
    if(worker_.joinable()) worker_.join();
}
Snapshot Engine::snapshot() const { std::lock_guard lock(mutex_);auto s=snapshot_;s.media=player_.status();return s; }
void Engine::selectPreview(int i) { std::lock_guard lock(mutex_);switcher_.preview(i); }
void Engine::setDve(Dve d) { std::lock_guard lock(mutex_);switcher_.setDve(d); }
void Engine::setKeyFill(KeyFill k) { std::lock_guard lock(mutex_);switcher_.setKeyFill(k); }
void Engine::setProgramCg(bool enabled) { std::lock_guard lock(mutex_);switcher_.setProgramCg(enabled); }
void Engine::cut() { std::lock_guard lock(mutex_);switcher_.cut(); }
void Engine::autoMix(unsigned n) { std::lock_guard lock(mutex_);switcher_.autoMix(n,format_); }
void Engine::setMuted(bool b) { std::lock_guard lock(mutex_);muted_=b; }
void Engine::run(Configuration config) {
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    // Keep the requested timer resolution when Windows 11 occludes this app.
    // Only control timer resolution; do not change execution-speed QoS here.
    PROCESS_POWER_THROTTLING_STATE policy{};
    policy.Version=PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    policy.ControlMask=PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    policy.StateMask=0;
    const bool timerPolicyApplied=SetProcessInformation(GetCurrentProcess(),
        ProcessPowerThrottling,&policy,sizeof(policy))!=FALSE;
    const DWORD timerPolicyError=timerPolicyApplied?ERROR_SUCCESS:GetLastError();
    const auto timerResolutionResult=timeBeginPeriod(1);
    { std::lock_guard lock(mutex_);
      snapshot_.timerPolicyApplied=timerPolicyApplied;
      snapshot_.timerPolicyError=timerPolicyError;
      snapshot_.timerResolutionResult=timerResolutionResult; }
    DWORD mmcssTask=0;
    HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Playback",&mmcssTask);
    const DWORD mmcssError=mmcss?ERROR_SUCCESS:GetLastError();
    HANDLE waitTimer=stopEvent_?CreateWaitableTimerExW(nullptr,nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE):nullptr;
    const DWORD waitTimerError=waitTimer?ERROR_SUCCESS:stopEvent_?GetLastError():ERROR_INVALID_HANDLE;
    { std::lock_guard lock(mutex_);
      snapshot_.mmcssApplied=mmcss!=nullptr;snapshot_.mmcssError=mmcssError;
      snapshot_.highResolutionWait=waitTimer!=nullptr;snapshot_.waitTimerError=waitTimerError; }
    // Capture callbacks finish before these queues are destroyed.
    std::array<Mailbox,4> mail;
    std::mutex audioMutex;
    std::array<AudioQueue,4> audio;
    std::array<std::unique_ptr<Input>,4> inputs;
    std::array<std::unique_ptr<Output>,2> outputs;
    const auto format=Format::of(config.mode);
    const bool audioNeeded=!config.receiveOnly&&(config.synthetic||!config.outputs[0].id.empty()||!config.outputs[1].id.empty());
    try {
        if(FAILED(com)) throw std::runtime_error("Unable to initialize COM on video thread.");
        GpuCompositor gpu;gpu.initialize(format,true);
        const bool mediaEnabled=!config.mediaPath.empty();
        if(mediaEnabled)player_.open(config.mediaPath,format,config.mediaLoop);
        if(!config.synthetic) {
            for(int i=0;i<4;++i) {
                if(i==3&&mediaEnabled)continue;
                if(config.inputs[i].id.empty()) continue;
                inputs[i]=makeInput(config.inputs[i]);
                inputs[i]->start(config.inputs[i],format,[&,i](FramePtr f) {
                    if(audioNeeded) { std::lock_guard lock(audioMutex);audio[i].push(f->audio); }
                    mail[i].publish(std::move(f));
                });
            }
            if(!config.receiveOnly)for(int i=0;i<2;++i) if(!config.outputs[i].id.empty()) { outputs[i]=makeOutput(config.outputs[i]);outputs[i]->start(config.outputs[i],format); }
        }
        { std::lock_guard lock(mutex_);snapshot_.starting=false;snapshot_.running=true;snapshot_.adapter=gpu.adapter(); }
        const unsigned outputMask=config.receiveOnly?0u:config.synthetic?3u:
            (outputs[0]?1u:0u)|(outputs[1]?2u:0u);
        FramePool patterns{12};
        std::array<FramePtr,4> frames;
        std::deque<std::pair<uint64_t,std::vector<int32_t>>> mediaAudio;
        double mediaPeak=-120.;
        uint64_t tick=0,overruns=0,audioUnderruns=0;
        auto base=std::chrono::steady_clock::now();
        auto rateTime=base;std::array<uint64_t,4> previousCounts{};std::array<double,4> rates{};
        uint64_t monitorFrames=0,previousMonitorFrames=0;double monitorRate=0;
        std::array<FramePtr,4> lastShown;std::array<uint64_t,4> skipped{};
        while(true) {
            RenderState state;bool muted;
            { std::lock_guard lock(mutex_);if(quit_) break;state=switcher_.state();muted=muted_; }
            auto begin=std::chrono::steady_clock::now();
            for(int i=0;i<4;++i) {
                if(i==3&&mediaEnabled) {
                    if(tick%format.ticksPerFrame()==0) {
                        const auto sequence=tick/format.ticksPerFrame();
                        auto sample=player_.pull(audioSamples(sequence,format));frames[i]=sample.video;
                        mediaPeak=audioPeakDb(sample.audio);mediaAudio.emplace_back(sequence,std::move(sample.audio));
                        while(mediaAudio.size()>4)mediaAudio.pop_front();
                    }
                }else if(config.synthetic) {
                    if(tick%format.ticksPerFrame()==0) {
                        auto f=patterns.acquire(format);if(!f) throw std::runtime_error("Test source pool exhausted");
                        fillPattern(*f,i,tick);frames[i]=f;
                    }
                } else frames[i]=mail[i].latest();
            }
            // GUI cadence is independent of SDI processing; adjustable without restarting.
            const bool show=tick%monitorInterval_.load()==0;
            if(show)for(int i=0;i<4;++i)if(frames[i]&&frames[i]!=lastShown[i]) {
                if(lastShown[i]&&frames[i]->sequence>lastShown[i]->sequence+1)skipped[i]+=frames[i]->sequence-lastShown[i]->sequence-1;
                lastShown[i]=frames[i];
            }
            const auto gpuBegin=std::chrono::steady_clock::now();
            GpuResult result;
            if(!config.receiveOnly||show) result=gpu.render(frames,state,tick,show,outputMask!=0,outputMask);
            const auto gpuEnd=std::chrono::steady_clock::now();
            std::array<double,2> outputPeaks{-120.,-120.};
            const auto clockFrame=result.output[0]?result.output[0]:result.output[1];
            if(clockFrame) {
                const auto count=audioSamples(clockFrame->sequence,format);
                const auto& audioState=result.outputState;
                std::array<std::vector<int32_t>,4> samples;
                { std::lock_guard lock(audioMutex);
                  for(int i=0;i<4;++i) {
                    samples[i]=audio[i].take(count);
                  } }
                if(mediaEnabled) {
                    const auto seq=clockFrame->sequence;
                    auto found=std::find_if(mediaAudio.begin(),mediaAudio.end(),[&](const auto& a){return a.first==seq;});
                    if(found!=mediaAudio.end())samples[3]=found->second;
                }
                for(int bus=0;bus<2;++bus) {
                    if(!result.output[bus])continue;
                    // The pool created this mutable object; nobody else has received it yet.
                    auto f=std::const_pointer_cast<Frame>(result.output[bus]);f->audio.assign(size_t(count)*2,0);
                    if(!muted) for(size_t k=0;k<f->audio.size();++k) {
                        double sample=0;
                        if(bus==0&&audioState.transitioning) sample=(1.-audioState.mix)*samples[audioState.transitionFrom.background][k]+audioState.mix*double(samples[audioState.transitionTo.background][k]);
                        else sample=samples[bus==0?audioState.program.background:audioState.preview.background][k];
                        f->audio[k]=int32_t(std::clamp(sample,double(INT32_MIN),double(INT32_MAX)));
                    }
                    outputPeaks[bus]=audioPeakDb(f->audio);if(outputs[bus]) outputs[bus]->submit(f);
                }
            }
            std::array<IoStats,6> stats;
            for(int i=0;i<6;++i) {
                if(i<4&&inputs[i]) stats[i]=inputs[i]->stats();
                if(i>=4&&outputs[i-4]) stats[i]=outputs[i-4]->stats();
                if(!stats[i].error.empty()) throw std::runtime_error(stats[i].error);
            }
            if(mediaEnabled) {auto m=player_.status();stats[3].frames=m.frames;stats[3].dropped=m.underruns;stats[3].audioPeak=mediaPeak;}
            const auto end=std::chrono::steady_clock::now();
            std::array<uint32_t,4> audioBuffered{};uint64_t audioOverflow=0;audioUnderruns=0;
            {std::lock_guard lock(audioMutex);for(int i=0;i<4;++i) {audioBuffered[i]=uint32_t(audio[i].bufferedFrames());audioOverflow+=audio[i].overflowFrames;audioUnderruns+=audio[i].underruns;}}
            const bool newMonitor=!result.monitors[0].rgba.empty();if(newMonitor)++monitorFrames;
            const double rateSeconds=std::chrono::duration<double>(end-rateTime).count();
            if(rateSeconds>=1) {for(int i=0;i<4;++i) {rates[i]=double(stats[i].frames-previousCounts[i])/rateSeconds;previousCounts[i]=stats[i].frames;}monitorRate=double(monitorFrames-previousMonitorFrames)/rateSeconds;previousMonitorFrames=monitorFrames;rateTime=end;}
            auto next=base+tickTime(tick+1);
            if(end>next) {
                ++overruns;
                // Keep the absolute 59.94 Hz clock through ordinary scheduling
                // jitter. Moving the base on every miss permanently slows audio.
                if(end-next>tickTime(4)) {base+=end-next;next=end;}
            }
            { std::unique_lock lock(mutex_);
              snapshot_.state=state;snapshot_.ticks=tick+1;snapshot_.overruns=overruns;snapshot_.audioUnderruns=audioUnderruns;
              snapshot_.renderMs=std::chrono::duration<double,std::milli>(end-begin).count();snapshot_.io=stats;
              snapshot_.inputFps=rates;
              snapshot_.monitorFrames=monitorFrames;snapshot_.monitorFps=monitorRate;snapshot_.monitorSkipped=skipped;
              snapshot_.audioBuffered=audioBuffered;snapshot_.audioOverflowFrames=audioOverflow;snapshot_.muted=muted;
              if(clockFrame)snapshot_.outputAudioPeak=outputPeaks;
              snapshot_.sourceMs=std::chrono::duration<double,std::milli>(gpuBegin-begin).count();snapshot_.gpuMs=std::chrono::duration<double,std::milli>(gpuEnd-gpuBegin).count();
              for(int i=0;i<4;++i) snapshot_.signal[i]=frames[i]&&(frames[i]->persistent||end-frames[i]->captured<std::chrono::milliseconds(500));
              if(newMonitor) snapshot_.monitors=std::move(result.monitors);
              switcher_.advance();
              if(waitTimer&&!quit_) {
                  const auto remaining=std::chrono::duration_cast<std::chrono::nanoseconds>(
                      next-std::chrono::steady_clock::now()).count();
                  if(remaining>0) {
                      LARGE_INTEGER due{};due.QuadPart=-((remaining+99)/100);
                      if(SetWaitableTimer(waitTimer,&due,0,nullptr,nullptr,FALSE)) {
                          HANDLE handles[]={stopEvent_,waitTimer};
                          lock.unlock();
                          const auto result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
                          const DWORD waitError=result==WAIT_FAILED?GetLastError():ERROR_SUCCESS;
                          lock.lock();
                          if(waitError) {
                              snapshot_.waitTimerError=waitError;snapshot_.highResolutionWait=false;
                              CloseHandle(waitTimer);waitTimer=nullptr;
                              wake_.wait_until(lock,next,[&]{return quit_;});
                          }
                      } else {
                          snapshot_.waitTimerError=GetLastError();snapshot_.highResolutionWait=false;
                          CloseHandle(waitTimer);waitTimer=nullptr;
                          wake_.wait_until(lock,next,[&]{return quit_;});
                      }
                  }
              } else wake_.wait_until(lock,next,[&]{return quit_;}); }
            ++tick;
        }
    } catch(const std::exception& e) { std::lock_guard lock(mutex_);snapshot_.error=e.what(); }
    for(auto& in:inputs) if(in) in->stop();
    player_.close();
    for(auto& out:outputs) if(out) out->stop();
    inputs={};outputs={};
    if(waitTimer)CloseHandle(waitTimer);
    if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
    if(timerResolutionResult==TIMERR_NOERROR)timeEndPeriod(1);if(SUCCEEDED(com)) CoUninitialize();
    std::lock_guard lock(mutex_);snapshot_.running=snapshot_.starting=false;
}
}
