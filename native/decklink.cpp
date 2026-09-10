#include "io.hpp"
#include <stdexcept>
#ifdef SW_HAS_DECKLINK
#include <windows.h>
#include <wrl/client.h>
#include "decklink_import.hpp"
#include <cstring>
#include <thread>
#include <deque>

namespace sw {
using Microsoft::WRL::ComPtr;
static void bcheck(HRESULT r,const char* what) { if(FAILED(r)) throw std::runtime_error(std::string("DeckLink: ")+what+" (HRESULT "+std::to_string(uint32_t(r))+")"); }
struct ComSession {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComSession() { if(FAILED(result)&&result!=RPC_E_CHANGED_MODE) bcheck(result,"COM initialization"); }
    ~ComSession() { if(SUCCEEDED(result)) CoUninitialize(); }
};
static auto bmode(Format f) { return f.interlaced?bmd::bmdModeHD1080i5994:bmd::bmdMode4K2160p5994; }
static std::vector<ComPtr<bmd::IDeckLink>> enumerate() {
    ComPtr<bmd::IDeckLinkIterator> iterator;
    bcheck(CoCreateInstance(__uuidof(bmd::CDeckLinkIterator),nullptr,CLSCTX_ALL,__uuidof(bmd::IDeckLinkIterator),reinterpret_cast<void**>(iterator.GetAddressOf())),"create device iterator");
    std::vector<ComPtr<bmd::IDeckLink>> result;
    for(;;) { ComPtr<bmd::IDeckLink> item;if(iterator->Next(&item)!=S_OK) break;result.push_back(item); }
    return result;
}
static ComPtr<bmd::IDeckLink> select(int index) {
    auto cards=enumerate();if(index<0||index>=int(cards.size())) throw std::runtime_error("DeckLink endpoint disappeared; refresh devices.");return cards[index];
}
static std::string utf8(BSTR text) {
    if(!text) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,text,SysStringLen(text),nullptr,0,nullptr,nullptr);std::string s(size_t(n),' ');
    WideCharToMultiByte(CP_UTF8,0,text,SysStringLen(text),s.data(),n,nullptr,nullptr);return s;
}
template<class T> static bool supports(T* port,Format f) {
    if(!port) return false;long supported=0;auto actual=bmode(f);
    // These enums have separate input/output types in the type library.
    HRESULT r;
    if constexpr(std::is_same_v<T,bmd::IDeckLinkInput>) r=port->DoesSupportVideoMode(bmd::bmdVideoConnectionSDI,bmode(f),bmd::bmdFormat8BitYUV,bmd::bmdNoVideoInputConversion,bmd::bmdSupportedVideoModeDefault,&actual,&supported);
    else r=port->DoesSupportVideoMode(bmd::bmdVideoConnectionSDI,bmode(f),bmd::bmdFormat8BitYUV,bmd::bmdNoVideoOutputConversion,bmd::bmdSupportedVideoModeDefault,&actual,&supported);
    return SUCCEEDED(r)&&supported&&actual==bmode(f);
}
std::vector<Endpoint> probeDeckLink(std::vector<std::string>& warnings) {
    std::vector<Endpoint> result;
    try {
        ComSession com;auto cards=enumerate();
        for(int i=0;i<int(cards.size());++i) {
            BSTR name=nullptr;cards[i]->GetDisplayName(&name);std::string label=utf8(name);SysFreeString(name);
            ComPtr<bmd::IDeckLinkProfileAttributes> attr;cards[i].As(&attr);
            __int64 duplex=0,io=0,sub=0;
            if(attr) { attr->GetInt(bmd::BMDDeckLinkDuplex,&duplex);attr->GetInt(bmd::BMDDeckLinkVideoIOSupport,&io);attr->GetInt(bmd::BMDDeckLinkNumberOfSubDevices,&sub); }
            if(duplex==bmd::bmdDuplexInactive) continue;
            ComPtr<bmd::IDeckLinkInput> input;ComPtr<bmd::IDeckLinkOutput> output;cards[i].As(&input);cards[i].As(&output);
            const bool cap=input&&(io&bmd::bmdDeviceSupportsCapture),play=output&&(io&bmd::bmdDeviceSupportsPlayback);
            // Capability is checked again for the selected direction immediately before starting.
            bool hd=(cap&&supports(input.Get(),Format::of(Mode::Hd)))||(play&&supports(output.Get(),Format::of(Mode::Hd)));
            bool uhd=(cap&&supports(input.Get(),Format::of(Mode::Uhd)))||(play&&supports(output.Get(),Format::of(Mode::Uhd)));
            ComPtr<bmd::IDeckLinkStatus> status;__int64 busy=0;std::string detail="subdevices="+std::to_string(sub)+"; duplex="+std::to_string(duplex);
            if(SUCCEEDED(cards[i].As(&status))&&SUCCEEDED(status->GetInt(bmd::bmdDeckLinkStatusBusy,&busy)))detail+="; capture="+std::string(busy&bmd::bmdDeviceCaptureBusy?"busy":"idle");
            result.push_back({"decklink:"+std::to_string(i),label,"decklink",detail,i,0,cap,play,uhd,hd});
        }
    } catch(const std::exception& e) { warnings.push_back(e.what()); }
    return result;
}
class Access {
    ComPtr<bmd::IDeckLinkVideoBuffer> buffer;
    bmd::_BMDBufferAccessFlags flags;
    bool started=false;
public:
    void* bytes=nullptr;
    Access(IUnknown* frame,bmd::_BMDBufferAccessFlags f):flags(f) {
        bcheck(frame->QueryInterface(__uuidof(bmd::IDeckLinkVideoBuffer),reinterpret_cast<void**>(buffer.GetAddressOf())),"get video buffer");
        bcheck(buffer->StartAccess(flags),"map video buffer");started=true;
        auto r=buffer->GetBytes(&bytes);if(FAILED(r)) { buffer->EndAccess(flags);started=false;bcheck(r,"get video bytes"); }
    }
    ~Access() { if(started) buffer->EndAccess(flags); }
};
class DeckInput final:public Input,public bmd::IDeckLinkInputCallback {
    ComPtr<bmd::IDeckLinkInput> input;
    ComPtr<bmd::IDeckLinkConfiguration> config;
    __int64 oldConnection=0;
    bool restoreConnection=false;
    bool videoEnabled=false,audioEnabled=false,callbackSet=false;
    std::atomic<ULONG> refs=1;
    std::atomic<bool> active=false;
    FramePool pool{6};FrameCallback callback;Format format;
    mutable std::mutex mutex;IoStats counters;
public:
    ~DeckInput() override { stop(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown||id==__uuidof(bmd::IDeckLinkInputCallback)) { *out=static_cast<bmd::IDeckLinkInputCallback*>(this);AddRef();return S_OK; }return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; } // Owner keeps the initial reference through StopStreams.
    void start(const Endpoint& e,Format f,FrameCallback cb) override {
        stop();format=f;callback=std::move(cb);counters={};
        auto card=select(e.device);bcheck(card.As(&input),"get input interface");
        ComPtr<bmd::IDeckLinkStatus> status;__int64 busy=0;
        if(SUCCEEDED(card.As(&status))&&SUCCEEDED(status->GetInt(bmd::bmdDeckLinkStatusBusy,&busy))&&(busy&bmd::bmdDeviceCaptureBusy)) {
            input.Reset();throw std::runtime_error("DeckLink input is already capturing. Stop the capture in the other application or SW window first.");
        }
        if(!supports(input.Get(),f)) { input.Reset();throw std::runtime_error("Selected DeckLink SDI input mode is unsupported in the current profile."); }
        try {
            if(SUCCEEDED(card.As(&config))) {
                restoreConnection=SUCCEEDED(config->GetInt(bmd::bmdDeckLinkConfigVideoInputConnection,&oldConnection));
                bcheck(config->SetInt(bmd::bmdDeckLinkConfigVideoInputConnection,bmd::bmdVideoConnectionSDI),"select SDI input");
            }
            bcheck(input->SetCallback(this),"set capture callback");
            callbackSet=true;
            bcheck(input->EnableVideoInput(bmode(f),bmd::bmdFormat8BitYUV,bmd::bmdVideoInputFlagDefault),"enable video input");
            videoEnabled=true;
            bcheck(input->EnableAudioInput(bmd::bmdAudioSampleRate48kHz,bmd::bmdAudioSampleType32bitInteger,2),"enable stereo audio input");
            audioEnabled=true;
            active=true;bcheck(input->StartStreams(),"start input streams");
        } catch(...) { stop();throw; }
    }
    void stop() noexcept override {
        active=false;if(input) { if(videoEnabled)input->StopStreams();if(callbackSet)input->SetCallback(nullptr);if(videoEnabled)input->DisableVideoInput();if(audioEnabled)input->DisableAudioInput(); }
        videoEnabled=audioEnabled=callbackSet=false;
        if(config&&restoreConnection) config->SetInt(bmd::bmdDeckLinkConfigVideoInputConnection,oldConnection);
        restoreConnection=false;config.Reset();input.Reset();
    }
    IoStats stats() const override { std::lock_guard lock(mutex);return counters; }
    HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(bmd::_BMDVideoInputFormatChangedEvents,bmd::IDeckLinkDisplayMode*,bmd::_BMDDetectedVideoInputFormatFlags) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(bmd::IDeckLinkVideoInputFrame* video,bmd::IDeckLinkAudioInputPacket* audio) override {
        if(!active||!video) return S_OK;
        try {
            if(video->GetFlags()&bmd::bmdFrameHasNoInputSource) { std::lock_guard lock(mutex);++counters.dropped;return S_OK; }
            if(video->GetWidth()!=format.width||video->GetHeight()!=format.height||video->GetPixelFormat()!=bmd::bmdFormat8BitYUV) throw std::runtime_error("Unexpected capture format. Stop and select the matching session mode.");
            auto frame=pool.acquire(format);if(!frame) { std::lock_guard lock(mutex);++counters.dropped;return S_OK; }
            { Access access(video,bmd::bmdBufferAccessRead);for(int y=0;y<format.height;++y) std::memcpy(frame->data.data()+size_t(y)*frame->stride,(uint8_t*)access.bytes+size_t(y)*video->GetRowBytes(),frame->stride); }
            if(audio) { void* bytes=nullptr;bcheck(audio->GetBytes(&bytes),"get audio bytes");const long count=audio->GetSampleFrameCount();if(bytes&&count>0&&count<8192) frame->audio.assign((int32_t*)bytes,(int32_t*)bytes+count*2); }
            { std::lock_guard lock(mutex);frame->sequence=counters.frames++; }
            frame->captured=std::chrono::steady_clock::now();callback(frame);
        } catch(const std::exception& e) { std::lock_guard lock(mutex);counters.error=e.what(); }
        return S_OK;
    }
};
class DeckOutput final:public Output,public bmd::IDeckLinkVideoOutputCallback {
    ComPtr<bmd::IDeckLinkOutput> output;
    std::atomic<ULONG> refs=1;
    mutable std::mutex mutex;
    std::vector<ComPtr<bmd::IDeckLinkMutableVideoFrame>> buffers;
    std::deque<bmd::IDeckLinkMutableVideoFrame*> free;
    IoStats counters;Format format;uint64_t scheduled=0,audioTime=0;
    bool active=false,playback=false;
public:
    ~DeckOutput() override { stop(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==__uuidof(bmd::IDeckLinkVideoOutputCallback)) { *out=static_cast<bmd::IDeckLinkVideoOutputCallback*>(this);AddRef();return S_OK; }return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    void start(const Endpoint& e,Format f) override {
        stop();format=f;counters={};scheduled=audioTime=0;
        auto card=select(e.device);bcheck(card.As(&output),"get output interface");
        if(!supports(output.Get(),f)) { output.Reset();throw std::runtime_error("Selected DeckLink SDI output mode is unsupported in the current profile."); }
        try {
            bcheck(output->EnableVideoOutput(bmode(f),bmd::bmdVideoOutputFlagDefault),"enable output");
            bcheck(output->EnableAudioOutput(bmd::bmdAudioSampleRate48kHz,bmd::bmdAudioSampleType32bitInteger,2,bmd::bmdAudioOutputStreamTimestamped),"enable audio output");
            bcheck(output->BeginAudioPreroll(),"begin audio preroll");
            bcheck(output->SetScheduledFrameCompletionCallback(this),"set output callback");
            int rowBytes=0;bcheck(output->RowBytesForPixelFormat(bmd::bmdFormat8BitYUV,f.width,&rowBytes),"query output stride");
            for(int i=0;i<5;++i) { ComPtr<bmd::IDeckLinkMutableVideoFrame> b;bcheck(output->CreateVideoFrame(f.width,f.height,rowBytes,bmd::bmdFormat8BitYUV,bmd::bmdFrameFlagDefault,&b),"create output frame");free.push_back(b.Get());buffers.push_back(b); }
            active=true;
        } catch(...) { stop();throw; }
    }
    bool submit(FramePtr f) override {
        bmd::IDeckLinkMutableVideoFrame* buffer=nullptr;
        { std::lock_guard lock(mutex);if(!active||free.empty()) { ++counters.dropped;return false; }buffer=free.front();free.pop_front(); }
        try {
            { Access access(buffer,bmd::bmdBufferAccessWrite);for(int y=0;y<format.height;++y) std::memcpy((uint8_t*)access.bytes+size_t(y)*buffer->GetRowBytes(),f->data.data()+size_t(y)*f->stride,f->stride); }
            bcheck(output->ScheduleVideoFrame(buffer,scheduled*format.frameDuration,format.frameDuration,format.timeScale),"schedule output frame");
            const unsigned count=audioSamples(scheduled,format);std::vector<int32_t> samples(size_t(count)*2,0);
            std::copy_n(f->audio.begin(),std::min(samples.size(),f->audio.size()),samples.begin());
            unsigned int written=0;bcheck(output->ScheduleAudioSamples(samples.data(),count,audioTime,48000,&written),"schedule audio");
            if(written!=count) throw std::runtime_error("DeckLink audio scheduler accepted only part of a packet.");
            audioTime+=written;
            ++scheduled;
            if(!playback&&scheduled>=3) { bcheck(output->EndAudioPreroll(),"end audio preroll");bcheck(output->StartScheduledPlayback(0,format.timeScale,1),"start scheduled playback");playback=true; }
            return true;
        } catch(const std::exception& e) { std::lock_guard lock(mutex);counters.error=e.what();return false; }
    }
    HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(bmd::IDeckLinkVideoFrame* frame,bmd::_BMDOutputFrameCompletionResult result) override {
        std::lock_guard lock(mutex);
        if(result!=bmd::bmdOutputFrameCompleted&&result!=bmd::bmdOutputFrameFlushed) ++counters.dropped;
        if(result!=bmd::bmdOutputFrameFlushed) ++counters.frames;
        for(auto& b:buffers) if(static_cast<bmd::IDeckLinkVideoFrame*>(b.Get())==frame) { free.push_back(b.Get());break; }return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped() override { return S_OK; }
    void stop() noexcept override {
        { std::lock_guard lock(mutex);active=false; }
        if(output) { __int64 stopped=0;output->StopScheduledPlayback(0,&stopped,format.timeScale);output->SetScheduledFrameCompletionCallback(nullptr);output->DisableAudioOutput();output->DisableVideoOutput(); }
        playback=false;output.Reset();std::lock_guard lock(mutex);free.clear();buffers.clear();
    }
    IoStats stats() const override { std::lock_guard lock(mutex);return counters; }
};
std::unique_ptr<Input> deckLinkInput() { return std::make_unique<DeckInput>(); }
std::unique_ptr<Output> deckLinkOutput() { return std::make_unique<DeckOutput>(); }
}
#else
namespace sw {
std::vector<Endpoint> probeDeckLink(std::vector<std::string>& warnings) { warnings.push_back("DeckLink Desktop Video type library was not available when this build was configured.");return {}; }
std::unique_ptr<Input> deckLinkInput() { throw std::runtime_error("DeckLink adapter not built"); }
std::unique_ptr<Output> deckLinkOutput() { throw std::runtime_error("DeckLink adapter not built"); }
}
#endif
