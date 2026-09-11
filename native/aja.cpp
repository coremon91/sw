#include "io.hpp"
#include "ntv2card.h"
#include "ntv2devicescanner.h"
#include "ntv2signalrouter.h"
#include "ntv2utils.h"
#include "ntv2formatdescriptor.h"
#include <windows.h>
#include <deque>
#include <map>
#include <thread>
#include <stdexcept>
#include <cstring>

namespace sw {
static constexpr ULWord signature=0x53574D58; // SWMX
static void ajaCheck(bool ok,const char* what) { if(!ok) throw std::runtime_error(std::string("AJA: ")+what); }
static NTV2VideoFormat ajaFormat(Format f) { return f.interlaced?NTV2_FORMAT_1080i_5994:NTV2_FORMAT_3840x2160p_5994; }
struct SavedChannel {
    NTV2Channel channel; NTV2VideoFormat format; NTV2FrameBufferFormat pixels; NTV2Mode mode;
    NTV2VANCMode vanc; NTV2Standard standard; NTV2AudioSystem outputAudio;
    bool transmit=false, twelveG=false, tsi=false, squares=false;
};
struct SavedAudio {
    NTV2AudioSystem system; ULWord channels; NTV2AudioRate rate; NTV2AudioBufferSize size;
    NTV2AudioLoopBack loopback; NTV2AudioSource source; NTV2EmbeddedAudioInput embedded;
};
struct AjaBoard {
    CNTV2Card card;
    std::mutex mutex;
    CNTV2SignalRouter routing;
    NTV2TaskMode task=NTV2_TASK_MODE_INVALID;
    bool multiformat=false;
    NTV2ChannelSet enabled;
    std::vector<SavedChannel> saved;
    std::vector<SavedAudio> audio;
    bool changed=false;
    explicit AjaBoard(int index):card(UWord(index)) {
        ajaCheck(card.IsOpen(),"open card failed");
        ajaCheck(card.AcquireStreamForApplication(signature,int32_t(GetCurrentProcessId())),"card is in use by another application; stop that application first");
        try {
            ajaCheck(card.GetTaskMode(task),"read task mode");
            ajaCheck(card.GetRouting(routing),"save signal routing");
            if(card.features().CanDoMultiFormat()) ajaCheck(card.GetMultiFormatMode(multiformat),"save multi-format mode");
            ajaCheck(card.GetEnabledChannels(enabled),"save enabled channels");
            for(UWord i=0;i<card.features().GetNumFrameStores();++i) {
                SavedChannel s{};s.channel=NTV2Channel(i);
                ajaCheck(card.GetVideoFormat(s.format,s.channel)&&card.GetFrameBufferFormat(s.channel,s.pixels)&&card.GetMode(s.channel,s.mode)&&card.GetVANCMode(s.vanc,s.channel),"save frame store settings");
                if(card.features().CanDo12gRouting()) ajaCheck(card.GetTsiFrameEnable(s.tsi,s.channel)&&card.Get4kSquaresEnable(s.squares,s.channel),"save UHD raster layout");
                if(i<card.features().GetNumVideoOutputs()) {
                    ajaCheck(card.GetSDITransmitEnable(s.channel,s.transmit)&&card.GetSDIOutputStandard(i,s.standard)&&card.GetSDIOutputAudioSystem(s.channel,s.outputAudio),"save SDI settings");
                    if(card.features().CanDo12gRouting()) ajaCheck(card.GetSDIOut12GEnable(s.channel,s.twelveG),"save 12G setting");
                }
                saved.push_back(s);
            }
            for(UWord i=0;i<card.features().GetNumAudioSystems();++i) {
                SavedAudio a{};a.system=NTV2AudioSystem(i);
                ajaCheck(card.GetNumberAudioChannels(a.channels,a.system)&&card.GetAudioRate(a.rate,a.system)&&card.GetAudioBufferSize(a.size,a.system)&&card.GetAudioLoopBack(a.loopback,a.system)&&card.GetAudioSystemInputSource(a.system,a.source,a.embedded),"save audio settings");audio.push_back(a);
            }
            changed=true;
            ajaCheck(card.SetTaskMode(NTV2_OEM_TASKS),"set application task mode");
            if(card.features().CanDoMultiFormat()) ajaCheck(card.SetMultiFormatMode(true),"enable independent frame stores");
        } catch(...) { restore();card.ReleaseStreamForApplication(signature,int32_t(GetCurrentProcessId()));throw; }
    }
    void restore() noexcept {
        if(!changed) return;
        for(const auto& s:saved) {
            card.SetVideoFormat(s.format,false,false,s.channel);card.SetFrameBufferFormat(s.channel,s.pixels);
            card.SetMode(s.channel,s.mode);card.SetVANCMode(s.vanc,s.channel);
            if(card.features().CanDo12gRouting()) { card.Set4kSquaresEnable(s.squares,s.channel);card.SetTsiFrameEnable(s.tsi,s.channel); }
            if(unsigned(s.channel)<card.features().GetNumVideoOutputs()) {
                card.SetSDITransmitEnable(s.channel,s.transmit);card.SetSDIOutputStandard(s.channel,s.standard);card.SetSDIOutputAudioSystem(s.channel,s.outputAudio);
                if(card.features().CanDo12gRouting()) card.SetSDIOut12GEnable(s.channel,s.twelveG);
            }
            if(enabled.count(s.channel)) card.EnableChannel(s.channel);else card.DisableChannel(s.channel);
        }
        for(const auto& a:audio) { card.SetNumberAudioChannels(a.channels,a.system);card.SetAudioRate(a.rate,a.system);card.SetAudioBufferSize(a.size,a.system);card.SetAudioLoopBack(a.loopback,a.system);card.SetAudioSystemInputSource(a.system,a.source,a.embedded); }
        card.ApplySignalRoute(routing,true);
        if(card.features().CanDoMultiFormat()) card.SetMultiFormatMode(multiformat);
        if(task!=NTV2_TASK_MODE_INVALID) card.SetTaskMode(task);
    }
    ~AjaBoard() {
        restore();
        card.ReleaseStreamForApplication(signature,int32_t(GetCurrentProcessId()));
    }
};
static std::mutex boardsMutex;
static std::map<int,std::weak_ptr<AjaBoard>> boards;
static std::shared_ptr<AjaBoard> boardFor(int index) {
    std::lock_guard lock(boardsMutex);
    if(auto board=boards[index].lock()) return board;
    auto board=std::make_shared<AjaBoard>(index);boards[index]=board;return board;
}
std::vector<Endpoint> probeAja(std::vector<std::string>& warnings) {
    std::vector<Endpoint> out;
    for(ULWord index=0;index<32;++index) {
        CNTV2Card card;
        if(!CNTV2DeviceScanner::GetDeviceAtIndex(index,card)) break;
        auto& f=card.features();
        const int channels=std::min<int>(f.GetNumFrameStores(),std::max(f.GetNumVideoInputs(),f.GetNumVideoOutputs()));
        std::string detail="deviceID="+std::to_string(uint32_t(card.GetDeviceID()))+"; 12G routing="+(f.CanDo12gRouting()?"yes":"no");
        ULWord owner=0;int32_t pid=0;
        if(card.GetStreamingApplication(owner,pid))detail+="; ownerPID="+std::to_string(pid);
        if(!f.CanDo12gRouting()) detail+="; UHD independent channels require a supported 12G frame-store firmware; no firmware changes are automatic";
        for(int i=0;i<channels;++i) {
            std::string channelDetail=detail;bool transmit=false;
            if(card.GetSDITransmitEnable(NTV2Channel(i),transmit))channelDetail+="; direction="+std::string(transmit?"output":"input");
            if(!transmit&&i<f.GetNumVideoInputs()) {
                const auto detected=card.GetInputVideoFormat(NTV2InputSource(NTV2_INPUTSOURCE_SDI1+i));
                channelDetail+="; signal="+std::string(detected==NTV2_FORMAT_UNKNOWN?"unknown":"locked")+"; detected="+NTV2VideoFormatToString(detected);
            }
            out.push_back({"aja:"+std::to_string(index)+":"+std::to_string(i),card.GetDisplayName()+" / SDI "+std::to_string(i+1),"aja",channelDetail,int(index),i,i<f.GetNumVideoInputs(),i<f.GetNumVideoOutputs(),f.CanDo12gRouting()&&f.CanDoVideoFormat(NTV2_FORMAT_3840x2160p_5994),f.CanDoVideoFormat(NTV2_FORMAT_1080i_5994)});
        }
    }
    if(out.empty()) warnings.push_back("No AJA devices detected.");
    return out;
}
class AjaStream {
public:
    std::shared_ptr<AjaBoard> board;
    NTV2Channel channel=NTV2_CHANNEL1;
    NTV2AudioSystem audioSystem=NTV2_AUDIOSYSTEM_INVALID;
    Format format;
    FramePool pool{6};
    std::atomic<bool> running=false;
    std::thread worker;
    mutable std::mutex mutex;
    std::deque<FramePtr> queue;
    IoStats counters;
    bool capture=false,initialized=false,inputSubscribed=false;
    unsigned audioChannels=16;
    FrameCallback callback;
    ~AjaStream() { stop(); }
    void start(const Endpoint& endpoint,Format f,bool input,FrameCallback cb={}) {
        stop();capture=input;callback=std::move(cb);format=f;channel=NTV2Channel(endpoint.channel);counters={};
        board=boardFor(endpoint.device);
        try {
            std::lock_guard lock(board->mutex);auto& c=board->card;
            if(!f.interlaced&&!c.features().CanDo12gRouting()) throw std::runtime_error("AJA UHD adapter requires native 12G frame stores. Current firmware is not supported; use HD or a qualified firmware/configuration.");
            auto vf=ajaFormat(f);
            ajaCheck(c.features().CanDoVideoFormat(vf),"video format unsupported");
            ajaCheck(c.features().CanDoFrameBufferFormat(NTV2_FBF_8BIT_YCBCR),"UYVY buffers unsupported");
            AUTOCIRCULATE_STATUS existing;
            ajaCheck(c.AutoCirculateGetStatus(channel,existing),"read stream state");
            if(existing.IsRunning()) throw std::runtime_error("AJA channel is already running.");
            ajaCheck(c.SetSDITransmitEnable(channel,!capture),"set SDI direction");
            ajaCheck(c.EnableChannel(channel),"enable frame store");
            ajaCheck(c.SetVANCMode(NTV2_VANCMODE_OFF,channel),"disable VANC raster");
            ajaCheck(c.SetVideoFormat(vf,false,false,channel),"set video format");
            ajaCheck(c.SetFrameBufferFormat(channel,NTV2_FBF_8BIT_YCBCR),"set pixel format");
            ajaCheck(c.SetMode(channel,capture?NTV2_MODE_CAPTURE:NTV2_MODE_DISPLAY),"set frame store direction");
            if(capture) {
                ajaCheck(c.EnableInputInterrupt(channel),"enable capture interrupt");
                ajaCheck(c.SubscribeInputVerticalEvent(channel),"subscribe capture vertical event");inputSubscribed=true;
            }
            if(c.features().CanDo12gRouting()) {
                ajaCheck(c.Set4kSquaresEnable(false,channel)&&c.SetTsiFrameEnable(!f.interlaced,channel),"set native 12G raster layout");
                ajaCheck(c.SetSDIOut12GEnable(channel,!f.interlaced),"configure 12G output");
            }
            if(capture) ajaCheck(c.Connect(GetFrameStoreInputXptFromChannel(channel),GetInputSourceOutputXpt(NTV2InputSource(NTV2_INPUTSOURCE_SDI1+endpoint.channel))),"route capture");
            else {
                ajaCheck(c.Connect(GetSDIOutputInputXpt(channel),GetFrameStoreOutputXptFromChannel(channel,false,false)),"route output");
                ajaCheck(c.SetSDIOutputStandard(channel,GetNTV2StandardFromVideoFormat(vf)),"set SDI standard");
            }
            audioSystem=NTV2AudioSystem(endpoint.channel);
            if(unsigned(endpoint.channel)>=c.features().GetNumAudioSystems()) audioSystem=NTV2_AUDIOSYSTEM_INVALID;
            if(audioSystem!=NTV2_AUDIOSYSTEM_INVALID) {
                audioChannels=std::min<unsigned>(16,c.features().GetMaxAudioChannels());
                ajaCheck(c.SetNumberAudioChannels(audioChannels,audioSystem),"set audio channels");
                ajaCheck(c.SetAudioRate(NTV2_AUDIO_48K,audioSystem),"set audio rate");
                ajaCheck(c.SetAudioBufferSize(NTV2_AUDIO_BUFFER_BIG,audioSystem),"set audio buffer");
                ajaCheck(c.SetAudioLoopBack(NTV2_AUDIO_LOOPBACK_OFF,audioSystem),"disable audio loopback");
                if(capture) ajaCheck(c.SetAudioSystemInputSource(audioSystem,NTV2_AUDIO_EMBEDDED,NTV2EmbeddedAudioInput(endpoint.channel)),"select embedded audio");
                else ajaCheck(c.SetSDIOutputAudioSystem(channel,audioSystem),"route embedded audio");
            }
            ajaCheck(capture?c.AutoCirculateInitForInput(channel,4,audioSystem):c.AutoCirculateInitForOutput(channel,4,audioSystem),"initialize four-frame AutoCirculate queue");
            initialized=true;
            if(capture) ajaCheck(c.AutoCirculateStart(channel),"start capture");
            running=true;worker=std::thread([this] { run(); });
        } catch(...) { stop();throw; }
    }
    bool submit(FramePtr frame) {
        std::lock_guard lock(mutex);
        if(!running) return false;
        if(queue.size()>=2) { ++counters.dropped;return false; }
        queue.push_back(std::move(frame));return true;
    }
    void run() noexcept {
        try {
            uint64_t count=0,lastHardwareDrops=0;
            std::vector<int32_t> audio(4096*audioChannels);
            while(running) {
                AUTOCIRCULATE_STATUS status;
                { std::lock_guard lock(board->mutex);ajaCheck(board->card.AutoCirculateGetStatus(channel,status),"read AutoCirculate status"); }
                const uint64_t hardwareDrops=status.GetDroppedFrameCount();
                { std::lock_guard lock(mutex);if(hardwareDrops>=lastHardwareDrops)counters.dropped+=hardwareDrops-lastHardwareDrops; }lastHardwareDrops=hardwareDrops;
                if(capture) {
                    if(!status.HasAvailableInputFrame()) { std::this_thread::sleep_for(std::chrono::milliseconds(2));continue; }
                    bool validInput=false;
                    {std::lock_guard lock(board->mutex);validInput=board->card.GetInputVideoFormat(NTV2InputSource(NTV2_INPUTSOURCE_SDI1+int(channel)),!format.interlaced)==ajaFormat(format);}
                    if(!validInput) {
                        // Drain unavailable inputs without copying an entire UHD raster to RAM.
                        AUTOCIRCULATE_TRANSFER discard;
                        {std::lock_guard lock(board->mutex);ajaCheck(board->card.AutoCirculateTransfer(channel,discard),"discard no-signal frame");}
                        std::lock_guard lock(mutex);++counters.noSignal;counters.audioPeak=-120.;continue;
                    }
                    auto frame=pool.acquire(format);
                    if(!frame) { std::lock_guard lock(mutex);++counters.dropped;std::this_thread::yield();continue; }
                    AUTOCIRCULATE_TRANSFER transfer;
                    transfer.SetVideoBuffer(reinterpret_cast<ULWord*>(frame->data.data()),ULWord(frame->data.size()));
                    if(audioSystem!=NTV2_AUDIOSYSTEM_INVALID) transfer.SetAudioBuffer(reinterpret_cast<ULWord*>(audio.data()),ULWord(audio.size()*4));
                    { std::lock_guard lock(board->mutex);ajaCheck(board->card.AutoCirculateTransfer(channel,transfer),"capture DMA"); }
                    { std::lock_guard lock(board->mutex);
                      if(board->card.GetInputVideoFormat(NTV2InputSource(NTV2_INPUTSOURCE_SDI1+int(channel)),!format.interlaced)!=ajaFormat(format)) {std::lock_guard statsLock(mutex);++counters.noSignal;counters.audioPeak=-120.;continue;} }
                    if(audioSystem!=NTV2_AUDIOSYSTEM_INVALID) {
                        size_t samples=std::min<size_t>(transfer.acTransferStatus.GetCapturedAudioByteCount()/4/audioChannels,audio.size()/audioChannels);
                        frame->audio.resize(samples*2);
                        for(size_t i=0;i<samples;++i) { frame->audio[i*2]=audio[i*audioChannels];frame->audio[i*2+1]=audio[i*audioChannels+1]; }
                    }
                    frame->sequence=count++;frame->captured=std::chrono::steady_clock::now();callback(frame);
                    std::lock_guard lock(mutex);++counters.frames;counters.audioFrames+=frame->audio.size()/2;counters.audioPeak=audioPeakDb(frame->audio);
                } else {
                    FramePtr frame;
                    if(status.CanAcceptMoreOutputFrames()) { std::lock_guard lock(mutex);if(!queue.empty()) { frame=queue.front();queue.pop_front(); } }
                    if(!frame) { std::this_thread::sleep_for(std::chrono::milliseconds(1));continue; }
                    AUTOCIRCULATE_TRANSFER transfer;
                    transfer.SetVideoBuffer(reinterpret_cast<ULWord*>(const_cast<uint8_t*>(frame->data.data())),ULWord(frame->data.size()));
                    if(audioSystem!=NTV2_AUDIOSYSTEM_INVALID) {
                        const size_t samples=audioSamples(frame->sequence,format);
                        std::fill(audio.begin(),audio.end(),0);
                        for(size_t i=0;i<samples&&i*2+1<frame->audio.size();++i) { audio[i*audioChannels]=frame->audio[i*2];audio[i*audioChannels+1]=frame->audio[i*2+1]; }
                        transfer.SetAudioBuffer(reinterpret_cast<ULWord*>(audio.data()),ULWord(samples*audioChannels*4));
                    }
                    { std::lock_guard lock(board->mutex);
                      ajaCheck(board->card.AutoCirculateTransfer(channel,transfer),"output DMA");
                      if(++count==3) ajaCheck(board->card.AutoCirculateStart(channel),"start pre-rolled output"); }
                    std::lock_guard lock(mutex);++counters.frames;
                }
            }
        } catch(const std::exception& e) { std::lock_guard lock(mutex);counters.error=e.what();running=false; }
    }
    void stop() noexcept {
        running=false;if(worker.joinable()) worker.join();
        if(board&&initialized) { std::lock_guard lock(board->mutex);board->card.AutoCirculateStop(channel); }
        if(board&&inputSubscribed) {std::lock_guard lock(board->mutex);board->card.UnsubscribeInputVerticalEvent(channel);}
        inputSubscribed=false;
        initialized=false;board.reset();std::lock_guard lock(mutex);queue.clear();
    }
    IoStats stats() const { std::lock_guard lock(mutex);return counters; }
};
class AjaInput final:public Input {
    AjaStream stream;
public:
    void start(const Endpoint& e,Format f,FrameCallback cb) override { stream.start(e,f,true,std::move(cb)); }
    void stop() noexcept override { stream.stop(); }
    IoStats stats() const override { return stream.stats(); }
};
class AjaOutput final:public Output {
    AjaStream stream;
public:
    void start(const Endpoint& e,Format f) override { stream.start(e,f,false); }
    bool submit(FramePtr f) override { return stream.submit(std::move(f)); }
    void stop() noexcept override { stream.stop(); }
    IoStats stats() const override { return stream.stats(); }
};
std::unique_ptr<Input> ajaInput() { return std::make_unique<AjaInput>(); }
std::unique_ptr<Output> ajaOutput() { return std::make_unique<AjaOutput>(); }
}
