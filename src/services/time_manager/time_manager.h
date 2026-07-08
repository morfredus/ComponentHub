/**
 * TimeManager — Synchronisation NTP et accès à l'heure courante.
 *
 * begin() est sans effet si le WiFi n'est pas connecté ; à rappeler après
 * chaque (re)connexion WiFi (App s'en charge).
 */

#pragma once
#include <Arduino.h>

class TimeManager {
public:
    void begin(const char* tz = "UTC", const char* ntpServer = "pool.ntp.org");

    bool isSynced() const;
    time_t now() const;
    String isoTimestamp() const;
};

extern TimeManager timeMgr;
