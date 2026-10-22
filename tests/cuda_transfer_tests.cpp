#include "gpu.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace sw;
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static int luma(FramePtr f,int x,int y) {return f->data[size_t(y)*f->stride+x*2+1];}
static void solid(Frame& f,int y) {
    for(size_t n=0;n<f.data.size();n+=4){f.data[n]=f.data[n+2]=128;f.data[n+1]=f.data[n+3]=uint8_t(y);}
    f.persistent=true;
}
int main(int argc,char** argv) {
    try {
        std::cout<<std::unitbuf;
        const bool cuda=argc<2||std::string(argv[1])!="--d3d11";
        _putenv_s("SW_GPU_TRANSFER",cuda?"cuda":"d3d11");
        // Deep copies cannot share pin registrations with a different allocation.
        VideoBuffer buffer;buffer.resize(8192);int released=0;
        buffer.retainPin(std::shared_ptr<void>(buffer.data(),[&](void*){++released;}));
        auto copy=buffer;require(!copy.pinned(),"Copied buffer inherited another allocation's registration");
        require(uintptr_t(buffer.data())%4096==0,"Video buffer is not page aligned");
        buffer.resize(16384);require(released==1&&!buffer.pinned(),"Resize retained stale registration");
        for(auto mode:{Mode::Hd,Mode::Uhd}) {
            auto format=Format::of(mode);GpuCompositor gpu;gpu.initialize(format,true);
            std::cout<<gpu.adapter()<<"\n";
            if(cuda)require(gpu.adapter().find("CUDA pinned transfer")!=std::string::npos,"CUDA was silently bypassed");
            FramePool pool(16);std::array<FramePtr,4> inputs;
            RenderState state;state.program.background=state.preview.background=0;
            state.program.keyFill.enabled=true;state.program.keyFill.fillSource=3;state.program.keyFill.keySource=2;
            state.preview=state.program;
            FramePtr retained;
            double total=0,worst=0;unsigned samples=0;
            for(int frame=0;frame<48;++frame) {
                for(int source:{0,2,3}) {
                    auto f=pool.acquire(format);require(bool(f),"Input pool exhausted");
                    solid(*f,source==0?60:source==3?180:16);
                    if(source==2) {
                        // Both fields carry the same moving matte for this frame.
                        const int edge=64+frame*16;
                        for(int y=0;y<format.height;++y)for(int x=edge;x<edge+64;x+=2) {
                            const size_t n=size_t(y)*f->stride+x*2;f->data[n+1]=f->data[n+3]=235;
                        }
                    }
                    inputs[source]=std::move(f);
                }
                GpuResult result;const auto start=std::chrono::steady_clock::now();
                for(int field=0;field<format.ticksPerFrame();++field)
                    result=gpu.render(inputs,state,uint64_t(frame*format.ticksPerFrame()+field),frame%4==0);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(frame>=8){total+=ms;worst=std::max(worst,ms);++samples;}
                if(frame==0){require(!result.output[0],"Pipeline did not prime");continue;}
                require(result.output[0]&&result.output[1],"Missing output");
                require(result.output[0]->sequence==uint64_t(frame-1),"Moving CG output reordered or skipped");
                const int edge=64+(frame-1)*16;
                for(int bus=0;bus<2;++bus)for(int y:{100,101}) {
                    require(std::abs(luma(result.output[bus],edge+24,y)-180)<=3,"Moving matte and Fill lost alignment");
                    require(std::abs(luma(result.output[bus],edge-8,y)-60)<=3,"Moving CG frame repeated or wrong field");
                }
                if(frame==1)retained=result.output[0];
                require(std::abs(luma(retained,88,100)-180)<=3,"Output memory reused before consumer release");
            }
            std::cout<<format.label()<<" moving CG 47 frames passed; transfer/composite mean="<<total/samples<<" ms max="<<worst<<" ms (includes competing GPU activity)\n";
        }
        // PVW-only output must still function; local-only rendering must not read back SDI frames.
        for(unsigned mask:{0u,1u,2u}) {
            const auto format=Format::of(Mode::Hd);GpuCompositor gpu;gpu.initialize(format,true);
            FramePool pool(2);auto f=pool.acquire(format);solid(*f,100);
            std::array<FramePtr,4> inputs;inputs[0]=f;RenderState state;
            GpuResult result;
            for(int t=0;t<6;++t)result=gpu.render(inputs,state,t,true,mask!=0,mask);
            require(bool(result.output[0])==bool(mask&1)&&bool(result.output[1])==bool(mask&2),"Disabled output produced a full frame or enabled bus is missing");
        }
        std::cout<<"Pin lifecycle, moving CG, retained buffers and output masks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
