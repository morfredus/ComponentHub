# Feuille de route — ComponentHub

Cette feuille de route est indicative : elle liste, par horizon, les
fonctionnalités déduites du cahier des charges et des limites déjà identifiées
en cours de développement. L'ordre à l'intérieur d'un horizon n'est pas figé.

Elle couvre le **projet maître** (l'application de bureau, ce dépôt). Le
firmware **satellite** ESP32 vit dans le dépôt `ComponentHub-ESP32` et a sa
propre feuille de route ; les points qui concernent le lien entre les deux sont
regroupés dans la section [Lien maître ↔ satellite](#lien-maître--satellite-esp32).

## Principes du projet

ComponentHub est conçu autour de quelques principes simples :

- les données utilisateur ne sont jamais écrasées par une mise à jour ;
- le cœur métier (`src/domain/`) reste indépendant de la plateforme ;
- **la version bureau est le maître** : elle détient la base de référence, sur
  un stockage fiable et sans les limites mémoire de l'ESP32 (voir
  [docs/ADR-0001](docs/ADR-0001-desktop-maitre-esp32-satellite.md)) ; l'ESP32
  est un satellite de consultation/saisie ;
- l'interface est pensée pour fonctionner aussi bien à la souris qu'au clavier
  ou au tactile ;
- les fonctionnalités sont développées pour résoudre un besoin réel dans
  l'atelier avant d'être généralisées.

## Déjà livré (jusqu'à v1.4.x)

Socle fonctionnel de l'atelier, pour situer le reste :

- **Application de bureau Qt 6 / C++17**, native sur **Windows, Linux (x86_64)
  et Raspberry Pi (ARM64)** via CMake — voir
  [docs/BUILD_DESKTOP.md](docs/BUILD_DESKTOP.md).
- Cœur métier indépendant de la plateforme (`src/domain/`) ; persistance en
  **fichiers JSON** (nlohmann/json), écriture atomique, dans le dossier de
  configuration utilisateur.
- Inventaire unifié (composants / modules / outils / consommables), fiche
  complète, **recherche globale** (tous les champs texte, plusieurs mots) et
  filtres.
- Emplacements hiérarchiques ; catégories gérées (liste dédiée + création à la
  volée) ; **référentiels administrables** (types, fabricants, boîtiers,
  fournisseurs… avec fusion de doublons et propagation aux composants).
- Mouvements de stock avec historique et alerte de seuil.
- Projets et nomenclature : composants de l'inventaire **ou** éléments hors
  inventaire (« à acheter »), calcul des composants manquants.
- Documentation attachée (datasheets, liens…) aux composants et projets.
- Import / export CSV (format natif et compatible Bomist) ; **sauvegarde /
  restauration complète** de la base en archive `.tar`, encore
  **interchangeable avec le firmware ESP32**.
- Thème clair/sombre suivant l'OS.
- **Séparation bureau / firmware** : l'ESP32 est devenu un dépôt satellite
  autonome (`ComponentHub-ESP32`).

## Court terme

Fonctionnalités déjà amorcées ou naturellement prioritaires.

- **Photos** — plusieurs images par fiche (photo constructeur, réelle,
  rangement, brochage, étiquette). Réutiliser le système de documents existant
  (catégorie « photo ») avec un affichage en vignettes.
- **QR Codes** — génération d'un QR encodant le lien vers la fiche d'un
  composant ou le contenu d'un tiroir, affichage imprimable. Base du scan
  « ouvrir la fiche / voir le contenu » (côté satellite notamment).
- **Intégrité référentielle** — suppression en cascade ou nettoyage des
  références orphelines : supprimer un composant devrait aussi retirer ses
  mouvements de stock et documents ; supprimer un emplacement / une catégorie
  devrait mettre à jour (ou bloquer) les composants qui les référencent.

## Moyen terme

Fonctions du cahier des charges cohérentes avec l'architecture actuelle.

- **Impression d'étiquettes** — export vers DYMO puis Brother P-Touch :
  étiquette contenant nom, référence, QR Code et emplacement.
- **Checklist de validation** par composant (alimentation testée, brochage
  vérifié, bibliothèque validée, exemple fonctionnel, datasheet ajoutée, photo
  réalisée, emplacement attribué) — enrichit le champ « statut » actuel.
- **Liste d'achats complète** — au-delà de la détection du stock faible :
  fournisseur préféré, prix moyen, quantité à commander, génération d'une liste
  dédiée.
- **Bibliothèques par module** — bibliothèques Arduino / ESP-IDF / PlatformIO,
  exemples, snippets et remarques personnelles rattachés à un composant.
- **Historique étendu** — au-delà des mouvements de stock déjà tracés :
  modifications de fiche, changements d'emplacement et de quantité.
- **Sous-catégorie gérée** comme entité (à l'image de Catégorie), plutôt que
  champ texte libre.
- **Import CSV — encodage** : détection / conversion Windows-1252 en plus de
  l'UTF-8 aujourd'hui exigé (ex. export tableur « CSV séparé par ; »).

## Lien maître ↔ satellite (ESP32)

Chantier structurant ouvert par la séparation des deux projets (voir
[docs/ADR-0001](docs/ADR-0001-desktop-maitre-esp32-satellite.md)). Tant qu'il
n'existe pas, le satellite fonctionne encore de façon autonome avec sa propre
base (format JSON / `.tar` interchangeable).

- **Consolider le bureau** comme maître avant tout : c'est ici que vit la base
  de référence, sur stockage fiable.
- **Transformer l'ESP32 en satellite** au sens plein : consultation et
  modification du stock, scan de QR codes au plus près des tiroirs — sans
  détenir la source de vérité.
- **Définir le protocole / l'API** par lequel le satellite consulte et met à
  jour la base du maître (lien réseau). Prérequis déjà en place : nom mDNS du
  satellite distinct (`componenthub-esp32.local`) pour éviter tout télescopage
  avec le futur service du bureau.

## Long terme / Vision

Portabilité et écosystème — ce pour quoi l'architecture a été pensée dès le
départ.

- **Moteur de persistance évolutif côté bureau** — le stockage n'étant plus
  bridé par l'ESP32, le passage des fichiers JSON à **SQLite** (requêtes,
  volumétrie, historique) devient possible sans contrainte embarquée ; seules
  les implémentations des interfaces de dépôt (`src/domain/`) changent.
- **Interface tactile + contrôle physique** — bouton rotatif / façade dédiée,
  notamment sur le satellite (tourner = déplacer le focus, appuyer = valider).
- **Périphériques d'acquisition** — lecteur de codes-barres, RFID / NFC, caméra
  et reconnaissance automatique, ajoutés sans modifier le cœur (encapsulés
  derrière des interfaces, comme le reste de la plateforme).
- **Sauvegarde & synchronisation** — export SQLite, sauvegarde vers un NAS,
  synchronisation multi-postes, API REST publique.
- **Profils de domaine** — le moteur devra pouvoir être adapté à différents
  domaines (électronique, photographie, atelier, cuisine, modélisme…) sans
  modification du cœur métier.

---

Voir [CHANGELOG.md](CHANGELOG.md) pour ce qui a déjà été livré, et
[docs/ADR-0001](docs/ADR-0001-desktop-maitre-esp32-satellite.md) pour la
séparation bureau / ESP32 et les rôles maître / satellite.
