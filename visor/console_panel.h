#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace mg {

struct LogEntry {
    enum class Level : uint8_t {
        INFO,
        WARN,
        ERROR_LOG,
        SUCCESS
    };
    Level level = Level::INFO;
    std::string message;
    uint32_t timestamp_ms = 0;
};

class ConsolePanel {
public:
    static constexpr int MAX_ENTRIES = 500;

    std::vector<LogEntry> entries;
    bool auto_scroll = true;
    bool visible = true;
    int filter_level = -1; // -1 = all

    void addLog(LogEntry::Level level, const char* fmt, ...);
    void clear();
    void draw();
};

} // namespace mg
