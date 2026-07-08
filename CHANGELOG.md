# Journal des modifications

Toutes les évolutions notables du projet sont consignées dans ce fichier.

Le format s'inspire de [Keep a Changelog](https://keepachangelog.com/fr/1.0.0/),
et le projet suit le [versionnage sémantique](https://semver.org/lang/fr/)
(`VERSION` à la racine).

## [Unreleased]

### Modifié
- Bandeau supérieur **figé (sticky)** : l'en-tête de navigation et la zone de
  statut inline (messages, confirmations, progression) restent collés en haut
  de la fenêtre au défilement. Une confirmation de suppression déclenchée en
  bas d'une longue liste est ainsi toujours visible, sans remonter la page. La
  zone de statut ne réserve plus d'espace quand elle est vide (`display:none`
  au lieu de simplement masquée).
- Page Inventaire : la barre d'outils (recherche, filtres, « stock faible »,
  bouton Ajouter) et les **en-têtes de colonnes** restent eux aussi figés,
  empilés sous le bandeau — la liste défile sous des repères toujours
  visibles. Les hauteurs du bandeau et de la barre d'outils sont mesurées à
  l'exécution (variables CSS `--topbar-h` / `--inv-toolbar-h`) pour un
  empilement exact malgré leurs hauteurs variables.

## [1.2.0]

Version mineure consolidant en une version stable les évolutions et
correctifs livrés par petites touches en 1.1.1 → 1.1.10 : gestion des
catégories, nomenclature de projet enrichie (éléments hors inventaire,
vérification de disponibilité, impression/PDF), refonte de la navigation
(menu plat + onglets), tableau de bord, et divers correctifs. Le détail par
incrément figure ci-dessous.

## [1.1.10]

### Corrigé
- Menu d'actions ⋮ de l'inventaire : s'ouvre désormais **vers le haut** sur
  les dernières lignes quand la place manque en dessous — plus de
  débordement en bas de page ni de barre de défilement parasite.
- Collision de routage : `GET /api/projects/bom` (et `/missing`) était capté
  par le handler de liste `GET /api/projects` (ESPAsyncWebServer fait
  correspondre par préfixe), renvoyant la liste des projets au lieu de la
  nomenclature — d'où des lignes vides (« undefined ») et un ajout/retrait
  sans effet. Les routes spécifiques sont enregistrées avant la générique.

## [1.1.9]

### Corrigé
- mDNS ré-annoncé proprement à chaque reconnexion WiFi (`MDNS.end()` avant
  `MDNS.begin()`) — un second `begin()` pouvait laisser le répondeur muet.
  L'accès `componenthub.local` reste tributaire du client (voir
  `docs/WIFI_SETUP.md` : sous Windows, souvent Bonjour requis ; l'accès par
  IP est toujours fiable).
- Assets web embarqués servis avec `Cache-Control: no-cache` : évite qu'un
  navigateur serve un ancien HTML/JS après une mise à jour du firmware
  (URLs stables mais contenu variable d'une version à l'autre).

## [1.1.8]

### Ajouté
- Nomenclature de projet — éléments **hors inventaire** : ajout à un projet
  d'un élément non encore possédé (nom libre), automatiquement compté
  « à acheter ». Le champ d'ajout accepte un composant de l'inventaire
  (suggestions) ou un nouveau nom.
- Nomenclature de projet — **vérification de disponibilité** : résumé global
  et colonne d'état par ligne (✅ dispo / 🛒 à acheter).
- Nomenclature de projet — **impression / export PDF** : page imprimable
  (composants nécessaires + « Reste à acheter » avec coût estimé), via
  l'impression navigateur ou « Enregistrer en PDF ».

## [1.1.7]

### Modifié
- Navigation par **onglets** : les sections Paramètres et Système présentent
  leurs sous-pages via une barre d'onglets persistante (injectée sur chaque
  page de la section, onglet courant mis en évidence), en remplacement des
  gros boutons.

## [1.1.6]

### Modifié
- Page d'accueil (menu `Labo`) titrée « Tableau de bord » et débarrassée de
  son cartouche de bienvenue redondant — l'état de l'inventaire est affiché
  directement.

## [1.1.5]

