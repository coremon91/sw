#pragma once
#include "core.hpp"
#include <atomic>
#include <condition_variable>
#include <thread>

namespace sw {
struct MediaStatus {
    bool active=false,ready=false,playing=false,loop=false,eof=false;
    double position=0,duration=0,decodeMs=0;
    double videoMs=0,convertMs=0,audioMs=0;
    bool hardware=false;
    size_t buffered=0;
    uint64_t frames=0,underruns=0;
    std::string error;
};
struct MediaSample { FramePtr video; std::vector<int32_t> audio; };
// Decode ahead on a bounded worker; only the switcher clock consumes frames.
class MediaPlayer {
public:
    ~MediaPlayer();
    void open(std::string utf8Path,Format,bool loop=false);
    void close();
    void play();
    void pause();
    void cue();
    void seek(double seconds);
    void setLoop(bool);
    MediaStatus status() const;
    MediaSample pull(uint32_t outputSamples);
private:
    void decode(std::string,Format);
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    std::atomic<bool> quit_{false};
    MediaStatus status_;
    std::deque<FramePtr> queue_;
    FramePtr held_;
    std::deque<int32_t> pcm_;
    double seek_=0;
    uint64_t generation_=0;
};
}
