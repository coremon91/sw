#pragma once
#include "core.hpp"
#include <algorithm>
namespace sw {
// Cumulative totals allow interval averages to be recovered from sparse logs.
struct TimingMetric {
    double lastMs=0,totalMs=0,maxMs=0;
    uint64_t samples=0,over1Ms=0;
    void observe(double ms) {
        lastMs=std::max(0.,ms);totalMs+=lastMs;maxMs=std::max(maxMs,lastMs);
        ++samples;if(lastMs>1.)++over1Ms;
    }
};
struct InputSelectionMetric {
    uint64_t sequence=0,samples=0,repeats=0,skipped=0,resets=0;
    double ageMs=0;
    bool valid=false,history=false;
    void observe(const FramePtr& frame,std::chrono::steady_clock::time_point now,bool frameBoundary) {
        valid=frame&&!frame->persistent;
        if(!valid){history=false;ageMs=0;return;}
        ageMs=std::max(0.,std::chrono::duration<double,std::milli>(now-frame->captured).count());
        if(!frameBoundary)return; // Do not count the second interlaced field as a repeat.
        ++samples;
        if(history) {
            if(frame->sequence==sequence)++repeats;
            else if(frame->sequence>sequence)skipped+=frame->sequence-sequence-1;
            else ++resets;
        }
        sequence=frame->sequence;history=true;
    }
};
}