### Ajouté
- Sections `Paramètres` (Emplacements / Catégories / Import-Export) et
  `Système` (Fichiers / Mise à jour / Logs, plus Debug si `ENABLE_BOOT_LOG`
  est actif ; `/system` conserve ses informations système) regroupant leurs
  sous-pages.

## [1.1.4]

### Modifié
- Menu principal aplati à 5 entrées (Labo, Inventaire, Projets, Paramètres,
  Système) — sous-menus déroulants écartés pour rester navigable plus tard
  au bouton rotatif (voir `docs/ARCHITECTURE.md`, navigation « pilotable »).

## [1.1.3]

### Ajouté
- Formulaire composant — champ **Interface** avec suggestions courantes
  (I2C, SPI, UART, USB, GPIO, PWM, Analogique, CAN, 1-Wire, Bluetooth, WiFi,
  Ethernet, RS232, RS485, JTAG, SWD), tout en restant libre.
- Formulaire composant — champ **Catégorie** alimenté par la liste gérée
  côté serveur, plutôt que par les seuls composants affichés.

## [1.1.2]

### Ajouté
- Formulaire composant — champ **Emplacement** éditable : champ texte +
  suggestions (chemins existants), avec création à la volée d'un nouvel
  emplacement racine si le texte saisi ne correspond à aucun chemin connu.

## [1.1.1]

### Ajouté
- Entité `Category` (liste plate, sans hiérarchie) : page d'administration
  `/categories`, API `/api/inventory/categories`, et création automatique
  dès qu'un composant est enregistré avec une catégorie inconnue.

## [1.1.0]

### Ajouté
- Menu d'actions ⋮ par ligne de l'inventaire (Modifier, Mouvement,
  Documents, Photos, QR Code, Dupliquer, Supprimer), à la place des 4
  boutons fixes — prévu pour absorber de futures actions sans reprendre la
  mise en page. Photos et QR Code affichent un message "bientôt
  disponible" (fonctionnalités pas encore construites).
- Icônes de statut de validation (🟡 à tester, 🚧 en validation, 🟢 validé,
  🛑 défectueux, 📦 archivé) à la place du texte, dans la colonne Statut et
  les listes déroulantes — libellé complet conservé en infobulle.
- Filtre par statut sur la page Inventaire (manquait pour que la tuile
  "à tester" du tableau de bord pointe vers une vue utile).

### Modifié — architecture : interface web embarquée en PROGMEM
- L'UI (`web_src/`) n'est plus servie depuis LittleFS : elle est désormais
  compilée dans le firmware (PROGMEM), régénérée automatiquement avant
  chaque build (`tools/minify_web.py` puis `tools/generate_web_assets.py`,
  chaînés via `extra_scripts`). `pio run --target upload` met donc à jour
  firmware et interface web en une seule fois — une mise à jour OTA aussi.
  LittleFS ne contient plus que les données réelles de l'inventaire et les
  documents utilisateur (uploadés via `/api/files/upload`), plus jamais
  effacés par une mise à jour de l'UI. Voir
  `docs/ARCHITECTURE.md#interface-web-embarquée-progmem`.
- Nouveau service `src/services/web_assets/` (`WebAssets::find`/`send`) :
  seule couche à connaître la table d'assets générée.
- Renommage du dossier de sortie de minification `data/` → `web/`.

### Retiré
- `tools/sync_web.py` et `tools/package_web.py` : le problème qu'ils
  contournaient (perte de données lors d'une mise à jour de l'UI) n'existe
  plus structurellement, ces outils n'ont donc plus d'objet.

## [1.0.0]

Premier inventaire fonctionnel de ComponentHub. Le projet part de la base
ESP32-Foundation (services WiFi/OTA/stockage/logs) et y construit, par-dessus,
un cœur métier dédié à la gestion d'un atelier électronique.

### Ajouté — cœur métier (`src/domain/`)
- Domaine indépendant de la plateforme (aucune dépendance Arduino/ESP32) :
  entités `Component`, `Location`, `StockMovement`, `Project`,
  `ProjectComponent`, `Document`, interfaces de dépôt, et les services
  `InventoryService`, `ProjectService`, `DocumentService`,
  `ImportExportService`. Pensé pour un futur portage Raspberry Pi/Linux sans
  réécriture majeure (voir `docs/ARCHITECTURE.md`).
