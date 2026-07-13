# morfUpdate — copie vendorée (détection de mises à jour).
#
# Source de vérité : dépôt morfUpdate. Ce dossier en est une copie
# synchronisée par scripts/sync-morf.(ps1|sh). NE PAS éditer le code ici :
# modifier dans le dépôt puis resynchroniser. Version vendorée : voir VERSION.
#
# Deux cibles : le cœur (Core+Network) et la couche UI optionnelle (Widgets).

cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
set(CMAKE_AUTOMOC ON)

add_library(morfUpdate STATIC
    src/Version.cpp
    src/GitHubReleaseSource.cpp
    src/UpdateChecker.cpp
    include/morfupdate/Version.h
    include/morfupdate/ReleaseInfo.h
    include/morfupdate/morfUpdateConfig.h
    include/morfupdate/IUpdateSource.h
    include/morfupdate/GitHubReleaseSource.h
    include/morfupdate/UpdateChecker.h
)
add_library(morfUpdate::morfUpdate ALIAS morfUpdate)

target_include_directories(morfUpdate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(morfUpdate PUBLIC Qt6::Core Qt6::Network)

if(MSVC)
    target_compile_options(morfUpdate PRIVATE /utf-8)
endif()

# Couche UI (dialogue de notification). Désactivable pour une app sans interface.
option(MORFUPDATE_BUILD_WIDGETS "Compiler la couche UI morfUpdate (Qt Widgets)" ON)
if(MORFUPDATE_BUILD_WIDGETS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_library(morfUpdateWidgets STATIC
        src/UpdateDialog.cpp
        include/morfupdate/UpdateDialog.h
    )
    add_library(morfUpdate::Widgets ALIAS morfUpdateWidgets)
    target_link_libraries(morfUpdateWidgets PUBLIC morfUpdate::morfUpdate Qt6::Widgets)
    if(MSVC)
        target_compile_options(morfUpdateWidgets PRIVATE /utf-8)
    endif()
endif()
