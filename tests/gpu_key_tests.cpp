#include "gpu.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace sw;
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static FramePtr grey(Format f,int y,bool stale=false) {
    auto frame=std::make_shared<Frame>();frame->format=f;frame->stride=f.width*2;
    frame->captured=std::chrono::steady_clock::now()-(stale?std::chrono::seconds(2):std::chrono::seconds(0));
    frame->persistent=!stale;frame->data.resize(f.bytes());
    for(size_t i=0;i<frame->data.size();i+=4) {frame->data[i]=128;frame->data[i+1]=uint8_t(y);frame->data[i+2]=128;frame->data[i+3]=uint8_t(y);}
    return frame;
}
int main() try {
    for(bool interlaced:{false,true})for(bool pipelined:{false,true}) {
        Format f{64,32,interlaced,1001,interlaced?30000:60000};GpuCompositor gpu;gpu.initialize(f,pipelined);
        std::array<FramePtr,4> frames{grey(f,60),grey(f,180),grey(f,126),grey(f,100)};
        RenderState state;state.preview.background=3;state.dsk.preview=true;state.dsk.premultiplied=false;
        uint64_t tick=0;
        RgbaFramePtr cg;
        auto render=[&] {GpuResult r;for(int i=0;i<4*f.ticksPerFrame();++i)r=gpu.render(frames,state,tick++,true,true,cg);return r;};
        auto check=[&](int pgm,int pvw,const char* message) {
            auto r=render();for(int bus=0;bus<2;++bus) {
                require(bool(r.output[bus]),"GPU output missing");
                // Verify both fields, not only the first output scanline.
                for(int row=0;row<f.height;++row)require(std::abs(int(r.output[bus]->data[size_t(row)*f.width*2+1])-(bus?pvw:pgm))<=3,message);
            }
            return r;
        };
        auto preview=check(60,140,"preview key must not leak to PGM");
        require(std::abs(int(preview.monitors[0].rgba[0])-51)<=3,"input monitor must remain unkeyed");
        state.dsk.program=true;check(120,140,"straight-alpha half key");
        frames[2]=grey(f,16);check(60,100,"black key passes background");
        frames[2]=grey(f,235);check(180,180,"white key passes fill");
        state.dsk.invert=true;check(60,100,"inverted white key passes background");
        frames[2].reset();check(60,100,"missing inverted key must bypass overlay");
        frames[2]=grey(f,126,true);check(60,100,"stale key must bypass overlay");
        frames[2]=grey(f,126);state.dsk.invert=false;state.dsk.premultiplied=true;frames[1]=grey(f,98);
        check(120,140,"premultiplied fill must not be multiplied by alpha twice");
        state.dsk.opacity=.5f;check(90,120,"global opacity scales premultiplied fill and key together");
        frames[1].reset();check(60,100,"missing fill must bypass overlay");
        auto overlay=std::make_shared<RgbaFrame>();overlay->width=16;overlay->height=8;overlay->rgba.resize(16*8*4);
        for(size_t i=0;i<overlay->rgba.size();i+=4){overlay->rgba[i]=overlay->rgba[i+1]=overlay->rgba[i+2]=96;overlay->rgba[i+3]=128;}
        overlay->captured=std::chrono::steady_clock::now();cg=overlay;
        state.dsk.internal=true;state.dsk.opacity=1;frames[1].reset();frames[2].reset();
        check(120,140,"internal RGBA must composite without Fill/Key inputs");
        state.dsk.program=false;check(60,140,"internal PVW must not leak into PGM");
        state.dsk.program=true;state.dsk.opacity=0;check(60,100,"internal zero opacity");state.dsk.opacity=1;
        overlay=std::make_shared<RgbaFrame>(*overlay);overlay->captured=std::chrono::steady_clock::now()-std::chrono::seconds(2);cg=overlay;
        check(60,100,"stale internal CG must bypass overlay");
        overlay=std::make_shared<RgbaFrame>(*overlay);overlay->captured=std::chrono::steady_clock::now();overlay->rgba.resize(1);cg=overlay;
        check(60,100,"malformed RGBA must not read beyond the buffer");
        cg.reset();state.dsk.internal=false;state.dsk.luma=true;state.dsk.fill=0;state.dsk.key=0;
        state.dsk.premultiplied=false;state.dsk.program=true;state.dsk.preview=true;
        state.program.background=3;state.preview.background=3;
        frames[0]=grey(f,180);check(180,180,"single external Fill overlays internal player without Key input");
        frames[0]=grey(f,16);check(100,100,"single Fill black background is transparent");
        frames[0]=grey(f,180);state.dsk.program=false;check(100,180,"single Fill preview does not leak into PGM");
        state.dsk.program=true;frames[0]=grey(f,180,true);check(100,100,"stale single Fill bypasses overlay");
        frames[0].reset();check(100,100,"missing single Fill bypasses overlay");

    }
    // Use the actual HTML raster and UHD output sizes, including all four corners.
    for(bool pipelined:{false,true}) {
        const auto f=Format::of(Mode::Uhd);GpuCompositor gpu;gpu.initialize(f,pipelined);
        std::array<FramePtr,4> frames{grey(f,60),nullptr,nullptr,nullptr};
        auto overlay=std::make_shared<RgbaFrame>();overlay->width=1920;overlay->height=1080;
        overlay->rgba.resize(size_t(1920)*1080*4);
        for(int y=0;y<1080;++y)for(int x=0;x<1920;++x) {
            const bool corner=(x<16||x>=1904)&&(y<16||y>=1064);
            const bool title=x>=100&&x<700&&y>=800&&y<900;
            const size_t p=(size_t(y)*1920+x)*4;
            if(corner||title){overlay->rgba[p]=overlay->rgba[p+1]=overlay->rgba[p+2]=corner?255:96;overlay->rgba[p+3]=corner?255:128;}
        }
        RenderState state;state.dsk.internal=true;state.dsk.program=true;
        uint64_t tick=0;
        auto checkUhd=[&] {
            GpuResult r;
            for(int i=0;i<3;++i){overlay->captured=std::chrono::steady_clock::now();r=gpu.render(frames,state,tick++,false,true,overlay);}
            for(int bus=0;bus<2;++bus) {
                const auto& out=r.output[bus];require(bool(out),"UHD output missing");
                require(out->format.width==3840&&out->format.height==2160&&!out->format.interlaced,"Wrong UHD raster");
                require(out->data.size()==f.bytes(),"Wrong UHD output buffer size");
                const bool keyed=bus?state.dsk.preview:state.dsk.program;
                auto pixel=[&](int x,int y,int expected){require(std::abs(int(out->data[size_t(y)*out->stride+x*2+1])-expected)<=3,"UHD CG position, scale or alpha mismatch");};
                for(int y:{8,2152})for(int x:{8,3832})pixel(x,y,keyed?235:60);
                // 100,800..700,900 in HTML must become 200,1600..1400,1800 in UHD.
                pixel(204,1604,keyed?120:60);pixel(1396,1796,keyed?120:60);
                pixel(196,1604,60);pixel(1404,1796,60);pixel(204,1596,60);pixel(1396,1804,60);
                pixel(1920,1080,60);
            }
        };
        checkUhd();state.dsk.program=false;state.dsk.preview=true;checkUhd();
        state.dsk.preview=false;checkUhd();
    }
    std::cout<<"GPU Fill/Key and internal RGBA: progressive/interlaced, direct/pipelined, buses, opacity and signal loss passed.\n";
    std::cout<<"UHD 3840x2160: 1920x1080 CG scaled 2x, four corners, alpha, PGM/PVW isolation and OFF passed (direct/pipelined).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
