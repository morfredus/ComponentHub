// Test de fumée (headless) : vérifie que le domaine partagé compile et tourne
// sur PC, et que le stockage JSON round-trip correctement (accents + €).
// Non inclus dans l'application finale.
#include <cstdio>
#include <filesystem>

#include "AppContext.h"
#include "platform/Clock.h"

using namespace domain;

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "componenthub_smoke";
    fs::create_directories(dir);
    for (auto& e : fs::directory_iterator(dir)) fs::remove(e);

    chdesktop::AppContext ctx(dir.string());

    Component c;
    c.name = "Résistance 10kΩ";
    c.reference = "R-10K";
    c.notes = "Prix ~0,05 € — boîtier CMS";
    c.category = "Passifs";
    c.price = 0.05;
    c.quantity = 3;
    c.minStock = 5;  // sous le seuil -> doit apparaître en stock faible
    c = ctx.inventory.saveComponent(c);
    std::printf("saved id=%d name=%s\n", c.id, c.name.c_str());

    // Mouvement de stock daté.
    StockMovement mv;
    mv.componentId = c.id;
    mv.type = StockMovementType::In;
    mv.quantity = 10;
    mv.timestamp = chdesktop::nowIso();
    ctx.inventory.recordMovement(mv);

    // Relecture depuis le disque via une nouvelle instance.
    chdesktop::AppContext reloaded(dir.string());
    auto all = reloaded.inventory.listComponents();
    std::printf("reloaded %zu component(s)\n", all.size());
    for (auto& x : all)
        std::printf("  #%d %-20s qty=%d cat=%s notes=\"%s\"\n",
                    x.id, x.name.c_str(), x.quantity, x.category.c_str(), x.notes.c_str());

    // La catégorie doit avoir été créée automatiquement par saveComponent.
    std::printf("categories: %zu\n", reloaded.inventory.listCategories().size());

    std::printf("component file: %s\n", (dir / "inventory_components.json").string().c_str());
    return all.size() == 1 ? 0 : 1;
}
