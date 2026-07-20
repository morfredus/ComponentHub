#!/usr/bin/env bash
# Resynchronise les copies vendorées de morfBeacon / morfUpdate dans
# third_party/morf/ depuis les dépôts sources voisins.
#
# Source par défaut : le dossier parent du projet (ex. 01-Travail/).
# Surcharge possible : MORF_SRC_BASE=/chemin/vers/les/depots scripts/sync-morf.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"           # racine du projet
SRC_BASE="${MORF_SRC_BASE:-$(cd "$ROOT/.." && pwd)}"

sync_one() {
  local name="$1" srcdir="$2" dstdir="$3"
  if [ ! -d "$srcdir" ]; then
    echo "!! Source introuvable pour $name : $srcdir" >&2
    echo "   (définir MORF_SRC_BASE si les dépôts sont ailleurs)" >&2
    return 1
  fi
  rm -rf "$dstdir/include" "$dstdir/src"
  cp -r "$srcdir/include" "$dstdir/include"
  cp -r "$srcdir/src"     "$dstdir/src"
  cp    "$srcdir/VERSION" "$dstdir/VERSION"
  echo "OK  $name  (version $(cat "$dstdir/VERSION"))"
}

# Le dépôt source peut s'appeler « morfBeacon » ou « morfBeacon » selon
# l'organisation locale des clones : on prend le premier trouvé, sinon le script
# échouait silencieusement sur une copie de travail suffixée.
resolve_src() {
  local name="$1"
  if [ -d "$SRC_BASE/$name" ]; then echo "$SRC_BASE/$name"; else echo "$SRC_BASE/${name}"; fi
}

sync_one morfBeacon "$(resolve_src morfBeacon)" "$ROOT/third_party/morf/beacon"
sync_one morfUpdate "$(resolve_src morfUpdate)" "$ROOT/third_party/morf/update"
echo "Synchronisation terminée. Le CMakeLists vendoré n'est pas modifié."
