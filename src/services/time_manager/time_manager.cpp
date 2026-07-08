#include "time_manager.h"
#include "../log_manager/log_manager.h"

static const char* TAG = "Time";
static bool _started = false;

TimeManager timeMgr;

void TimeManager::begin(const char* tz, const char* ntpServer) {
    configTzTime(tz, ntpServer);
    _started = true;
    LOG_INFO(TAG, "Synchronisation NTP démarrée (serveur %s, tz %s)", ntpServer, tz);
}

bool TimeManager::isSynced() const {
    if (!_started) return false;
    time_t t = time(nullptr);
    return t > 1700000000;  // grossièrement après 2023 — évite l'epoch 1970 non synchronisé
}

time_t TimeManager::now() const { return time(nullptr); }

String TimeManager::isoTimestamp() const {
    time_t t = now();
    struct tm tmInfo;
    gmtime_r(&t, &tmInfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmInfo);
    return String(buf);
}
