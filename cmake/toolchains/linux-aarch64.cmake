# Toolchain CMake : cross-compilation vers Linux ARM64 (aarch64), pour le
# preset "linux-arm64-cross" (voir CMakePresets.json).
#
# Prérequis côté machine hôte :
#   - une toolchain croisée "aarch64-linux-gnu-" (gcc/g++) ;
#   - un SYSROOT Linux ARM64 contenant Qt6 et nlohmann-json (headers + libs).
#
# Variables d'environnement (optionnelles) :
#   CH_CROSS_PREFIX : préfixe des outils (défaut : "aarch64-linux-gnu-")
#   CH_SYSROOT      : chemin du sysroot cible

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(DEFINED ENV{CH_CROSS_PREFIX})
    set(_ch_prefix "$ENV{CH_CROSS_PREFIX}")
else()
    set(_ch_prefix "aarch64-linux-gnu-")
endif()

set(CMAKE_C_COMPILER   "${_ch_prefix}gcc")
set(CMAKE_CXX_COMPILER "${_ch_prefix}g++")

if(DEFINED ENV{CH_SYSROOT})
    set(CMAKE_SYSROOT "$ENV{CH_SYSROOT}")
    list(APPEND CMAKE_FIND_ROOT_PATH "$ENV{CH_SYSROOT}")
endif()

# Outils (moc, rcc…) cherchés sur l'HÔTE ; libs/headers/paquets dans la CIBLE.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
