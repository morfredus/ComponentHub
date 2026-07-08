/**
 * BootLogModule — Journal de redémarrage (raison du reset, derniers logs,
 * instantané système, dernier état WiFi) survivant aux crashs/watchdog.
 *
 * Module optionnel et autonome : entièrement gardé par ENABLE_BOOT_LOG
 * (project_config.h). Pour le désactiver, commentez cette macro — pour le
 * supprimer complètement, voir la note en bas de ce fichier.
 *
 * Fonctionnement :
 *   - Un buffer circulaire des derniers logs est conservé en RTC_NOINIT_ATTR
 *     (RAM qui survit à un reboot logiciel/crash/watchdog, mais pas à une
 *     coupure d'alimentation ni à un reset franc). Chaque ligne est un objet
 *     JSON compact (timestamp, niveau, tag, message, heap libre, plus gros
 *     bloc libre au moment du log).
 *   - Le même bloc RTC conserve aussi un instantané périodique de l'état
 *     système (RuntimeStats), le dernier "task" connu (setLastTask()) et le
 *     dernier état WiFi observé — mis à jour par loop().
 *   - Au démarrage suivant, begin() lit la raison du reset et, si le buffer
 *     précédent est valide, persiste tout cela via StorageManager (borné à
 *     MAX_BOOT_LOG_ENTRIES, FIFO).
 *   - boot_count / crash_count sont stockés en NVS via ConfigManager (seule
 *     mémoire du projet qui survit aussi à une coupure secteur).
 *
 * Suppression complète si plus nécessaire :
 *   1. Retirer le bloc ENABLE_BOOT_LOG de log_manager.cpp (capture des logs).
 *   2. Retirer l'instanciation conditionnelle dans main.cpp.
 *   3. Retirer web_src/debug.html et son entrée dans web_src/menu.js.
 *   4. Supprimer ce dossier (src/modules/boot_log/) et la macro
 *      ENABLE_BOOT_LOG / les constantes associées dans project_config.h.
 *   Aucune autre partie du framework ne dépend de ce module.
 */

#pragma once
#include <Arduino.h>
#include <functional>
#include "../../core/module.h"

// Instantané périodique de l'état système, rafraîchi toutes les
// BOOT_LOG_STATS_INTERVAL_MS millisecondes — survit en RTC.
struct BootLogRuntimeStats {
    uint32_t uptime       = 0;
    uint32_t freeHeap     = 0;
    uint32_t largestBlock = 0;
    uint32_t devicesCount = 0;
    uint32_t pagesServed  = 0;
    uint32_t apiCalls     = 0;
};

class BootLogModule : public Module {
public:
    const char* name() const override { return "BootLog"; }

    // Lit la raison du reset précédent, persiste l'historique, puis
    // réinitialise le buffer RTC pour le boot en cours.
    void begin() override;

    // Heartbeat (uptime/instantané) — peu coûteux, appelé à chaque loop().
    void loop() override;

    // Routes /api/bootlog (GET/DELETE) et /debug.
    void registerRoutes(WebRouter& router) override;

    // Ajoute une ligne au buffer circulaire courant — appelé depuis le hook
    // optionnel de LogManager (voir log_manager.cpp, bloc ENABLE_BOOT_LOG).
    void capture(const char* level, const char* tag, const char* msg);

    // Dernière "tâche" en cours (ex: "Scan réseau"), conservée en RTC et
    // incluse dans l'entrée persistée au boot suivant.
    void setLastTask(const String& task);

    // Fournisseur optionnel du nombre d'équipements connus, inclus dans les
    // RuntimeStats périodiques.
    void setDevicesCountProvider(std::function<uint32_t()> provider);

    // Compteurs cumulés exposés dans RuntimeStats.
    void notePageServed();
    void noteApiCall();

    // Historique des boots persistés, du plus récent au plus ancien (JSON).
    String getLogJson() const;

    // Vide l'historique persisté (ne touche pas aux compteurs NVS).
    void clear();

private:
    uint32_t _lastStatsSnapMs = 0;
    std::function<uint32_t()> _devicesCountProvider;
};

extern BootLogModule bootLogModule;
