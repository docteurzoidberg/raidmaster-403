# Raidmaster 403 — UI LVGL v9.
#
# Fetch LVGL via FetchContent (version pinned v9.2.2). On passe LV_CONF_PATH en define sur la
# cible lvgl pour qu'elle utilise notre lv_conf.h local plutôt que celui qu'elle chercherait
# dans son propre arbre. On atténue les warnings de LVGL (code tiers) sans toucher aux warnings
# stricts imposés à notre propre code (-Wall -Wextra -Wpedantic décidés dans le CMakeLists top-level).

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
add_library(raidmaster_ui STATIC
    ui.c
    theme.c
    fonts.c
    ui_widgets.c
    img_cap_arrow.c
    ui_input.c
    ui_demo.c
    screen_boot.c
    screen_raid.c
    screen_balade.c
    screen_menu.c
    screen_calib.c
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

target_include_directories(raidmaster_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(raidmaster_ui PUBLIC lvgl)

# LVGL propage sa propre -DLV_CONF_PATH=<chemin absolu> (sans guillemets) à ses consommateurs
# via INTERFACE_COMPILE_DEFINITIONS. On ne redéfinit pas ici (sinon warning « redefined »).
