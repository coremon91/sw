#include "io.hpp"
#include <set>
#include <stdexcept>
namespace sw {
std::vector<Endpoint> probeReceiveDevices(const std::string& backend,std::vector<std::string>& warnings) {
    if(backend=="decklink") return probeDeckLink(warnings);
#ifdef SW_HAS_AJA
    if(backend=="aja") return probeAja(warnings);
#endif
    warnings.push_back("Receive backend unavailable: "+backend);return {};
}
std::vector<Endpoint> probeDevices(std::vector<std::string>& warnings) {
    auto devices=probeDeckLink(warnings);
#ifdef SW_HAS_AJA
    auto aja=probeAja(warnings); devices.insert(devices.end(),aja.begin(),aja.end());
#else
    warnings.push_back("AJA SDK adapter was not included in this build.");
#endif
    return devices;
}
std::unique_ptr<Input> makeInput(const Endpoint& e) {
    if(e.backend=="decklink") return deckLinkInput();
#ifdef SW_HAS_AJA
    if(e.backend=="aja") return ajaInput();
#endif
    throw std::runtime_error("Input backend unavailable: "+e.backend);
}
std::unique_ptr<Output> makeOutput(const Endpoint& e) {
    if(e.backend=="decklink") return deckLinkOutput();
#ifdef SW_HAS_AJA
    if(e.backend=="aja") return ajaOutput();
#endif
    throw std::runtime_error("Output backend unavailable: "+e.backend);
}
void validateRouting(const std::array<Endpoint,4>& inputs,const std::array<Endpoint,2>& outputs,Mode mode) {
    std::set<std::string> used;
    auto check=[&](const Endpoint& e,bool input) {
        if(e.id.empty()||!(input?e.input:e.output)) throw std::runtime_error("Select a valid endpoint for every input and output.");
        if(!used.insert(e.id).second) throw std::runtime_error("A physical endpoint cannot be assigned twice: "+e.label);
        if(!(mode==Mode::Uhd?e.uhd:e.hd)) throw std::runtime_error("Selected format is unavailable on "+e.label+". "+e.detail);
    };
    for(size_t i=0;i<inputs.size();++i) {
        const auto& e=inputs[i];
        if(e.backend=="media"&&(i!=3||e.id!="media:4"))throw std::runtime_error("Internal player is available on INPUT 4 only.");
        check(e,true);
    }
    for(const auto& e:outputs) check(e,false);
}
void validateReceiveRouting(const std::array<Endpoint,4>& inputs,Mode mode,const std::string& backend) {
    if(backend!="decklink"&&backend!="aja") throw std::runtime_error("Unknown receive backend.");
    std::set<std::string> used;
    for(const auto& e:inputs) {
        if(e.id.empty()) continue;
        if(e.backend!=backend||!e.input) throw std::runtime_error("Select inputs from the chosen receive card only: "+backend);
        if(!used.insert(e.id).second) throw std::runtime_error("The same input cannot be assigned twice.");
        if(!(mode==Mode::Uhd?e.uhd:e.hd)) throw std::runtime_error("Selected input does not support the session format.");
    }
    if(used.empty()) throw std::runtime_error("Select at least one input from "+backend+".");
}
}
