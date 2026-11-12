# Simulateur PC : fenêtre SDL 480x320 landscape, résolution exacte de l'écran (AXS15231B).
# Étape 1 du plan : squelette seul, aucune logique métier ni LVGL (ça vient en étape 3).

# SDL2 (${SIM_SDL2_TARGET}) : trouvé par le CMakeLists.txt racine, partagé avec hal_sim (son).

# demo_record.c + gif_writer.c : mode enregistrement d'un GIF de démo (tools/demo_gif.sh).
# script.c : lecteur des actions.txt des scénarios, partagé avec le sim web (simweb/).
add_executable(raidmaster-sim main.c script.c demo_record.c gif_writer.c)
# Intégration LVGL :
# On link l'UI pour piloter LVGL depuis la fenêtre SDL que ce binaire ouvre déjà.
# raidmaster_core avant raidmaster_hal_sim : le core appelle le HAL (hal_audio_play depuis jingles.c),
# CMake le répète après raidmaster_hal_sim pour les appels dans l'autre sens.
target_link_libraries(raidmaster-sim PRIVATE ${SIM_SDL2_TARGET} raidmaster_ui raidmaster_core raidmaster_hal_sim)

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
