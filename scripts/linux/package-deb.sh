#!/usr/bin/env bash
#
# ComponentHub — construction d'un paquet Debian (.deb).
#
# Produit dist/componenthub_<version>_<arch>.deb, installable via
#   sudo apt install ./componenthub_<version>_<arch>.deb
# Le binaire est lié dynamiquement au Qt du système : le paquet DÉCLARE ses
# dépendances (Qt6…) pour qu'apt les installe automatiquement.
#
# À utiliser sur Debian / Ubuntu / Raspberry Pi OS, après une compilation
# NATIVE sur la même famille de distribution :
#   cmake --preset linux        (x86_64)   ->  build/ComponentHub
#   cmake --preset linux-arm64  (ARM64)    ->  build-arm64/ComponentHub
#
# Usage :
#   scripts/linux/package-deb.sh [--build <dossier>] [--maintainer "Nom <email>"] [--depends "pkg, …"]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

APP_NAME="ComponentHub"   # nom du binaire compilé
CMD="componenthub"        # nom du paquet / commande / .desktop / icône
BUILD_DIR=""
MAINTAINER="${DEB_MAINTAINER:-morfredus <morfredus@users.noreply.github.com>}"
DEPENDS_OVERRIDE=""

BUILD_CANDIDATES=("$ROOT/build" "$ROOT/build-arm64" "$ROOT/build-arm64-cross")

die() { printf 'Erreur : %s\n' "$1" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --build)      BUILD_DIR="${2:-}"; shift ;;
        --maintainer) MAINTAINER="${2:-}"; shift ;;
        --depends)    DEPENDS_OVERRIDE="${2:-}"; shift ;;
        -h|--help)
            cat <<'EOF'
ComponentHub — construction d'un paquet Debian (.deb).
Usage : scripts/linux/package-deb.sh [options]
  --build <dossier>            Dossier de compilation (défaut : auto — build/ puis build-arm64/)
  --maintainer "Nom <email>"   Champ Maintainer du paquet
  --depends "pkg, pkg…"        Force les dépendances (sinon détection auto)
  -h, --help                   Affiche cette aide
EOF
            exit 0 ;;
        *) die "option inconnue : $1 (voir --help)" ;;
    esac
    shift
done

[ "$(uname -s)" = "Linux" ] || die "ce script doit être exécuté sous Linux."
command -v dpkg-deb >/dev/null 2>&1 || die "dpkg-deb introuvable (paquet 'dpkg')."

if [ -n "$BUILD_DIR" ]; then
    BINARY="$BUILD_DIR/$APP_NAME"
    [ -x "$BINARY" ] || die "binaire introuvable : $BINARY (compile d'abord, ou --build <dossier>)"
else
    for dir in "${BUILD_CANDIDATES[@]}"; do
        if [ -x "$dir/$APP_NAME" ]; then BUILD_DIR="$dir"; BINARY="$dir/$APP_NAME"; break; fi
    done
    [ -n "${BINARY:-}" ] || die \
"binaire introuvable dans : ${BUILD_CANDIDATES[*]}
  Compile d'abord :  cmake --preset linux && cmake --build --preset linux"
fi
echo "Binaire : $BINARY"

VERSION="$(head -n1 "$ROOT/VERSION" | tr -d '[:space:]')"
[ -n "$VERSION" ] || die "fichier VERSION vide ou absent."

ARCH="$(dpkg --print-architecture)"
ICON_SRC="$ROOT/resources/logo.png"
[ -f "$ICON_SRC" ] || die "icône introuvable : $ICON_SRC"

detect_depends() {
    ldd "$BINARY" 2>/dev/null | awk '/=> \//{print $3}' | sort -u \
        | while read -r lib; do dpkg -S "$lib" 2>/dev/null | cut -d: -f1; done \
        | sort -u | paste -sd, - | sed 's/,/, /g'
}

