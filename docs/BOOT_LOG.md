# Module optionnel — BootLog (journal de redémarrage)

`BootLog` est un module métier optionnel, fourni à titre d'exemple concret
de module **entièrement désactivable, voire supprimable**, sans aucun
impact sur le reste du framework. Il illustre le découplage Module/Service
décrit dans [docs/ARCHITECTURE.md](ARCHITECTURE.md).

## À quoi ça sert

Capturer le maximum d'informations utiles juste avant un redémarrage
(volontaire, crash, watchdog, brownout...), sans avoir besoin d'un moniteur
série branché au moment des faits :

- Raison du reset précédent (`esp_reset_reason()` — panic, watchdog,
  brownout, reset logiciel, mise sous tension...).
- Les derniers logs applicatifs émis juste avant ce reset.
- Un instantané périodique de l'état système (heap libre, plus gros bloc
  libre, nombre d'équipements connus, pages/API servies) — rafraîchi toutes
  les `BOOT_LOG_STATS_INTERVAL_MS` millisecondes.
- Le dernier état WiFi connu (statut, RSSI, IP).
- La dernière "tâche" signalée via `setLastTask()` (ex. "Scan réseau"),
  utile pour savoir ce qui tournait juste avant un crash muet.
- Compteurs cumulés `boot_count` / `crash_count`, persistés en NVS via
  `ConfigManager` (survivent même à une coupure secteur).

L'historique des redémarrages (borné, FIFO) est consultable en JSON sur
`GET /api/bootlog`, et affiché sur la page web `/debug`
(`web_src/debug.html`).

## Activation / désactivation

Le module est **désactivé par défaut**. Pour l'activer, décommenter une
seule ligne dans `include/project_config.h` :

```cpp
#define ENABLE_BOOT_LOG
```

Pour le désactiver à nouveau, recommenter cette même ligne — rien d'autre
à toucher. Constantes ajustables, définies juste après dans le même fichier :

```cpp
#define MAX_BOOT_LOG_ENTRIES        10      // nombre de boots conservés (FIFO)
#define BOOT_LOG_BUFFER_LINES       20      // lignes de logs conservées par boot
#define BOOT_LOG_LINE_MAX_LEN       160     // longueur max d'une ligne de log
#define BOOT_LOG_STATS_INTERVAL_MS  30000   // période de l'instantané système
```

## Points d'intégration au framework

Le module suit le pattern `Module` standard (`src/core/module.h`) et
s'enregistre comme tout autre module dans `src/main.cpp` :

```cpp
#ifdef ENABLE_BOOT_LOG
#include "modules/boot_log/boot_log.h"
#endif
...
#ifdef ENABLE_BOOT_LOG
    app.modules.add(&bootLogModule);
#endif
```

Un seul point d'intégration supplémentaire, hors du pattern `Module`
standard, est nécessaire pour capturer les logs applicatifs : un hook dans
`src/services/log_manager/log_manager.cpp`, qui transmet chaque ligne de
log au buffer circulaire du module :

```cpp
#ifdef ENABLE_BOOT_LOG
    bootLogModule.capture(_levelName(level), tag, buf);
#endif
```

Le lien de navigation vers `/debug` apparaît automatiquement dans le menu
(`web_src/menu.js`) **uniquement si le module est actif** : `menu.js` sonde
`GET /api/bootlog` au chargement de chaque page et n'ajoute le lien que si
la route répond (`r.ok`). Module désactivé ou retiré ⇒ la route n'existe
pas (404) ⇒ aucun lien affiché, sans aucune configuration supplémentaire.

Ce sont les **trois seuls fichiers du framework** touchés par ce module
(`main.cpp`, `log_manager.cpp` et `web_src/menu.js`) : les deux premiers via un
bloc `#ifdef ENABLE_BOOT_LOG` isolé et trivial à identifier/retirer, le
troisième via une sonde HTTP sans dépendance de compilation.

## API publique du module (`BootLogModule`, instance globale `bootLogModule`)

