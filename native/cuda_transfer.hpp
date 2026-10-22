#pragma once
#include "core.hpp"
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>

namespace sw {
struct CudaCopy { ID3D11Texture2D* texture; const VideoBuffer* buffer; size_t pitch,height; };
// D3D11 keeps ownership of composition. CUDA transfers between D3D textures and
// the exact host buffers consumed/produced by SDI; no intermediate CPU memcpy.
class CudaTransfer {
public:
    CudaTransfer();
    ~CudaTransfer();
    bool initialize(ID3D11Device*,std::string& reason);
    void registerTexture(ID3D11Texture2D*);
    void upload(const std::vector<CudaCopy>&);
    void download(const std::vector<CudaCopy>&);
    void wait();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