if [ -n "$DEPENDS_OVERRIDE" ]; then
    DEPENDS="$DEPENDS_OVERRIDE"
else
    DEPENDS="$(detect_depends || true)"
    [ -n "$DEPENDS" ] || DEPENDS="libc6, libqt6core6, libqt6gui6, libqt6widgets6"
    # Le plugin xcb (chargé par dlopen) exige libxcb-cursor0 depuis Qt 6.5.
    case ", $DEPENDS," in
        *", libxcb-cursor0,"*) : ;;
        *) DEPENDS="$DEPENDS, libxcb-cursor0" ;;
    esac
fi

PKGDIR="$ROOT/dist/deb/${CMD}_${VERSION}_${ARCH}"
rm -rf "$PKGDIR"
install -Dm755 "$BINARY" "$PKGDIR/usr/bin/$CMD"

sed "s|^Exec=.*|Exec=/usr/bin/$CMD|" "$SCRIPT_DIR/componenthub.desktop" \
    > "$PKGDIR/usr/share/applications/$CMD.desktop"
chmod 644 "$PKGDIR/usr/share/applications/$CMD.desktop"

if command -v magick >/dev/null 2>&1 || command -v convert >/dev/null 2>&1; then
    CONVERT="convert"; command -v magick >/dev/null 2>&1 && CONVERT="magick"
    for size in 16 24 32 48 64 128 256; do
        dest="$PKGDIR/usr/share/icons/hicolor/${size}x${size}/apps/$CMD.png"
        mkdir -p "$(dirname "$dest")"
        "$CONVERT" "$ICON_SRC" -resize "${size}x${size}" "$dest"
    done
else
    install -Dm644 "$ICON_SRC" "$PKGDIR/usr/share/icons/hicolor/256x256/apps/$CMD.png"
fi

install -d "$PKGDIR/usr/share/doc/$CMD"
cat > "$PKGDIR/usr/share/doc/$CMD/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: ComponentHub
Source: https://morfredus.fr

Files: *
Copyright: 2026 morfredus
License: GPL-3.0-only
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License, version 3.
 .
 On Debian systems, the full text is in /usr/share/common-licenses/GPL-3.
EOF
printf '%s (%s) unstable; urgency=low\n\n  * ComponentHub %s.\n\n -- %s  %s\n' \
    "$CMD" "$VERSION" "$VERSION" "$MAINTAINER" "$(date -R)" \
    | gzip -9n > "$PKGDIR/usr/share/doc/$CMD/changelog.Debian.gz"

INSTALLED_KB="$(du -sk "$PKGDIR" | cut -f1)"

install -d "$PKGDIR/DEBIAN"
cat > "$PKGDIR/DEBIAN/control" <<EOF
Package: $CMD
Version: $VERSION
Architecture: $ARCH
Maintainer: $MAINTAINER
Installed-Size: $INSTALLED_KB
Depends: $DEPENDS
Section: electronics
Priority: optional
Homepage: https://morfredus.fr
Description: Workshop electronics inventory (technical memory)
 ComponentHub keeps the full technical memory of an electronics workshop:
 components, stock and movements, physical locations, modules, tools,
 consumables, documentation and projects (bill of materials).
 .
 Cross-platform Qt/C++ desktop application (Windows, Linux, Raspberry Pi),
 sharing its core with the ESP32 firmware and interchangeable .tar backups.
EOF

OUT="$ROOT/dist/${CMD}_${VERSION}_${ARCH}.deb"
if dpkg-deb --help 2>&1 | grep -q -- '--root-owner-group'; then
    dpkg-deb --build --root-owner-group "$PKGDIR" "$OUT"
else
    dpkg-deb --build "$PKGDIR" "$OUT"
fi

SIZE="$(du -h "$OUT" | cut -f1)"
echo "OK — $OUT ($SIZE, $ARCH)"
echo "Dépendances : $DEPENDS"
echo "Installation :  sudo apt install \"$OUT\""
