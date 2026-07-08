# Feuille de route — ComponentHub

Cette feuille de route est indicative : elle liste, par horizon, les
fonctionnalités déduites du cahier des charges et
des limites déjà identifiées en cours de développement. L'ordre à l'intérieur
d'un horizon n'est pas figé.

## Principes du projet

ComponentHub est conçu autour de quelques principes simples :

- les données utilisateur ne sont jamais écrasées par une mise à jour ;
- le cœur métier reste indépendant de la plateforme ;
- l'interface est pensée pour fonctionner aussi bien à la souris, au clavier, au tactile qu'avec un futur bouton rotatif ;
- les fonctionnalités sont développées pour résoudre un besoin réel dans l'atelier avant d'être généralisées.

## Déjà livré (jusqu'à v1.2.0)

Socle fonctionnel de l'atelier, pour situer le reste :

- Cœur métier indépendant de la plateforme (`src/domain/`), persistance JSON
  atomique sur LittleFS.
- Inventaire unifié (composants / modules / outils / consommables), fiche
  complète, recherche et filtres (type, catégorie, statut, stock faible).
- Emplacements hiérarchiques ; catégories gérées (liste dédiée + création à
  la volée) ; champs Emplacement / Catégorie / Interface éditables avec
  suggestions.
- Mouvements de stock avec historique et alerte de seuil.
- Projets et nomenclature : composants de l'inventaire **ou** éléments hors
  inventaire (« à acheter »), vérification de disponibilité, impression /
  export PDF de la liste des composants nécessaires et du reste à acheter.
- Documentation attachée (datasheets, liens...) aux composants et projets.
- Import / export CSV (format natif et compatible Bomist).
- Interface web embarquée dans le firmware (PROGMEM) — un seul
  `pio run --target upload` met à jour firmware **et** interface, sans jamais
  toucher aux données ; navigation « pilotable » (menu plat + onglets),
  pensée pour un futur bouton rotatif ; tableau de bord d'état de l'atelier.

## Court terme

Fonctionnalités déjà amorcées ou explicitement signalées comme « bientôt
disponible » dans l'interface.

- **Photos** (entrée déjà présente dans le menu ⋮ de l'inventaire) —
  plusieurs images par fiche (photo constructeur, réelle, rangement,
  brochage, étiquette). Réutiliser le système de documents existant
  (catégorie « photo ») avec un affichage en vignettes.
- **QR Codes** (entrée déjà présente dans le menu ⋮) — génération d'un QR
  encodant le lien direct vers la fiche d'un composant ou le contenu d'un
  tiroir, affichage imprimable. Base du scan « ouvrir la fiche / voir le
  contenu ».
- **Intégrité référentielle** — suppression en cascade ou nettoyage des
  références orphelines : supprimer un composant devrait aussi retirer ses
  mouvements de stock et documents ; supprimer un emplacement / une
  catégorie devrait mettre à jour (ou bloquer) les composants qui les
  référencent. Limite connue documentée dans
  [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#limites-connues).

## Moyen terme

Fonctions du cahier des charges cohérentes avec l'architecture actuelle.

- **Impression d'étiquettes** — export vers DYMO puis Brother P-Touch :
  étiquette contenant nom, référence, QR Code et emplacement.
- **Checklist de validation** par composant (alimentation testée, brochage
  vérifié, bibliothèque validée, exemple fonctionnel, datasheet ajoutée,
  photo réalisée, emplacement attribué) — enrichit le champ « statut »
  actuel.
- **Liste d'achats complète** — au-delà de la détection du stock faible :
  fournisseur préféré, prix moyen, quantité à commander, génération d'une
  liste dédiée.
- **Bibliothèques par module** — bibliothèques Arduino / ESP-IDF /
  PlatformIO, exemples, snippets et remarques personnelles rattachés à un
  composant.
- **Historique étendu** — au-delà des mouvements de stock déjà tracés :
  modifications de fiche, changements d'emplacement et de quantité.
- **Sous-catégorie gérée** comme entité (à l'image de Catégorie), plutôt que
  champ texte libre.
- **Import CSV — encodage** : détection / conversion Windows-1252 en plus de
  l'UTF-8 aujourd'hui exigé (ex. export tableur « CSV séparé par ; »).
- **Temps réel (WebSocket)** — ESPAsyncWebServer le supporte nativement mais
  ce n'est pas encore câblé : mises à jour poussées, cohérence entre
  plusieurs clients ouverts simultanément.

## Long terme / Vision

Portabilité et écosystème — ce pour quoi l'architecture a été pensée dès le
départ.

- **Portage du cœur** sur Raspberry Pi / Linux / Windows — `src/domain/`
  n'a aucune dépendance ESP32 ; il « suffit » de ré-implémenter les
  interfaces de dépôt (ex. SQLite) et de servir `web/` depuis le disque
  (l'API `WebAssets::send` est le seul point à adapter). Objectif ~90 % de
  code réutilisable.
- **Interface tactile + bouton rotatif** — l'UI est déjà conçue « pilotable »
  (éléments focalisables, pas de menus à ouvrir), mais le contrôle physique
  n'est pas implémenté.
- **Périphériques d'acquisition** — lecteur de codes-barres, RFID / NFC,
  caméra et reconnaissance automatique, ajoutés sans modifier le cœur
  (encapsulés derrière des interfaces, comme le reste de la plateforme).
- **Sauvegarde & synchronisation** — export SQLite, sauvegarde vers un NAS,
  synchronisation Raspberry Pi / PC, API REST publique.
- **Profils de domaine** - Le moteur devra pouvoir être adapté à différents domaines
  (électronique, photographie, atelier, cuisine, modélisme…)
  sans modification du cœur métier.

---

Voir [CHANGELOG.md](CHANGELOG.md) pour ce qui a déjà été livré, et
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) pour les choix de conception qui
rendent ces évolutions possibles sans réécriture majeure.
