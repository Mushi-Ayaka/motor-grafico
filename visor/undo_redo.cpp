#include "undo_redo.h"

namespace mg {

void UndoRedo::saveState(const std::string& source) {
    // If we're not at the end of history, truncate future states
    if (current_index >= 0 && current_index < (int)history.size() - 1) {
        history.erase(history.begin() + current_index + 1, history.end());
    }

    // Don't save if same as current
    if (current_index >= 0 && current_index < (int)history.size()) {
        if (history[current_index] == source)
            return;
    }

    // Add new state
    history.push_back(source);
    current_index = (int)history.size() - 1;

    // Trim if too large
    if ((int)history.size() > MAX_HISTORY) {
        int excess = (int)history.size() - MAX_HISTORY;
        history.erase(history.begin(), history.begin() + excess);
        current_index -= excess;
    }
}

bool UndoRedo::undo(std::string& out_source) {
    if (!canUndo()) return false;

    current_index--;
    out_source = history[current_index];
    return true;
}

bool UndoRedo::redo(std::string& out_source) {
    if (!canRedo()) return false;

    current_index++;
    out_source = history[current_index];
    return true;
}

void UndoRedo::clear() {
    history.clear();
    current_index = -1;
}

bool UndoRedo::canUndo() const {
    return current_index > 0;
}

bool UndoRedo::canRedo() const {
    return current_index >= 0 && current_index < (int)history.size() - 1;
}

int UndoRedo::undoCount() const {
    return canUndo() ? current_index : 0;
}

int UndoRedo::redoCount() const {
    if (current_index < 0) return 0;
    return (int)history.size() - 1 - current_index;
}

} // namespace mg
