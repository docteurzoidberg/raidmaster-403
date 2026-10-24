# HAL du simulateur PC : stockage JSON, GPS (rejeu .nmea ou trajet simulé), horloge POSIX,
# alimentation choisie au clavier, assets générés au build (animation de démarrage).
add_library(raidmaster_hal_sim STATIC
    hal_storage_json.c
    hal_gps_sim.c
    hal_time_posix.c
    hal_power_sim.c
    hal_asset_file.c
)
target_include_directories(raidmaster_hal_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(raidmaster_hal_sim PUBLIC raidmaster_core)
# Les assets (boot_anim.bin…) sont générés à la racine du dossier de build (ui/CMakeLists.txt).
target_compile_definitions(raidmaster_hal_sim PRIVATE RAIDMASTER_ASSET_DIR="${CMAKE_BINARY_DIR}")
