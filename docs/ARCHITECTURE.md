# Architecture — ComponentHub

## Vision

ComponentHub n'est pas un simple inventaire de composants électroniques :
c'est la mémoire technique complète d'un atelier — composants, modules,
outils, consommables, emplacements, projets, documentation et historique,
au même endroit.

Le projet part de la base **ESP32-Foundation** (services génériques WiFi,
web, OTA, stockage, logs, configuration, réutilisables tels quels) et
construit par-dessus un cœur métier dédié, pensé dès le départ pour être
**portable** : le firmware ESP32-S3 est la première cible, mais l'objectif
est qu'environ 90 % du code métier soit réutilisable sur un futur portage
Raspberry Pi / Linux / Windows, sans réécriture majeure.

## Principe directeur

> Le domaine métier (`src/domain/`) ne connaît jamais la plateforme
> (LittleFS, ArduinoJson, WiFi, GPIO...). La plateforme ne connaît jamais
> les règles métier — elle se contente d'implémenter les interfaces que le
> domaine expose.

```
domain/    → entités + interfaces de dépôt + services métier — AUCUNE
             dépendance Arduino/ESP32. C'est la partie à porter telle
             quelle sur une autre plateforme.
storage/   → implémentation ESP32 des interfaces de dépôt (fichiers JSON
             sur LittleFS via ArduinoJson). Un portage Raspberry Pi/Linux
             n'a besoin de ré-implémenter que ces interfaces (ex: SQLite).
core/      → App, Module, ModuleManager — orchestration uniquement,
             hérité d'ESP32-Foundation.
services/  → WiFi, Web, OTA, Storage (bas niveau), Config, Log, SystemInfo,
             mDNS, Time — hérité d'ESP32-Foundation, inchangé.
api/       → WebRouter — façade de routage HTTP exposée aux modules.
modules/   → colle domaine <-> HTTP : chaque module possède ses propres
             instances de dépôts (storage/) et de service (domain/), et
             traduit JSON HTTP <-> types du domaine.
web_src/   → interface web (HTML/CSS/JS), embarquée dans le firmware en
             PROGMEM (voir plus bas) — plus servie depuis LittleFS.
```

## Cœur métier (`src/domain/`)

Aucun fichier de ce dossier n'inclut `<Arduino.h>` ni aucun en-tête
ESP32/ArduinoJson : uniquement `<string>`, `<vector>`, `<optional>`,
`<map>`. C'est ce qui permet de le compiler tel quel dans un environnement
hébergé (Raspberry Pi, PC) le jour venu.

### Entités

| Entité | Fichier | Rôle |
|---|---|---|
| `Component` | `component.h` | Fiche complète d'un composant/module/outil/consommable (`kind` distingue ces familles). |
| `Location` | `location.h` | Emplacement, hiérarchisable via `parentId`. |
| `Category` | `category.h` | Catégorie de composant, liste plate (pas de hiérarchie). Créée à la volée par `InventoryService::saveComponent` si le champ texte `category` d'un composant ne correspond à aucune entrée existante. |
| `StockMovement` | `stock_movement.h` | Mouvement de stock (in/out/correction/inventory). |
| `Project` | `project.h` | Fiche projet. |
| `ProjectComponent` | `project_component.h` | Ligne de nomenclature : un composant de l'inventaire *ou* un élément hors inventaire (`componentId == 0` + `label` libre, toujours « à acheter »), en quantité. |
| `Document` | `document.h` | Document rattaché à un composant ou un projet (`ownerKind`/`ownerId`). |

### Interfaces de dépôt (`repositories.h`, `project_repositories.h`, `document_repository.h`)

Chaque entité persistée a une interface `I<Entité>Repository` (CRUD minimal
+ `saveAll` pour les upserts en lot). Le domaine ne connaît que ces
interfaces ; `src/storage/` fournit l'unique implémentation actuelle
(fichiers JSON sur LittleFS, une classe `<Entité>JsonRepository` par
interface).

### Services métier

