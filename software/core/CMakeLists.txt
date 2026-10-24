# Core portable (C11, aucune dépendance matérielle) : même code sur le sim PC et l'ESP32.
#
# Ce fichier sert aux deux builds : bibliothèque raidmaster_core pour le sim (CMake simple) et
# composant ESP-IDF « core » pour le firmware (firmware/CMakeLists.txt l'ajoute par
# EXTRA_COMPONENT_DIRS). La liste des sources est commune.

# minmea (décodage NMEA) est copié tel quel dans third_party/minmea (licence WTFPL / MIT /
# LGPL-3, voir third_party/minmea/README.md) et compilé dans la bibliothèque du core.
# timegm(), absent en C11 strict et sous newlib, y est remplacé par rm_timegm (gps_parser.c)
# via minmea_port/minmea_compat.h.
set(RAIDMASTER_CORE_SRCS
    gps_parser.c
    gps_synth.c
    trip_engine.c
    app.c
    synth.c
    jingles.c
    third_party/minmea/minmea.c
)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${RAIDMASTER_CORE_SRCS}
                           INCLUDE_DIRS include
                           PRIV_INCLUDE_DIRS third_party/minmea minmea_port)
    set(_core_lib ${COMPONENT_LIB})
else()
    add_library(raidmaster_core STATIC ${RAIDMASTER_CORE_SRCS})
    target_include_directories(raidmaster_core
        PUBLIC include
        PRIVATE third_party/minmea minmea_port
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(raidmaster_core PUBLIC m)
    endif()
    set(_core_lib raidmaster_core)
endif()

target_compile_definitions(${_core_lib} PRIVATE MINMEA_INCLUDE_COMPAT)
# Ses avertissements ne sont pas les nôtres : silencés sur ce seul fichier.
set_source_files_properties(third_party/minmea/minmea.c PROPERTIES COMPILE_OPTIONS "-w")
