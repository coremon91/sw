#pragma once
#include "core.hpp"
namespace sw {
struct Image { int width=0,height=0; std::vector<uint8_t> rgba; };
struct RgbaFrame:Image {
    // RGBA8 with premultiplied alpha; monotonic timestamp for fail-open handling.
    std::chrono::steady_clock::time_point captured;
    uint64_t sequence=0;
};
using RgbaFramePtr=std::shared_ptr<const RgbaFrame>;
struct GpuResult { std::array<FramePtr,2> output; std::array<Image,6> monitors; RenderState outputState; };
class GpuCompositor {
public:
    GpuCompositor();
    ~GpuCompositor();
    void initialize(Format,bool pipelined=false);
    GpuResult render(const std::array<FramePtr,4>&,const RenderState&,uint64_t tick,bool monitors,bool produceOutputs=true,RgbaFramePtr cg={});
    std::string adapter() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
