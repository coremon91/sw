#include "gpu.hpp"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <stdexcept>

namespace sw {
using Microsoft::WRL::ComPtr;
static void hr(HRESULT r,const char* text) { if(FAILED(r)) throw std::runtime_error(std::string(text)+" (HRESULT "+std::to_string(uint32_t(r))+")"); }
static constexpr char shader[]=R"HLSL(
struct Scene { float4 rect; float4 crop; float4 fx; int4 ids; };
cbuffer Params : register(b0) { float4 sourceInfo[4]; Scene a; Scene b; float4 options; };
Texture2D<float4> s0:register(t0); Texture2D<float4> s1:register(t1);
Texture2D<float4> s2:register(t2); Texture2D<float4> s3:register(t3);
Texture2D<float4> composite:register(t4);
struct V { float4 position:SV_POSITION; float2 uv:TEXCOORD0; };
V vertex(uint id:SV_VertexID) {
    V o; o.uv=float2((id<<1)&2,id&2); o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1); return o;
}
float4 loadPacked(int source,int2 p) {
    if(source==0) return s0.Load(int3(p,0)); if(source==1) return s1.Load(int3(p,0));
    if(source==2) return s2.Load(int3(p,0)); return s3.Load(int3(p,0));
}
float3 pixel(int source,int2 p) {
    float4 info=sourceInfo[source]; p=clamp(p,int2(0,0),int2(info.xy)-1);
    if(info.z>0) p.y=min(int(info.y)-1,(p.y/2)*2+int(options.y));
    float4 c=loadPacked(source,int2(p.x/2,p.y))*255;
    float y=((p.x&1)==0?c.y:c.w)-16, u=c.x-128,v=c.z-128;
    return saturate(float3(y/219+1.5748*v/224,y/219-.187324*u/224-.468124*v/224,y/219+1.8556*u/224));
}
float3 sampleSource(int source,float2 uv) {
    if(sourceInfo[source].w<.5) return float3(.025,.03,.035);
    float2 p=saturate(uv)*sourceInfo[source].xy-.5;
    int2 q=int2(floor(p)); float2 f=frac(p);
    [branch] if(all(f<.001)) return pixel(source,q);
    return lerp(lerp(pixel(source,q),pixel(source,q+int2(1,0)),f.x),lerp(pixel(source,q+int2(0,1)),pixel(source,q+int2(1,1)),f.x),f.y);
}
float3 scene(Scene s,float2 uv) {
    float3 base=sampleSource(s.ids.x,uv);
    if(s.ids.z==0) return base;
    float2 p=uv-(s.rect.xy+s.rect.zw*.5);
    // Rotate in square-pixel space, not in stretched normalized coordinates.
    p.x*=16.0/9.0;
    float c=cos(s.fx.x),sn=sin(s.fx.x);
    p=float2(c*p.x+sn*p.y,-sn*p.x+c*p.y); p.x/=16.0/9.0;
    float2 local=p/s.rect.zw+.5;
    if(any(local<0)||any(local>1)) return base;
    float2 edge=min(local,1-local)*s.rect.zw;
    float3 over=(min(edge.x,edge.y)<s.fx.z) ? float3(.92,.95,.96) : sampleSource(s.ids.y,lerp(s.crop.xy,1-s.crop.zw,local));
    return lerp(base,over,s.fx.y);
}
float4 compose(V i):SV_TARGET {
    float3 color=scene(a,i.uv);
    [branch] if(options.x>0) color=lerp(color,scene(b,i.uv),options.x);
    return float4(color,1);
}
float3 yuv(float3 c) {
    return float3(16+219*dot(c,float3(.2126,.7152,.0722)),128+224*dot(c,float3(-.114572,-.385428,.5)),128+224*dot(c,float3(.5,-.454153,-.045847)))/255;
}
float4 pack(V i):SV_TARGET {
    int2 p=int2(i.position.xy); if(options.z>0 && (p.y&1)!=int(options.y)) discard;
    float3 c0=yuv(composite.Load(int3(p.x*2,p.y,0)).rgb),c1=yuv(composite.Load(int3(p.x*2+1,p.y,0)).rgb);
    return float4((c0.y+c1.y)*.5,c0.x,(c0.z+c1.z)*.5,c1.x);
}
)HLSL";
struct alignas(16) SceneParams { float rect[4],crop[4],fx[4]; int ids[4]; };
struct alignas(16) Params { float sourceInfo[4][4]; SceneParams a,b; float options[4]; };
static SceneParams params(const Scene& s) {
    const auto& d=s.dve;
    return {{d.x,d.y,d.width,d.height},{d.cropLeft,d.cropTop,d.cropRight,d.cropBottom},{d.rotation*3.1415926535f/180.f,d.opacity,d.border,0},{s.background,d.source,d.enabled?1:0,0}};
}
struct Surface {
    ComPtr<ID3D11Texture2D> texture,staging;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11ShaderResourceView> resource;
    int width=0,height=0;
};
struct GpuCompositor::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> compose,pack;
    ComPtr<ID3D11Buffer> constants;
    std::array<Surface,4> sources;
    std::array<Surface,2> scenes,packed;
    std::array<std::array<ComPtr<ID3D11Texture2D>,3>,2> readbacks;
    bool pipelined=false;
    uint64_t completed=0;
    RenderState previousState;
    Surface monitor;
    ComPtr<ID3D11Texture2D> previousMonitor;
    bool monitorReady=false;
    std::array<FramePtr,4> held;
    FramePool outputPool{10};
    Format format;
    Params p{};
    std::string adapterName;
    Surface create(int w,int h,bool render,bool stage) {
        Surface s; s.width=w; s.height=h;
        D3D11_TEXTURE2D_DESC d{}; d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|(render?D3D11_BIND_RENDER_TARGET:0);
        hr(device->CreateTexture2D(&d,nullptr,&s.texture),"Create GPU texture");
        hr(device->CreateShaderResourceView(s.texture.Get(),nullptr,&s.resource),"Create GPU resource view");
        if(render) hr(device->CreateRenderTargetView(s.texture.Get(),nullptr,&s.target),"Create GPU target");
        if(stage) {
            d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            hr(device->CreateTexture2D(&d,nullptr,&s.staging),"Create readback buffer");
        }
        return s;
    }
    Surface createUpload(int w,int h) {
        Surface s;s.width=w;s.height=h;
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        hr(device->CreateTexture2D(&d,nullptr,&s.texture),"Create upload texture");hr(device->CreateShaderResourceView(s.texture.Get(),nullptr,&s.resource),"Create upload view");return s;
    }
    void upload(Surface& s,const Frame& f) {
        D3D11_MAPPED_SUBRESOURCE map{};hr(context->Map(s.texture.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map),"Map upload texture");
        if(map.RowPitch==unsigned(f.stride)) std::memcpy(map.pData,f.data.data(),size_t(f.stride)*f.format.height);
        else for(int y=0;y<f.format.height;++y)std::memcpy(static_cast<uint8_t*>(map.pData)+size_t(y)*map.RowPitch,f.data.data()+size_t(y)*f.stride,f.stride);
        context->Unmap(s.texture.Get(),0);
    }
    ComPtr<ID3DBlob> compile(const char* entry,const char* model) {
        ComPtr<ID3DBlob> blob,error;
        HRESULT r=D3DCompile(shader,sizeof(shader)-1,"sw-compositor",nullptr,nullptr,entry,model,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error);
        if(FAILED(r)) throw std::runtime_error(error?std::string((char*)error->GetBufferPointer(),error->GetBufferSize()):"Shader compilation failed");
        return blob;
    }
    void draw(Surface& to,ID3D11PixelShader* ps,int x=0,int y=0,int width=0,int height=0) {
        context->UpdateSubresource(constants.Get(),0,nullptr,&p,0,0);
        auto* cb=constants.Get();context->PSSetConstantBuffers(0,1,&cb);
        D3D11_VIEWPORT vp{float(x),float(y),float(width?width:to.width),float(height?height:to.height),0,1};context->RSSetViewports(1,&vp);
        auto* target=to.target.Get();context->OMSetRenderTargets(1,&target,nullptr);
        context->PSSetShader(ps,nullptr,0);context->Draw(3,0);
        context->OMSetRenderTargets(0,nullptr,nullptr);
    }
    void mapRead(ID3D11Texture2D* staging,int width,int height,uint8_t* dst,int stride) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr(context->Map(staging,0,D3D11_MAP_READ,0,&mapped),"GPU readback");
        if(mapped.RowPitch==unsigned(stride))std::memcpy(dst,mapped.pData,size_t(stride)*height);
        else for(int y=0;y<height;++y) std::memcpy(dst+size_t(y)*stride,(uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch,size_t(width)*4);
        context->Unmap(staging,0);
    }
    void read(Surface& s,uint8_t* dst,int stride) {
        context->CopyResource(s.staging.Get(),s.texture.Get());mapRead(s.staging.Get(),s.width,s.height,dst,stride);
    }
};
GpuCompositor::GpuCompositor():impl_(std::make_unique<Impl>()) {}
GpuCompositor::~GpuCompositor()=default;
std::string GpuCompositor::adapter() const { return impl_->adapterName; }
void GpuCompositor::initialize(Format format,bool pipelined) {
    impl_=std::make_unique<Impl>();auto& g=*impl_;g.format=format;
    g.pipelined=pipelined;
    D3D_FEATURE_LEVEL level;
    hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&g.device,&level,&g.context),"Create hardware D3D11 device");
    ComPtr<IDXGIDevice> dx;ComPtr<IDXGIAdapter> adapter;g.device.As(&dx);dx->GetAdapter(&adapter);
    DXGI_ADAPTER_DESC ad{};adapter->GetDesc(&ad);
    char name[256]{};WideCharToMultiByte(CP_UTF8,0,ad.Description,-1,name,256,nullptr,nullptr);g.adapterName=name;
    auto v=g.compile("vertex","vs_5_0"),c=g.compile("compose","ps_5_0"),p=g.compile("pack","ps_5_0");
    hr(g.device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&g.vs),"Create vertex shader");
    hr(g.device->CreatePixelShader(c->GetBufferPointer(),c->GetBufferSize(),nullptr,&g.compose),"Create compositor shader");
    hr(g.device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&g.pack),"Create YUV shader");
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Params);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    hr(g.device->CreateBuffer(&bd,nullptr,&g.constants),"Create parameters");
    for(int i=0;i<2;++i) {
        g.scenes[i]=g.create(format.width,format.height,true,false);
        g.packed[i]=g.create(format.width/2,format.height,true,true);
        if(pipelined) {
            D3D11_TEXTURE2D_DESC d{};g.packed[i].staging->GetDesc(&d);
            for(auto& buffer:g.readbacks[i])hr(g.device->CreateTexture2D(&d,nullptr,&buffer),"Create pipelined readback");
        }
        const float black[]={128.f/255,16.f/255,128.f/255,16.f/255};g.context->ClearRenderTargetView(g.packed[i].target.Get(),black);
    }
    g.monitor=g.create(1440,540,true,true);
    if(pipelined) {D3D11_TEXTURE2D_DESC d{};g.monitor.staging->GetDesc(&d);hr(g.device->CreateTexture2D(&d,nullptr,&g.previousMonitor),"Create monitor readback");}
    g.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.context->VSSetShader(g.vs.Get(),nullptr,0);
}
GpuResult GpuCompositor::render(const std::array<FramePtr,4>& frames,const RenderState& state,uint64_t tick,bool showMonitors) {
    auto& g=*impl_;GpuResult result;
    const int field=g.format.interlaced?int(tick%2):0;
    // Hold one captured interlaced frame over both output fields.
    if(!g.format.interlaced||field==0) for(int i=0;i<4;++i) {
        if(frames[i]) {
            const auto& f=*frames[i];
            if(g.sources[i].width!=f.format.width/2||g.sources[i].height!=f.format.height)
                g.sources[i]=g.createUpload(f.format.width/2,f.format.height);
            if(frames[i]!=g.held[i]) g.upload(g.sources[i],f);
        }
        g.held[i]=frames[i];
    }
    for(int i=0;i<4;++i) {
        const auto& f=g.held[i];
        g.p.sourceInfo[i][0]=float(f?f->format.width:1920);g.p.sourceInfo[i][1]=float(f?f->format.height:1080);
        g.p.sourceInfo[i][2]=f&&f->format.interlaced?1.f:0.f;
        g.p.sourceInfo[i][3]=f&&std::chrono::steady_clock::now()-f->captured<std::chrono::milliseconds(500)?1.f:0.f;
    }
    ID3D11ShaderResourceView* inputViews[4];for(int i=0;i<4;++i) inputViews[i]=g.sources[i].resource.Get();
    g.context->PSSetShaderResources(0,4,inputViews);
    g.p.options[1]=float(tick%2);g.p.options[2]=g.format.interlaced?1.f:0.f;
    for(int out=0;out<2;++out) {
        const bool mix=out==0&&state.transitioning;
        const auto& scene=out==0?state.program:state.preview;
        g.p.a=params(mix?state.transitionFrom:scene);g.p.b=params(mix?state.transitionTo:scene);g.p.options[0]=mix?state.mix:0;
        ID3D11ShaderResourceView* empty=nullptr;g.context->PSSetShaderResources(4,1,&empty);
        g.draw(g.scenes[out],g.compose.Get());
        auto* view=g.scenes[out].resource.Get();g.context->PSSetShaderResources(4,1,&view);
        g.draw(g.packed[out],g.pack.Get());g.context->PSSetShaderResources(4,1,&empty);
        if(!g.format.interlaced||field==1) {
            if(g.pipelined) {g.context->CopyResource(g.readbacks[out][g.completed%3].Get(),g.packed[out].texture.Get());continue;}
            auto f=g.outputPool.acquire(g.format);
            if(!f) throw std::runtime_error("Output frame pool exhausted; outputs are retaining frames.");
            g.read(g.packed[out],f->data.data(),f->stride);f->sequence=tick/g.format.ticksPerFrame();
            f->captured=std::chrono::steady_clock::now(); result.output[out]=f;
        }
    }
    result.outputState=state;
    if(g.pipelined&&(!g.format.interlaced||field==1)) {
        g.context->Flush();
        if(g.completed>0) {
            for(int out=0;out<2;++out) {
                auto f=g.outputPool.acquire(g.format);if(!f)throw std::runtime_error("Pipelined output pool exhausted");
                g.mapRead(g.readbacks[out][(g.completed-1)%3].Get(),g.format.width/2,g.format.height,f->data.data(),f->stride);
                f->sequence=g.completed-1;f->captured=std::chrono::steady_clock::now();result.output[out]=f;
            }
            result.outputState=g.previousState;
        }
        g.previousState=state;++g.completed;
    }
    if(showMonitors) {
      for(int i=0;i<6;++i) {
        Scene scene;scene.background=i<4?i:0;
        bool mix=i==4&&state.transitioning;
        if(i>=4) scene=i==4?state.program:state.preview;
        g.p.a=params(mix?state.transitionFrom:scene);g.p.b=params(mix?state.transitionTo:scene);g.p.options[0]=mix?state.mix:0;
        g.draw(g.monitor,g.compose.Get(),(i%3)*480,(i/3)*270,480,270);
      }
      g.context->CopyResource(g.monitor.staging.Get(),g.monitor.texture.Get());
      if(!g.pipelined||g.monitorReady) {
        std::vector<uint8_t> atlas(1440*540*4);
        g.mapRead(g.pipelined?g.previousMonitor.Get():g.monitor.staging.Get(),1440,540,atlas.data(),1440*4);
        for(int i=0;i<6;++i) {auto& image=result.monitors[i];image.width=480;image.height=270;image.rgba.resize(480*270*4);
          for(int y=0;y<270;++y)std::memcpy(image.rgba.data()+size_t(y)*480*4,atlas.data()+(size_t((i/3)*270+y)*1440+(i%3)*480)*4,480*4);
        }
      }
      if(g.pipelined) {g.monitor.staging.Swap(g.previousMonitor);g.monitorReady=true;g.context->Flush();}
    }
    return result;
}
}
