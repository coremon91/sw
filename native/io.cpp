#include "io.hpp"
#include <set>
#include <stdexcept>
namespace sw {
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
    for(const auto& e:inputs) check(e,true);
    for(const auto& e:outputs) check(e,false);
}
}
