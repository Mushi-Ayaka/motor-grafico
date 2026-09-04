// anomaly_gate.cpp - T-110: Anomaly Gate (validación estática de AST/runtime).
#include "anomaly_gate.h"
#include <cmath>

namespace herm { using f64 = double; }
#include "deps/lenguaje-hermetico/contrato/rih.h"

namespace mg {

std::vector<Anomaly> AnomalyGate::validateAST(const herm::Rih& rih) {
    std::vector<Anomaly> anomalies;

    for (auto& node : rih.nodes) {
        // 1. SDF nodes vacíos
        if (node.type == herm::NodeType::SDF && node.sdf_nodes.empty()) {
            Anomaly a;
            a.severity = Anomaly::Severity::SEV_WARNING;
            a.message = "node '" + node.name + "' es SDF pero no tiene sdf_nodes";
            anomalies.push_back(a);
        }

        // 2. Material ID fuera de rango
        if (node.material_id.has_value() && *node.material_id >= (uint32_t)rih.materials.size()) {
            Anomaly a;
            a.severity = Anomaly::Severity::SEV_ERROR;
            a.message = "node '" + node.name + "' material_id fuera de rango: " + std::to_string(*node.material_id);
            anomalies.push_back(a);
        }

        // 3. Parámetros de primitiva no-finitos
        for (auto& sdf : node.sdf_nodes) {
            for (int i = 0; i < 4; i++) {
                if (!sdf.params[i].is_expr && !std::isfinite(sdf.params[i].constant)) {
                    Anomaly a;
                    a.severity = Anomaly::Severity::SEV_ERROR;
                    a.message = "sdf param[" + std::to_string(i) + "] no-finito en '" + node.name + "'";
                    anomalies.push_back(a);
                }
            }
            // k negativo en smooth_*
            if (sdf.k.has_value() && *sdf.k < 0.0f) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_WARNING;
                a.message = "smooth k negativo en '" + node.name + "'";
                anomalies.push_back(a);
            }
            // spacing <= 0 en repeat
            if (sdf.type == herm::SdfType::REPEAT && sdf.spacing.has_value() && *sdf.spacing <= 0.0f) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "repeat spacing <= 0 en '" + node.name + "'";
                anomalies.push_back(a);
            }
        }

        // 4. Def sin children
        if (node.type == herm::NodeType::GROUP && node.children.empty()) {
            Anomaly a;
            a.severity = Anomaly::Severity::SEV_INFO;
            a.message = "group '" + node.name + "' sin children";
            anomalies.push_back(a);
        }

