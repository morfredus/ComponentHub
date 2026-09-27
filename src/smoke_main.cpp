// Test de fumée (headless) : vérifie que le domaine compile et tourne sur PC,
// et que le stockage SQLite conserve correctement les données (accents + €).
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
    c.notes = "Prix ~0,05 € - boîtier CMS";
    c.category = "Passifs";
    c.price = 0.05;
    c.quantity = 3;
    c.minStock = 5;  // sous le seuil -> doit apparaître en stock faible
    c = ctx.inventory.saveComponent(c);
    std::printf("saved id=%s name=%s\n", c.id.c_str(), c.name.c_str());

    // Mouvement de stock daté.
    StockMovement mv;
    mv.componentId = c.id;
    mv.type = StockMovementType::In;
    mv.quantity = 10;
    mv.timestamp = chdesktop::nowIso();
    ctx.inventory.recordMovement(mv);

    // Les autres tables doivent elles aussi traverser une fermeture/réouverture
    // de base : c'est le minimum pour détecter une colonne ou un dépôt oublié.
    Location location;
    location.name = "Étagère A";
    location = ctx.inventory.saveLocation(location);

    Project project;
    project.name = "Test SQLite";
    project = ctx.projects_service.saveProject(project);

    ProjectComponent projectComponent;
    projectComponent.projectId = project.id;
    projectComponent.componentId = c.id;
    projectComponent.quantityRequired = 2;
    ctx.projectComponents.save(projectComponent);

    Document document;
    document.ownerId = c.id;
    document.title = "Fiche test";
    document.url = "https://example.test/datasheet";
    ctx.documents.save(document);

    RefItem reference;
    reference.list = "manufacturers";
    reference.value = "morfredus";
    ctx.referentials.save(reference);

    // Chaque écriture locale doit être visible pour le PUSH incrémental.
    bool allPendingForSync = true;
    for (auto* repository : ctx.syncables())
        allPendingForSync = allPendingForSync && !repository->exportForSync(0).empty();

    // Relecture depuis le disque via une nouvelle instance.
    chdesktop::AppContext reloaded(dir.string());
    auto all = reloaded.inventory.listComponents();
    std::printf("reloaded %zu component(s)\n", all.size());
    for (auto& x : all)
        std::printf("  #%s %-20s qty=%d cat=%s notes=\"%s\"\n",
                    x.id.c_str(), x.name.c_str(), x.quantity, x.category.c_str(), x.notes.c_str());

    // La catégorie doit avoir été créée automatiquement par saveComponent et
    // toutes les tables doivent être relues depuis SQLite.
    const bool complete = all.size() == 1 &&
        reloaded.inventory.listCategories().size() == 1 &&
        reloaded.inventory.listLocations().size() == 1 &&
        reloaded.projects_service.listProjects().size() == 1 &&
        reloaded.projectComponents.findByProject(project.id).size() == 1 &&
        reloaded.documents.findByOwner(DocumentOwnerKind::Component, c.id).size() == 1 &&
        reloaded.referentials.findByList("manufacturers").size() == 1 &&
        reloaded.inventory.history(c.id).size() == 1 && allPendingForSync;
    std::printf("all SQLite repositories: %s\n", complete ? "ok" : "failed");

    std::printf("database: %s\n", (dir / "componenthub.sqlite").string().c_str());
    return complete ? 0 : 1;
}
