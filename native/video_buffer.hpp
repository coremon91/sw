#pragma once
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif

namespace sw {
// Page boundaries keep independently registered CUDA host buffers from sharing
// a page. The portable core still works without a CUDA runtime or GPU.
template<class T> struct PageAllocator {
    using value_type=T;
    PageAllocator()=default;
    template<class U> PageAllocator(const PageAllocator<U>&) noexcept {}
    T* allocate(size_t n) {
        if(n>(std::numeric_limits<size_t>::max()-4095)/sizeof(T))throw std::bad_alloc();
        const auto bytes=(n*sizeof(T)+4095)&~size_t(4095);
#ifdef _WIN32
        auto p=_aligned_malloc(bytes,4096);
#else
        auto p=std::aligned_alloc(4096,bytes);
#endif
        if(!p)throw std::bad_alloc();return static_cast<T*>(p);
    }
    void deallocate(T* p,size_t) noexcept {
#ifdef _WIN32
        _aligned_free(p);
#else
        std::free(p);
#endif
    }
    template<class U> bool operator==(const PageAllocator<U>&) const noexcept {return true;}
};
class VideoBuffer {
    std::vector<uint8_t,PageAllocator<uint8_t>> bytes_;
    // Destroy registrations before the storage. Copies get independent storage
    // and must never inherit a registration that belongs to the original pointer.
    mutable std::shared_ptr<void> pin_;
public:
    VideoBuffer()=default;
    VideoBuffer(const VideoBuffer& other):bytes_(other.bytes_) {}
    VideoBuffer(VideoBuffer&&)=default;
    VideoBuffer& operator=(VideoBuffer other) noexcept {swap(other);return *this;}
    void swap(VideoBuffer& other) noexcept {bytes_.swap(other.bytes_);pin_.swap(other.pin_);}
    void resize(size_t n) {if(n!=bytes_.size())pin_.reset();bytes_.resize(n);}
    size_t size() const noexcept {return bytes_.size();}
    uint8_t* data() noexcept {return bytes_.data();}
    const uint8_t* data() const noexcept {return bytes_.data();}
    uint8_t& operator[](size_t i) {return bytes_[i];}
    const uint8_t& operator[](size_t i) const {return bytes_[i];}
    bool pinned() const noexcept {return bool(pin_);}
    void retainPin(std::shared_ptr<void> pin) const {pin_=std::move(pin);}
};
}