        // 5. Instance sin def_id
        if (node.type == herm::NodeType::INSTANCE && !node.def_id.has_value()) {
            Anomaly a;
            a.severity = Anomaly::Severity::SEV_ERROR;
            a.message = "instance '" + node.name + "' sin def_id";
            anomalies.push_back(a);
        }
    }

    // 6. Defs con id duplicado
    for (size_t i = 0; i < rih.defs.size(); i++) {
        for (size_t j = i + 1; j < rih.defs.size(); j++) {
            if (rih.defs[i].id == rih.defs[j].id) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "def id duplicado: " + std::to_string(rih.defs[i].id);
                anomalies.push_back(a);
                break;
            }
        }
    }

    // 7. Escena sin nodos
    if (rih.nodes.empty() && rih.defs.empty()) {
        Anomaly a;
        a.severity = Anomaly::Severity::SEV_WARNING;
        a.message = "escena vacia (sin nodos ni defs)";
        anomalies.push_back(a);
    }

    // 8. Transform params non-finite
    for (auto& node : rih.nodes) {
        for (int i = 0; i < 3; i++) {
            if (!node.transform.translate[i].is_expr && !std::isfinite(node.transform.translate[i].constant)) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "translate[" + std::to_string(i) + "] non-finite in '" + node.name + "'";
                anomalies.push_back(a);
            }
            if (!node.transform.rotate[i].is_expr && !std::isfinite(node.transform.rotate[i].constant)) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "rotate[" + std::to_string(i) + "] non-finite in '" + node.name + "'";
                anomalies.push_back(a);
            }
            if (!node.transform.scale[i].is_expr && !std::isfinite(node.transform.scale[i].constant)) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "scale[" + std::to_string(i) + "] non-finite in '" + node.name + "'";
                anomalies.push_back(a);
            }
            // Scale zero
            if (!node.transform.scale[i].is_expr && node.transform.scale[i].constant == 0.0f) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_WARNING;
                a.message = "scale[" + std::to_string(i) + "] = 0 in '" + node.name + "'";
                anomalies.push_back(a);
            }
        }
    }

    // 9. Camera non-finite
    if (!std::isfinite(rih.scene.camera.position.x) || !std::isfinite(rih.scene.camera.position.y) ||
        !std::isfinite(rih.scene.camera.position.z)) {
        Anomaly a;
        a.severity = Anomaly::Severity::SEV_ERROR;
        a.message = "camera position non-finite";
        anomalies.push_back(a);
    }
    if (!std::isfinite(rih.scene.camera.fov) || rih.scene.camera.fov <= 0.0f) {
        Anomaly a;
        a.severity = Anomaly::Severity::SEV_ERROR;
        a.message = "camera fov invalid (<=0 or non-finite)";
        anomalies.push_back(a);
    }

    // 10. Light non-finite
    for (auto& l : rih.lights) {
        if (!std::isfinite(l.intensity)) {
            Anomaly a;
            a.severity = Anomaly::Severity::SEV_ERROR;
            a.message = "light '" + l.name + "' intensity non-finite";
            anomalies.push_back(a);
        }
        if (l.intensity < 0.0f) {
            Anomaly a;
            a.severity = Anomaly::Severity::SEV_WARNING;
            a.message = "light '" + l.name + "' intensity < 0";
            anomalies.push_back(a);
        }
    }

    // 11. Material name duplicates
    for (size_t i = 0; i < rih.materials.size(); i++) {
        for (size_t j = i + 1; j < rih.materials.size(); j++) {
            if (rih.materials[i].name == rih.materials[j].name && !rih.materials[i].name.empty()) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_WARNING;
                a.message = "material name duplicado: '" + rih.materials[i].name + "'";
                anomalies.push_back(a);
                break;
            }
        }
    }

    return anomalies;
}

std::vector<Anomaly> AnomalyGate::validateBytecode(const uint8_t* bc, uint32_t size) {
    std::vector<Anomaly> anomalies;
    if (!bc || size == 0) {
        Anomaly a;
        a.severity = Anomaly::Severity::SEV_ERROR;
        a.message = "bytecode vacío";
        anomalies.push_back(a);
        return anomalies;
    }

    // Scan bytecode for basic integrity
    uint32_t i = 0;
    while (i < size) {
        uint8_t op = bc[i];

        // End opcode found
        if (op == 0x1E) { // ONT_END
            break;
        }

        // Opcodes that take a 4-byte float operand
        if (op == 0x00) { // ONT_CONST
            if (i + 5 > size) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "ONT_CONST truncado en offset " + std::to_string(i);
                anomalies.push_back(a);
                break;
            }
            float val;
            memcpy(&val, &bc[i + 1], 4);
            if (!std::isfinite(val)) {
                Anomaly a;
                a.severity = Anomaly::Severity::SEV_ERROR;
                a.message = "CONST no-finito en offset " + std::to_string(i);
                anomalies.push_back(a);
            }
            i += 5;
            continue;
        }

        i++;
    }

    return anomalies;
}

bool AnomalyGate::hasBlockingErrors(const std::vector<Anomaly>& anomalies) {
    for (auto& a : anomalies) {
        if (a.severity == Anomaly::Severity::SEV_ERROR) return true;
    }
    return false;
}

std::string AnomalyGate::summary(const std::vector<Anomaly>& anomalies) {
    if (anomalies.empty()) return "OK (sin anomalías)";

    int errors = 0, warnings = 0, infos = 0;
    for (auto& a : anomalies) {
        if (a.severity == Anomaly::Severity::SEV_ERROR) errors++;
        else if (a.severity == Anomaly::Severity::SEV_WARNING) warnings++;
        else infos++;
    }

    std::string s;
    if (errors > 0) s += std::to_string(errors) + " error(s) ";
    if (warnings > 0) s += std::to_string(warnings) + " warning(s) ";
    if (infos > 0) s += std::to_string(infos) + " info(s) ";
    if (!s.empty()) s.pop_back();
    return s;
}

} // namespace mg
