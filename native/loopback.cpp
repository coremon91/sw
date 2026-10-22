#include "io.hpp"
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <fstream>
namespace {
// A marker identifies our output bus and frame number in the received SDI raster.
uint64_t marker(unsigned bus,uint32_t sequence) { return (uint64_t(0xD39A)<<48)|(uint64_t(bus)<<40)|(uint64_t(sequence)<<8)|uint8_t(sequence^(sequence>>8)^(sequence>>16)^(sequence>>24)^bus^0xA7); }
void stamp(sw::Frame& f,unsigned bus,uint32_t sequence) {
    const auto code=marker(bus,sequence);
    for(int bit=0;bit<64;++bit)for(int y=16;y<48;++y)for(int x=16+bit*24;x<16+(bit+1)*24;x+=2) {
        auto* p=f.data.data()+size_t(y)*f.stride+x*2;p[0]=p[2]=128;p[1]=p[3]=(code&(uint64_t(1)<<(63-bit)))?235:16;
    }
}
bool decode(const sw::Frame& f,unsigned& bus,uint32_t& sequence) {
    uint64_t code=0;for(int bit=0;bit<64;++bit)code=(code<<1)|(f.data[size_t(32)*f.stride+(16+bit*24+12)*2+1]>128?1:0);
    bus=unsigned((code>>40)&255);sequence=uint32_t(code>>8);return bus<2&&code==marker(bus,sequence);
}
struct Received { uint64_t frames=0,valid=0,gaps=0,repeats=0;uint32_t last=0;int bus=-1;double seconds=0;std::chrono::steady_clock::time_point first;sw::FramePtr image; };
}
int main(int argc,char** argv) {
    int seconds=12,outputCount=1;sw::Mode mode=sw::Mode::Uhd;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];if(arg=="--hd")mode=sw::Mode::Hd;
        else if(arg=="--seconds"&&i+1<argc)seconds=std::clamp(std::stoi(argv[++i]),3,60);
        else if(arg=="--outputs"&&i+1<argc)outputCount=std::clamp(std::stoi(argv[++i]),1,2);
        else if(arg!="--uhd") { std::cerr<<"Usage: sw_loopback [--uhd|--hd] [--seconds 3..60] [--outputs 1|2]\n";return 1; }
    }
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);timeBeginPeriod(1);
    std::array<std::unique_ptr<sw::Input>,4> inputs;std::array<std::unique_ptr<sw::Output>,2> outputs;
    std::mutex mutex;std::array<Received,4> received;std::array<sw::IoStats,2> outputStats;int code=1;
    try {
        if(FAILED(com))throw std::runtime_error("COM initialization failed");
        std::vector<std::string> warnings;auto devices=sw::probeDevices(warnings);std::vector<sw::Endpoint> captures,playouts;
        for(const auto& d:devices) {if(d.backend=="decklink"&&d.input)captures.push_back(d);if(d.backend=="aja"&&d.output)playouts.push_back(d);}
        if(captures.size()<4||playouts.size()<size_t(outputCount))throw std::runtime_error("Loopback requires four DeckLink endpoints and the requested AJA output channels.");
        auto format=sw::Format::of(mode);
        for(int i=0;i<4;++i) {
            inputs[i]=sw::makeInput(captures[i]);inputs[i]->start(captures[i],format,[&,i](sw::FramePtr f) {
                unsigned bus;uint32_t number;bool valid=decode(*f,bus,number);std::lock_guard lock(mutex);auto& r=received[i];++r.frames;
                if(valid) { if(r.valid) {if(number==r.last)++r.repeats;else if(number>r.last+1)r.gaps+=number-r.last-1;}else r.first=f->captured;
                    ++r.valid;r.last=number;r.bus=int(bus);r.seconds=std::chrono::duration<double>(f->captured-r.first).count();r.image=f; }
            });
        }
        for(int i=0;i<outputCount;++i) {outputs[i]=sw::makeOutput(playouts[i]);outputs[i]->start(playouts[i],format);}
        std::cout<<"Sending "<<format.label()<<" from AJA SDI 1"<<(outputCount==2?" + 2":"")<<"; receiving all four DeckLink endpoints for "<<seconds<<" seconds.\n"<<std::flush;
        sw::FramePool pool(16);auto base=std::chrono::steady_clock::now();uint64_t count=0;
        const uint64_t limit=uint64_t(seconds)*format.timeScale/format.frameDuration;
        for(;count<limit;++count) {
            for(int bus=0;bus<outputCount;++bus) {
                auto f=pool.acquire(format);if(!f)throw std::runtime_error("Loopback frame pool exhausted");sw::fillPattern(*f,bus,count);stamp(*f,bus,uint32_t(count));f->sequence=count;
                outputs[bus]->submit(f);auto stats=outputs[bus]->stats();if(!stats.error.empty())throw std::runtime_error(stats.error);
            }
            for(auto& input:inputs) {auto stats=input->stats();if(!stats.error.empty())throw std::runtime_error(stats.error);}
            std::this_thread::sleep_until(base+sw::tickTime((count+1)*format.ticksPerFrame()));
        }
        for(int i=0;i<outputCount;++i)outputStats[i]=outputs[i]->stats();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        for(auto& input:inputs)input->stop();
        std::lock_guard lock(mutex);std::array<bool,2> busMatched{};
        std::cout<<"{\"mode\":\""<<format.label()<<"\",\"submitted\":"<<count<<",\"inputs\":[";
        for(int i=0;i<4;++i) {const auto& r=received[i];if(i)std::cout<<',';
            std::cout<<"{\"endpoint\":\""<<captures[i].id<<"\",\"received\":"<<r.frames<<",\"marker_matches\":"<<r.valid<<",\"aja_bus\":"<<r.bus<<",\"gaps\":"<<r.gaps<<",\"repeats\":"<<r.repeats<<",\"receive_fps\":"<<(r.seconds>0?double(r.valid-1)/r.seconds:0)<<"}";if(r.bus>=0&&r.bus<outputCount&&r.valid>100)busMatched[r.bus]=true;
        }
        std::cout<<"],\"outputs\":[";for(int i=0;i<outputCount;++i) {if(i)std::cout<<',';std::cout<<"{\"transferred\":"<<outputStats[i].frames<<",\"dropped\":"<<outputStats[i].dropped<<"}";}
        bool matched=true;for(int i=0;i<outputCount;++i)matched=matched&&busMatched[i];
        std::cout<<"],\"signal_verified\":"<<(matched?"true":"false")<<"}\n";code=matched?0:2;
    } catch(const std::exception& e) {std::cerr<<"Loopback stopped: "<<e.what()<<"\n";}
    for(auto& in:inputs)if(in)in->stop();for(auto& out:outputs)if(out)out->stop();inputs={};outputs={};
    timeEndPeriod(1);if(SUCCEEDED(com))CoUninitialize();return code;
}
