# ComponentHub

![Platform](https://img.shields.io/badge/Plateforme-Windows%20%7C%20Linux%20%7C%20Raspberry%20Pi-lightgrey)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt)
![Build](https://img.shields.io/badge/CMake-3.21+-064F8C?logo=cmake)

**La mémoire technique d'un atelier électronique** : inventaire de composants,
modules, outils et consommables ; stock et mouvements ; emplacements
hiérarchiques ; documentation attachée ; projets et leur nomenclature.

Ce n'est pas un simple inventaire : l'objectif est de retrouver en un clic, pour
n'importe quel composant, son stock, son emplacement, ses datasheets et les
projets qui l'utilisent.

![Vue Inventaire de ComponentHub](docs/pictures/inventaire.png)

ComponentHub est une **application de bureau Qt / C++17**, native sur
**Windows, Linux (x86_64) et Raspberry Pi (ARM64)**. C'est le **projet maître**,
détenteur de la base de données de référence de l'atelier — sur un stockage
fiable, sans les limites mémoire de l'ESP32. Un **firmware ESP32**, développé
désormais dans un **dépôt séparé** (`ComponentHub-ESP32`, voisin de celui-ci),
en devient un **satellite** : terminal mobile de consultation/modification du
stock (scan QR) qui, à terme, consulte la base de cette version bureau. Le
pourquoi de cette séparation est détaillé dans
[docs/ADR-0001](docs/ADR-0001-desktop-maitre-esp32-satellite.md).

## Cœur métier

Le cœur métier [`src/domain/`](src/domain/) — entités, services (inventaire,
projets, documents, import/export), CSV — ne dépend ni de Qt ni d'Arduino. Le
firmware ESP32 en possède **sa propre copie** dans son dépôt : les deux projets,
autrefois issus d'un cœur commun, évoluent maintenant séparément.

Le **format des fichiers JSON reste identique** entre bureau et firmware, et les
**sauvegardes `.tar` (tout compris) sont interchangeables** carte ↔ PC.

| | Application bureau (ce dépôt) | Firmware ESP32 (dépôt séparé) |
|---|---|---|
| Build | CMake + Qt 6 | PlatformIO (Arduino) |
| Stockage | fichiers JSON (nlohmann/json) | fichiers JSON (LittleFS) |
| Interface | Qt Widgets (thème clair/sombre) | web embarquée (PROGMEM) |

## Aperçu

**Fiche composant** — tout sur un composant (général, caractéristiques, achat/
stock, documents, notes) :

![Fiche composant](docs/pictures/fiche-composant.png)

**Projets** — nomenclature (BOM) et calcul des composants manquants :

![Projets et nomenclature](docs/pictures/projets.png)

**Import / Export** — sauvegarde complète `.tar` (interchangeable avec l'ESP32)
et CSV par table :

![Import / Export](docs/pictures/import-export.png)

## Compilation (bureau)

Dépendances : CMake ≥ 3.21, un compilateur C++17, Ninja, **Qt 6 (Widgets)**,
**nlohmann-json**.

### Windows (MSYS2 / MinGW)

```sh
cmake --preset mingw
cmake --build --preset mingw
# -> build-mingw/ComponentHub.exe (DLL Qt + MinGW déployées automatiquement)
```

Sous VS Code : les tâches **CMake: Build (MinGW)** et **ComponentHub: Run**
(`.vscode/tasks.json`) font la même chose en un raccourci.

### Linux (x86_64) / Raspberry Pi (ARM64)

```sh
cmake --preset linux        # ou linux-arm64 sur Raspberry Pi
cmake --build --preset linux
```

## Paquets distribuables

Comme SiteWatch, les scripts de packaging sont à la racine, dans `scripts/` :

```sh
# Windows : ZIP autonome (exe + DLL + plugins Qt)
powershell -ExecutionPolicy Bypass -File scripts\windows\package-win.ps1

# Linux : paquet Debian (.deb, lié au Qt du système)
scripts/linux/package-deb.sh

# Linux : AppImage autonome (Qt embarqué)
scripts/linux/package-appimage.sh

# Linux : intégration au menu du bureau (binaire déjà compilé)
scripts/linux/install.sh
```

Les artefacts sont produits dans `dist/`.

## Structure du dépôt

```
ComponentHub/
├── CMakeLists.txt          application bureau (cible principale)
├── CMakePresets.json       presets mingw / linux / linux-arm64 / cross
├── cmake/toolchains/       toolchain de cross-compilation ARM64
├── scripts/{windows,linux}/ compilation VS Code + packaging (zip, deb, AppImage)
├── resources/              app.qrc, thèmes (light/dark), icône (logo.png, app.ico)
├── src/
│   ├── domain/             cœur métier PARTAGÉ (aucune dépendance Qt/Arduino)
│   ├── ui/                 interface Qt (Theme, Icons, pages, dialogues)
│   ├── storage/            dépôts fichier JSON (desktop)
│   ├── platform/           archive .tar, horloge
│   └── main.cpp
├── docs/
├── VERSION                 version de l'application bureau
└── LICENSE
```

> Le firmware ESP32 vit dans un dépôt distinct (`ComponentHub-ESP32`) et n'est
> plus inclus ici.

## Documentation

| Document | Contenu |
|---|---|
| [docs/BUILD_DESKTOP.md](docs/BUILD_DESKTOP.md) | Détails de compilation multi-plateforme (bureau) |
| [docs/ADR-0001](docs/ADR-0001-desktop-maitre-esp32-satellite.md) | Décision : version bureau maître, ESP32 satellite |
| [CHANGELOG.md](CHANGELOG.md) | Historique des versions |

> La documentation propre au firmware (architecture embarquée, API REST, WiFi,
> boot log) a été déplacée dans le dépôt `ComponentHub-ESP32`.

## Licence

Distribué sous [licence GPL-3.0-only](LICENSE).
