# Simulateur PC : fenêtre SDL 480x320 landscape, résolution exacte de l'écran (AXS15231B).
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

# demo_record.c + gif_writer.c : mode enregistrement d'un GIF de démo (tools/demo_gif.sh).
add_executable(raidmaster-sim main.c demo_record.c gif_writer.c)
# UI-agent exception: integration LVGL (voir prompts/ui.md).
# On link l'UI pour piloter LVGL depuis la fenêtre SDL que ce binaire ouvre déjà.
target_link_libraries(raidmaster-sim PRIVATE ${SIM_SDL2_TARGET} raidmaster_ui raidmaster_hal_sim)

# libm pour sinf() dans ui_demo.c (via raidmaster_ui). CMake ne le propage pas tout seul.
if(UNIX AND NOT APPLE)
    target_link_libraries(raidmaster-sim PRIVATE m)
endif()

# Chemin de sortie prévisible : build-sim/raidmaster-sim (vs build-sim/sim/raidmaster-sim).
set_target_properties(raidmaster-sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# GIF de démo régénéré automatiquement à chaque build qui change le sim (donc toute modif de l'UI,
# liée statiquement). Enregistrement déterministe (temps virtuel) : docs/demo/raidmaster-demo.gif
# n'est réécrit que si son contenu change. Désactivable : -DRAIDMASTER_DEMO_GIF=OFF.
# À la main (échelle ×2, autre sortie) : tools/demo_gif.sh.
option(RAIDMASTER_DEMO_GIF "Régénérer docs/demo/raidmaster-demo.gif à chaque build du sim" ON)
if(RAIDMASTER_DEMO_GIF)
    set(DEMO_GIF_TMP "${CMAKE_BINARY_DIR}/demo-gif/raidmaster-demo.gif")
    set(DEMO_GIF_OUT "${RAIDMASTER_REPO_DIR}/docs/demo/raidmaster-demo.gif")
    add_custom_command(
        OUTPUT "${DEMO_GIF_TMP}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/demo-gif"
        COMMAND ${CMAKE_COMMAND} -E env
                SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
                TMPDIR=${CMAKE_BINARY_DIR}/demo-gif
                RAIDMASTER_RECORD=${DEMO_GIF_TMP} RAIDMASTER_RECORD_SCALE=1
                $<TARGET_FILE:raidmaster-sim>
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RAIDMASTER_REPO_DIR}/docs/demo"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DEMO_GIF_TMP}" "${DEMO_GIF_OUT}"
        DEPENDS raidmaster-sim ${RAIDMASTER_BOOT_ANIM_BIN}
        COMMENT "GIF de démo de l'UI → docs/demo/raidmaster-demo.gif"
        VERBATIM)
    add_custom_target(demo_gif ALL DEPENDS "${DEMO_GIF_TMP}")
endif()
