# Raidmaster 403 — numéro de version (software/VERSION, seule source) et en-tête généré
# raidmaster_version.h : RAIDMASTER_VERSION_STR, affiché par l'écran de démarrage
# (« v0.1.0 · SIM », « v0.1.0 · ESP32 · fe63e9e »).
#
#   raidmaster_version_read(<var>)
#       numéro lu dans VERSION (PROJECT_VER du firmware : version d'ESP-IDF, journal de boot).
#   raidmaster_version_header(<cible> <plateforme> <avec_git>)
#       génère l'en-tête et l'ajoute aux includes de <cible>. Avec git : hash court du commit,
#       suivi de « * » si des fichiers suivis ont des modifications non commitées ; recalculé à
#       chaque build (cible <cible>_version), le fichier n'est réécrit que s'il change. Sans git
#       (sim) : écrit à la configuration. Le sim n'affiche pas de hash : il serait sur l'écran de
#       démarrage du GIF de démo, qui changerait à chaque commit.

get_filename_component(_rm_version_file "${CMAKE_CURRENT_LIST_DIR}/../VERSION" ABSOLUTE)
set(_rm_version_script "${CMAKE_CURRENT_LIST_DIR}/version_header.cmake")

function(raidmaster_version_read var)
    file(STRINGS "${_rm_version_file}" v LIMIT_COUNT 1)
    string(STRIP "${v}" v)
    set(${var} "${v}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rm_version_file}")
endfunction()

function(raidmaster_version_header target platform with_git)
    set(dir "${CMAKE_CURRENT_BINARY_DIR}/raidmaster_version")
    set(out "${dir}/raidmaster_version.h")
    set(args -DOUT=${out} -DVERSION_FILE=${_rm_version_file} -DPLATFORM=${platform}
             -DWITH_GIT=${with_git} -DREPO=${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..)
    if(with_git)
        add_custom_target(${target}_version
            COMMAND "${CMAKE_COMMAND}" ${args} -P "${_rm_version_script}"
            BYPRODUCTS "${out}"
            COMMENT "Version (${platform})"
            VERBATIM)
        add_dependencies(${target} ${target}_version)
    else()
        execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -P "${_rm_version_script}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rm_version_file}")
    endif()
    target_include_directories(${target} PRIVATE "${dir}")
endfunction()
