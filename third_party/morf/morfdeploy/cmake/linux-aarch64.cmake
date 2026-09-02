# Toolchain CMake : cross-compilation Linux ARM64 (aarch64) depuis un hote x86_64,
# pour le preset "linux-arm64-cross" (voir CMakePresets.json de chaque projet).
#
# SOURCE UNIQUE. Ce fichier vit dans morfDeploy et est vendore dans chaque projet
# via scripts/sync-morf.(sh|ps1) sous third_party/morf/morfdeploy/cmake/. Ne pas le
# dupliquer ni l'editer par depot : `morf doctor` verifie les copies vendorees.
#
# Cible = Raspberry Pi sous Debian 13 (Trixie), ARM64. Le SYSROOT doit donc etre un
# sysroot Debian Trixie arm64 (memes versions glibc + Qt6 que le Pi), construit une
# fois, reproductible et reutilisable hors-ligne :
#     morfDeploy/scripts/build-arm64-sysroot.sh
# Le build natif "linux-arm64" (sur le Pi) reste la voie de reference ; ce toolchain
# ne concerne QUE le cross-build reproductible depuis x86_64 (WSL ou Linux).
#
# Variables d'environnement :
#   MORF_SYSROOT       (OBLIGATOIRE) chemin du sysroot ARM64 (Qt6 + deps de la cible).
#   MORF_CROSS_PREFIX  prefixe des outils croises. Defaut : "aarch64-linux-gnu-".
#   MORF_QT_HOST_PATH  Qt6 HOTE x86_64 (moc/rcc/uic executables sur l'hote). Defaut :
#                      "/usr" (paquet qt6-base-dev amd64). Le Qt CIBLE (arm64) vient
#                      du sysroot ; sans cette separation, Qt tenterait d'executer un
#                      moc arm64 sur l'hote et le build echouerait.

if(NOT DEFINED ENV{MORF_SYSROOT})
    message(FATAL_ERROR
        "MORF_SYSROOT n'est pas defini. Le cross-build ARM64 exige un sysroot Debian "
        "Trixie arm64. Le construire une fois avec morfDeploy/scripts/build-arm64-sysroot.sh, "
        "puis exporter MORF_SYSROOT=<chemin> avant `cmake --preset linux-arm64-cross`.")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(DEFINED ENV{MORF_CROSS_PREFIX})
    set(_morf_prefix "$ENV{MORF_CROSS_PREFIX}")
else()
    set(_morf_prefix "aarch64-linux-gnu-")
endif()
set(CMAKE_C_COMPILER   "${_morf_prefix}gcc")
set(CMAKE_CXX_COMPILER "${_morf_prefix}g++")

set(CMAKE_SYSROOT        "$ENV{MORF_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "$ENV{MORF_SYSROOT}")

# Multiarch Debian : triplet des libs cible (le linker et find_library en ont besoin).
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

# Outils (compilateurs, moc, rcc...) cherches sur l'HOTE ; libs/headers/paquets
# UNIQUEMENT dans la CIBLE (le sysroot). C'est la regle d'or de la cross-compilation.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Qt6 : moc/rcc/uic sont des binaires arm64 (dans le sysroot). Deux strategies :
#   - DEFAUT (reproductible, retenu) : on laisse Qt utiliser les outils de la CIBLE
#     (sysroot), executes en EMULATION via qemu-user-static. Le preset exporte alors
#     QEMU_LD_PREFIX=$MORF_SYSROOT pour que qemu trouve le loader (ld-linux-aarch64)
#     et les libs Qt arm64 dans le sysroot. Versions outils/Qt identiques par
#     construction, AUCUN Qt hote requis. Seuls moc/rcc/uic sont emules (rapides) ;
#     le compilateur reste le vrai cross-gcc x86_64.
#   - AVANCE (plus rapide) : fournir un Qt6 HOTE x86_64 de MEME version que la cible
#     via MORF_QT_HOST_PATH ; Qt utilisera alors ces outils natifs (pas d'emulation).
# Ne PAS forcer un QT_HOST_PATH par defaut : un Qt hote de version differente casse
# le build (Qt exige outils hote == Qt cible).
if(DEFINED ENV{MORF_QT_HOST_PATH})
    set(QT_HOST_PATH "$ENV{MORF_QT_HOST_PATH}" CACHE PATH "Qt6 hote (outils moc/rcc/uic)")
endif()

# pkg-config resout dans le sysroot (certains find_package s'appuient dessus).
set(ENV{PKG_CONFIG_SYSROOT_DIR} "$ENV{MORF_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "$ENV{MORF_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:$ENV{MORF_SYSROOT}/usr/share/pkgconfig")
