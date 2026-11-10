#!/usr/bin/env bash
# Resynchronizes the vendored copies of morfBeacon / morfUpdate in
# third_party/morf/ from the neighboring source repositories.
#
# Default source: the parent folder of the project (e.g. 01-Travail/).
# Can be overridden: MORF_SRC_BASE=/path/to/the/repositories scripts/sync-morf.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"           # project root
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

# The source repository may be called "morfBeacon" or "morfBeacon_travail" depending on
# the local organization of the clones: we take the first one found, otherwise the script
# used to fail silently on a suffixed working copy.
resolve_src() {
  local name="$1"
  if [ -d "$SRC_BASE/$name" ]; then echo "$SRC_BASE/$name"; else echo "$SRC_BASE/${name}_travail"; fi
}

sync_one morfBeacon "$(resolve_src morfBeacon)" "$ROOT/third_party/morf/beacon"
sync_one morfUpdate "$(resolve_src morfUpdate)" "$ROOT/third_party/morf/update"
# Deployment core (morfdeploy): vendored ONLY for build recording
# (record_compile.cmake/.py, called by CMakeLists). Source of
# truth: the "morfDeploy" repository (or its working clone).
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
