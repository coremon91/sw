#include "io.hpp"
#include <stdexcept>
#include <iostream>
int main() try {
    std::array<sw::Endpoint,4> in;std::array<sw::Endpoint,2> out;
    for(int i=0;i<4;++i)in[i]={std::to_string(i),"input","test","",i,0,true,false,true,true};
    for(int i=0;i<2;++i)out[i]={std::to_string(i+4),"output","test","",i+4,0,false,true,true,true};
    sw::validateRouting(in,out,sw::Mode::Uhd);
    auto rejected=[&] { try { sw::validateRouting(in,out,sw::Mode::Uhd); }catch(const std::runtime_error&) {return;}throw std::runtime_error("Invalid routing accepted"); };
    out[1].id=in[0].id;rejected();out[1].id="5";
    out[1].uhd=false;rejected();out[1].uhd=true;
    out[1].output=false;rejected();
    std::cout<<"Duplicate ports, unsupported format and wrong direction rejected.\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
