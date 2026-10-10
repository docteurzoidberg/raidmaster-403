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

# ui_app.c lit l'état du core (core/app) : même code sur le sim et l'ESP32.
target_link_libraries(raidmaster_ui PUBLIC lvgl raidmaster_core)

# Animation de démarrage : fichier « RMBA » (palette 256 + images indexées) généré depuis la planche
# rendue dans Blender (variante retenue : HD, grille lisse, glow fort). Python + Pillow requis ;
# par défaut le premier trouvé parmi : .venv du dépôt, venv du projet 403 parent, python3 du système.
# Sans eux, le sim démarre sur l'écran de boot statique.
if(NOT RAIDMASTER_PYTHON)
    find_program(_rm_python NAMES python3 python
                 PATHS "${RAIDMASTER_REPO_DIR}/.venv/bin" "${RAIDMASTER_REPO_DIR}/../../.venv/bin"
                 NO_DEFAULT_PATH)
    if(NOT _rm_python)
        find_program(_rm_python NAMES python3 python)
    endif()
    set(RAIDMASTER_PYTHON "${_rm_python}" CACHE FILEPATH
        "Python avec Pillow pour générer l'animation de démarrage" FORCE)
    unset(_rm_python CACHE)
endif()
set(BOOT_ANIM_SHEET "${RAIDMASTER_REPO_DIR}/docs/mockups/boot-anim/2e_selection_hd_glow_fort.png")
set(BOOT_ANIM_BIN   "${CMAKE_BINARY_DIR}/boot_anim.bin")
execute_process(COMMAND "${RAIDMASTER_PYTHON}" -c "import PIL"
                RESULT_VARIABLE BOOT_ANIM_PY_RC OUTPUT_QUIET ERROR_QUIET)
if(BOOT_ANIM_PY_RC EQUAL 0 AND EXISTS "${BOOT_ANIM_SHEET}")
    add_custom_command(
        OUTPUT "${BOOT_ANIM_BIN}"
        COMMAND "${RAIDMASTER_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/boot_anim/build_boot_anim.py"
                "${BOOT_ANIM_SHEET}" "${BOOT_ANIM_BIN}" --frames 36 --cols 6 --fps 12
        DEPENDS "${BOOT_ANIM_SHEET}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/boot_anim/build_boot_anim.py"
        COMMENT "Animation de démarrage → boot_anim.bin"
        VERBATIM)
    add_custom_target(boot_anim_bin ALL DEPENDS "${BOOT_ANIM_BIN}")
    add_dependencies(raidmaster_ui boot_anim_bin)
    target_compile_definitions(raidmaster_ui PRIVATE RAIDMASTER_BOOT_ANIM_PATH="${BOOT_ANIM_BIN}")
    set(RAIDMASTER_BOOT_ANIM_BIN "${BOOT_ANIM_BIN}" CACHE INTERNAL "")
else()
    message(WARNING "Animation de démarrage désactivée : Python+Pillow (${RAIDMASTER_PYTHON}) "
                    "ou planche ${BOOT_ANIM_SHEET} introuvable.")
    set(RAIDMASTER_BOOT_ANIM_BIN "" CACHE INTERNAL "")
endif()

# LVGL propage sa propre -DLV_CONF_PATH=<chemin absolu> (sans guillemets) à ses consommateurs
# via INTERFACE_COMPILE_DEFINITIONS. On ne redéfinit pas ici (sinon warning « redefined »).
