cmake_minimum_required(VERSION 3.21)

# Version lue depuis le fichier VERSION à la racine.
set(CH_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CH_VERSION_FILE}")
file(STRINGS "${CH_VERSION_FILE}" CH_VERSION LIMIT_COUNT 1)
string(STRIP "${CH_VERSION}" CH_VERSION)

project(ComponentHub VERSION "${CH_VERSION}" LANGUAGES CXX)

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
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET ComponentHub POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --no-translations --compiler-runtime "$<TARGET_FILE:ComponentHub>"
            COMMENT "Déploiement des DLL Qt à côté de l'exécutable")
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
        get_filename_component(COMPONENTHUB_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(COMPONENTHUB_MINGW_ROOT "${COMPONENTHUB_COMPILER_BIN}" DIRECTORY)
        get_filename_component(COMPONENTHUB_MSYS_ROOT "${COMPONENTHUB_MINGW_ROOT}" DIRECTORY)
        set(COMPONENTHUB_MSYS_BASH "${COMPONENTHUB_MSYS_ROOT}/usr/bin/bash.exe")
        if(EXISTS "${COMPONENTHUB_MSYS_BASH}")
            add_custom_command(TARGET ComponentHub POST_BUILD
                # Do not use `bash` from PATH: Windows may resolve it to WSL,
                # which cannot read this Windows workspace. The matching MSYS2
                # Bash is derived from the active MinGW compiler instead.
                COMMAND "${COMPONENTHUB_MSYS_BASH}" "${COMPONENTHUB_DEPLOY_SCRIPT}" "${COMPONENTHUB_TARGET_DIR}"
                COMMENT "Copie des DLL MinGW à côté de l'exécutable")
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
