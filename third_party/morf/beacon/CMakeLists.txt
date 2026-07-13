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
