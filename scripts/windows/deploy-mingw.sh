#!/usr/bin/env bash
# Copies next to the executable every non-Qt DLL of /mingw64/bin that
# the application depends on (gcc runtime, freetype, harfbuzz, zlib, pcre2...).
# windeployqt only handles the Qt DLLs; this script completes the job to make the exe
# launchable outside the MSYS2 shell (double-click).
#
# Usage: deploy-mingw.sh <executable_folder>
set -e
# CMake may launch this script from PowerShell, outside the usual MSYS2 shell.
# Find the Unix tools and the toolchain again before calling ldd/grep/awk.
export PATH="/mingw64/bin:/usr/bin:$PATH"
cd "$1" 2>/dev/null || exit 0
for f in *.exe *.dll */*.dll; do
  [ -f "$f" ] && ldd "$f" 2>/dev/null
done | grep -iE '/mingw64/bin/' | awk '{print $3}' | sort -u | while read -r dll; do
  cp -u "$dll" . 2>/dev/null || true
done
