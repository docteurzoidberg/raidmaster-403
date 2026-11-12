cmake_minimum_required(VERSION 3.20)
project(raidmaster_403 C)

# C11 strict partout : la logique métier (core/) doit rester portable PC ↔ ESP32.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Racine du dépôt : le code est dans software/, la doc (GIF de démo, maquettes) dans ../docs/.
get_filename_component(RAIDMASTER_REPO_DIR "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)

# Cible de build : "sim" (PC SDL, défaut) ou "web" (simulateur WebAssembly, emcc : tools/web.sh,
# preset web). Le firmware ESP32 est un projet ESP-IDF à part (firmware/, construit par idf.py via
# tools/fw.sh) qui reprend les mêmes core/ et ui/.
if(NOT DEFINED RAIDMASTER_TARGET)
    set(RAIDMASTER_TARGET "sim" CACHE STRING "Build target: sim, web (firmware : tools/fw.sh)" FORCE)
endif()
set_property(CACHE RAIDMASTER_TARGET PROPERTY STRINGS "sim" "web" "firmware")
message(STATUS "Raidmaster target: ${RAIDMASTER_TARGET}")

# Warnings stricts côté sim (GCC/Clang). Le firmware ESP-IDF a ses propres flags via idf.py.
if(RAIDMASTER_TARGET STREQUAL "sim")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
    # SDL2 : essaie find_package (standard CMake), tombe sur pkg-config sinon (Debian/Ubuntu/WSL
    # installent SDL2 via libsdl2-dev qui pose les .pc mais pas toujours les SDL2Config.cmake).
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        set(SIM_SDL2_TARGET PkgConfig::SDL2)
    else()
        # Les deux formes selon la version du module trouvé : cible SDL2::SDL2 moderne, sinon variables.
        if(TARGET SDL2::SDL2)
            set(SIM_SDL2_TARGET SDL2::SDL2)
        else()
            add_library(sim_sdl2_fallback INTERFACE)
            target_include_directories(sim_sdl2_fallback INTERFACE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(sim_sdl2_fallback INTERFACE ${SDL2_LIBRARIES})
            set(SIM_SDL2_TARGET sim_sdl2_fallback)
        endif()
    endif()

    # Intégration LVGL :
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
elseif(RAIDMASTER_TARGET STREQUAL "web")
    # Simulateur web : mêmes core/ et ui/, hal_simweb/ (qui reprend hal_sim/ sauf stockage et son),
    # front-end simweb/ ; sortie dans build-web/site/, recopiée dans mockup/sim/ par tools/web.sh.
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "La cible web se construit avec emcc : tools/web.sh (preset web).")
    endif()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Le module est publié : chemins des sources (__FILE__ des assertions LVGL) relatifs au dépôt, sans le
    # chemin du poste qui l'a construit.
    add_compile_options("-ffile-prefix-map=${RAIDMASTER_REPO_DIR}/=")
    if(FETCHCONTENT_SOURCE_DIR_LVGL)
        add_compile_options("-ffile-prefix-map=${FETCHCONTENT_SOURCE_DIR_LVGL}=lvgl")
    endif()
    # Python (gen_route.py pour les traces des scénarios, animation de démarrage), comme le sim.
    if(NOT RAIDMASTER_PYTHON)
        find_program(RAIDMASTER_PYTHON NAMES python3 python
                     PATHS "${RAIDMASTER_REPO_DIR}/.venv/bin" "${RAIDMASTER_REPO_DIR}/../../.venv/bin")
    endif()
    add_subdirectory(core)
    add_subdirectory(hal_simweb)
    add_subdirectory(ui)
    add_subdirectory(simweb)
elseif(RAIDMASTER_TARGET STREQUAL "firmware")
    message(FATAL_ERROR
        "Le firmware se construit avec ESP-IDF : tools/fw.sh build (voir firmware/README.md).")
else()
    message(FATAL_ERROR "RAIDMASTER_TARGET inconnu : ${RAIDMASTER_TARGET} (attendu : sim, web ou firmware)")
endif()
