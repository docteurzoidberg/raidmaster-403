# Core portable (C11, aucune dépendance matérielle) : même code sur le sim PC et l'ESP32.

# minmea (décodage NMEA) est copié tel quel dans third_party/minmea (licence WTFPL / MIT /
# LGPL-3, voir third_party/minmea/README.md) et compilé dans la bibliothèque du core.
# timegm(), absent en C11 strict et sous newlib, y est remplacé par rm_timegm (gps_parser.c)
# via minmea_port/minmea_compat.h.
add_library(raidmaster_core STATIC
    gps_parser.c
    trip_engine.c
    app.c
    third_party/minmea/minmea.c
)
target_include_directories(raidmaster_core
    PUBLIC include
    PRIVATE third_party/minmea minmea_port
)
target_compile_definitions(raidmaster_core PRIVATE MINMEA_INCLUDE_COMPAT)
# Ses avertissements ne sont pas les nôtres : silencés sur ce seul fichier.
set_source_files_properties(third_party/minmea/minmea.c PROPERTIES COMPILE_OPTIONS "-w")
if(UNIX AND NOT APPLE)
    target_link_libraries(raidmaster_core PUBLIC m)
endif()
