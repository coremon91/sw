#pragma once
#include "core.hpp"
namespace sw {
struct Image { int width=0,height=0; std::vector<uint8_t> rgba; };
struct GpuResult { std::array<FramePtr,2> output; std::array<Image,6> monitors; RenderState outputState; };
class GpuCompositor {
public:
    GpuCompositor();
    ~GpuCompositor();
    void initialize(Format,bool pipelined=false);
    GpuResult render(const std::array<FramePtr,4>&,const RenderState&,uint64_t tick,bool monitors);
    std::string adapter() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
