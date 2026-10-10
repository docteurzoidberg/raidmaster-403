# HAL du simulateur PC : stockage JSON, GPS (rejeu .nmea ou trajet simulé), horloge POSIX.
add_library(raidmaster_hal_sim STATIC
    hal_storage_json.c
    hal_gps_sim.c
    hal_time_posix.c
)
target_include_directories(raidmaster_hal_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(raidmaster_hal_sim PUBLIC raidmaster_core)
