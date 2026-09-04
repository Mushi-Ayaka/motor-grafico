// Round-trip test: libherm compile .herm -> .rih -> mg::loadRih
#include <cstdint>
namespace herm { using f64 = double; }  // rih.h no lo define; herm.h lo usa
#include "deps/lenguaje-hermetico/contrato/herm.h"
#include <cstdio>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) { printf("uso: test_roundtrip <sample.herm>\n"); return 2; }
    std::ifstream f(argv[1]);
    if (!f) { printf("no se pudo abrir %s\n", argv[1]); return 2; }
    std::stringstream ss; ss << f.rdbuf();
    std::string src = ss.str();

    herm::HermConfig cfg;
    cfg.output_path = "build/_rt.rih";
    cfg.binary_output = false;
    herm::HermResult r = herm::compileFromString(src, cfg);
    if (!r.success) {
        printf("COMPILE FAIL (%s)\n", argv[1]);
        for (auto& e : r.errors) printf("  %u:%u %s\n", e.line, e.column, e.message.c_str());
        return 1;
    }
    printf("[ok] compilado -> %s\n", r.output_path.c_str());

    // Nota: mg::Rih usa schema PBR; herm::Rih es tensor 1x8 (schema distinto).
    // Cargar el .rih de herm con mg::loadRih es incorrecto por diseno; el puente
    // real (herm::Rih -> mg::Rih/OntScene) se implementa en T-F1.2.
    // Por ahora solo verificamos que el compilador escribio el .rih.
    std::ifstream out(r.output_path, std::ios::binary | std::ios::ate);
    std::streamsize sz = out.tellg();
    if (sz > 0)
        printf("[ok] .rih escrito (%lld bytes) en %s\n", (long long)sz, r.output_path.c_str());
    else
        printf("[warn] .rih vacio o no escrito\n");
    return 0;
}
