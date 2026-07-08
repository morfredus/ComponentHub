#pragma once
#include "../domain/repositories.h"

class StockMovementJsonRepository : public domain::IStockMovementRepository {
public:
    explicit StockMovementJsonRepository(const char* path = "/inventory_stock_movements.json") : _path(path) {}

    std::vector<domain::StockMovement> findByComponent(domain::Id componentId) const override;
    domain::StockMovement add(domain::StockMovement movement) override;

private:
    const char* _path;
};
