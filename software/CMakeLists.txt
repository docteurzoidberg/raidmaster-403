cmake_minimum_required(VERSION 3.20)
project(raidmaster_403 C)

# C11 strict partout : la logique métier (core/) doit rester portable PC ↔ ESP32.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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
    add_subdirectory(sim)
elseif(RAIDMASTER_TARGET STREQUAL "firmware")
    message(FATAL_ERROR
        "La cible 'firmware' sera finalisée à l'étape 5 du plan (voir PROMPT.md § 6). "
        "Utiliser le preset 'sim' pour le moment.")
else()
    message(FATAL_ERROR "RAIDMASTER_TARGET inconnu : ${RAIDMASTER_TARGET} (attendu : sim ou firmware)")
endif()
