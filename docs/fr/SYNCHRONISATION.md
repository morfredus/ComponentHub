# Synchronisation (morfSync)

ComponentHub peut **retrouver les mêmes données sur plusieurs postes** (Windows,
Linux, Raspberry Pi) via un petit serveur de réseau local, **morfSync**,
sans aucun cloud.

## Principe : base locale souveraine (offline-first)

- ComponentHub **fonctionne toujours en local**, même sans réseau : chaque poste
  a sa copie complète des données.
- Le hub ne sert qu'à **réconcilier** les copies quand il est joignable. S'il est
  absent, l'application continue normalement.
- Vous pouvez même choisir de **ne jamais synchroniser** (mode local explicite).

## Prérequis

Un **morfSync** qui tourne sur une machine de votre réseau (idéalement le
Raspberry Pi, allumé en permanence). Voir son dépôt pour l'installation ; en
résumé, une fois lancé il écoute sur `http://<ip-ou-nom>:8080`.

## Configurer

**Réglages → Synchronisation** :

1. **Adresse du hub** : `http://morfsync.local:8080` ou `http://<ip-du-pi>:8080`.
2. **Jeton** (facultatif) : uniquement si le hub en exige un.
3. **Tester la connexion** pour vérifier, puis **Synchroniser maintenant**.

### Options

| Option | Effet |
|--------|-------|
| **Synchroniser au démarrage** | À l'ouverture, si le hub répond, synchronise automatiquement (sonde courte, non bloquante ; « Recherche du hub… » s'affiche). |
| **Proposer une synchro en quittant** | Demande, à la fermeture, si vous voulez synchroniser. |
| **Utiliser uniquement en local** | Désactive **toute** synchronisation. Choix explicite ; grise la section. |

Sous les boutons, la **dernière synchronisation** est rappelée (date + nombre
d'éléments reçus / envoyés).

## Ce qui est synchronisé

Toutes les tables : composants, emplacements, catégories, projets, nomenclatures,
référentiels et documents. L'**intégrité référentielle est préservée** — un
composant et ce qu'il référence (son emplacement, sa catégorie…) arrivent
ensemble, donc un composant s'affiche correctement quel que soit le poste.

Les suppressions sont **propagées** (suppression logique). L'historique des
mouvements de stock n'est pas synchronisé (journal local). Les documents
synchronisent leur lien/chemin, pas le fichier binaire lui-même.

La synchronisation est **incrémentale** : seules les entités modifiées depuis le
dernier envoi sont transmises — une synchro « à vide » ne transfère rien.

## Robustesse : réinitialisation du hub

Si le hub voit son dossier de données **déplacé ou réinitialisé** (son journal
repart de zéro), ComponentHub le **détecte automatiquement** à la synchro
suivante et **re-synchronise tout** — vous n'avez rien à faire, plus besoin de
supprimer un fichier d'état à la main. (Nécessite morfSync ≥ 0.2.5.)

## Identifiants et migration

Depuis la v1.7, chaque entité possède un identifiant permanent **UUID** (au lieu
d'un entier), indispensable pour une identité sans collision entre postes.

> ⚠️ **Une base existante à identifiants entiers n'est pas convertie
> automatiquement.** Repartez d'une base vierge et **réimportez vos données**
> (format Bomist ou CSV natif) via **Import / Export**. Ouvrir une ancienne base
> avec la v1.7 provoque des comportements erratiques (identifiants illisibles).

## Sauvegarde de la configuration

Les réglages (adresse du hub, options, apparence) vivent hors du dossier de
données. Dans **Import / Export** :

- la **sauvegarde complète** (`.tar`) inclut désormais la configuration ;
- **« Configuration seule »** exporte/restaure uniquement les réglages en JSON,
  pratique pour déployer les mêmes réglages sur un autre poste.
