#include "timing_metrics.hpp"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    sw::InputSelectionMetric m;const auto now=std::chrono::steady_clock::now();
    auto f=std::make_shared<sw::Frame>();f->captured=now;f->sequence=10;
    m.observe(f,now,true);m.observe(f,now,false);
    check(m.samples==1&&m.repeats==0,"HD second field counted twice");
    m.observe(f,now,true);f->sequence=13;m.observe(f,now,true);
    check(m.samples==3&&m.repeats==1&&m.skipped==2,"Repeat/skip accounting");
    f->sequence=1;m.observe(f,now,true);check(m.resets==1&&m.skipped==2,"Sequence reset underflow");
    m.observe({},now,true);f->sequence=100;m.observe(f,now,true);
    check(m.skipped==2,"Reconnect treated as lost frames");
    f->persistent=true;m.observe(f,now,true);check(!m.valid&&m.samples==5,"Persistent frame counted");
    sw::TimingMetric t;t.observe(-1);t.observe(0.5);t.observe(2);
    check(t.samples==3&&t.totalMs==2.5&&t.maxMs==2&&t.over1Ms==1,"Timing aggregate");
    std::cout<<"Timing metrics passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
