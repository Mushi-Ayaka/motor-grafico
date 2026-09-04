// herm_editor.h - Editor de codigo .herm con line numbers, file I/O, error markers.
#pragma once
#include <string>
#include <vector>

namespace mg {

struct HermEditorError {
    int line;           // 1-indexed
    int column;         // 0-indexed
    std::string message;
};

struct HermEditor {
    std::string source;         // contenido actual del editor
    std::string file_path;      // ruta del archivo abierto (vacío si sin guardar)
    bool modified = false;      // hay cambios sin guardar?

    // Error markers
    std::vector<HermEditorError> errors;
    bool has_errors = false;

    // UI state
    bool visible = true;
    float text_height = 300.0f;

    // Inicializa con un ejemplo por defecto
    void initDefault();

    // File I/O
    bool openFile(const char* path);
    bool saveFile(const char* path = nullptr); // nullptr = guardar en file_path
    bool saveAsDialog(); // abre dialogo Win32 Save

    // Syntax: retorna color para una palabra clave
    // Devuelve nullptr si no es keyword
    static const char* keywordColor(const char* word);

    // Render del editor completo
    void draw();

    // Callback de compile (se llama desde el editor)
    using CompileCallback = bool (*)(const std::string& src, std::string* errOut, void* user_data);
    void setCompileCallback(CompileCallback cb, void* user_data);

    // Para Undo/Redo: fuerza recarga del text buffer desde source
    void reloadFromSource() { text_buf_dirty = true; }

private:
    CompileCallback compile_cb = nullptr;
    void* compile_user_data = nullptr;
    char text_buf[65536] = {};  // buffer para InputTextMultiline
    bool text_buf_dirty = true; // necesita recargar de source
    int  cursor_line = 1;
    int  cursor_col = 0;
};

} // namespace mg
