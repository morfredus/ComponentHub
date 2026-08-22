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

# Le dépôt source peut s'appeler « morfBeacon » ou « morfBeacon_travail » selon
# l'organisation locale des clones : on prend le premier trouvé, sinon le script
# échouait silencieusement sur une copie de travail suffixée.
resolve_src() {
  local name="$1"
  if [ -d "$SRC_BASE/$name" ]; then echo "$SRC_BASE/$name"; else echo "$SRC_BASE/${name}_travail"; fi
}

sync_one morfBeacon "$(resolve_src morfBeacon)" "$ROOT/third_party/morf/beacon"
sync_one morfUpdate "$(resolve_src morfUpdate)" "$ROOT/third_party/morf/update"
# Coeur de deploiement (morfdeploy) : vendore UNIQUEMENT pour l'enregistrement des
# compilations (record_compile.cmake/.py, appele par le CMakeLists). Source de
# verite : depot « morfDeploy » (ou son clone de travail).
if [ -d "$SRC_BASE/morfDeploy" ]; then
  DEPLOY_SRC="$SRC_BASE/morfDeploy/morfdeploy"; DEPLOY_VER="$SRC_BASE/morfDeploy/VERSION"
else
  DEPLOY_SRC="$SRC_BASE/morfDeploy_travail/morfdeploy"; DEPLOY_VER="$SRC_BASE/morfDeploy_travail/VERSION"
fi
DEPLOY_DST="$ROOT/third_party/morf/morfdeploy"
if [ -d "$DEPLOY_SRC" ]; then
  rm -rf "$DEPLOY_DST"; mkdir -p "$DEPLOY_DST"
  cp -r "$DEPLOY_SRC/." "$DEPLOY_DST/"
  find "$DEPLOY_DST" -name __pycache__ -type d -prune -exec rm -rf {} +
  [ -f "$DEPLOY_VER" ] && cp "$DEPLOY_VER" "$DEPLOY_DST/VERSION"
  echo "OK  morfdeploy$([ -f "$DEPLOY_DST/VERSION" ] && echo "  (version $(cat "$DEPLOY_DST/VERSION"))")"
else
  echo "!! Source introuvable pour morfdeploy : $DEPLOY_SRC" >&2
fi
echo "Synchronisation terminée. Le CMakeLists vendoré n'est pas modifié."
