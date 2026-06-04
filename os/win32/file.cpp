#include "../os.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mg {

bool FileMapping::open(const wchar_t* path) {
    close();
    _file_handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (_file_handle == INVALID_HANDLE_VALUE) {
        _file_handle = nullptr;
        return false;
    }
    LARGE_INTEGER li;
    GetFileSizeEx(_file_handle, &li);
    _size = (size_t)li.QuadPart;
    _map_handle = CreateFileMappingW(_file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!_map_handle) {
        CloseHandle(_file_handle);
        _file_handle = nullptr;
        return false;
    }
    _data = MapViewOfFile(_map_handle, FILE_MAP_READ, 0, 0, 0);
    if (!_data) {
        CloseHandle(_map_handle);
        CloseHandle(_file_handle);
        _map_handle = nullptr;
        _file_handle = nullptr;
        return false;
    }
    return true;
}

void FileMapping::close() {
    if (_data) { UnmapViewOfFile(_data); _data = nullptr; }
    if (_map_handle) { CloseHandle(_map_handle); _map_handle = nullptr; }
    if (_file_handle) { CloseHandle(_file_handle); _file_handle = nullptr; }
    _size = 0;
}

} // namespace mg
