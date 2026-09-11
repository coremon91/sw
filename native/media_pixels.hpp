#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
namespace sw {
// Rec.709 limited-range YUV -> UYVY. Nearest chroma expansion preserves field
// separation for interlaced 4:2:0. SSE2 is part of the Windows x64 baseline.
inline void packFileYuv(const uint8_t* const planes[3],const int strides[3],uint8_t* output,int outputStride,
                        int width,int height,bool planar,bool halfHeight,bool wide,int shift,bool interlaced) {
    const auto bits=_mm_cvtsi32_si128(shift);
    auto packed16=[&](const uint8_t* p) {
        if(!wide)return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
        auto a=_mm_srl_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)),bits);
        auto b=_mm_srl_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p+16)),bits);
        return _mm_packus_epi16(a,b);
    };
    auto sample=[&](const uint8_t* p,int i)->uint8_t {
        return wide?uint8_t(std::min(255,int(reinterpret_cast<const uint16_t*>(p)[i]>>shift))):p[i];
    };
    for(int y=0;y<height;++y) {
        const int cy=halfHeight?(interlaced?(y/4)*2+y%2:y/2):y;
        const auto* luma=planes[0]+size_t(y)*strides[0];
        const auto* u=planes[1]+size_t(cy)*strides[1];const auto* v=planar?planes[2]+size_t(cy)*strides[2]:nullptr;
        auto* dst=output+size_t(y)*outputStride;const int bytes=wide?2:1;int x=0;
        for(;x+16<=width;x+=16) {
            const auto yy=packed16(luma+x*bytes);__m128i uv;
            if(planar) {
                __m128i uu,vv;
                if(wide) {
                    const auto uw=_mm_srl_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(u+x)),bits);
                    const auto vw=_mm_srl_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(v+x)),bits);
                    uu=_mm_packus_epi16(uw,uw);vv=_mm_packus_epi16(vw,vw);
                }else{uu=_mm_loadl_epi64(reinterpret_cast<const __m128i*>(u+x/2));vv=_mm_loadl_epi64(reinterpret_cast<const __m128i*>(v+x/2));}
                uv=_mm_unpacklo_epi8(uu,vv);
            }else uv=packed16(u+x*bytes);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+x*2),_mm_unpacklo_epi8(uv,yy));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+x*2+16),_mm_unpackhi_epi8(uv,yy));
        }
        for(;x<width;x+=2) {
            dst[x*2]=sample(u,planar?x/2:x);dst[x*2+1]=sample(luma,x);
            dst[x*2+2]=planar?sample(v,x/2):sample(u,x+1);dst[x*2+3]=sample(luma,x+1);
        }
    }
}
}
