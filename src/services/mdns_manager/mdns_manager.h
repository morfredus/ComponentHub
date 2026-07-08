/**
 * MdnsManager — Résolution de nom réseau local (http://<hostname>.local)
 *
 * Indépendant de WiFiManager : start() doit être rappelé à chaque
 * reconnexion WiFi (App s'en charge via le callback WiFiManager::begin/loop).
 */

#pragma once
#include <Arduino.h>

class MdnsManager {
public:
    bool start(const char* hostname);
    bool isActive() const { return _active; }

private:
    bool _active = false;
};

extern MdnsManager mdnsMgr;
