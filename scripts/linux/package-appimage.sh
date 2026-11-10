#!/usr/bin/env bash
#
# ComponentHub - building a self-contained AppImage.
#
# Produces dist/ComponentHub-<version>-x86_64.AppImage: a single, portable
# file embedding Qt. The end user makes it executable and runs it -
# no build, no Qt installation.
#
# Prerequisites: ComponentHub built in Release (cmake --preset linux && cmake
# --build --preset linux), and an internet connection on the first run
# (download of linuxdeploy + the Qt plugin, cached afterwards).
#
# Usage: scripts/linux/package-appimage.sh [--build <folder>]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

APP_NAME="ComponentHub"
CMD="componenthub"
ARCH="x86_64"
BUILD_DIR="$ROOT/build"

die() { printf 'Erreur : %s\n' "$1" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --build) BUILD_DIR="${2:-}"; shift ;;
        -h|--help)
            echo "Usage : scripts/linux/package-appimage.sh [--build <dossier>]"; exit 0 ;;
        *) die "option inconnue : $1 (voir --help)" ;;
    esac
    shift
done

[ "$(uname -s)" = "Linux" ] || die "ce script doit être exécuté sous Linux."

BINARY="$BUILD_DIR/$APP_NAME"
[ -x "$BINARY" ] || die \
"binaire introuvable : $BINARY
  Compile d'abord :  cmake --preset linux && cmake --build --preset linux"

VERSION="$(head -n1 "$ROOT/VERSION" | tr -d '[:space:]')"
[ -n "$VERSION" ] || die "fichier VERSION vide ou absent."

ICON_SRC="$ROOT/resources/logo.png"
[ -f "$ICON_SRC" ] || die "icône introuvable : $ICON_SRC"

TOOLS_DIR="$ROOT/.cache/appimage-tools"
mkdir -p "$TOOLS_DIR"
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-$ARCH.AppImage"
PLUGIN_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-$ARCH.AppImage"
BASE_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous"

# An AppImage is an ELF: each tool is validated (ELF magic + minimum size)
# before being reused from the cache. A half-written download would otherwise
# stay there forever and break the extraction ("Failed to open
# squashfs image"), because fetch() skipped any file already present.
appimage_ok() {
    [ -s "$1" ] || return 1
    [ "$(od -An -tx1 -N4 "$1" 2>/dev/null | tr -d ' \n')" = "7f454c46" ] || return 1
    # Deliberately high threshold: linuxdeploy and its Qt plugin weigh >10 MB.
    # A truncated download (seen at 1.5 MB) passes the ELF magic but has an
    # incomplete squashfs ("Failed to open squashfs image"); 4 MB rejects it so it is fetched again.
    [ "$(stat -c%s "$1" 2>/dev/null || echo 0)" -gt 4000000 ] || return 1
    return 0
}
fetch() {
    local url="$1" dst="$2"
    [ -f "$dst" ] && appimage_ok "$dst" && return 0
    rm -f "$dst"
    echo "Téléchargement : $(basename "$dst")"
    if command -v wget >/dev/null 2>&1; then wget -q -O "$dst" "$url" || die "échec du téléchargement de $url"
    elif command -v curl >/dev/null 2>&1; then curl -fsSL -o "$dst" "$url" || die "échec du téléchargement de $url"
    else die "ni wget ni curl disponible."; fi
    chmod +x "$dst"
    appimage_ok "$dst" || die "outil AppImage corrompu après téléchargement : $dst (relancer pour re-télécharger)"
}
fetch "$BASE_URL/linuxdeploy-$ARCH.AppImage"          "$LINUXDEPLOY"
fetch "$QT_URL/linuxdeploy-plugin-qt-$ARCH.AppImage"  "$PLUGIN_QT"

APPDIR="$ROOT/dist/AppDir"
rm -rf "$APPDIR"
install -Dm755 "$BINARY" "$APPDIR/usr/bin/$CMD"

# .desktop file: normalize it to LF before handing it to linuxdeploy.
# A CRLF .desktop (Windows checkout / editing under WSL) makes linuxdeploy read
# "Icon=name\r", which then looks for an icon literally named
# "name\r" and gives up with "Could not find suitable icon for Icon entry".
# So every end-of-line carriage return is removed, whatever the state of the source.
DESKTOP_DST="$APPDIR/usr/share/applications/$CMD.desktop"
mkdir -p "$(dirname "$DESKTOP_DST")"
sed 's/\r$//' "$SCRIPT_DIR/componenthub.desktop" > "$DESKTOP_DST"
chmod 644 "$DESKTOP_DST"

# --- Icon: guarantee an icon of the RIGHT DIMENSIONS, without an external tool ----
# linuxdeploy requires an icon whose REAL file size matches the
# hicolor/<size> folder; otherwise "Could not find suitable icon for Icon entry".
# So the dimensions are read from the PNG header (IHDR, big-endian) and the
# icon is placed in the exact folder -- reliable for any source, even without
# ImageMagick. (The Windows magick.exe, visible through /mnt/c on the WSL PATH, is
# deliberately ignored: it does not read /mnt paths and produces invalid
# icons -- this was the cause of the icon refusal.)
png_dim() { printf '%d' "0x$(od -An -tx1 -j"$1" -N4 "$2" | tr -d ' \n')"; }
ICON_W="$(png_dim 16 "$ICON_SRC")"
ICON_H="$(png_dim 20 "$ICON_SRC")"
ICON_MAIN="$APPDIR/usr/share/icons/hicolor/${ICON_W}x${ICON_H}/apps/$CMD.png"
install -Dm644 "$ICON_SRC" "$ICON_MAIN"
install -Dm644 "$ICON_SRC" "$APPDIR/usr/share/pixmaps/$CMD.png"

# Bonus: extra standard sizes if a NATIVE (Linux) ImageMagick exists.
CONVERT=""
for c in /usr/bin/magick /usr/bin/convert; do [ -x "$c" ] && { CONVERT="$c"; break; }; done
if [ -n "$CONVERT" ]; then
    for size in 32 48 64 128 256; do
        dest="$APPDIR/usr/share/icons/hicolor/${size}x${size}/apps/$CMD.png"
        mkdir -p "$(dirname "$dest")"
        "$CONVERT" "$ICON_SRC" -resize "${size}x${size}" "$dest" && ICON_MAIN="$dest"
    done
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export VERSION
if command -v qmake6 >/dev/null 2>&1; then export QMAKE="$(command -v qmake6)"
elif command -v qmake  >/dev/null 2>&1; then export QMAKE="$(command -v qmake)"; fi

echo "Construction de l'AppImage (Qt embarqué)…"
( cd "$ROOT/dist" && "$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/$CMD" \
    --desktop-file "$APPDIR/usr/share/applications/$CMD.desktop" \
    --icon-file "$ICON_MAIN" \
    --plugin qt \
    --output appimage )

RESULT="$(ls -t "$ROOT/dist"/*.AppImage 2>/dev/null | head -n1)"
[ -n "$RESULT" ] || die "AppImage non produite."
FINAL="$ROOT/dist/${APP_NAME}-${VERSION}-${ARCH}.AppImage"
[ "$RESULT" = "$FINAL" ] || mv -f "$RESULT" "$FINAL"
chmod +x "$FINAL"

SIZE="$(du -h "$FINAL" | cut -f1)"
echo "OK - $FINAL ($SIZE)"
