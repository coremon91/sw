#pragma once
#include "gpu.hpp"
#include "io.hpp"
#include <condition_variable>
#include <deque>
#include <thread>
namespace sw {
struct Configuration {
    Mode mode=Mode::Hd;
    bool synthetic=true;
    bool receiveOnly=false;
    std::array<Endpoint,4> inputs;
    std::array<Endpoint,2> outputs;
};
struct Snapshot {
    bool running=false, starting=false, synthetic=true;
    bool receiveOnly=false;
    std::array<bool,4> assigned{};
    std::array<double,4> inputFps{};
    RenderState state;
    std::array<Image,6> monitors;
    std::array<bool,4> signal{};
    std::array<IoStats,6> io;
    uint64_t ticks=0, overruns=0, audioUnderruns=0;
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
private:
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