| Méthode | Rôle |
|---|---|
| `begin()` | Appelée automatiquement par `ModuleManager::beginAll()`. Lit la raison du dernier reset, persiste l'historique du boot précédent via `StorageManager`, incrémente `boot_count`/`crash_count` via `ConfigManager`, puis réinitialise le buffer RTC pour le boot en cours. |
| `loop()` | Appelée automatiquement par `ModuleManager::loopAll()`. Mise à jour de l'uptime à chaque appel, instantané complet (heap, WiFi, compteurs) toutes les `BOOT_LOG_STATS_INTERVAL_MS` ms. |
| `registerRoutes(router)` | Appelée automatiquement par `ModuleManager::registerAllRoutes()`. Enregistre `GET /api/bootlog`, `DELETE /api/bootlog` et `GET /debug`. |
| `capture(level, tag, msg)` | Ajoute une ligne au buffer circulaire courant. Appelée depuis le hook de `LogManager` (voir ci-dessus) — pas d'appel direct nécessaire dans un module métier. |
| `setLastTask(const String&)` | À appeler avant une opération risquée (scan réseau, écriture fichier...) pour savoir ce qui tournait juste avant un crash muet. |
| `setDevicesCountProvider(std::function<uint32_t()>)` | Fournisseur optionnel du nombre d'équipements connus, inclus dans les instantanés périodiques. |
| `notePageServed()` / `noteApiCall()` | Compteurs cumulés, à appeler depuis un module exposant ses propres pages/routes si on souhaite les voir apparaître dans les instantanés. |
| `getLogJson()` | Historique des boots persistés (JSON), du plus récent au plus ancien — utilisé par la route `GET /api/bootlog`. |
| `clear()` | Vide l'historique persisté (ne touche pas aux compteurs NVS `boot_count`/`crash_count`). |

## Point d'attention — RAM RTC et `kMagic`

Le buffer circulaire et l'instantané système sont conservés dans une zone
`RTC_NOINIT_ATTR` (RAM qui survit à un reboot logiciel/crash/watchdog, mais
pas à une coupure d'alimentation ni à un reset franc par bouton). Cette RAM
**survit aussi à un flash de firmware** (elle n'est pas effacée par
l'écriture de la partition application).

Un `kMagic` (défini dans `boot_log.cpp`) sert à valider que le contenu RTC
est exploitable au boot suivant. **Si la structure interne `BootLogRtcData`
change de layout** (champs ajoutés/retirés/réordonnés, ou changement de
`BOOT_LOG_BUFFER_LINES`/`BOOT_LOG_LINE_MAX_LEN`), **`kMagic` doit être
incrémenté** dans `boot_log.cpp`. Sans cela, le premier boot après un flash
relirait les anciens octets RTC à travers le nouveau layout et produirait
des données corrompues (pouvant casser le `JSON.parse()` côté page
`/debug`).

## Limite connue

Pas de capture de trace d'appel (stack trace) au moment d'un PANIC : le
framework Arduino n'expose aucun hook applicatif exécuté pendant un PANIC
ESP-IDF — à cet instant, le code utilisateur (donc ce module) ne tourne
déjà plus. Une vraie capture de backtrace nécessiterait le composant
ESP-IDF `esp_core_dump` (zone flash/RTC dédiée + outil décodeur côté PC),
hors de portée d'un module applicatif autonome. La trace réelle du PANIC
reste donc visible uniquement sur le moniteur série (comportement par
défaut de l'ESP-IDF) ; ce module se contente de capturer la raison du reset
et les derniers logs applicatifs avant la coupure, ce qui couvre l'essentiel
des besoins de débogage sans moniteur série branché.

## Suppression complète

Si le module n'est définitivement plus nécessaire, au-delà de la simple
désactivation (`// #define ENABLE_BOOT_LOG`) :

1. Retirer le bloc `#ifdef ENABLE_BOOT_LOG` dans
   `src/services/log_manager/log_manager.cpp` (capture des logs) et son
   `#include` associé en tête de fichier.
2. Retirer le bloc `#ifdef ENABLE_BOOT_LOG` (include + `app.modules.add`)
   dans `src/main.cpp`.
3. Supprimer le dossier `src/modules/boot_log/` (`boot_log.h`, `boot_log.cpp`).
4. Supprimer `web_src/debug.html`.
5. Retirer la macro `ENABLE_BOOT_LOG` et les constantes associées dans
   `include/project_config.h`.
6. Retirer, si présentes, les règles CSS `.bootlog-entry` dans `web_src/style.css`.
7. Optionnel : retirer la sonde `/api/bootlog` dans `web_src/menu.js` (sans
   risque de l'oublier — une fois la route absente, elle ne fait jamais
   apparaître de lien et reste un appel HTTP mort sans conséquence).

Aucune autre partie du framework ne dépend de ce module.
