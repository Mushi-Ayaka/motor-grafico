// herm_bridge.h - Puente herm::Rih (tensor 1x8) -> mg::Scene / mg::OntScene.
//
// El compilador hermetico (lenguaje-hermetico) emite RIH en esquema tensor:
//   Material.tensor[8] = [x, y, z, w, r, g, b, a]
//
// Este modulo ofrece dos rutas de conversion:
//   1. mg::Scene     (render/scene.h) — renderer CPU legacy, consume mg::Node con sdf_type string.
//   2. mg::OntScene  (render/scene.h) — renderer GPU/Vulkan, consume .ont binario (BVH + bytecode).
//
// Ambas rutas son NO passthrough JSON (ver docs/04-guia-uso.md sec. 16).
#pragma once
#include <string>
#include <vector>
#include "deps/lenguaje-hermetico/contrato/rih.h"

namespace mg {
struct Scene;
struct OntScene;

// --- CPU path (Scene) ---
bool convertHermToScene(const herm::Rih& in, Scene& out);
// Compila `src` (.herm) y convierte el RIH resultante a mg::Scene.
// Devuelve true si compilo y convirtio. En error, `errOut` trae el mensaje.
bool compileHermToScene(const std::string& src, Scene& out, std::string* errOut = nullptr);

// --- GPU/Vulkan path (OntScene) ---
// Convierte herm::Rih a OntScene (binario .ont en memoria: BVH + bytecode + materiales OntMaterial).
// El OntScene resultante se puede usar directamente con renderer.ont_scene / loadOnt.
bool convertHermToOntScene(const herm::Rih& in, OntScene& out);
bool compileHermToOntScene(const std::string& src, OntScene& out, std::string* errOut = nullptr);

// --- Utility ---
// Evalúa una expresión hermética como constante.
// Si is_expr=false, retorna el valor constante directo.
// Si is_expr=true, intenta parsear el string como float literal.
// Si contiene variables (w,t,x,y,z) o es dinámica, retorna 0.0f (v2 dynamic evaluation).
float evalExprConstPublic(const herm::Expr& e);
} // namespace mg
