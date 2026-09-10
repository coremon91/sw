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
    std::array<sw::Endpoint,4> receive;
    auto rejectReceive=[&] {try {sw::validateReceiveRouting(receive,sw::Mode::Uhd);}catch(const std::runtime_error&){return;}throw std::runtime_error("Invalid receive-only routing accepted");};
    rejectReceive();
    receive[2]={"decklink:0","DeckLink","decklink","",0,0,true,true,true,true};
    sw::validateReceiveRouting(receive,sw::Mode::Uhd); // One input, no output required, any slot.
    receive[0]=receive[2];rejectReceive();receive[0]={};
    receive[2].backend="aja";rejectReceive();receive[2].backend="decklink";
    receive[2].uhd=false;rejectReceive();receive[2].uhd=true;
    receive[2].input=false;rejectReceive();
    std::cout<<"Duplicate ports, unsupported format and wrong direction rejected.\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
