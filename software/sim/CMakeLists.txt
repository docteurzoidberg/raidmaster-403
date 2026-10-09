# Simulateur PC : fenêtre SDL 480x320 landscape, résolution exacte du ST7796.
# Étape 1 du plan : squelette seul, aucune logique métier ni LVGL (ça vient en étape 3).

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

add_executable(tripmaster-sim main.c)
target_link_libraries(tripmaster-sim PRIVATE ${SIM_SDL2_TARGET})

# Chemin de sortie prévisible : build-sim/tripmaster-sim (vs build-sim/sim/tripmaster-sim).
set_target_properties(tripmaster-sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
