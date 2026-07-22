# Architecture — ComponentHub (version bureau)

Ce document explique **comment le code est organisé et pourquoi**. Il s'adresse
autant au contributeur qu'au débutant curieux qui veut lire les sources et
comprendre « qui fait quoi ». Les règles de contribution (portabilité, style)
sont dans [CONTRIBUTING.md](../../CONTRIBUTING.md).

## Idée directrice : des couches qui ne dépendent que « vers l'intérieur »

ComponentHub est découpé en couches. Une couche ne connaît que celles situées
**en dessous** d'elle, jamais au-dessus :

```
  Interface Qt (src/ui/)              ← la SEULE couche qui connaît Qt
        │  appelle
        ▼
  Services métier (src/domain/)       ← les règles, indépendantes de tout
        │  s'appuient sur des interfaces
        ▼
  Dépôts / stockage (src/storage/)    ← lecture/écriture des fichiers JSON
        │
  Aides plateforme (src/platform/)    ← archive .tar, horloge (API portable)
```

> **Le cœur métier (`src/domain/`) ne connaît jamais la plateforme** : ni Qt, ni
> le système d'exploitation. Il ne manipule que des types standard C++
> (`std::string`, `std::vector`, `std::optional`, `std::map`). C'est précisément
> ce qui permet de le compiler tel quel sur Windows, Linux et Raspberry Pi.

Concrètement, cela veut dire qu'on peut :

- changer l'**interface** (une page Qt) sans toucher aux règles métier ;
- changer le **stockage** (passer un jour de JSON à SQLite) sans toucher ni à
  l'interface ni aux règles ;
- **tester** la logique métier sans lancer d'interface graphique.

## Les dossiers, un par un

```
src/
├── domain/     Cœur métier : entités + interfaces de dépôt + services.
│               AUCUNE dépendance Qt/Arduino/OS. C++17 pur.
├── storage/    Persistance bureau : fichiers JSON (nlohmann/json) qui
│               implémentent les interfaces de dépôt du domaine.
├── platform/   Aides proches de l'OS derrière une API portable (Tar, Clock).
├── ui/         La SEULE couche qui connaît Qt (Theme, Icons, pages, dialogues).
└── main.cpp    Assemblage : crée l'AppContext, l'ouvre dans la MainWindow.
```

### `src/domain/` — le cœur métier

Trois familles de fichiers :

- **Entités** (`component.h`, `location.h`, `category.h`, `project.h`,
  `stock_movement.h`, `document.h`, `project_component.h`, `referential.h`) :
  de simples structures de données décrivant ce qu'on stocke.
- **Interfaces de dépôt** (`repositories.h`, `project_repositories.h`,
  `document_repository.h`, `referential_repository.h`) : ce que le domaine
  *attend* du stockage (charger, sauver, sauver en lot), sans savoir *comment*
  c'est fait. Ce sont des classes abstraites (`I…Repository`).
- **Services** (`inventory_service`, `project_service`, `document_service`,
  `import_export_service`, plus `csv`) : **toute la logique**. Exemples : ajouter
  un composant, enregistrer un mouvement de stock et mettre à jour la quantité,
  calculer les composants manquants d'un projet, importer/exporter du CSV.

Un service ne connaît que des **interfaces** de dépôt, jamais leur
implémentation. C'est ce qui rend le cœur portable et testable.

### `src/storage/` — la persistance bureau

`FileRepositories` fournit **l'unique implémentation actuelle** des interfaces du
domaine : chaque table est un fichier JSON `{ "nextId": N, "items": [ … ] }`
(`JsonStore.h`). Les écritures sont **atomiques** (fichier `.tmp` puis
renommage), et un fichier illisible est signalé plutôt que traité en silence
comme vide.

Un futur backend (SQLite, par exemple) se limiterait à ré-implémenter ces
interfaces — le reste du programme ne changerait pas.

### `src/platform/` — les aides proches du système

Ce qui touche à l'OS mais doit rester **portable** : `Tar.cpp` (archive `.tar`
de sauvegarde/restauration, au format ustar) et `Clock.h`
(horodatage). Si un jour un besoin n'a *aucune* solution portable, c'est **ici,
et seulement ici**, qu'un cas particulier serait isolé — avec un rendu garanti
identique sur les trois OS (voir la règle de portabilité dans
[CONTRIBUTING.md](../../CONTRIBUTING.md)).

### `src/ui/` — l'interface Qt

La seule couche qui inclut Qt. On y trouve :

