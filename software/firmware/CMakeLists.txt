# Raidmaster 403 — firmware ESP32-S3 (carte Waveshare ESP32-S3-Touch-LCD-3.5B), projet ESP-IDF.
#
# Build, flash et moniteur : tools/fw.sh (depuis software/), qui appelle idf.py avec le dossier de
# build software/build-firmware/ (sdkconfig y compris : sdkconfig.defaults fait foi).
#
# Le code est partagé avec le simulateur : core/ et ui/ sont les mêmes dossiers, déclarés ici
# comme composants ESP-IDF (leurs CMakeLists.txt servent aux deux builds) ; hal_esp32/
# implémente les interfaces HAL du core (core/include/hal/) et les pilotes de la carte.
#
# Licence : GPL-3.0.

cmake_minimum_required(VERSION 3.16)

get_filename_component(RAIDMASTER_SOFTWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${RAIDMASTER_SOFTWARE_DIR}/core"
    "${RAIDMASTER_SOFTWARE_DIR}/ui"
    "${RAIDMASTER_SOFTWARE_DIR}/hal_esp32"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Version : software/VERSION (cmake/version.cmake), aussi pour ESP-IDF (esp_app_desc, journal).
include("${RAIDMASTER_SOFTWARE_DIR}/cmake/version.cmake")
raidmaster_version_read(PROJECT_VER)

# Même lv_conf.h que le sim (ui/lv_conf.h), inclus par lv_conf_esp32.h qui ne change que ce qui
# dépend de la cible (allocateur).
idf_build_set_property(COMPILE_DEFINITIONS "LV_CONF_PATH=${CMAKE_CURRENT_LIST_DIR}/main/lv_conf_esp32.h" APPEND)

project(raidmaster-403)

# Version affichée au démarrage, avec le hash git du firmware : en-tête du composant ui.
idf_component_get_property(_ui_lib ui COMPONENT_LIB)
raidmaster_version_header(${_ui_lib} "ESP32" ON)

# Animation de démarrage (cmake/boot_anim.cmake) : même fichier que le sim, écrit dans la
# partition boot_anim par « idf.py flash » (pas par app-flash, plus rapide pendant le dev).
include("${RAIDMASTER_SOFTWARE_DIR}/cmake/boot_anim.cmake")
set(BOOT_ANIM_BIN "${CMAKE_BINARY_DIR}/boot_anim.bin")
raidmaster_boot_anim(boot_anim_bin "${BOOT_ANIM_BIN}" _boot_anim_ok)
if(_boot_anim_ok)
    esptool_py_flash_to_partition(flash "boot_anim" "${BOOT_ANIM_BIN}")
    add_dependencies(flash boot_anim_bin)
endif()
