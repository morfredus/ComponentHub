// Smoke test (headless): checks that the domain compiles and runs on a PC,
// and that SQLite storage keeps the data correctly (accents + €).
// Not part of the final application.
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
    c.minStock = 5;  // below the threshold -> must show up in low stock
    c = ctx.inventory.saveComponent(c);
    std::printf("saved id=%s name=%s\n", c.id.c_str(), c.name.c_str());

    // Dated stock movement.
    StockMovement mv;
    mv.componentId = c.id;
    mv.type = StockMovementType::In;
    mv.quantity = 10;
    mv.timestamp = chdesktop::nowIso();
    ctx.inventory.recordMovement(mv);

    // The other tables must also survive closing and reopening
    // the database: this is the minimum to detect a forgotten column or repository.
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

    // Every local write must be visible to the incremental PUSH.
    bool allPendingForSync = true;
    for (auto* repository : ctx.syncables())
        allPendingForSync = allPendingForSync && !repository->exportForSync(0).empty();

    // Reading back from disk through a new instance.
    chdesktop::AppContext reloaded(dir.string());
    auto all = reloaded.inventory.listComponents();
    std::printf("reloaded %zu component(s)\n", all.size());
    for (auto& x : all)
        std::printf("  #%s %-20s qty=%d cat=%s notes=\"%s\"\n",
                    x.id.c_str(), x.name.c_str(), x.quantity, x.category.c_str(), x.notes.c_str());

    // The category must have been created automatically by saveComponent and
    // all tables must be read back from SQLite.
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