- **`MainWindow`** : la coquille (barre latérale de navigation + pile de pages +
  barre de menus).
- **Une page par section** : `InventoryPage`, `CategoriesPage`, `LocationsPage`,
  `ProjectsPage`, `ReferentialsPage`, `ImportExportPage`, `SettingsPage`, et le
  dialogue `ComponentDialog` (la fiche composant).
- **`Theme`** : thème clair/sombre. Les couleurs viennent de jetons
  (`resources/themes/*.theme` + `app.qss`), **jamais codées en dur** dans les
  widgets — c'est ce qui fait fonctionner les deux thèmes.
- **`Icons`** : pictogrammes dessinés au `QPainter` (grille 24×24), teintés à la
  couleur du thème — pas de police d'icônes ni de module SVG à installer.
- **`UiKit.h`** : petites fabriques communes (titre de page, boutons, indices).

Les pages **n'écrivent jamais dans les fichiers directement** : elles appellent
les services du domaine via l'`AppContext`.

### L'assemblage : `AppContext` et `main.cpp`

`AppContext` (dans `src/AppContext.h`) est le **point d'assemblage** : pour un
dossier de données donné, il crée les dépôts fichier puis les services qui s'en
servent, et les expose. Chaque page reçoit une référence à cet `AppContext`.
`main.cpp` se contente de : choisir le dossier de données
(`QStandardPaths::AppDataLocation`), appliquer le thème, créer l'`AppContext`,
ouvrir la `MainWindow`.

```
main.cpp ─► AppContext(dossier) ─► crée dépôts (storage/) + services (domain/)
                                        │
                          MainWindow ───┘  (chaque page reçoit l'AppContext)
```

## Où va mon changement ?

| Ce que vous ajoutez | Où |
|---|---|
| Une règle / un calcul métier | un **service** dans `src/domain/` |
| Un champ ou une table à stocker | une entité + interface (`src/domain/`) + son implémentation JSON (`src/storage/`) |
| Un écran, un bouton, un dialogue | `src/ui/` (qui appelle les services) |
| Un service rendu par l'OS (fichiers, temps, archive) | une aide portable dans `src/platform/` |

Respecter ces frontières, c'est ce qui garde le projet **facile à maintenir** :
les bugs restent localisés, et chaque couche évolue sans casser les autres.

## Le format des données

Chaque table est un fichier JSON dans le dossier de données de l'utilisateur
(voir [USER_MANUAL.md](USER_MANUAL.md) pour le chemin exact) :
`inventory_components.json`, `inventory_locations.json`,
`inventory_categories.json`, `inventory_stock_movements.json`,
`inventory_documents.json`, `projects.json`, `project_components.json`,
`referentials.json`. Chacun a la forme `{ "nextId": N, "items": [ … ] }`. Une
**sauvegarde `.tar`** (menu Import/Export) emballe tout cela en un seul fichier,
restaurable à tout moment.

## Modules communs : supervision réseau et mises à jour

Deux petites bibliothèques partagées avec les autres applications de l'atelier
sont **vendorées** dans `third_party/morf/` (compilées dans l'exécutable, sans
dépendance externe) :

- **morfBeacon** (`third_party/morf/beacon/`) — annonce la présence de l'appli sur
  le réseau (heartbeat UDP) et expose ses métriques (`/status` HTTP), pour le
  morfDashboard. Câblé dans `src/main.cpp`.
- **morfUpdate** (`third_party/morf/update/`) — vérifie les *releases* GitHub.
  Câblé dans `src/ui/MainWindow` (menu Aide + vérification au démarrage).

Comme le reste, ces modules ne remontent jamais dans les couches supérieures :
l'interface (`src/ui/`, `src/main.cpp`) les utilise, le cœur métier les ignore.
Détails et maintenance : [SUPERVISION_ET_MAJ.md](SUPERVISION_ET_MAJ.md).

## Pour aller plus loin

- Supervision réseau et mises à jour :
  [SUPERVISION_ET_MAJ.md](SUPERVISION_ET_MAJ.md).
- Pourquoi le bureau est le maître et l'ESP32 un satellite :
  [ADR-0001](ADR-0001-desktop-maitre-esp32-satellite.md).
- Compiler le projet : [GETTING_STARTED.md](GETTING_STARTED.md) et
  [BUILD_DESKTOP.md](BUILD_DESKTOP.md).
- Règles de contribution et de portabilité : [CONTRIBUTING.md](../../CONTRIBUTING.md).
