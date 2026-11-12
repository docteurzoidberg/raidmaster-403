# Raidmaster 403 — build.json du sim web : nom versionné du module, version, empreinte git (hash court
# du commit, « * » si des fichiers suivis ont des modifications non commitées), animation embarquée.
# Appelé après chaque édition de liens (simweb/CMakeLists.txt) ; le fichier n'est réécrit que s'il change.
#   cmake -DOUT=… -DNAME=… -DVERSION=… -DREPO=… -DANIM=0|1 -P build_json.cmake

set(git "")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${REPO}" rev-parse --short HEAD
                    OUTPUT_VARIABLE git OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${REPO}" status --porcelain --untracked-files=no
                    OUTPUT_VARIABLE dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(git AND dirty)
        set(git "${git}*")
    endif()
endif()
if(ANIM)
    set(anim true)
else()
    set(anim false)
endif()
set(text "{\n  \"module\": \"${NAME}\",\n  \"version\": \"${VERSION}\",\n  \"git\": \"${git}\",\n  \"boot_anim\": ${anim}\n}\n")
set(old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
    file(WRITE "${OUT}" "${text}")
endif()
