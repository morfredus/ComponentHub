/**
 * InventoryModule — Module métier "inventaire de composants".
 *
 * Colle la couche domaine (InventoryService, indépendante de la plateforme)
 * aux dépôts JSON/LittleFS et à WebRouter : c'est la seule partie qui
 * connaît à la fois le domaine et l'ESP32 (Arduino, ArduinoJson, WebServer).
 */

#pragma once
#include "../../core/module.h"
#include "../../domain/inventory_service.h"
#include "../../storage/component_repository.h"
#include "../../storage/location_repository.h"
#include "../../storage/category_repository.h"
#include "../../storage/stock_movement_repository.h"

class InventoryModule : public Module {
public:
    const char* name() const override { return "Inventory"; }
    void begin() override;
    void registerRoutes(WebRouter& router) override;

private:
    ComponentJsonRepository     _componentRepo;
    LocationJsonRepository      _locationRepo;
    CategoryJsonRepository      _categoryRepo;
    StockMovementJsonRepository _movementRepo;
    domain::InventoryService    _service{_componentRepo, _locationRepo, _categoryRepo, _movementRepo};

    void _registerPages(WebRouter& router);
    void _registerComponentRoutes(WebRouter& router);
    void _registerLocationRoutes(WebRouter& router);
    void _registerCategoryRoutes(WebRouter& router);
    void _registerStockRoutes(WebRouter& router);
};
