#pragma once
#include "gpu.hpp"
#include "io.hpp"
#include "media.hpp"
#include <condition_variable>
#include <deque>
#include <thread>
namespace sw {
struct Configuration {
    Mode mode=Mode::Hd;
    bool synthetic=true;
    bool receiveOnly=false;
    std::string receiveBackend="decklink";
    std::string mediaPath;
    bool mediaLoop=false;
    std::array<Endpoint,4> inputs;
    std::array<Endpoint,2> outputs;
};
struct Snapshot {
    MediaStatus media;
    bool running=false, starting=false, synthetic=true;
    bool receiveOnly=false;
    std::array<bool,4> assigned{};
    std::array<double,4> inputFps{};
    std::array<uint64_t,4> monitorSkipped{};
    std::array<uint32_t,4> audioBuffered{};
    std::array<double,2> outputAudioPeak{-120.,-120.};
    bool muted=true;
    uint64_t audioOverflowFrames=0;
    RenderState state;
    std::array<Image,6> monitors;
    std::array<bool,4> signal{};
    std::array<IoStats,6> io;
    uint64_t ticks=0, overruns=0, audioUnderruns=0;
    uint64_t monitorFrames=0;
    double monitorFps=0;
    double renderMs=0, sourceMs=0, gpuMs=0;
    std::string adapter,error;
};
class Engine {
public:
    ~Engine();
    void start(Configuration);
    void stop();
    Snapshot snapshot() const;
    void selectPreview(int);
    void setDve(Dve);
    void cut();
    void autoMix(unsigned frames);
    void setMuted(bool);
    MediaPlayer& player(){return player_;}
private:
    MediaPlayer player_;
    void run(Configuration);
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool quit_=false, muted_=true;
    Format format_;
    Switcher switcher_;
    Snapshot snapshot_;
};
}
