/**
 * StockMovement - Stock movement recorded for a component.
 *
 * In/Out adjust the quantity relatively; Correction/Inventory
 * set it to the given value (see InventoryService::recordMovement).
 */

#pragma once
#include <string>
#include "ids.h"

namespace domain {

enum class StockMovementType { In, Out, Correction, Inventory };

inline const char* toString(StockMovementType t) {
    switch (t) {
        case StockMovementType::In:         return "in";
        case StockMovementType::Out:        return "out";
        case StockMovementType::Correction: return "correction";
        case StockMovementType::Inventory:  return "inventory";
    }
    return "in";
}

inline StockMovementType stockMovementTypeFromString(const std::string& s) {
    if (s == "out")        return StockMovementType::Out;
    if (s == "correction") return StockMovementType::Correction;
    if (s == "inventory")  return StockMovementType::Inventory;
    return StockMovementType::In;
}

struct StockMovement {
    Id id = kNoId;
    Id componentId = kNoId;
    StockMovementType type = StockMovementType::In;
    int quantity = 0;       // quantity of the movement; the direction is given by `type`
    std::string timestamp;  // ISO 8601, set by the platform layer (clock)
    std::string note;
};

} // namespace domain
