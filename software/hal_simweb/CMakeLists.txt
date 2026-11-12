# HAL du simulateur web (WebAssembly, cible RAIDMASTER_TARGET=web) : stockage dans le localStorage du
# navigateur et son par Web Audio (seuls fichiers propres au web) ; GPS (rejeu, récepteur M8N émulé),
# boussole émulée, horloge, alimentation et assets repris tels quels de hal_sim/ (inchangé).
set(_hal_sim "${CMAKE_CURRENT_SOURCE_DIR}/../hal_sim")
add_library(raidmaster_hal_simweb STATIC
    hal_storage_web.c
    hal_audio_web.c
    ${_hal_sim}/hal_gps_sim.c
    ${_hal_sim}/hal_mag_sim.c
    ${_hal_sim}/hal_time_posix.c
    ${_hal_sim}/hal_power_sim.c
    ${_hal_sim}/hal_asset_file.c
)
target_include_directories(raidmaster_hal_simweb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${_hal_sim})
target_link_libraries(raidmaster_hal_simweb PUBLIC raidmaster_core)
# boot_anim.bin est préchargé dans le système de fichiers du module, sous /assets (simweb/CMakeLists.txt).
target_compile_definitions(raidmaster_hal_simweb PRIVATE RAIDMASTER_ASSET_DIR="/assets")
