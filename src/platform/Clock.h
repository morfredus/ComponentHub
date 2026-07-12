// Horodatage ISO 8601 local — équivalent desktop du time_manager de l'ESP32,
// utilisé pour dater les mouvements de stock.
#pragma once

#include <ctime>
#include <string>

namespace chdesktop {

inline std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv);
    return std::string(buf);
}

} // namespace chdesktop
