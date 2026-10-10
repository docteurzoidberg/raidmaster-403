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

# core/app : hal_storage remplacé par une version en mémoire.
add_executable(test_app test_app.c fake_storage.c)
target_link_libraries(test_app PRIVATE raidmaster_core unity)
add_test(NAME app COMMAND test_app)

# Scénarios de bout en bout : le simulateur sans fenêtre rejoue une trace et un script
# d'actions avec vérifications (tests/fixtures/scenarios/<id>/{trace.nmea,actions.txt}).
set(SCENARIOS
    "s01_ligne_droite_boutons|100"
    "s02_arret_bruit|100"
    "s03_calibration_auto|10"
)
foreach(entry ${SCENARIOS})
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 id)
    list(GET parts 1 speed)
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/scenarios/${id}")
    add_test(NAME scenario_${id}
             COMMAND $<TARGET_FILE:raidmaster-sim> --no-storage --speed ${speed}
                     --replay "${dir}/trace.nmea" --script "${dir}/actions.txt"
                     --out "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(scenario_${id} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        TIMEOUT 60)
endforeach()
