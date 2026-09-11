#include "core.hpp"
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace sw {
std::chrono::nanoseconds tickTime(uint64_t tick) {
    // Split to avoid multiplying an unbounded tick count by a billion.
    return std::chrono::nanoseconds((tick / 60000) * 1001000000000LL + (tick % 60000) * 1001000000000LL / 60000);
}
uint32_t audioSamples(uint64_t frame, const Format& f) {
    const uint64_t numerator = uint64_t(48000) * f.frameDuration;
    const auto phase = frame % uint64_t(f.timeScale);
    return uint32_t(((phase + 1) * numerator) / f.timeScale - (phase * numerator) / f.timeScale);
}
uint64_t audioSampleTime(uint64_t frame,const Format& f) {
    const uint64_t n=48000ULL*f.frameDuration;
    return (frame/uint64_t(f.timeScale))*n+(frame%uint64_t(f.timeScale))*n/uint64_t(f.timeScale);
}
uint64_t recoverOutputFrame(uint64_t planned,int64_t hardwareTime,const Format& f) {
    if(hardwareTime<0)return planned;
    const uint64_t current=uint64_t(hardwareTime)/uint64_t(f.frameDuration);
    // A missed deadline must not leave all subsequent audio packets in the past.
    return planned<=current+1?current+4:planned;
}
double audioPeakDb(const std::vector<int32_t>& samples) {
    int64_t peak=0;for(auto s:samples)peak=std::max(peak,std::abs(int64_t(s)));
    return peak?std::max(-120.,20*std::log10(double(peak)/2147483648.)):-120.;
}
void AudioQueue::push(const std::vector<int32_t>& samples) {
    data_.insert(data_.end(),samples.begin(),samples.begin()+samples.size()/2*2);
    while(data_.size()>9600) {data_.pop_front();data_.pop_front();++overflowFrames;}
}
std::vector<int32_t> AudioQueue::take(uint32_t frames) {
    std::vector<int32_t> result(size_t(frames)*2,0);
    // Retain 20 ms of input jitter reserve before beginning/recovering playback.
    if(!primed_) {if(bufferedFrames()<frames+960)return result;primed_=true;}
    if(bufferedFrames()<frames) {++underruns;primed_=false;return result;}
    for(auto& s:result) {s=data_.front();data_.pop_front();}return result;
}
void Dve::sanitize() {
    auto finite = [](float n,float fallback) { return std::isfinite(n) ? n : fallback; };
    source = std::clamp(source,0,3);
    x = std::clamp(finite(x,0),-1.f,1.f); y = std::clamp(finite(y,0),-1.f,1.f);
    width = std::clamp(finite(width,.3f),.02f,2.f); height = std::clamp(finite(height,.3f),.02f,2.f);
    cropLeft = std::clamp(finite(cropLeft,0),0.f,.95f);
    cropRight = std::clamp(finite(cropRight,0),0.f,.95f-cropLeft);
    cropTop = std::clamp(finite(cropTop,0),0.f,.95f);
    cropBottom = std::clamp(finite(cropBottom,0),0.f,.95f-cropTop);
    opacity = std::clamp(finite(opacity,1),0.f,1.f);
    rotation = std::remainder(finite(rotation,0),360.f);
    border = std::clamp(finite(border,0),0.f,.05f);
}
Switcher::Switcher() { reset(); }
void Switcher::reset() { state_={}; state_.preview.background=1; elapsed_=duration_=0; }
bool Switcher::preview(int source) {
    if(source<0||source>3||state_.transitioning) return false;
    state_.preview.background=source; return true;
}
bool Switcher::setDve(Dve dve) {
    if(state_.transitioning) return false;
    dve.sanitize(); state_.preview.dve=dve; return true;
}
bool Switcher::cut() {
    if(state_.transitioning) return false;
    std::swap(state_.program,state_.preview); return true;
}
bool Switcher::autoMix(uint32_t frames,const Format& f) {
    if(state_.transitioning||frames==0||frames>600) return false;
    state_.transitionFrom=state_.program; state_.transitionTo=state_.preview;
    state_.transitioning=true; state_.mix=0;
    elapsed_=0; duration_=uint64_t(frames)*f.ticksPerFrame(); return true;
}
void Switcher::advance() {
    if(!state_.transitioning) return;
    ++elapsed_;
    state_.mix=std::min(1.f,float(elapsed_)/float(duration_));
    if(elapsed_>=duration_) {
        state_.program=state_.transitionTo; state_.preview=state_.transitionFrom;
        state_.transitioning=false; state_.mix=0;
    }
}
FramePool::FramePool(size_t slots) { for(size_t i=0;i<slots;++i) frames_.push_back(std::make_shared<Frame>()); }
std::shared_ptr<Frame> FramePool::acquire(const Format& format) {
    std::lock_guard lock(mutex_);
    for(auto& frame:frames_) if(frame.use_count()==1) {
        frame->format=format; frame->stride=format.width*2;
        frame->data.resize(format.bytes()); frame->audio.clear();
        return frame;
    }
    return {};
}
void Mailbox::publish(FramePtr frame) { std::lock_guard lock(mutex_); latest_=std::move(frame); }
FramePtr Mailbox::latest() const { std::lock_guard lock(mutex_); return latest_; }
void Mailbox::clear() { std::lock_guard lock(mutex_); latest_.reset(); }
void fillPattern(Frame& frame,int source,uint64_t tick) {
    static const uint8_t bars[8][3]={{235,128,128},{219,16,138},{188,154,16},{173,42,26},{78,214,230},{63,102,240},{32,240,118},{16,128,128}};
    const int w=frame.format.width,h=frame.format.height;
    const int position=int((tick*8+source*137)%uint64_t(w));
    for(int x=0;x<w;x+=2) {
        const auto& c=bars[((x*8/w)+source)%8];
        auto* p=frame.data.data()+x*2;
        const bool marker=std::abs(x-position)<24;
        p[0]=marker?128:c[1]; p[1]=marker?235:c[0]; p[2]=marker?128:c[2]; p[3]=marker?235:c[0];
    }
    for(int y=1;y<h;++y) std::memcpy(frame.data.data()+size_t(y)*frame.stride,frame.data.data(),size_t(w)*2);
    frame.sequence=tick; frame.captured=std::chrono::steady_clock::now();
}
std::string jsonEscape(const std::string& s) {
    std::ostringstream out;
    for(unsigned char c:s) {
        if(c=='"'||c=='\\') out<<'\\'<<c;
        else if(c<32) out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c);
        else out<<c;
    }
    return out.str();
}
}
