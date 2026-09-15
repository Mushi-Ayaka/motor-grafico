#include "undo_redo.h"

namespace mg {

void UndoRedo::executeCommand(std::shared_ptr<UndoRedoCommand> cmd) {
    // If we're not at the end of history, truncate future states
    if (current_index >= 0 && current_index < (int)history.size() - 1) {
        history.erase(history.begin() + current_index + 1, history.end());
    }

    // Execute the command
    cmd->execute();

    // Add to history
    history.push_back(cmd);
    current_index = (int)history.size() - 1;

    // Trim if too large
    if ((int)history.size() > MAX_HISTORY) {
        int excess = (int)history.size() - MAX_HISTORY;
        history.erase(history.begin(), history.begin() + excess);
        current_index -= excess;
    }
}

bool UndoRedo::undo() {
    if (!canUndo()) return false;

    history[current_index]->undo();
    current_index--;
    return true;
}

bool UndoRedo::redo() {
    if (!canRedo()) return false;

    current_index++;
    history[current_index]->execute();
    return true;
}

void UndoRedo::clear() {
    history.clear();
    current_index = -1;
}

bool UndoRedo::canUndo() const {
    return current_index >= 0;
}

bool UndoRedo::canRedo() const {
    return current_index >= 0 && current_index < (int)history.size() - 1;
}

int UndoRedo::undoCount() const {
    return canUndo() ? current_index + 1 : 0;
}

int UndoRedo::redoCount() const {
    if (current_index < 0) return 0;
    return (int)history.size() - 1 - current_index;
}

std::string UndoRedo::undoDescription() const {
    if (!canUndo()) return "";
    return history[current_index]->description();
}

std::string UndoRedo::redoDescription() const {
    if (!canRedo()) return "";
    return history[current_index + 1]->description();
}

} // namespace mg
