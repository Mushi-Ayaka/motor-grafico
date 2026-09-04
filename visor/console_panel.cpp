#include "console_panel.h"
#include "imgui.h"
#include <cstdarg>
#include <cstdio>
#include <windows.h>

namespace mg {

void ConsolePanel::addLog(LogEntry::Level level, const char* fmt, ...) {
    LogEntry e;
    e.level = level;
    e.timestamp_ms = (uint32_t)GetTickCount();

    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    e.message = buf;

    entries.push_back(e);
    if ((int)entries.size() > MAX_ENTRIES)
        entries.erase(entries.begin());
}

void ConsolePanel::clear() {
    entries.clear();
}

void ConsolePanel::draw() {
    if (!visible) return;

    if (ImGui::Begin("Console")) {
        // Toolbar
        if (ImGui::Button("Clear")) clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll);
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        // Filter
        if (ImGui::Button("All")) filter_level = -1;
        ImGui::SameLine();
        if (ImGui::Button("Errors")) filter_level = (int)LogEntry::Level::ERROR_LOG;
        ImGui::SameLine();
        if (ImGui::Button("Warnings")) filter_level = (int)LogEntry::Level::WARN;

        ImGui::Separator();

        // Log entries
        ImGui::BeginChild("##log_scroll", ImVec2(-FLT_MIN, -FLT_MIN), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

        for (auto& e : entries) {
            if (filter_level >= 0 && (int)e.level != filter_level)
                continue;

            ImVec4 color;
            const char* prefix;
            switch (e.level) {
                case LogEntry::Level::INFO:     color = ImVec4(0.7f,0.7f,0.7f,1); prefix = "[INFO] "; break;
                case LogEntry::Level::WARN:     color = ImVec4(1,0.8f,0,1); prefix = "[WARN] "; break;
                case LogEntry::Level::ERROR_LOG: color = ImVec4(1,0.3f,0.3f,1); prefix = "[ERR]  "; break;
                case LogEntry::Level::SUCCESS:  color = ImVec4(0.3f,1,0.3f,1); prefix = "[OK]   "; break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s%s", prefix, e.message.c_str());
            ImGui::PopStyleColor();
        }

        if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace mg
