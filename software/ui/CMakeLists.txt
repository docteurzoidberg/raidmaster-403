# Raidmaster 403 — UI LVGL v9.
#
# Ce fichier sert aux deux builds : bibliothèque raidmaster_ui pour le sim, composant ESP-IDF
# « ui » pour le firmware (firmware/CMakeLists.txt l'ajoute par EXTRA_COMPONENT_DIRS). La liste
# des sources est commune.
#
# Sim : LVGL via FetchContent (version pinned v9.2.2). On passe LV_CONF_PATH en define sur la
# cible lvgl pour qu'elle utilise notre lv_conf.h local plutôt que celui qu'elle chercherait
# dans son propre arbre. On atténue les warnings de LVGL (code tiers) sans toucher aux warnings
# stricts imposés à notre propre code (-Wall -Wextra -Wpedantic décidés dans le CMakeLists top-level).
# Firmware : LVGL du registre de composants Espressif, même version (idf_component.yml), même
# lv_conf.h (firmware/main/lv_conf_esp32.h l'inclut et ne change que l'allocateur).

set(RAIDMASTER_UI_SRCS
    ui.c
    ui_app.c
    theme.c
    fonts.c
    ui_widgets.c
    boot_anim.c
    img_cap_arrow.c
    ui_input.c
    ui_demo.c
    screen_boot.c
    screen_raid.c
    screen_balade.c
    screen_menu.c
    screen_calib.c
    screen_magcal.c
    # Polices bitmap Share Tech Mono (générées par fonts/gen_fonts.sh).
    fonts/stm_12.c
    fonts/stm_13.c
    fonts/stm_14.c
    fonts/stm_15.c
    fonts/stm_16.c
    fonts/stm_17.c
    fonts/stm_25.c
    fonts/stm_29.c
    fonts/stm_34.c
    fonts/stm_38.c
    fonts/stm_44.c
    fonts/stm_72.c
    fonts/stm_134.c
    fonts/stm_190.c
)

if(ESP_PLATFORM)
    # ui_app.c lit l'état du core (core/app) : même code sur le sim et l'ESP32.
    idf_component_register(SRCS ${RAIDMASTER_UI_SRCS}
                           INCLUDE_DIRS "."
                           REQUIRES core)
    return()
endif()

include(FetchContent)

set(LVGL_GIT_TAG "v9.2.2" CACHE STRING "Tag LVGL à fetcher")

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        ${LVGL_GIT_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# LVGL expose certaines options CMake qu'il faut fixer avant MakeAvailable.
# On veut utiliser notre lv_conf.h via LV_CONF_PATH (chemin absolu).
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "Chemin de lv_conf.h local" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS    ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(lvgl)

# LVGL = code tiers : on désactive les warnings stricts sur sa cible uniquement.
if(TARGET lvgl)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(lvgl PRIVATE -w)
    endif()
endif()

# Bibliothèque statique de notre UI applicative. Elle expose les headers pour que sim/ y accède.
add_library(raidmaster_ui STATIC ${RAIDMASTER_UI_SRCS})

target_include_directories(raidmaster_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ui_app.c lit l'état du core (core/app) : même code sur le sim et l'ESP32.
target_link_libraries(raidmaster_ui PUBLIC lvgl raidmaster_core)

# Version affichée au démarrage (cmake/version.cmake) ; le firmware ajoute le hash git
# (firmware/CMakeLists.txt).
include("${RAIDMASTER_REPO_DIR}/software/cmake/version.cmake")
raidmaster_version_header(raidmaster_ui "SIM" OFF)

# Animation de démarrage (cmake/boot_anim.cmake) : boot_anim.bin dans le dossier de build, lu par
# hal_sim (hal_asset « boot_anim »). Sans Python + Pillow, le sim démarre sur l'écran de boot
# statique.
include("${RAIDMASTER_REPO_DIR}/software/cmake/boot_anim.cmake")
set(BOOT_ANIM_BIN "${CMAKE_BINARY_DIR}/boot_anim.bin")
raidmaster_boot_anim(boot_anim_bin "${BOOT_ANIM_BIN}" _boot_anim_ok)
if(_boot_anim_ok)
    add_dependencies(raidmaster_ui boot_anim_bin)
    set(RAIDMASTER_BOOT_ANIM_BIN "${BOOT_ANIM_BIN}" CACHE INTERNAL "")
else()
    set(RAIDMASTER_BOOT_ANIM_BIN "" CACHE INTERNAL "")
endif()

# LVGL propage sa propre -DLV_CONF_PATH=<chemin absolu> (sans guillemets) à ses consommateurs
# via INTERFACE_COMPILE_DEFINITIONS. On ne redéfinit pas ici (sinon warning « redefined »).
