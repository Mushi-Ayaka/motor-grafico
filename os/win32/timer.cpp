#include "../os.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mg {

void Timer::init() {
    QueryPerformanceFrequency((LARGE_INTEGER*)&_freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&_start);
    _last = _start;
}

f64 Timer::now() const {
    u64 now;
    QueryPerformanceCounter((LARGE_INTEGER*)&now);
    return (f64)(now - _start) / (f64)_freq;
}

f64 Timer::delta() {
    u64 now;
    QueryPerformanceCounter((LARGE_INTEGER*)&now);
    f64 dt = (f64)(now - _last) / (f64)_freq;
    _last = now;
    return dt;
}

} // namespace mg
