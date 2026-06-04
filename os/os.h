#pragma once
#include <cstdint>
#include <cstddef>

namespace mg {

using u8  = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

// ---------------------------------------------------------------------------
// Arena allocator — bump allocator on VirtualAlloc pages
// ---------------------------------------------------------------------------
struct Arena {
    void  init(size_t reserve_size = 64 * 1024 * 1024);
    void* alloc(size_t size, size_t align = 16);
    void  reset();
    void  shutdown();

    void*  base     = nullptr;
    size_t capacity = 0;
    size_t used     = 0;
};

// ---------------------------------------------------------------------------
// File mapping — memory-map a file read-only via CreateFileMappingW
// ---------------------------------------------------------------------------
struct FileMapping {
    bool open(const wchar_t* path);
    void close();

    const void* data() const { return _data; }
    size_t      size() const { return _size; }

    void* _data        = nullptr;
    size_t _size       = 0;
    void* _file_handle = nullptr;
    void* _map_handle  = nullptr;
};

// ---------------------------------------------------------------------------
// Timer — QueryPerformanceCounter wrapper
// ---------------------------------------------------------------------------
struct Timer {
    void init();
    f64  now() const;
    f64  delta();

    u64 _freq  = 0;
    u64 _start = 0;
    u64 _last  = 0;
};

// ---------------------------------------------------------------------------
// Window — Win32 window wrapper
// ---------------------------------------------------------------------------
struct Window {
    bool open(int width, int height, const char* title);
    bool pump();           // false when WM_QUIT
    void close();

    void*  hwnd    = nullptr;
    void*  hinst   = nullptr;
    int    width   = 0;
    int    height  = 0;
    bool   running = false;

    static Window* self(void* hwnd);
};

} // namespace mg
