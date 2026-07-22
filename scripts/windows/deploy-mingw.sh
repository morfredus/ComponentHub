#!/usr/bin/env bash
# Copie a cote de l'executable toutes les DLL non-Qt de /mingw64/bin dont
# depend l'application (runtime gcc, freetype, harfbuzz, zlib, pcre2...).
# windeployqt ne s'occupe que des DLL Qt ; ce script complete pour rendre l'exe
# lançable hors du shell MSYS2 (double-clic).
#
# Usage : deploy-mingw.sh <dossier_de_l_executable>
set -e
cd "$1" 2>/dev/null || exit 0
for f in *.exe *.dll */*.dll; do
  [ -f "$f" ] && ldd "$f" 2>/dev/null
done | grep -iE '/mingw64/bin/' | awk '{print $3}' | sort -u | while read -r dll; do
  cp -u "$dll" . 2>/dev/null || true
done
