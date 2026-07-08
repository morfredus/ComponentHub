/**
 * App — Point d'orchestration du framework.
 *
 * Démarre les services dans l'ordre correct (stockage, logs implicites,
 * WiFi, mDNS, OTA, web, temps) puis délègue au ModuleManager pour le métier.
 * main.cpp se limite à enregistrer ses modules et appeler app.begin()/loop().
 */

#pragma once
#include <Arduino.h>
#include "module_manager.h"

class App {
public:
    void begin();
    void loop();

    ModuleManager modules;
};

extern App app;
