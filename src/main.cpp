/**
 * main.cpp — Point d'entrée. Ne contient que l'enregistrement des modules
 * métier du projet ; toute la logique d'infrastructure vit dans App/services.
 */

#include <Arduino.h>
#include "core/app.h"
#include "project_config.h"
#include "modules/inventory/inventory_module.h"
#include "modules/document/document_module.h"
#include "modules/project/project_module.h"
#include "modules/importexport/importexport_module.h"
#ifdef ENABLE_BOOT_LOG
#include "modules/boot_log/boot_log.h"
#endif

static InventoryModule inventoryModule;
static DocumentModule documentModule;
static ProjectModule projectModule;
static ImportExportModule importExportModule;

void setup() {
    Serial.begin(115200);

    app.modules.add(&inventoryModule);
    app.modules.add(&documentModule);
    app.modules.add(&projectModule);
    app.modules.add(&importExportModule);
#ifdef ENABLE_BOOT_LOG
    app.modules.add(&bootLogModule);
#endif
    app.begin();
}

void loop() {
    app.loop();
}
