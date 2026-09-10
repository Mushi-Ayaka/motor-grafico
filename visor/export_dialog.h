#pragma once
#include <string>
#include <vector>

namespace mg {

struct ExportOptions {
    std::string output_name = "my_game";
    std::string output_dir = ".";
    bool include_scripts = true;
    bool include_bridges = true;
    bool optimize = true;
};

class ExportDialog {
public:
    bool visible = false;
    ExportOptions options;
    bool export_in_progress = false;
    std::string export_log;

    void draw();
    void startExport();

private:
    void runExport();
};

} // namespace mg
