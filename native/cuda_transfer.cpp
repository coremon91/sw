#include "cuda_transfer.hpp"
#include <cuda_runtime_api.h>
#include <cuda_d3d11_interop.h>
#include <wrl/client.h>
#include <stdexcept>
#include <unordered_map>
#include <iostream>
#include <cstdlib>

namespace sw {
static void trace(const char* text) {if(std::getenv("SW_CUDA_TRACE"))std::cerr<<text<<std::endl;}
static void checkCuda(cudaError_t e,const char* operation) {
    if(e!=cudaSuccess)throw std::runtime_error(std::string(operation)+": "+cudaGetErrorString(e));
}
struct HostPin {
    void* pointer=nullptr;
    int device=0;
    ~HostPin() {if(pointer){cudaSetDevice(device);cudaHostUnregister(pointer);}}
};
struct CudaTransfer::Impl {
    struct Resource {Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;cudaGraphicsResource_t cuda=nullptr;};
    std::unordered_map<ID3D11Texture2D*,Resource> resources;
    cudaStream_t stream=nullptr;
    int device=0;
    ~Impl() {
        if(stream) {
            cudaSetDevice(device);cudaStreamSynchronize(stream);
            for(auto& [key,r]:resources)cudaGraphicsUnregisterResource(r.cuda);
            cudaStreamDestroy(stream);
        }
    }
    void pin(const VideoBuffer& b) {
        if(b.pinned())return;
        auto registration=std::make_shared<HostPin>();registration->device=device;
        checkCuda(cudaHostRegister(const_cast<uint8_t*>(b.data()),b.size(),cudaHostRegisterDefault),"CUDA pin video buffer");
        registration->pointer=const_cast<uint8_t*>(b.data());b.retainPin(std::move(registration));
    }
    void copy(const std::vector<CudaCopy>& copies,bool toGpu) {
        if(copies.empty())return;
        trace(toGpu?"upload begin":"download begin");
        checkCuda(cudaSetDevice(device),"CUDA select device");
        std::vector<cudaGraphicsResource_t> mapped;
        for(const auto& copy:copies) {
            pin(*copy.buffer);
            auto it=resources.find(copy.texture);
            if(it==resources.end())throw std::runtime_error("Unregistered CUDA video texture");
            if(copy.pitch*copy.height>copy.buffer->size())throw std::runtime_error("CUDA transfer exceeds host buffer");
            mapped.push_back(it->second.cuda);
        }
        checkCuda(cudaGraphicsMapResources(int(mapped.size()),mapped.data(),stream),"CUDA map D3D11 video textures");
        trace("mapped");
        try {
            for(size_t i=0;i<copies.size();++i) {
                const auto& copy=copies[i];cudaArray_t array=nullptr;
                checkCuda(cudaGraphicsSubResourceGetMappedArray(&array,mapped[i],0,0),"CUDA texture array");
                if(toGpu)checkCuda(cudaMemcpy2DToArrayAsync(array,0,0,copy.buffer->data(),copy.pitch,copy.pitch,copy.height,cudaMemcpyHostToDevice,stream),"CUDA upload video");
                else checkCuda(cudaMemcpy2DFromArrayAsync(const_cast<uint8_t*>(copy.buffer->data()),copy.pitch,array,0,0,copy.pitch,copy.height,cudaMemcpyDeviceToHost,stream),"CUDA download video");
            }
        } catch(...) {
            cudaGraphicsUnmapResources(int(mapped.size()),mapped.data(),stream);
            cudaStreamSynchronize(stream);throw;
        }
        checkCuda(cudaGraphicsUnmapResources(int(mapped.size()),mapped.data(),stream),"CUDA release D3D11 video textures");
        trace("unmapped");
        // Upload owners may be recycled after render returns. Downloads retain
        // their FramePtr in the compositor until wait() confirms completion.
        if(toGpu)checkCuda(cudaStreamSynchronize(stream),"CUDA upload completion");
        trace("copy queued/complete");
    }
};
CudaTransfer::CudaTransfer():impl_(std::make_unique<Impl>()) {}
CudaTransfer::~CudaTransfer()=default;
bool CudaTransfer::initialize(ID3D11Device* d3d,std::string& reason) {
    trace("CUDA initialize");
    unsigned count=0;int devices[8]{};
    auto error=cudaD3D11GetDevices(&count,devices,8,d3d,cudaD3D11DeviceListAll);
    if(error!=cudaSuccess||count!=1) {reason=error==cudaSuccess?"D3D11 adapter is not a single CUDA device":cudaGetErrorString(error);cudaGetLastError();return false;}
    impl_->device=devices[0];error=cudaSetDevice(impl_->device);
    if(error==cudaSuccess)error=cudaStreamCreateWithFlags(&impl_->stream,cudaStreamNonBlocking);
    if(error!=cudaSuccess){reason=cudaGetErrorString(error);cudaGetLastError();return false;}
    trace("CUDA initialized");return true;
}
void CudaTransfer::registerTexture(ID3D11Texture2D* texture) {
    trace("register texture");
    if(impl_->resources.contains(texture))return;
    Impl::Resource resource;resource.texture=texture;
    checkCuda(cudaGraphicsD3D11RegisterResource(&resource.cuda,texture,cudaGraphicsRegisterFlagsNone),"CUDA register D3D11 video texture");
    impl_->resources.emplace(texture,std::move(resource));
    trace("registered texture");
}
void CudaTransfer::upload(const std::vector<CudaCopy>& copies) {impl_->copy(copies,true);}
void CudaTransfer::download(const std::vector<CudaCopy>& copies) {impl_->copy(copies,false);}
void CudaTransfer::wait() {checkCuda(cudaSetDevice(impl_->device),"CUDA select device");checkCuda(cudaStreamSynchronize(impl_->stream),"CUDA output completion");}
}
