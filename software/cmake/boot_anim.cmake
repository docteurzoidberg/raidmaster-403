# Raidmaster 403 — animation de démarrage : fichier « RMBA » (palette 256 + images indexées)
# généré depuis la planche rendue dans Blender (variante retenue : HD, grille lisse, glow fort)
# par ui/assets/boot_anim/build_boot_anim.py. Commun au sim (ui/CMakeLists.txt : fichier lu par
# hal_sim) et au firmware (firmware/CMakeLists.txt : flashé dans la partition boot_anim).
#
# Format (RAIDMASTER_BOOT_ANIM_FORMAT, à trancher) : 2 (défaut) images codées par différence,
# 1,1 Mo ; 1 images brutes, 5,5 Mo. Mêmes pixels, ui/boot_anim.c lit les deux.
#
#   raidmaster_boot_anim(<cible> <fichier.bin> <var_ok>)
#
# Crée la cible <cible> qui produit <fichier.bin> et met <var_ok> à TRUE, ou se contente d'un
# avertissement (<var_ok> à FALSE) si Python + Pillow ou la planche manquent : le sim et la
# carte démarrent alors sur l'écran de boot statique.
#
# Python : RAIDMASTER_PYTHON s'il est donné, sinon le premier trouvé parmi le .venv du dépôt, le
# venv du projet 403 parent et le python3 du système.

get_filename_component(_rm_software_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(_rm_repo_dir "${_rm_software_dir}/.." ABSOLUTE)

function(raidmaster_boot_anim target output ok_var)
    if(NOT RAIDMASTER_PYTHON)
        find_program(_rm_python NAMES python3 python
                     PATHS "${_rm_repo_dir}/.venv/bin" "${_rm_repo_dir}/../../.venv/bin"
                     NO_DEFAULT_PATH)
        if(NOT _rm_python)
            find_program(_rm_python NAMES python3 python)
        endif()
        set(RAIDMASTER_PYTHON "${_rm_python}" CACHE FILEPATH
            "Python avec Pillow pour générer l'animation de démarrage" FORCE)
        unset(_rm_python CACHE)
    endif()
    set(RAIDMASTER_BOOT_ANIM_FORMAT 2 CACHE STRING "Animation de démarrage : 2 (par différence, 1,1 Mo) ou 1 (brute, 5,5 Mo)")
    set_property(CACHE RAIDMASTER_BOOT_ANIM_FORMAT PROPERTY STRINGS 1 2)
    set(sheet "${_rm_repo_dir}/docs/mockups/boot-anim/6_synchro_glissade.png")
    set(script "${_rm_software_dir}/ui/assets/boot_anim/build_boot_anim.py")
    execute_process(COMMAND "${RAIDMASTER_PYTHON}" -c "import PIL, numpy"
                    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
    if(NOT rc EQUAL 0 OR NOT EXISTS "${sheet}")
        message(WARNING "Animation de démarrage désactivée : Python + Pillow + numpy "
                        "(${RAIDMASTER_PYTHON}) ou planche ${sheet} introuvable.")
        set(${ok_var} FALSE PARENT_SCOPE)
        return()
    endif()
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${RAIDMASTER_PYTHON}" "${script}" "${sheet}" "${output}" --frames 36 --cols 6 --fps 12
                --format ${RAIDMASTER_BOOT_ANIM_FORMAT}
        DEPENDS "${sheet}" "${script}"
        COMMENT "Animation de démarrage (v${RAIDMASTER_BOOT_ANIM_FORMAT}) → ${output}"
        VERBATIM)
    add_custom_target(${target} ALL DEPENDS "${output}")
    set(${ok_var} TRUE PARENT_SCOPE)
endfunction()
