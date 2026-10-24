# Raidmaster 403 — écrit raidmaster_version.h (mode script, appelé par cmake/version.cmake).
#   cmake -DOUT=<en-tête> -DVERSION_FILE=<VERSION> -DPLATFORM=<SIM|ESP32> -DWITH_GIT=<ON|OFF>
#         -DREPO=<dossier du dépôt> -P version_header.cmake
# Le fichier n'est réécrit que si son contenu change (pas de recompilation inutile).

file(STRINGS "${VERSION_FILE}" ver LIMIT_COUNT 1)
string(STRIP "${ver}" ver)
set(str "v${ver} · ${PLATFORM}")
if(WITH_GIT)
    execute_process(COMMAND git -C "${REPO}" rev-parse --short HEAD
                    OUTPUT_VARIABLE hash RESULT_VARIABLE rc
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(rc EQUAL 0 AND hash)
        execute_process(COMMAND git -C "${REPO}" status --porcelain --untracked-files=no
                        OUTPUT_VARIABLE dirty ERROR_QUIET)
        if(dirty)
            string(APPEND hash "*")
        endif()
        string(APPEND str " · ${hash}")
    endif()
endif()

set(content "/* Généré par cmake/version_header.cmake : ne pas modifier. */\n#pragma once\n\n#define RAIDMASTER_VERSION_STR \"${str}\"\n")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
    if(old STREQUAL content)
        return()
    endif()
endif()
file(WRITE "${OUT}" "${content}")
