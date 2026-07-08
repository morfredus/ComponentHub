/**
 * SystemInfo — État système en JSON (générique, sans dépendance métier)
 */

#pragma once
#include <Arduino.h>

class SystemInfo {
public:
    // {"version":"","uptime":"","free_heap":"","wifi":"","ip":""}
    String getJson() const;
};

extern SystemInfo systemInfo;
