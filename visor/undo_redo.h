#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace mg {

// Command Pattern for Undo/Redo
struct UndoRedoCommand {
    virtual ~UndoRedoCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

// Text edit command (for .herm source changes)
struct TextEditCommand : UndoRedoCommand {
    std::string& target;
    std::string old_text;
    std::string new_text;
    std::string desc;

    TextEditCommand(std::string& target, std::string old_text, std::string new_text, const std::string& desc)
        : target(target), old_text(std::move(old_text)), new_text(std::move(new_text)), desc(desc) {}

    void execute() override { target = new_text; }
    void undo() override { target = old_text; }
    std::string description() const override { return desc; }
};

// Node transform command (for gizmo changes)
struct TransformCommand : UndoRedoCommand {
    float* target;      // pointer to translate/rotate/scale component
    float old_value;
    float new_value;
    std::string desc;

    TransformCommand(float* target, float old_value, float new_value, const std::string& desc)
        : target(target), old_value(old_value), new_value(new_value), desc(desc) {}

    void execute() override { *target = new_value; }
    void undo() override { *target = old_value; }
    std::string description() const override { return desc; }
};

class UndoRedo {
public:
    static constexpr int MAX_HISTORY = 100;

    void executeCommand(std::shared_ptr<UndoRedoCommand> cmd);
    bool undo();
    bool redo();
    void clear();

    bool canUndo() const;
    bool canRedo() const;

    int undoCount() const;
    int redoCount() const;

    std::string undoDescription() const;
    std::string redoDescription() const;

private:
    std::vector<std::shared_ptr<UndoRedoCommand>> history;
    int current_index = -1; // points to the current state
};

} // namespace mg
