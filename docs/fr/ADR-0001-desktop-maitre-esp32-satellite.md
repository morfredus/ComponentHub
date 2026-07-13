# ADR-0001 — Version bureau maître, ESP32 satellite

- **Statut** : accepté
- **Date** : 2026-07-12
- **Version** : 1.4.0 (bureau et firmware)
- **Portée** : les deux dépôts — `ComponentHub` (bureau) et
  `ComponentHub-ESP32` (firmware), désormais séparés.

> Un ADR (*Architecture Decision Record*) fige **pourquoi** une décision a été
> prise, son contexte et ses conséquences — pour que le choix reste
> compréhensible dans plusieurs années, conformément à la vision « mémoire
> technique de l'atelier ».

## Contexte

Jusqu'à la 1.3.0, ComponentHub était **avant tout un firmware ESP32-S3** : la
base de données de l'atelier (composants, mouvements de stock, projets,
documents) vivait sur la carte, en fichiers JSON sur LittleFS. Une version
bureau Qt venait d'apparaître, partageant le même cœur métier par référence
(source unique à la racine, compilée par les deux cibles).

Deux problèmes de fond rendent l'ESP32 inadapté au rôle de **détenteur de la
base de référence** :

1. **Limites matérielles de l'ESP32-S3.** RAM (~320 Ko) et partition de
   stockage sont contraintes. La base de l'atelier est faite pour grossir
   pendant des années (des milliers de composants, l'historique complet des
   mouvements, les documents joints). Les dépôts JSON relisent/réécrivent
   l'intégralité d'un fichier à chaque opération : le coût grimpe avec la
   taille des données — un import de 39 lignes avait déjà provoqué un reset
   *watchdog* (~150 s) sur l'ESP32, incident qui avait motivé l'ajout d'une
   écriture en lot. Ce n'est pas une base pensée pour croître indéfiniment sur
   cette plateforme.

2. **La carte SD comme extension a été écartée pour fiabilité insuffisante.**
   La voie « naturelle » pour repousser la limite de stockage sur ESP32 serait
   une **carte SD**. Elle est rejetée : dans un usage d'atelier (coupures
   d'alimentation, connecteur SD sujet aux faux contacts, usure des cellules
   flash bas de gamme, corruption de système de fichiers), une carte SD offre
   une **fiabilité réduite** — inacceptable pour la base qui EST la mémoire de
   l'atelier. On ne veut pas confier la source de vérité à un support fragile.

Conséquence : la base de référence doit vivre là où le stockage est fiable et
la capacité non contrainte — **un PC (ou un Raspberry Pi)**, pas l'ESP32.

## Décision

**La version bureau devient le projet maître et le détenteur de la base de
données de référence.** L'ESP32 devient un **satellite** de cette version
bureau.

1. **Bureau = maître.** L'application Qt/CMake détient la base de référence, sur
   un stockage fiable (disque PC/RPi), sans les limites mémoire de l'ESP32.
   Elle peut faire évoluer librement son moteur de persistance (fichiers JSON
   aujourd'hui, SQLite envisageable demain) sans contrainte embarquée, et gère
   les sauvegardes complètes (`.tar`).

2. **ESP32 = satellite.** Le firmware n'est plus le détenteur de la base : il
   devient un **terminal mobile** de l'atelier, pour **consulter le stock, le
   modifier, scanner des QR codes** au plus près des tiroirs. À terme, il
   **consulte et met à jour la base de la version bureau** (via un lien réseau /
   une API à définir) plutôt que d'héberger la source de vérité. Son rôle change
   donc de nature : de cœur de l'application à périphérique de saisie/consultation.

3. **Deux dépôts autonomes.** Les rôles divergeant, les deux projets sont
   séparés en dépôts indépendants, chacun avec **sa propre copie** de
   `src/domain/`. Ils n'ont plus de fichier partagé et évoluent séparément.
   Le firmware ESP32 est extrait vers `ComponentHub-ESP32`, voisin de ce dépôt.

## Alternatives écartées

| Alternative | Raison du rejet |
|---|---|
| **Carte SD sur l'ESP32** pour agrandir le stockage | Fiabilité réduite (corruption, faux contacts, usure) — inacceptable pour la base de référence. |
| **Garder un monorepo à cœur partagé** (référence unique) | Le couplage fort n'a plus de sens une fois les rôles divergents (maître vs satellite) : chaque cible a désormais son propre rythme d'évolution. |
| **Rester ESP32-centré**, bureau accessoire | Bloque la base sous les limites mémoire de l'ESP32, exactement le problème à résoudre. |

## Conséquences

**Positives**

- La base de référence n'est plus bridée par la RAM/flash de l'ESP32 ni exposée
  à un support peu fiable ; elle peut grossir sur des années et évoluer
  (JSON → SQLite) côté bureau sans contrainte embarquée.
- L'ESP32, déchargé du rôle de maître, peut être optimisé pour son nouveau
  métier (mobilité, scan, consultation rapide).
- Deux dépôts indépendants = évolutions, releases et versionnages découplés.

**Négatives / dette assumée**

- **Duplication du domaine** (`src/domain/` copié dans chaque dépôt) : plus de
  source unique, resynchronisation manuelle si une règle métier doit rester
  commune. Accepté car les cœurs sont appelés à diverger.
- **Protocole bureau ↔ satellite à définir** : l'ESP32 « consulte la BDD du
  bureau » suppose une API / un mécanisme de synchronisation réseau qui
  n'existe pas encore. C'est le principal chantier ouvert par cette décision.
- Le **format JSON / `.tar` reste identique** pour l'instant (sauvegardes
  interchangeables carte ↔ PC), ce qui préserve l'interopérabilité tant que le
  protocole de synchro n'est pas en place — mais ce contrat devra être tenu
  explicitement, dépôts séparés obligent.

## Suites

- Définir l'API de consultation/modification exposée par le bureau (ou le
  format d'échange) que le satellite ESP32 consommera.
- *(Fait)* Docs purement firmware (`ARCHITECTURE.md`, `API.md`, `BOOT_LOG.md`,
  `WIFI_SETUP.md`) déplacées vers le dépôt `ComponentHub-ESP32`.
