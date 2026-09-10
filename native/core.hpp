#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <deque>

namespace sw {
enum class Mode { Hd, Uhd };
struct Format {
    int width = 1920, height = 1080;
    bool interlaced = true;
    int64_t frameDuration = 1001, timeScale = 30000;
    static Format of(Mode mode) { return mode == Mode::Uhd ? Format{3840,2160,false,1001,60000} : Format{}; }
    size_t bytes() const { return size_t(width) * height * 2; }
    int ticksPerFrame() const { return interlaced ? 2 : 1; }
    std::string label() const { return interlaced ? "1080i29.97 (59.94 fields/s)" : "2160p59.94"; }
};
// All engine ticks represent a progressive frame or one interlaced field.
constexpr int64_t tickNumerator = 1001, tickDenominator = 60000;
std::chrono::nanoseconds tickTime(uint64_t tick);
uint32_t audioSamples(uint64_t frame, const Format& format);
uint64_t audioSampleTime(uint64_t frame,const Format& format);
uint64_t recoverOutputFrame(uint64_t planned,int64_t hardwareTime,const Format& format);
double audioPeakDb(const std::vector<int32_t>& samples);
class AudioQueue {
public:
    void push(const std::vector<int32_t>& samples);
    std::vector<int32_t> take(uint32_t frames);
    size_t bufferedFrames() const {return data_.size()/2;}
    uint64_t underruns=0,overflowFrames=0;
private:
    std::deque<int32_t> data_;
    bool primed_=false;
};

struct Dve {
    bool enabled = false;
    int source = 1;
    float x = .67f, y = .62f, width = .29f, height = .29f;
    float cropLeft = 0, cropTop = 0, cropRight = 0, cropBottom = 0;
    float rotation = 0, opacity = 1, border = .004f;
    void sanitize();
};
struct Scene { int background = 0; Dve dve; };
struct RenderState {
    Scene program, preview, transitionFrom, transitionTo;
    bool transitioning = false;
    float mix = 0;
};
class Switcher {
public:
    Switcher();
    const RenderState& state() const { return state_; }
    bool preview(int source);
    bool setDve(Dve dve);
    bool cut();
    bool autoMix(uint32_t outputFrames, const Format& format);
    void advance();
    void reset();
private:
    RenderState state_;
    uint64_t elapsed_ = 0, duration_ = 0;
};

struct Frame {
    Format format;
    int stride = 0;
    uint64_t sequence = 0;
    std::chrono::steady_clock::time_point captured;
    std::vector<uint8_t> data; // UYVY, limited-range Rec.709, 8-bit 4:2:2.
    std::vector<int32_t> audio; // Interleaved stereo, signed 32-bit / 48kHz.
};
using FramePtr = std::shared_ptr<const Frame>;
class FramePool {
public:
    explicit FramePool(size_t slots = 6);
    std::shared_ptr<Frame> acquire(const Format& format);
private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<Frame>> frames_;
};
class Mailbox {
public:
    void publish(FramePtr frame);
    FramePtr latest() const;
    void clear();
private:
    mutable std::mutex mutex_;
    FramePtr latest_;
};
void fillPattern(Frame& frame, int source, uint64_t tick);
std::string jsonEscape(const std::string& value);
}
