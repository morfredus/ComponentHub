#!/usr/bin/env bash
#
# ComponentHub — construction d'une AppImage autonome.
#
# Produit dist/ComponentHub-<version>-x86_64.AppImage : un fichier unique et
# portable embarquant Qt. L'utilisateur final le rend exécutable et le lance —
# aucune compilation, aucune installation de Qt.
#
# Prérequis : ComponentHub compilé en Release (cmake --preset linux && cmake
# --build --preset linux), et une connexion internet au premier appel
# (téléchargement de linuxdeploy + greffon Qt, mis en cache ensuite).
#
# Usage : scripts/linux/package-appimage.sh [--build <dossier>]
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

# Un AppImage est un ELF : on valide chaque outil (magie ELF + taille plancher)
# avant de le réutiliser depuis le cache. Un téléchargement à moitié écrit y
# resterait sinon indéfiniment et casserait l'extraction (« Failed to open
# squashfs image »), car fetch() sautait tout fichier déjà présent.
appimage_ok() {
    [ -s "$1" ] || return 1
    [ "$(od -An -tx1 -N4 "$1" 2>/dev/null | tr -d ' \n')" = "7f454c46" ] || return 1
    # Seuil volontairement haut : linuxdeploy et son greffon Qt pesent >10 Mo.
    # Un telechargement tronque (vu a 1,5 Mo) passe la magie ELF mais a un squashfs
    # incomplet (« Failed to open squashfs image ») ; 4 Mo le rejette pour re-tirer.
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

# Fichier .desktop : le normaliser en LF avant de le confier a linuxdeploy.
# Un .desktop en CRLF (checkout Windows / edition sous WSL) fait lire
# « Icon=nom\r » a linuxdeploy, qui cherche alors une icone litteralement nommee
# « nom\r » et abandonne avec « Could not find suitable icon for Icon entry ».
# On retire donc tout retour chariot de fin de ligne, quel que soit l'etat de la source.
DESKTOP_DST="$APPDIR/usr/share/applications/$CMD.desktop"
mkdir -p "$(dirname "$DESKTOP_DST")"
sed 's/\r$//' "$SCRIPT_DIR/componenthub.desktop" > "$DESKTOP_DST"
chmod 644 "$DESKTOP_DST"

# --- Icone : garantir une icone AUX BONNES DIMENSIONS, sans outil externe ----
# linuxdeploy exige une icone dont la taille REELLE du fichier corresponde au
# dossier hicolor/<taille> ; sinon « Could not find suitable icon for Icon entry ».
# On lit donc les dimensions dans l'entete PNG (IHDR, gros-boutiste) et on depose
# l'icone dans le dossier exact -- fiable pour n'importe quelle source, meme sans
# ImageMagick. (Le magick.exe de Windows, visible via /mnt/c sur le PATH WSL, est
# volontairement ignore : il ne lit pas les chemins /mnt et fabrique des icones
# invalides -- c'etait la cause du refus d'icone.)
png_dim() { printf '%d' "0x$(od -An -tx1 -j"$1" -N4 "$2" | tr -d ' \n')"; }
ICON_W="$(png_dim 16 "$ICON_SRC")"
ICON_H="$(png_dim 20 "$ICON_SRC")"
ICON_MAIN="$APPDIR/usr/share/icons/hicolor/${ICON_W}x${ICON_H}/apps/$CMD.png"
install -Dm644 "$ICON_SRC" "$ICON_MAIN"
install -Dm644 "$ICON_SRC" "$APPDIR/usr/share/pixmaps/$CMD.png"

# Bonus : tailles standard supplementaires si un ImageMagick NATIF (Linux) existe.
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
echo "OK — $FINAL ($SIZE)"
