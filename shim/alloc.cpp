// Global C++ allocator override — see alloc.h. Every `operator new` allocation
// is prefixed with a header recording how to free it, so the active allocator
// may change at runtime without mismatching frees.
#include "alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace {

struct Header {
    void* base;      // the real allocation start (what the underlying free wants)
    size_t size;     // total bytes handed to the underlying allocator
    size_t align;    // alignment handed to the underlying allocator
    LuauFreeFn free; // how to release `base`
    void* ctx;
};

constexpr size_t kBaseAlign = alignof(std::max_align_t);

LuauAllocFn g_alloc = nullptr;
LuauFreeFn g_free = nullptr;
void* g_ctx = nullptr;

// libc fallback (used before luau_set_allocator, and as the "default").
void* libcAlloc(void*, size_t size, size_t align) {
    void* p = nullptr;
    if (align < sizeof(void*))
        align = sizeof(void*);
    if (posix_memalign(&p, align, size) != 0)
        return nullptr;
    return p;
}
void libcFree(void*, void* ptr, size_t, size_t) {
    std::free(ptr);
}

void* wrapAlloc(size_t size, size_t align) {
    if (align < kBaseAlign)
        align = kBaseAlign;

    const bool custom = g_alloc != nullptr;
    LuauAllocFn af = custom ? g_alloc : libcAlloc;
    LuauFreeFn ff = custom ? g_free : libcFree;
    void* cx = custom ? g_ctx : nullptr;

    // room for: header + alignment slack + payload, all from one base block.
    const size_t total = sizeof(Header) + align + size;
    char* base = static_cast<char*>(af(cx, total, kBaseAlign));
    if (base == nullptr)
        throw std::bad_alloc();

    // place the user pointer aligned, leaving a header immediately before it.
    const uintptr_t raw = reinterpret_cast<uintptr_t>(base) + sizeof(Header);
    const uintptr_t user = (raw + (align - 1)) & ~static_cast<uintptr_t>(align - 1);
    Header* h = reinterpret_cast<Header*>(user - sizeof(Header));
    h->base = base;
    h->size = total;
    h->align = kBaseAlign;
    h->free = ff;
    h->ctx = cx;
    return reinterpret_cast<void*>(user);
}

void wrapFree(void* p) {
    if (p == nullptr)
        return;
    Header* h = reinterpret_cast<Header*>(static_cast<char*>(p) - sizeof(Header));
    h->free(h->ctx, h->base, h->size, h->align);
}

} // namespace

extern "C" void luau_set_allocator(LuauAllocFn alloc, LuauFreeFn free, void* ctx) {
    g_alloc = alloc;
    g_free = free;
    g_ctx = ctx;
}

// ---- replaced global operators ---------------------------------------------

void* operator new(std::size_t n) { return wrapAlloc(n, 0); }
void* operator new[](std::size_t n) { return wrapAlloc(n, 0); }
void* operator new(std::size_t n, std::align_val_t a) { return wrapAlloc(n, static_cast<size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return wrapAlloc(n, static_cast<size_t>(a)); }

void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try { return wrapAlloc(n, 0); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
    try { return wrapAlloc(n, 0); } catch (...) { return nullptr; }
}

void operator delete(void* p) noexcept { wrapFree(p); }
void operator delete[](void* p) noexcept { wrapFree(p); }
void operator delete(void* p, std::size_t) noexcept { wrapFree(p); }
void operator delete[](void* p, std::size_t) noexcept { wrapFree(p); }
void operator delete(void* p, std::align_val_t) noexcept { wrapFree(p); }
void operator delete[](void* p, std::align_val_t) noexcept { wrapFree(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { wrapFree(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { wrapFree(p); }
