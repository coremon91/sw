#include "io.hpp"
#include <iostream>
int main() {
    std::vector<std::string> warnings;auto devices=sw::probeDevices(warnings);
    std::cout<<"{\"devices\":[";bool first=true;
    for(const auto& d:devices) {
        if(!first) std::cout<<',';first=false;
        std::cout<<"{\"id\":\""<<sw::jsonEscape(d.id)<<"\",\"label\":\""<<sw::jsonEscape(d.label)<<"\",\"input\":"<<(d.input?"true":"false")<<",\"output\":"<<(d.output?"true":"false")<<",\"hd\":"<<(d.hd?"true":"false")<<",\"uhd\":"<<(d.uhd?"true":"false")<<",\"detail\":\""<<sw::jsonEscape(d.detail)<<"\"}";
    }
    std::cout<<"],\"warnings\":[";first=true;for(const auto& w:warnings) { if(!first) std::cout<<',';first=false;std::cout<<'"'<<sw::jsonEscape(w)<<'"'; }std::cout<<"]}\n";
    return devices.empty()?1:0;
}