| Service | Dépend de | Rôle |
|---|---|---|
| `InventoryService` | `IComponentRepository`, `ILocationRepository`, `ICategoryRepository`, `IStockMovementRepository` | CRUD composants/emplacements/catégories, recherche/filtres, enregistrement des mouvements de stock (met à jour la quantité), détection du stock sous le seuil minimum. |
| `ProjectService` | `IProjectRepository`, `IProjectComponentRepository`, `IComponentRepository` (lecture seule) | CRUD projets, nomenclature, calcul des composants manquants (quantité requise vs. stock réel). Supprimer un projet nettoie sa nomenclature en cascade. |
| `DocumentService` | `IDocumentRepository` | Façade fine au-dessus du dépôt (CRUD par propriétaire). |
| `ImportExportService` | `IComponentRepository`, `ILocationRepository` | Export/import CSV (format natif et Bomist) — voir [docs/API.md](API.md#import--export) pour le détail du mapping. |

## Persistance (`src/storage/`)

LittleFS ne contient plus que deux catégories de données, toutes deux
créées à l'exécution — jamais pré-remplies au moment du build : les
fichiers JSON de l'inventaire, et les documents utilisateur uploadés
(datasheets, PDF...) via `/api/files/upload`. L'interface web elle-même
(HTML/CSS/JS) n'y vit plus du tout — voir
[Interface web embarquée (PROGMEM)](#interface-web-embarquée-progmem).

Chaque dépôt JSON stocke un tableau dans un fichier LittleFS dédié
(`/inventory_components.json`, `/inventory_locations.json`,
`/inventory_stock_movements.json`, `/projects.json`,
`/project_components.json`, `/inventory_documents.json`), sous la forme
`{"nextId": N, "items": [...]}`. `json_store_util.h` centralise le
chargement/l'écriture :

- **Écriture atomique** — `save()` écrit d'abord dans un fichier `.tmp`
  puis le renomme par-dessus l'original. Une coupure d'alimentation en
  plein milieu d'écriture laisse le fichier de données existant intact
  (seul le `.tmp`, jamais lu, serait incomplet).
- **Corruption détectée, jamais silencieuse** — si un fichier existant ne
  peut pas être parsé, une erreur est journalisée (`LOG_ERROR`) au lieu de
  traiter silencieusement l'inventaire comme vide.
- **`saveAll` pour les imports en masse** — un upsert en boucle
  (`save()` par ligne) relit/réécrit tout le fichier à chaque élément, un
  coût quadratique qui a réellement provoqué un reset watchdog en usage
  réel (import de 39 lignes ≈ 150s avant l'incident). `saveAll()` fait un
  seul chargement, applique tous les changements en mémoire, puis une seule
  écriture.

## Modules (`src/modules/`)

| Module | Pages | Rôle |
|---|---|---|
| `InventoryModule` | `/inventory`, `/locations`, `/categories`, `/settings` | Composants, emplacements, catégories, mouvements de stock. `/settings` est une page de navigation (cartouches) sans lien direct avec l'inventaire — aucun module ne la possède plus légitimement qu'un autre. |
| `ProjectModule` | `/projects` | Projets, nomenclature, composants manquants. |
| `DocumentModule` | — (utilisé en dialog depuis inventaire/projets) | Documents rattachés à un composant ou un projet. |
| `ImportExportModule` | `/import-export` | Export/import CSV, suivi des dates de dernier import/export/sauvegarde. |
| `BootLogModule` | `/debug` | Optionnel (`ENABLE_BOOT_LOG`) — voir [docs/BOOT_LOG.md](BOOT_LOG.md). |

Chaque module possède **ses propres instances** des dépôts JSON dont il a
besoin (ex. `InventoryModule` et `ProjectModule` ont chacun leur
`ComponentJsonRepository`, pointant vers le même fichier). Ces dépôts sont
sans état — chaque appel relit/réécrit LittleFS — donc plusieurs instances
restent cohérentes sans coordination particulière ; c'est ce qui permet à
chaque module de rester indépendant des autres sans registre central de
dépendances.

Référence complète des routes exposées par chaque module :
[docs/API.md](API.md).

## Flux de démarrage (`App::begin`)

1. `ConfigManager::begin` — namespace NVS partagé.
2. `StorageManager::begin` — montage LittleFS.
3. `WiFiManager::begin` — connexion ou portail captif (voir
   [docs/WIFI_SETUP.md](WIFI_SETUP.md)).
4. Au premier WiFi OK (callback) : `MdnsManager`, `OtaManager`,
   `TimeManager` (NTP — nécessaire pour un horodatage correct des
   mouvements de stock et des exports/imports).
5. `WebManager::begin` — démarre `AsyncWebServer` et les routes core
   (`/`, `/logs`, `/files`, `/system`, `/ota`).
6. `ModuleManager::registerAllRoutes` — routes métier via `WebRouter`.
7. `ModuleManager::beginAll` — initialisation des modules métier.

## Pourquoi ESPAsyncWebServer (et pas WebServer synchrone) ?

Le socle ESP32-Foundation utilisait `WebServer` (synchrone, zéro
dépendance). ComponentHub bascule sur **ESPAsyncWebServer** : seule la
couche `services/web_manager/`, `api/api_router/` et
`services/ota_manager/` connaît ce choix — WiFi, stockage, logs,
configuration restent identiques à la base. Le portage a nécessité de
réécrire les handlers de chaque module (`AsyncWebServerRequest*` au lieu de
`server.arg(...)`), mais aucune ligne de `src/core/` ni `src/domain/` n'a dû
changer, exactement ce que ce découplage est censé garantir.

`WebRouter` (`src/api/api_router/api_router.h`) expose en plus deux
méthodes que l'API native d'ESPAsyncWebServer ne fournit pas directement :

- `withBody(path, method, handler)` — accumule les chunks d'un corps de
  requête (JSON, CSV) et n'appelle `handler` qu'une fois le corps complet
  reçu.
- `withUpload(path, method, onDone, onUpload)` — upload de fichier
  multipart (utilisé par `/api/files/upload` et `/api/ota/update`).

## Interface web embarquée (PROGMEM)

L'UI (`web_src/`) n'est plus servie depuis LittleFS : elle est **compilée
dans le firmware**, en PROGMEM. Pipeline, entièrement automatique
(`extra_scripts` dans `platformio.ini`, avant chaque `pio run`) :

```
web_src/  --[tools/minify_web.py]-->  web/  --[tools/generate_web_assets.py]-->  src/generated/web_assets_data.cpp
  (source)         (minifié)                        (tableaux d'octets PROGMEM + table de routage)
```

`src/services/web_assets/web_assets.h` expose l'API stable utilisée par
`WebManager` et les modules : `WebAssets::find(path)` (recherche par chemin
exact) et `WebAssets::send(request, path)` (répond avec le bon
Content-Type, connu à la compilation). `src/generated/web_assets_data.cpp`
est régénéré à chaque build — jamais édité à la main, jamais versionné
(`.gitignore`).

**Pourquoi ce choix plutôt que LittleFS** : `pio run --target uploadfs`
reflashe l'intégralité de la partition LittleFS — y compris les fichiers de
données de l'inventaire, puisqu'ils vivent sur la même partition que
l'ancienne UI. Ce n'était pas juste un risque théorique : c'est
concrètement ce qui a effacé l'inventaire à plusieurs reprises en cours de
développement. Avec l'UI en PROGMEM :

- `pio run --target upload` met à jour firmware **et** interface web en un
  seul geste — une mise à jour OTA aussi.
- LittleFS ne contient plus que les données de l'inventaire : plus aucune
  opération de build ne peut jamais l'effacer.
- Plus besoin d'étape séparée (`uploadfs`) ni d'outil de synchronisation
  dédié pour mettre à jour l'UI sur un appareil déjà provisionné.
- Portage Raspberry Pi : un serveur hébergé peut servir `web/` directement
  depuis le disque (pas de contrainte flash/PROGMEM) — seule
  l'implémentation de `WebAssets::send` changerait, jamais son API.

Limite acceptée : toute modification de `web_src/` nécessite de reflasher
le firmware (`pio run --target upload`), pas de mise à jour à chaud de
l'UI seule. Pour un outil mono-utilisateur sur réseau local, ce compromis
est jugé largement préférable au risque de perte de données.

## Principe d'interface — navigation "pilotable"

Le menu principal reste volontairement **plat** (`Labo`, `Inventaire`,
`Projets`, `Paramètres`, `Système` — pas de sous-menu déroulant au
survol/clic) : l'objectif est une interface utilisable aujourd'hui à la
souris/au tactile, mais conçue pour rester navigable plus tard avec un
simple bouton rotatif (tourner = déplacer le focus, appuyer = valider) — un
contrôle physique qui n'implémente pas la notion d'un menu qu'il faudrait
d'abord "ouvrir". Concrètement :

- `Paramètres` et `Système` sont des **sections** : un regroupement de
  sous-pages (Paramètres → Emplacements/Catégories/Import-Export ; Système →
  Fichiers/Mise à jour/Logs/Debug) présenté sous forme d'une **barre
  d'onglets** (`.section-tabs`/`.section-tab`), pas de menus déroulants.
- La barre d'onglets d'une section est **injectée par `menu.js`** en haut de
  chaque page de la section — page racine (`/settings`, `/system`) comme
  sous-pages (`/locations`, `/files`...). L'onglet de la page courante est
  mis en évidence (`.section-tab.active`) et le lien de menu principal
  correspondant reste actif. On peut ainsi passer d'une page sœur à l'autre
  sans revenir en arrière. Définitions centralisées dans `menu.js` (jamais
  dupliquées dans les pages) ; l'onglet `Debug` n'est ajouté que si la sonde
  `/api/bootlog` répond (module `ENABLE_BOOT_LOG` actif).
- Onglets et accès rapides (tableau de bord) sont des `<a>` natifs, jamais
  des `<div>` avec un gestionnaire de clic : le focus clavier (et donc, plus
  tard, la sélection au bouton rotatif) fonctionne sans code supplémentaire.

## Ajouter un nouveau module

1. Créer `src/modules/<nom>/<nom>_module.h` : une classe héritant de
   `Module` (`src/core/module.h`), redéfinir au minimum `name()`, et
   `registerRoutes(WebRouter&)` si le module expose des routes HTTP.
2. Si le module a besoin de persistance : définir l'entité et l'interface
   de dépôt dans `src/domain/`, l'implémentation JSON dans `src/storage/`.
   La logique métier (règles, calculs) va dans un service `domain/`, jamais
   dans le module lui-même.
3. Instancier et enregistrer le module dans `src/main.cpp` :
   ```cpp
   static MonModule monModule;
   // dans setup(), avant app.begin() :
   app.modules.add(&monModule);
   ```
4. **Piège classique avec les lambdas de route** : dans
   `registerRoutes(WebRouter& router)`, ne jamais capturer `router`
   lui-même par référence dans un handler HTTP (c'est un objet temporaire,
   détruit à la fin de `registerRoutes()`). Utiliser `router.get/post/on/
   withBody/withUpload(...)` directement — ces méthodes ne capturent que ce
   dont elles ont besoin en interne.
5. **Ordre d'enregistrement des routes = priorité de correspondance.**
   ESPAsyncWebServer teste les handlers dans l'ordre où ils sont enregistrés
   et fait correspondre par préfixe : un handler sur `/api/foo` capte aussi
   `/api/foo/bar` (`url.startsWith("/api/foo/")`). Toujours enregistrer les
   routes spécifiques (`/api/foo/bar`) **avant** la route générique
   (`/api/foo`), sous peine que la liste réponde à la place de la sous-route
   (bug réel rencontré sur `/api/projects` vs `/api/projects/bom`).

## Limites connues

- Pas de suppression en cascade des documents/mouvements de stock quand un
  composant est supprimé (les mouvements/documents deviennent orphelins,
  sans effet fonctionnel visible mais accumulés silencieusement).
- Sous-catégorie reste un champ texte libre sur `Component`, sans entité
  dédiée (contrairement à Catégorie, qui en a une depuis peu). Catégorie
  n'a par ailleurs aucune hiérarchie (liste plate) — seuls les emplacements
  sont hiérarchisables, conformément au cahier des charges.
- Supprimer une Catégorie ou un Emplacement ne modifie pas les composants
  qui les référençaient encore par leur nom/id (pas de nettoyage en
  cascade, cohérent avec la limite ci-dessus sur les documents/mouvements).
- Import/export CSV suppose un fichier encodé en UTF-8 — pas de détection
  ni de conversion Windows-1252 côté ESP32.
