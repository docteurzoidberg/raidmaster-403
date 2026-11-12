# Tests unitaires du core (Unity), lancés sur PC : ctest --preset sim

include(FetchContent)
FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)
# Assertions sur double (coordonnées GPS).
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
target_compile_options(unity PRIVATE -w)

add_executable(test_gps_parser test_gps_parser.c)
target_link_libraries(test_gps_parser PRIVATE raidmaster_core unity)
# Traces longues (non versionnées) : rejouées si présentes, sinon tests ignorés.
target_compile_definitions(test_gps_parser PRIVATE
    RAIDMASTER_TRACES_DIR="${CMAKE_SOURCE_DIR}/references/traces-brutes")
add_test(NAME gps_parser COMMAND test_gps_parser)

add_executable(test_trip_engine test_trip_engine.c)
target_link_libraries(test_trip_engine PRIVATE raidmaster_core unity)
target_compile_definitions(test_trip_engine PRIVATE
    RAIDMASTER_TRACES_DIR="${CMAKE_SOURCE_DIR}/references/traces-brutes")
add_test(NAME trip_engine COMMAND test_trip_engine)

# core/app : hal_storage en mémoire, horloge de la carte figée.
add_executable(test_app test_app.c fake_storage.c fake_clock.c)
target_link_libraries(test_app PRIVATE raidmaster_core unity)
add_test(NAME app COMMAND test_app)

# core/synth et core/jingles : hal_audio factice dans le test.
add_executable(test_synth test_synth.c)
target_link_libraries(test_synth PRIVATE raidmaster_core unity)
add_test(NAME synth COMMAND test_synth)

# Liaison avec le récepteur u-blox (puck M8 ou M9 soudé) : trames UBX et automate
add_executable(test_ubx test_ubx.c)
target_link_libraries(test_ubx PRIVATE raidmaster_core unity)
add_test(NAME ubx COMMAND test_ubx)

add_executable(test_gps_link test_gps_link.c)
target_link_libraries(test_gps_link PRIVATE raidmaster_core unity)
add_test(NAME gps_link COMMAND test_gps_link)

# Scénarios de bout en bout : le simulateur sans fenêtre rejoue une trace et un script
# d'actions avec vérifications (tests/fixtures/scenarios/<id>/actions.txt). La trace est
# trace.nmea (versionnée, ex. enregistrement réel) ou générée au build depuis route.txt par
# tests/tools/gen_route.py. Liste « id|vitesse de rejeu », rangée par phase.
set(SCENARIOS
    # Phase 1 — fondations
    "s10_stationary_noise|100"
    "s02_highway_cruise|200"
    # Phase 2 — roulage nominal
    "s03_country_road|200"
    "s01_city_commute|100"
    # Phase 3 — actions utilisateur
    "s20_raz_burst|10"
    "s22_increment_sequence|10"
    "s21_calib_mid_stage|100"
    "s23_menu_during_drive|20"
    "s24_ligne_droite_boutons|100"
    "s25_calibration_auto|10"
    # Phase 4 — cas limites GPS
    "s13_cold_start|10"
    "s15_backtrack|10"
    "s11_tunnel_loss|10"
    "s14_fix_degraded_3d_to_2d|50"
    # Phase 5 — longue durée
    "s30_full_raid_day|1000"
    "s31_power_loss_mid|1"
    # Phases 6 et 7 — parties GPS (boussole : à venir)
    "s04_mountain_pass|100"
    "s05_offroad_slow|100"
    "s12_canyon_hdop|50"
    "s16_antimeridian|10"
    "s17_midnight_utc|10"
)
set(_traces)
foreach(entry ${SCENARIOS})
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 id)
    list(GET parts 1 speed)
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/scenarios/${id}")
    if(EXISTS "${dir}/route.txt")
        set(trace "${CMAKE_CURRENT_BINARY_DIR}/scenarios/${id}.nmea")
        add_custom_command(OUTPUT "${trace}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/scenarios"
            COMMAND "${RAIDMASTER_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_route.py"
                    "${dir}/route.txt" --out "${trace}" --quiet
            DEPENDS "${dir}/route.txt" "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_route.py"
            COMMENT "Trace du scénario ${id}"
            VERBATIM)
        list(APPEND _traces "${trace}")
    else()
        set(trace "${dir}/trace.nmea")
    endif()
    add_test(NAME scenario_${id}
             COMMAND $<TARGET_FILE:raidmaster-sim> --no-storage --speed ${speed}
                     --replay "${trace}" --script "${dir}/actions.txt"
                     --out "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(scenario_${id} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        TIMEOUT 120)
endforeach()
add_custom_target(scenario_traces ALL DEPENDS ${_traces})
