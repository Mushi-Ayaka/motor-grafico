#pragma once
#include <string>
#include <vector>
#include <functional>

namespace mg {

struct Command {
    std::string name;
    std::string category;
    std::string shortcut;
    std::function<void()> action;
};

class CommandPalette {
public:
    bool visible = false;
    char search_buf[256] = {};
    int selected_idx = 0;
    std::vector<Command> commands;
    std::vector<int> filtered_indices;

    void draw();
    void toggle();
    void registerCommand(const char* name, const char* category, const char* shortcut, std::function<void()> action);
    void buildDefaultCommands();

private:
    void filterCommands();
};

} // namespace mg
