cmake_minimum_required(VERSION 3.20)
project(raidmaster_403 C)

# C11 strict partout : la logique métier (core/) doit rester portable PC ↔ ESP32.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Racine du dépôt : le code est dans software/, la doc (GIF de démo, maquettes) dans ../docs/.
get_filename_component(RAIDMASTER_REPO_DIR "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)

# Cible de build : "sim" (PC SDL, défaut) ou "firmware" (ESP-IDF, étape 5+).
# Peut être forcée par un preset ou en ligne de commande (-DRAIDMASTER_TARGET=sim).
if(NOT DEFINED RAIDMASTER_TARGET)
    set(RAIDMASTER_TARGET "sim" CACHE STRING "Build target: sim or firmware" FORCE)
endif()
set_property(CACHE RAIDMASTER_TARGET PROPERTY STRINGS "sim" "firmware")
message(STATUS "Raidmaster target: ${RAIDMASTER_TARGET}")

# Warnings stricts côté sim (GCC/Clang). Le firmware ESP-IDF a ses propres flags via idf.py.
if(RAIDMASTER_TARGET STREQUAL "sim")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
    # UI-agent exception: integration LVGL (voir prompts/ui.md).
    # Pour que la cible sim puisse afficher quelque chose, on construit la lib UI (LVGL + nos
    # écrans) et le binaire sim. Les warnings LVGL sont silencés sur sa propre cible depuis
    # ui/CMakeLists.txt sans toucher aux warnings de notre code.
    add_subdirectory(core)
    add_subdirectory(hal_sim)
    add_subdirectory(ui)
    add_subdirectory(sim)
    # Tests unitaires du core (Unity) : ctest --preset sim
    option(RAIDMASTER_TESTS "Construire les tests unitaires du core" ON)
    if(RAIDMASTER_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
elseif(RAIDMASTER_TARGET STREQUAL "firmware")
    message(FATAL_ERROR
        "La cible 'firmware' sera finalisée à l'étape 5 du plan (voir PROMPT.md § 6). "
        "Utiliser le preset 'sim' pour le moment.")
else()
    message(FATAL_ERROR "RAIDMASTER_TARGET inconnu : ${RAIDMASTER_TARGET} (attendu : sim ou firmware)")
endif()
