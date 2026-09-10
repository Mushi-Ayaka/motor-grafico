#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace mg {

struct FileEntry {
    std::string name;
    std::string path;       // full path
    bool is_directory = false;
    bool is_expanded = false;
    std::vector<FileEntry> children;
};

class SourceBrowser {
public:
    bool visible = true;
    std::string root_path;      // project root
    FileEntry root;
    FileEntry* selected = nullptr;

    void draw();
    void refresh();

    // Callback when file is opened
    typedef void (*OpenCallback)(const char* path, void* user_data);
    OpenCallback on_open = nullptr;
    void* open_user_data = nullptr;

private:
    void drawNode(FileEntry& entry, int depth);
    void scanDirectory(const std::string& path, FileEntry& parent);
};

} // namespace mg
