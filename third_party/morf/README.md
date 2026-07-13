# Modules communs « morf » (copie vendorée)

Copie autonome des bibliothèques partagées de morfredus :

- `beacon/` — **morfBeacon** : supervision LAN (heartbeat UDP « je suis actif » +
  endpoint HTTP `/status`).
- `update/` — **morfUpdate** : détection de mises à jour (GitHub Releases) + un
  dialogue de notification Qt.

## Pourquoi vendoré ?

Le projet se compile **sans dépendre d'aucun dépôt externe** : le code est ici et
lié **statiquement** dans l'exécutable. Rien de plus à livrer dans les paquets
installables (ZIP Windows, `.deb`, AppImage) — tout est dans le binaire.
Fonctionne à l'identique sous Windows, Linux x64 et Raspberry Pi (ARM64), car
c'est compilé avec le même toolchain et le même Qt que l'application.

## Mise à jour

Ne pas éditer le code ici : la **source de vérité** est dans les dépôts voisins
`morfBeacon` / `morfUpdate`. Pour resynchroniser :

```sh
# Windows
powershell -ExecutionPolicy Bypass -File scripts\sync-morf.ps1
# Linux / Raspberry Pi
scripts/sync-morf.sh
```

Le script recopie `include/`, `src/` et `VERSION` ; il ne touche pas au
`CMakeLists.txt` vendoré (volontairement allégé). Si les dépôts sources sont
ailleurs, définir la variable d'environnement `MORF_SRC_BASE`.

Versions vendorées : voir `beacon/VERSION` et `update/VERSION`.
