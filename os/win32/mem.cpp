#include "../os.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mg {

void Arena::init(size_t reserve_size) {
    shutdown();
    base = VirtualAlloc(nullptr, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
    if (!base) return;
    // Commit first 64KB
    VirtualAlloc(base, 64 * 1024, MEM_COMMIT, PAGE_READWRITE);
    capacity = reserve_size;
    used = 0;
}

void* Arena::alloc(size_t size, size_t align) {
    if (!base) return nullptr;
    // Align used pointer
    size_t misalign = used % align;
    size_t pad = misalign ? (align - misalign) : 0;
    size_t needed = used + pad + size;
    if (needed > capacity) return nullptr;
    // Commit pages lazily (64KB chunks)
    if (needed > used) {
        size_t committed = ((used + 65535) / 65536) * 65536;
        if (needed > committed) {
            size_t to_commit = needed - committed;
            to_commit = ((to_commit + 65535) / 65536) * 65536;
            VirtualAlloc((u8*)base + committed, to_commit, MEM_COMMIT, PAGE_READWRITE);
        }
    }
    void* ptr = (u8*)base + used + pad;
    used += pad + size;
    return ptr;
}

void Arena::reset() {
    used = 0;
}

void Arena::shutdown() {
    if (base) {
        VirtualFree(base, 0, MEM_RELEASE);
        base = nullptr;
    }
    capacity = 0;
    used = 0;
}

} // namespace mg