- Persistance JSON sur LittleFS (`src/storage/`), écritures atomiques
  (fichier temporaire + renommage) et détection explicite de fichier
  corrompu au lieu d'un vidage silencieux de l'inventaire.

### Ajouté — inventaire
- Fiche composant complète (référence, fabricant, caractéristiques
  électriques, prix/fournisseur, dates, garantie, état, provenance, notes),
  avec un champ `kind` (composant / module / outil / consommable) traitant
  ces familles comme des facettes filtrables du même inventaire plutôt que
  des systèmes séparés.
- Emplacements hiérarchiques personnalisables (arborescence libre).
- Mouvements de stock (entrée/sortie/correction/inventaire) avec historique
  par composant, et détection automatique du stock sous le seuil minimum.
- Workflow "pièces récupérées" : mode récupération dans le formulaire
  d'ajout, qui conserve provenance/emplacement/état d'un ajout à l'autre.
- Recherche instantanée et filtres (type, catégorie, statut, stock faible).

### Ajouté — projets
- Fiche projet (version, firmware, dépôt Git, statut) et nomenclature de
  composants nécessaires, avec calcul immédiat des quantités manquantes par
  rapport au stock réel.

### Ajouté — documentation attachée
- Datasheets, manuels, schémas, pinouts ou liens utiles, rattachables à un
  composant ou un projet (réutilise le gestionnaire de fichiers existant
  plutôt que de dupliquer un pipeline d'upload).

### Ajouté — import / export
- Export/import CSV au format natif (sauvegarde complète, réimportable) et
  au format compatible [Bomist](https://bomist.com/) (rapprochement par
  référence, résolution/création automatique des emplacements par nom).
- Suivi des dates de dernier import / export / sauvegarde, exposé sur le
  tableau de bord.

### Ajouté — interface web
- Bascule du serveur web synchrone (`WebServer`) vers **ESPAsyncWebServer**,
  seule couche (`web_manager`, `api_router`, `ota_manager`) à connaître ce
  choix — le reste du framework (WiFi, stockage, logs) reste inchangé.
- Nouveau tableau de bord (page **Labo**, page d'accueil) : état détaillé de
  l'inventaire (références, pièces, valeur, stock faible, liste d'achats en
  attente, composants à tester, projets, dates import/export/sauvegarde) et
  accès rapide aux actions courantes.
- Pages Inventaire, Emplacements, Projets, Import/Export.
- Mise en page limitée à 1060px (menu compris), formulaires de saisie
  réorganisés en grille responsive (moins de défilement vertical, jamais de
  défilement horizontal).
- Remplacement des popups bloquants (`alert`/`confirm`) par une zone de
  statut inline partagée (confirmations, messages de succès/erreur,
  progression réelle de l'import/export via `XMLHttpRequest`), réservée en
  permanence dans la mise en page pour ne jamais décaler l'interface.
- Pied de page (nom du projet + version) sur toutes les pages.

### Corrigé
- Import CSV : le rapprochement et la mise à jour se faisaient ligne par
  ligne, chacune relisant/réécrivant tout le fichier JSON des composants et
  rescannant les emplacements — un coût quadratique ayant provoqué un reset
  watchdog (tâche bloquée ~150s) lors de l'import d'un fichier de 39 lignes.
  Le rapprochement se fait désormais entièrement en mémoire à partir d'un
  seul instantané disque, avec une écriture en lot (`saveAll`).
- Risque de perte de données : `pio run --target uploadfs` reflashe toute la
  partition LittleFS (donc l'inventaire) à chaque mise à jour de
  l'interface web. Ajout de `tools/sync_web.py`, qui pousse les fichiers web
  un par un via l'API HTTP existante sans jamais toucher aux données ;
  `tools/minify_web.py` avertit désormais avant de proposer `uploadfs`.

### Retiré
- `examples/` et `src/modules/example_module/` (démonstrations génériques
  de la base ESP32-Foundation, sans usage dans ComponentHub).
- `docs/INTEGRATION_GUIDE.md` (guide générique "démarrer un nouveau projet
  depuis le framework") et les captures d'écran génériques de
  `docs/pictures/` — contenu propre à ESP32-Foundation, sans rapport avec
  l'interface réelle de ComponentHub.
