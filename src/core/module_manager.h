/**
 * ModuleManager — Registre et orchestrateur des modules métier.
 *
 * Permet d'ajouter des modules optionnels (Module*) sans que le core ou les
 * services aient besoin de les connaître. main.cpp enregistre les modules
 * métier de son projet ; App se contente d'appeler begin()/loop()/
 * registerRoutes() sur chacun d'eux.
 */

#pragma once
#include <vector>
#include "module.h"

class ModuleManager {
public:
    void add(Module* module) {
        if (module) _modules.push_back(module);
    }

    void beginAll() {
        for (Module* m : _modules) m->begin();
    }

    void loopAll() {
        for (Module* m : _modules) m->loop();
    }

    void registerAllRoutes(WebRouter& router) {
        for (Module* m : _modules) m->registerRoutes(router);
    }

    const std::vector<Module*>& modules() const { return _modules; }

private:
    std::vector<Module*> _modules;
};
