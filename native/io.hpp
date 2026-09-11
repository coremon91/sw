#pragma once
#include "core.hpp"
#include <atomic>
#include <functional>

namespace sw {
struct Endpoint {
    std::string id,label,backend,detail;
    int device=0, channel=0;
    bool input=false,output=false,uhd=false,hd=false;
};
struct IoStats {
    uint64_t frames=0,dropped=0; std::string error;
    uint64_t noSignal=0,audioFrames=0,timingRecoveries=0,audioPartialWrites=0;
    uint32_t bufferedAudioFrames=0;
    double audioPeak=-120.;
};
inline Endpoint mediaEndpoint(){return {"media:4","Internal player / IN 4","media","",0,0,true,false,true,true};}
using FrameCallback=std::function<void(FramePtr)>;
class Input {
public:
    virtual ~Input()=default;
    virtual void start(const Endpoint&,Format,FrameCallback)=0;
    virtual void stop() noexcept=0;
    virtual IoStats stats() const=0;
};
class Output {
public:
    virtual ~Output()=default;
    virtual void start(const Endpoint&,Format)=0;
    virtual bool submit(FramePtr)=0;
    virtual void stop() noexcept=0;
    virtual IoStats stats() const=0;
};
std::vector<Endpoint> probeDevices(std::vector<std::string>& warnings);
std::vector<Endpoint> probeReceiveDevices(const std::string& backend,std::vector<std::string>& warnings);
std::unique_ptr<Input> makeInput(const Endpoint&);
std::unique_ptr<Output> makeOutput(const Endpoint&);
std::vector<Endpoint> probeDeckLink(std::vector<std::string>&);
std::unique_ptr<Input> deckLinkInput();
std::unique_ptr<Output> deckLinkOutput();
#ifdef SW_HAS_AJA
std::vector<Endpoint> probeAja(std::vector<std::string>&);
std::unique_ptr<Input> ajaInput();
std::unique_ptr<Output> ajaOutput();
#endif
void validateRouting(const std::array<Endpoint,4>&,const std::array<Endpoint,2>&,Mode);
void validateReceiveRouting(const std::array<Endpoint,4>&,Mode,const std::string& backend="decklink");
}
