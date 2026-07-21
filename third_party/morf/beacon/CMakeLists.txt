# morfBeacon — copie vendorée (supervision LAN : heartbeat UDP + /status HTTP).
#
# Source de vérité : dépôt morfBeacon. Ce dossier en est une copie
# synchronisée par scripts/sync-morf.(ps1|sh). NE PAS éditer le code ici :
# modifier dans le dépôt puis resynchroniser. Version vendorée : voir VERSION.
#
# Volontairement autonome (ni presets, ni exemples) : ne dépend que de Qt.

cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
set(CMAKE_AUTOMOC ON)

add_library(morfBeacon STATIC
    src/Heartbeat.cpp
    src/StatusServer.cpp
    src/PresenceService.cpp
    include/morfbeacon/PresenceConfig.h
    include/morfbeacon/IMetricsProvider.h
    include/morfbeacon/Heartbeat.h
    include/morfbeacon/StatusServer.h
    include/morfbeacon/PresenceService.h
)
add_library(morfBeacon::morfBeacon ALIAS morfBeacon)

target_include_directories(morfBeacon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(morfBeacon PUBLIC Qt6::Core Qt6::Network)

if(MSVC)
    target_compile_options(morfBeacon PRIVATE /utf-8)
endif()

# --- Verification de fraicheur de la copie (informative, jamais bloquante) ----
#
# Compare cette copie a la source canonique SI le depot morfBeacon se trouve a
# cote dans l'espace de travail. Elle ne modifie RIEN : une resynchronisation
# automatique ferait dependre le binaire produit de l'etat d'un depot voisin,
# donc deux compilations du meme commit pourraient differer, et une CI sans ce
# voisin ne compilerait pas la meme chose. C'est exactement la reproductibilite
# que la recopie vendoree existe pour garantir.
#
# Elle se contente donc de le DIRE, au moment ou l'on compile. Resynchroniser
# reste un geste explicite, qui laisse un commit derriere lui.
set(_mb_local "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
foreach(_mb_up "${CMAKE_CURRENT_SOURCE_DIR}/../../../../morfBeacon"
               "${CMAKE_CURRENT_SOURCE_DIR}/../../../../morfBeacon_travail")
    if(EXISTS "${_mb_up}/VERSION" AND EXISTS "${_mb_local}")
        file(STRINGS "${_mb_local}" _mb_v LIMIT_COUNT 1)
        file(STRINGS "${_mb_up}/VERSION" _mb_u LIMIT_COUNT 1)
        string(STRIP "${_mb_v}" _mb_v)
        string(STRIP "${_mb_u}" _mb_u)
        if(NOT _mb_v STREQUAL _mb_u)
            message(STATUS "morfBeacon vendore : ${_mb_v}  |  amont disponible : ${_mb_u}"
                           "  ->  ./scripts/sync-morf.sh pour mettre a jour")
        else()
            message(STATUS "morfBeacon vendore : ${_mb_v} (a jour)")
        endif()
        break()
    endif()
endforeach()
