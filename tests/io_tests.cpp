#include "io.hpp"
#include <stdexcept>
#include <iostream>
int main() try {
    std::array<sw::Endpoint,4> in;std::array<sw::Endpoint,2> out;
    for(int i=0;i<4;++i)in[i]={std::to_string(i),"input","test","",i,0,true,false,true,true};
    for(int i=0;i<2;++i)out[i]={std::to_string(i+4),"output","test","",i+4,0,false,true,true,true};
    sw::validateRouting(in,out,sw::Mode::Uhd);
    in[3]=sw::mediaEndpoint();sw::validateRouting(in,out,sw::Mode::Uhd);
    const auto first=in[0];in[0]=in[3];bool mediaRejected=false;
    try{sw::validateRouting(in,out,sw::Mode::Uhd);}catch(...){mediaRejected=true;}
    if(!mediaRejected)throw std::runtime_error("Player accepted outside INPUT 4");in[0]=first;
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
    receive[2]={"aja:0:0","KONA 5 SDI 1","aja","",0,0,true,true,true,true};
    sw::validateReceiveRouting(receive,sw::Mode::Uhd,"aja");
    sw::validateReceiveRouting(receive,sw::Mode::Hd,"aja");
    auto rejectAja=[&] {try {sw::validateReceiveRouting(receive,sw::Mode::Uhd,"aja");}catch(const std::runtime_error&){return;}throw std::runtime_error("Invalid KONA receive routing accepted");};
    receive[0]=receive[2];rejectAja();receive[0]={};
    receive[2].backend="decklink";rejectAja();receive[2].backend="aja";
    receive[2].uhd=false;rejectAja();receive[2].uhd=true;
    receive[2].input=false;rejectAja();receive[2]={};rejectAja();
    std::cout<<"Duplicate ports, unsupported format and wrong direction rejected.\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
