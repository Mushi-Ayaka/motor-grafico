// test_bridge.cpp - Verifica herm::Rih -> mg::Scene (puente del editor).
// Compila un .herm y lo convierte a mg::Scene, sin escribir a disco ni usar mg::loadRih.
#include <cstdint>
namespace herm { using f64 = double; }
#include "deps/lenguaje-hermetico/contrato/herm.h"
#include "render/scene.h"
#include "core/herm_bridge.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) { printf("uso: test_bridge <sample.herm>\n"); return 2; }
    std::ifstream f(argv[1]);
    if (!f) { printf("no se pudo abrir %s\n", argv[1]); return 2; }
    std::stringstream ss; ss << f.rdbuf();
    std::string src = ss.str();

    mg::Scene scene;
    std::string err;
    if (!mg::compileHermToScene(src, scene, &err)) {
        printf("COMPILE FAIL %s\n%s\n", argv[1], err.c_str());
        return 1;
    }
    printf("[ok] %s -> nodes=%zu materials=%zu lights=%zu w=%u h=%u\n",
        argv[1], scene.nodes.size(), scene.materials.size(), scene.lights.size(),
        scene.width, scene.height);
    return 0;
}
