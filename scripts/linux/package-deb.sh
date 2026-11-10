#!/usr/bin/env bash
#
# ComponentHub - building a Debian package (.deb).
#
# Produit dist/componenthub_<version>_<arch>.deb, installable via
#   sudo apt install ./componenthub_<version>_<arch>.deb
# The binary is dynamically linked to the system Qt: the package DECLARES its
# dependencies (Qt6...) so that apt installs them automatically.
#
# To be used on Debian / Ubuntu / Raspberry Pi OS, after a NATIVE
# build on the same distribution family:
#   cmake --preset linux        (x86_64)   ->  build/ComponentHub
#   cmake --preset linux-arm64  (ARM64)    ->  build-arm64/ComponentHub
#
# Usage :
#   scripts/linux/package-deb.sh [--build <folder>] [--maintainer "Name <email>"] [--depends "pkg, ..."]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

APP_NAME="ComponentHub"   # name of the compiled binary
CMD="componenthub"        # name of the package / command / .desktop / icon
BUILD_DIR=""
MAINTAINER="${DEB_MAINTAINER:-morfredus <morfredus@users.noreply.github.com>}"
DEPENDS_OVERRIDE=""

BUILD_CANDIDATES=("$ROOT/build" "$ROOT/build-arm64" "$ROOT/build-arm64-cross")

die() { printf 'Erreur : %s\n' "$1" >&2; exit 1; }

# Real architecture of an ELF, read from the header (e_machine, offset 18, little-endian).
# 0xb7 = aarch64, 0x3e = x86-64. Used to recognize a cross-built binary without trusting
# the host: under x86_64 WSL, an arm64 binary (build-arm64-cross/) must be packaged
# as arm64, not amd64.
_elf_arch() {
    case "$(od -An -tx1 -j18 -N2 "$1" 2>/dev/null | tr -d ' ')" in
        b700) echo arm64 ;;
        3e00) echo x86_64 ;;
        *)    echo "" ;;
    esac
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build)      BUILD_DIR="${2:-}"; shift ;;
        --maintainer) MAINTAINER="${2:-}"; shift ;;
        --depends)    DEPENDS_OVERRIDE="${2:-}"; shift ;;
        -h|--help)
            cat <<'EOF'
ComponentHub - construction d'un paquet Debian (.deb).
Usage : scripts/linux/package-deb.sh [options]
  --build <dossier>            Dossier de compilation (défaut : auto - build/ puis build-arm64/)
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

# Cross build: an aarch64 binary on a host that is not aarch64 (x86_64 WSL).
# The package arch and its Depends then CANNOT come from the host -- `dpkg
# --print-architecture` returns amd64, and `ldd`/`dpkg -S` are blind to an arm64 ELF.
CROSS_ARM64=0
if [ "$(_elf_arch "$BINARY")" = "arm64" ] && [ "$(uname -m)" != "aarch64" ]; then
    CROSS_ARM64=1
fi

if [ "$CROSS_ARM64" = "1" ]; then
    ARCH="arm64"
else
    ARCH="$(dpkg --print-architecture)"
fi
ICON_SRC="$ROOT/resources/logo.png"
[ -f "$ICON_SRC" ] || die "icône introuvable : $ICON_SRC"

detect_depends() {
    ldd "$BINARY" 2>/dev/null | awk '/=> \//{print $3}' | sort -u \
        | while read -r lib; do dpkg -S "$lib" 2>/dev/null | cut -d: -f1; done \
        | sort -u | paste -sd, - | sed 's/,/, /g'
}

# Depends of a cross-built binary: NEEDED sonames read with the target's objdump, then
# resolved in the sysroot's .shlibs -- the proven logic of morfdeploy
# (cross_depends), reused from the vendored copy rather than rewritten.
cross_depends() {
    ROOT="$ROOT" python3 - "$BINARY" "$MORF_SYSROOT" "aarch64-linux-gnu-objdump" <<'PY'
import os, sys
from pathlib import Path
sys.path.insert(0, os.path.join(os.environ["ROOT"], "third_party", "morf"))
from morfdeploy.package import cross_depends as _cd
print(", ".join(_cd(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3])))
PY
}

if [ -n "$DEPENDS_OVERRIDE" ]; then
    DEPENDS="$DEPENDS_OVERRIDE"
elif [ "$CROSS_ARM64" = "1" ]; then
    command -v aarch64-linux-gnu-objdump >/dev/null 2>&1 \
        || die "objdump cible manquant : installe binutils-aarch64-linux-gnu."
    [ -n "${MORF_SYSROOT:-}" ] && [ -d "$MORF_SYSROOT" ] \
        || die "MORF_SYSROOT absent : sysroot arm64 requis pour les Depends croises."
    DEPENDS="$(cross_depends)"
    [ -n "$DEPENDS" ] || die "Depends croises vides (sysroot sans .shlibs ?)."
    # The xcb plugin (loaded through dlopen) requires libxcb-cursor0 since Qt 6.5.
    case ", $DEPENDS," in
        *", libxcb-cursor0,"*) : ;;
        *) DEPENDS="$DEPENDS, libxcb-cursor0" ;;
    esac
else
    DEPENDS="$(detect_depends || true)"
    [ -n "$DEPENDS" ] || DEPENDS="libc6, libqt6core6, libqt6gui6, libqt6widgets6"
    # The xcb plugin (loaded through dlopen) requires libxcb-cursor0 since Qt 6.5.
    case ", $DEPENDS," in
        *", libxcb-cursor0,"*) : ;;
        *) DEPENDS="$DEPENDS, libxcb-cursor0" ;;
    esac
fi

# Prepare the package tree in the NATIVE file system (ext4 under
# WSL, or the Pi's disk), NEVER on /mnt/c: DrvFs forces mode 777 on the
# files of the Windows drive and ignores chmod there, yet dpkg-deb refuses a
# DEBIAN/ control folder in 777 (it requires <= 0775). Only the final .deb goes to dist/.
STAGE_ROOT="$(mktemp -d)"
trap 'rm -rf "$STAGE_ROOT"' EXIT
PKGDIR="$STAGE_ROOT/${CMD}_${VERSION}_${ARCH}"
install -Dm755 "$BINARY" "$PKGDIR/usr/bin/$CMD"

# The `>` redirection does not create parent folders: the .desktop shortcuts
# folder is prepared before writing into it (otherwise "No such file or directory").
install -d "$PKGDIR/usr/share/applications"
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
install -d "$ROOT/dist"   # staging is no longer under dist/: create the target of the .deb
if dpkg-deb --help 2>&1 | grep -q -- '--root-owner-group'; then
    dpkg-deb --build --root-owner-group "$PKGDIR" "$OUT"
else
    dpkg-deb --build "$PKGDIR" "$OUT"
fi

SIZE="$(du -h "$OUT" | cut -f1)"
echo "OK - $OUT ($SIZE, $ARCH)"
echo "Dépendances : $DEPENDS"
echo "Installation :  sudo apt install \"$OUT\""
