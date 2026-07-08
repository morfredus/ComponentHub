# ComponentHub

La mémoire technique d'un atelier électronique : inventaire de composants,
modules, outils et consommables, emplacements personnalisables, projets et
leur nomenclature, documentation attachée, import/export — le tout piloté
depuis un navigateur, sans application ni compte à créer.

Ce n'est pas un simple inventaire : l'objectif est de retrouver en un clic,
pour n'importe quel composant, son stock, son emplacement, ses datasheets,
et les projets qui l'utilisent.

## Fonctionnalités

- **Inventaire unifié** — composants, modules, outils et consommables dans
  la même base, filtrables par type, catégorie, statut de validation
  (icônes façon signalisation : 🟡 à tester, 🚧 en validation, 🟢 validé,
  🛑 défectueux, 📦 archivé) ou stock faible.
- **Fiche composant complète** — référence, fabricant, caractéristiques
  électriques, prix/fournisseur, dates d'achat/réception, garantie, état,
  provenance, notes. Catégorie, emplacement et interface se choisissent
  dans une liste existante ou se créent à la volée en tapant une nouvelle
  valeur.
- **Emplacements hiérarchiques** — arborescence libre (ex. *Atelier > Armoire
  A > Tiroir A12 > Sachet 3*), personnalisable sans limite de profondeur.
- **Catégories** — liste plate gérée séparément (page dédiée, comme
  Emplacements/Projets), alimentée automatiquement à mesure qu'elles sont
  saisies sur une fiche composant.
- **Menu d'actions par composant** (⋮) — Modifier, Mouvement, Documents,
  Dupliquer, Supprimer, prêt à accueillir Photos et QR Code.
- **Mouvements de stock** — entrée/sortie/correction/inventaire, historique
  complet par composant, alerte automatique sous le seuil minimum.
- **Projets** — fiche projet (version, firmware, dépôt Git) et nomenclature :
  composants de l'inventaire **ou** éléments pas encore possédés (marqués
  « à acheter »), vérification de disponibilité en un coup d'œil, et
  impression / export PDF de la liste des composants nécessaires et du reste
  à acheter.
- **Documentation** — datasheets, manuels, schémas, pinouts ou liens utiles
  attachés à un composant ou un projet.
- **Import / Export CSV** — format natif (sauvegarde complète,
  réimportable) et format compatible [Bomist](https://bomist.com/)
  (rapprochement par référence, résolution automatique des emplacements).
- **Tableau de bord (Labo)** — état détaillé de l'inventaire en un coup
  d'œil : références, pièces, stock faible, liste d'achats, composants à
  tester, projets, dates des derniers import/export/sauvegarde.
- Interface web responsive, sans rechargement de page, sans popup
  bloquant — uniquement des messages inline dans une zone dédiée.
- **Menu principal plat** (Labo, Inventaire, Projets, Paramètres, Système —
  pas de sous-menu déroulant) : Paramètres et Système regroupent leurs
  sous-pages dans une barre d'onglets, pensée pour rester navigable plus tard
  au bouton rotatif (souris/tactile aujourd'hui, contrôle physique demain —
  voir [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#principe-dinterface--navigation-pilotable)).

## Plateforme

Développé pour un **ESP32-S3-WROOM-1-N16R8**, sous **PlatformIO**
(framework Arduino). Le cœur métier (`src/domain/`) n'a aucune dépendance
ESP32/Arduino — voir [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) pour le
détail de cette séparation, pensée pour permettre un futur portage
Raspberry Pi / Linux sans réécriture majeure.

## Démarrage rapide

```bash
cp include/secrets_example.h include/secrets.h   # identifiants WiFi de développement (optionnel)
pio run                                          # compile le firmware (régénère l'UI embarquée automatiquement)
pio run --target upload                          # téléverse — firmware ET interface web en une seule fois
```

L'interface web (`web_src/`) est **embarquée dans le firmware** (PROGMEM),
pas servie depuis LittleFS : un simple `pio run --target upload` met à jour
le firmware et l'UI ensemble, sans étape séparée (`uploadfs`) et surtout
**sans jamais effacer l'inventaire**, qui reste seul sur LittleFS. Voir
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#interface-web-embarquée-progmem)
pour le détail.

Au premier démarrage, si aucun réseau WiFi n'est encore connu, l'ESP32
ouvre un portail de configuration — voir
[docs/WIFI_SETUP.md](docs/WIFI_SETUP.md) pour le déroulé complet.

## Documentation

| Document | Contenu |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Architecture en couches (Core / Domain / Storage / Modules / Web), portabilité, flux de démarrage |
| [docs/API.md](docs/API.md) | Référence complète de l'API REST (inventaire, emplacements, stock, documents, projets, import/export) |
| [docs/WIFI_SETUP.md](docs/WIFI_SETUP.md) | Configuration WiFi, portail captif, premier démarrage |
| [docs/BOOT_LOG.md](docs/BOOT_LOG.md) | Module optionnel de journal de redémarrage (diagnostic crash/watchdog) |
| [ROADMAP.md](ROADMAP.md) | Feuille de route (court / moyen / long terme, vision) |
| [CHANGELOG.md](CHANGELOG.md) | Historique des versions |

## Structure du projet

```
ComponentHub/
├── platformio.ini
├── include/
│       board_config.h        brochage
│       project_config.h      réglages globaux (nom du projet, fonctionnalités activées...)
│       secrets_example.h     modèle pour include/secrets.h (gitignored)
├── src/
│   │   main.cpp               point d'entrée : enregistrement des modules
│   ├── core/                  App, Module, ModuleManager — orchestration uniquement
│   ├── api/api_router/        WebRouter — façade de routage HTTP (ESPAsyncWebServer)
│   ├── services/              WiFi, Web, OTA, Storage, Config, Log, SystemInfo, mDNS, Time,
│   │                          WebAssets (interface web embarquée, voir plus bas)
│   ├── domain/                cœur métier pur (aucune dépendance Arduino/ESP32) :
│   │                          entités (dont Category), interfaces de dépôt,
│   │                          InventoryService, ProjectService, DocumentService,
│   │                          ImportExportService
│   ├── storage/                dépôts JSON/LittleFS (implémentent les interfaces de domain/)
│   └── modules/                modules métier (glue domaine <-> HTTP) :
│       ├── inventory/          composants, emplacements, catégories, mouvements de stock
│       ├── project/            projets et nomenclature
│       ├── document/           documentation attachée
│       ├── importexport/       import/export CSV
│       └── boot_log/           optionnel — journal de redémarrage
├── web_src/                    interface web (HTML/CSS/JS), à modifier ici
├── web/                        généré par tools/minify_web.py, ne pas éditer
├── src/generated/               généré par tools/generate_web_assets.py, ne pas éditer —
│                                tableaux PROGMEM compilés dans le firmware
├── docs/
└── tools/
        build_info.py          génère les infos de build (date, commit, version)
        minify_web.py          web_src/ -> web/
        generate_web_assets.py web/ -> src/generated/web_assets_data.cpp (PROGMEM)
        release.py             pipeline de release complet
        version_generator.py   génère VERSION
```

## Licence

Distribué sous [licence GPL-3.0-only](LICENSE).
