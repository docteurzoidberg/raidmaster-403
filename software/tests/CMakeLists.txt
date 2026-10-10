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
