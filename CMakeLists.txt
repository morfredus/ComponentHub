cmake_minimum_required(VERSION 3.21)

# Version lue depuis le fichier VERSION à la racine.
set(CH_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CH_VERSION_FILE}")
file(STRINGS "${CH_VERSION_FILE}" CH_VERSION LIMIT_COUNT 1)
string(STRIP "${CH_VERSION}" CH_VERSION)

project(ComponentHub VERSION "${CH_VERSION}" LANGUAGES CXX)


# --- Enregistrement des compilations (best-effort) ------------------------
# Chronometre le vrai travail des compilateurs et signale la duree a
# morfAnalytics (POST /api/monitor/activity), QUEL QUE SOIT le declencheur :
# `cmake --build` direct, `morf upgrade` ou un deploiement morfDeploy (un
# marqueur evite le double comptage cote morfDeploy). Sans URL configuree
# (machine hors parc), c'est un no-op silencieux. A appeler AVANT les cibles.
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/morf/morfdeploy/record_compile.cmake")
morf_enable_compile_recording()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CH_DOMAIN "${CMAKE_SOURCE_DIR}/src/domain")

# --- Dépendances ----------------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# --- Modules communs « morf » (copies vendorées dans third_party/) ---------
# Autonome : aucune dépendance à un dépôt externe, compilé dans l'exécutable.
# Voir third_party/morf/README.md. Resynchro : scripts/sync-morf.(ps1|sh).
add_subdirectory(third_party/morf/beacon)
add_subdirectory(third_party/morf/update)

# =====================================================================
# Cœur portable : le domaine (src/domain/) + le stockage fichier desktop.
# Ne dépend NI de Qt NI d'Arduino.
# =====================================================================
add_library(componenthub_core STATIC
    ${CH_DOMAIN}/csv.cpp
    ${CH_DOMAIN}/inventory_service.cpp
    ${CH_DOMAIN}/project_service.cpp
    ${CH_DOMAIN}/import_export_service.cpp
    src/storage/FileRepositories.cpp
    src/platform/Tar.cpp
    src/net/HttpClient.cpp        # client HTTP minimal (synchro HomeServerHub)
    src/sync/SyncService.cpp      # orchestration PUSH/PULL vers le hub
)
target_include_directories(componenthub_core PUBLIC ${CH_DOMAIN} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(componenthub_core PUBLIC nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(componenthub_core PUBLIC ws2_32)   # winsock2 (HttpClient)
endif()
if(MSVC)
    target_compile_options(componenthub_core PRIVATE /utf-8)
endif()

# =====================================================================
# Application graphique Qt (le SEUL endroit qui dépend de Qt).
# =====================================================================
set(CH_APP_RESOURCES resources/app.qrc)
if(WIN32)
    list(APPEND CH_APP_RESOURCES resources/app.rc)   # icône de l'exécutable Windows
endif()

add_executable(ComponentHub WIN32
    src/main.cpp
    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/MainWindow.cpp
    src/ui/InventoryPage.cpp
    src/ui/ComponentDialog.cpp
    src/ui/CategoriesPage.cpp
    src/ui/LocationsPage.cpp
    src/ui/ProjectsPage.cpp
    src/ui/ReferentialsPage.cpp
    src/ui/ImportExportPage.cpp
    src/ui/SettingsPage.cpp
    ${CH_APP_RESOURCES}
)
target_link_libraries(ComponentHub PRIVATE componenthub_core Qt6::Widgets morfBeacon::morfBeacon morfUpdate::morfUpdate morfUpdate::Widgets)
target_compile_definitions(ComponentHub PRIVATE CH_APP_VERSION="${CH_VERSION}")
if(MSVC)
    target_compile_options(ComponentHub PRIVATE /utf-8)
endif()

# --- Déploiement Windows : DLL Qt (windeployqt) + DLL MinGW (script) -------
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt
        HINTS "${Qt6_DIR}/../../../bin" "$ENV{QT_ROOT}/bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET ComponentHub POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --no-translations --compiler-runtime "$<TARGET_FILE:ComponentHub>"
            COMMENT "Deploiement des DLL Qt a cote de l'executable")
    endif()
    if(NOT MSVC)
        function(componenthub_msys_path windows_path output)
            string(REGEX REPLACE "^([A-Za-z]):/" "/\\1/" converted "${windows_path}")
            string(SUBSTRING "${converted}" 1 1 drive)
            string(TOLOWER "${drive}" drive)
            string(SUBSTRING "${converted}" 2 -1 tail)
            set(${output} "/${drive}${tail}" PARENT_SCOPE)
        endfunction()
        componenthub_msys_path("${CMAKE_SOURCE_DIR}/scripts/windows/deploy-mingw.sh"
                               COMPONENTHUB_DEPLOY_SCRIPT)
        componenthub_msys_path("${CMAKE_CURRENT_BINARY_DIR}" COMPONENTHUB_TARGET_DIR)
        set(_componenthub_cxx "${CMAKE_CXX_COMPILER}")
        if(NOT EXISTS "${_componenthub_cxx}")
            find_program(_componenthub_cxx NAMES "${CMAKE_CXX_COMPILER}" g++.exe)
        endif()
        get_filename_component(COMPONENTHUB_COMPILER_BIN "${_componenthub_cxx}" DIRECTORY)
        get_filename_component(COMPONENTHUB_MINGW_ROOT "${COMPONENTHUB_COMPILER_BIN}" DIRECTORY)
        get_filename_component(COMPONENTHUB_MSYS_ROOT "${COMPONENTHUB_MINGW_ROOT}" DIRECTORY)
        find_program(COMPONENTHUB_MSYS_BASH bash.exe
            HINTS "${COMPONENTHUB_MSYS_ROOT}/usr/bin"
            NO_DEFAULT_PATH)
        if(COMPONENTHUB_MSYS_BASH)
            add_custom_command(TARGET ComponentHub POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E env
                    "PATH=${COMPONENTHUB_MSYS_ROOT}/usr/bin;${COMPONENTHUB_COMPILER_BIN}"
                    --
                    "${COMPONENTHUB_MSYS_BASH}" "${COMPONENTHUB_DEPLOY_SCRIPT}" "${COMPONENTHUB_TARGET_DIR}"
                VERBATIM
                COMMENT "Copie des DLL MinGW a cote de l'executable")
        else()
            message(WARNING "MSYS2 Bash not found; non-Qt MinGW DLL collection is skipped")
        endif()
    endif()
endif()

# --- Test de fumée headless (optionnel) ----------------------------------
option(CH_BUILD_SMOKE "Construire le test de fumée headless" OFF)
if(CH_BUILD_SMOKE)
    add_executable(ch_smoke src/smoke_main.cpp)
    target_link_libraries(ch_smoke PRIVATE componenthub_core)
endif()

# Signale la duree de compile a morfAnalytics apres le lien de l'executable.
morf_record_target_build(ComponentHub "ComponentHub")
