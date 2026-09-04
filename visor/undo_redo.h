#pragma once
#include <string>
#include <vector>

namespace mg {

class UndoRedo {
public:
    static constexpr int MAX_HISTORY = 100;

    void saveState(const std::string& source);
    bool undo(std::string& out_source);
    bool redo(std::string& out_source);
    void clear();

    bool canUndo() const;
    bool canRedo() const;

    int undoCount() const;
    int redoCount() const;

private:
    std::vector<std::string> history;
    int current_index = -1; // points to the current state
};

} // namespace mg
