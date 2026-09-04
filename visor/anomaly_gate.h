#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace herm { struct Rih; }

namespace mg {

struct Anomaly {
    enum class Severity : uint8_t {
        SEV_INFO,
        SEV_WARNING,
        SEV_ERROR
    };

    Severity severity = Severity::SEV_ERROR;
    int line = 0;
    std::string message;
    std::string context;
};

class AnomalyGate {
public:
    static std::vector<Anomaly> validateAST(const herm::Rih& rih);
    static std::vector<Anomaly> validateBytecode(const uint8_t* bc, uint32_t size);
    static bool hasBlockingErrors(const std::vector<Anomaly>& anomalies);
    static std::string summary(const std::vector<Anomaly>& anomalies);
};

} // namespace mg
