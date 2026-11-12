# Simulateur web (WebAssembly, cible RAIDMASTER_TARGET=web, construit par tools/web.sh) : core/ + ui/ +
# hal_simweb/ + main.c, et le lecteur des actions.txt du sim PC (sim/script.c).
#
# Sortie dans build-web/site/ : raidmaster-sim-<VERSION>.js / .wasm / .data (numéro de version dans le
# nom : le cache de GitHub Pages ne sert pas un ancien module), build.json (nom des fichiers, version,
# empreinte git) et scenarios/ (traces et actions des scénarios du dépôt, index.json) ; tools/web.sh
# recopie le tout dans web/sim/.

include("${RAIDMASTER_REPO_DIR}/software/cmake/version.cmake")
raidmaster_version_read(RAIDMASTER_VERSION)
set(_site "${CMAKE_BINARY_DIR}/site")
set(_name "raidmaster-sim-${RAIDMASTER_VERSION}")

add_executable(raidmaster-sim-web main.c ../sim/script.c)
target_include_directories(raidmaster-sim-web PRIVATE ../sim)
target_link_libraries(raidmaster-sim-web PRIVATE raidmaster_ui raidmaster_core raidmaster_hal_simweb)
set_target_properties(raidmaster-sim-web PROPERTIES
    OUTPUT_NAME "${_name}"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${_site}")

# Animation de démarrage : boot_anim.bin généré par ui/CMakeLists.txt (planche présente), sinon
# RAIDMASTER_WEB_BOOT_ANIM (fichier déjà construit, ex. build-sim/boot_anim.bin), sinon aucune : le
# sim démarre sur la silhouette fixe.
set(RAIDMASTER_WEB_BOOT_ANIM "" CACHE FILEPATH "boot_anim.bin à embarquer si la planche manque")
set(_anim "")
if(RAIDMASTER_BOOT_ANIM_BIN)
    set(_anim "${RAIDMASTER_BOOT_ANIM_BIN}")
    add_dependencies(raidmaster-sim-web boot_anim_bin)
elseif(RAIDMASTER_WEB_BOOT_ANIM AND EXISTS "${RAIDMASTER_WEB_BOOT_ANIM}")
    set(_anim "${RAIDMASTER_WEB_BOOT_ANIM}")
    message(STATUS "Animation de démarrage du web : ${_anim}")
else()
    message(WARNING "Sim web sans animation de démarrage (ni planche, ni RAIDMASTER_WEB_BOOT_ANIM).")
endif()

set(_exports
    _sim_start _sim_frame _sim_run _sim_render _sim_set_rate _sim_button _sim_key _sim_fb _sim_info
    _sim_save _malloc _free)
string(REPLACE ";" "," _exports "${_exports}")
target_link_options(raidmaster-sim-web PRIVATE
    "SHELL:-sMODULARIZE=1"
    "SHELL:-sEXPORT_NAME=createRaidmasterSim"
    "SHELL:-sENVIRONMENT=web,node"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sSTACK_SIZE=1MB"
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:-sEXPORTED_FUNCTIONS=${_exports}"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=FS,ccall,HEAPU8,HEAP16,HEAPF64,UTF8ToString,stringToUTF8,lengthBytesUTF8"
    "--no-entry")
if(_anim)
    target_link_options(raidmaster-sim-web PRIVATE "SHELL:--preload-file ${_anim}@/assets/boot_anim.bin")
    set_property(TARGET raidmaster-sim-web APPEND PROPERTY LINK_DEPENDS "${_anim}")
endif()

# build.json : ce que la page charge (nom versionné du module, version, empreinte git), réécrit à
# chaque build.
add_custom_command(TARGET raidmaster-sim-web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -DOUT=${_site}/build.json -DNAME=${_name} -DVERSION=${RAIDMASTER_VERSION}
            -DREPO=${RAIDMASTER_REPO_DIR} -DANIM=$<BOOL:${_anim}> -P "${CMAKE_CURRENT_SOURCE_DIR}/build_json.cmake"
    VERBATIM)

# Scénarios du dépôt : traces générées (gen_route.py, déterministe) ou versionnées, actions, index.
file(GLOB _routes "${RAIDMASTER_REPO_DIR}/software/tests/fixtures/scenarios/*/route.txt"
                  "${RAIDMASTER_REPO_DIR}/software/tests/fixtures/scenarios/*/actions.txt"
                  "${RAIDMASTER_REPO_DIR}/software/tests/fixtures/scenarios/*/trace.nmea")
add_custom_command(
    OUTPUT "${_site}/scenarios/index.json"
    COMMAND "${RAIDMASTER_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/scenarios.py"
            --repo "${RAIDMASTER_REPO_DIR}" --out "${_site}/scenarios"
    DEPENDS ${_routes} "${CMAKE_CURRENT_SOURCE_DIR}/scenarios.py"
            "${RAIDMASTER_REPO_DIR}/software/tests/CMakeLists.txt"
            "${RAIDMASTER_REPO_DIR}/software/tests/fixtures/scenarios/README.md"
            "${RAIDMASTER_REPO_DIR}/software/tests/tools/gen_route.py"
    COMMENT "Scénarios du sim web → ${_site}/scenarios"
    VERBATIM)
add_custom_target(scenarios_web ALL DEPENDS "${_site}/scenarios/index.json")
