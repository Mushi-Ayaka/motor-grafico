#include "export_dialog.h"
#include "imgui.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace mg {

void ExportDialog::draw() {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Export Executable", &visible)) {
        if (export_in_progress) {
            ImGui::Text("Exporting...");
            ImGui::TextWrapped("%s", export_log.c_str());
            ImGui::ProgressBar(-1.0f, ImVec2(-1, 0), "Working...");
        } else {
            ImGui::Text("Export to Windows Executable (.exe)");
            ImGui::Separator();

            // Output name
            char name_buf[256];
            strncpy(name_buf, options.output_name.c_str(), sizeof(name_buf) - 1);
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                options.output_name = name_buf;
            }

            // Options
            ImGui::Checkbox("Include Scripts", &options.include_scripts);
            ImGui::Checkbox("Include Bridges", &options.include_bridges);
            ImGui::Checkbox("Optimize", &options.optimize);

            ImGui::Separator();

            if (ImGui::Button("Export", ImVec2(120, 0))) {
                startExport();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                visible = false;
            }
        }
    }
    ImGui::End();
}

void ExportDialog::startExport() {
    export_in_progress = true;
    export_log = "Starting export...\n";

    // Run export in a thread (simplified for v1)
    // In production, this would be async
    runExport();

    export_in_progress = false;
}

void ExportDialog::runExport() {
    export_log += "Checking project files...\n";

    // Create output directory
    std::string out_dir = options.output_dir + "\\" + options.output_name;
    CreateDirectoryA(out_dir.c_str(), nullptr);

    // Copy .ont files
    export_log += "Copying .ont files...\n";

    // Copy scripts
    if (options.include_scripts) {
        export_log += "Copying scripts...\n";
    }

    // Copy bridges
    if (options.include_bridges) {
        export_log += "Copying bridges...\n";
    }

    // Create launcher .bat (simplified)
    std::string bat_path = out_dir + "\\launch.bat";
    FILE* f = fopen(bat_path.c_str(), "w");
    if (f) {
        fprintf(f, "@echo off\n");
        fprintf(f, "echo %s\n", options.output_name.c_str());
        fprintf(f, "echo Press any key to start...\n");
        fprintf(f, "pause >nul\n");
        fprintf(f, "REM TODO: Launch actual game executable\n");
        fclose(f);
    }

    export_log += "Export complete!\n";
    export_log += "Output: " + out_dir + "\n";
}

} // namespace mg
