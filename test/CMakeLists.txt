include_directories(.)

if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

function(except_file var excluded_file)
    set(list_var "")
    foreach(f ${ARGN})
        if(NOT ${f} MATCHES ".*${excluded_file}")
            list(APPEND list_var ${f})
        endif()
    endforeach(f)
    set(${var} "${list_var}" PARENT_SCOPE)
endfunction(except_file)

# Replace some source files with stubs
except_file(TEST_CORE_FILES "core/image.c" ${CORE_FILES})
except_file(TEST_CORE_FILES "core/lang.c" ${TEST_CORE_FILES})
except_file(TEST_CORE_FILES "core/speed.c" ${TEST_CORE_FILES})
except_file(TEST_BUILDING_FILES "building/model.c" ${BUILDING_FILES})

add_executable(translationcheck
    translation/check.c
    stub/log.c
    ${PROJECT_SOURCE_DIR}/src/core/encoding.c
    ${PROJECT_SOURCE_DIR}/src/core/encoding_japanese.c
    ${PROJECT_SOURCE_DIR}/src/core/encoding_korean.c
    ${PROJECT_SOURCE_DIR}/src/core/encoding_simp_chinese.c
    ${PROJECT_SOURCE_DIR}/src/core/encoding_trad_chinese.c
    ${PROJECT_SOURCE_DIR}/src/core/string.c
    ${TRANSLATION_FILES}
)

add_executable(compare
    sav/compare.c
    sav/sav_compare.c
    stub/log.c
    ${PROJECT_SOURCE_DIR}/src/core/zip.c
)

add_executable(autopilot
    sav/sav_compare.c
    sav/run.c
    stub/image.c
    stub/input.c
    stub/lang.c
    stub/log.c
    stub/model.c
    stub/sound_device.c
    stub/ui.c
    stub/video.c
    ${PROJECT_SOURCE_DIR}/src/platform/file_manager.c
    ${TEST_CORE_FILES}
    ${TEST_BUILDING_FILES}
    ${CITY_FILES}
    ${EMPIRE_FILES}
    ${FIGURE_FILES}
    ${FIGURETYPE_FILES}
    ${GAME_FILES}
    ${MAP_FILES}
    ${SCENARIO_FILES}
    ${SOUND_FILES}
    ${EDITOR_FILES}
    ${MP_FILES}
)

# Headless simulation tool: checksums, traces, idempotence (doc/mp/TESTING.md)
add_executable(simtool
    sim/simtool.c
    sim/log_quiet.c
    stub/image.c
    stub/input.c
    stub/lang.c
    stub/model.c
    stub/sound_device.c
    stub/ui.c
    stub/video.c
    ${PROJECT_SOURCE_DIR}/src/platform/file_manager.c
    ${TEST_CORE_FILES}
    ${TEST_BUILDING_FILES}
    ${CITY_FILES}
    ${EMPIRE_FILES}
    ${FIGURE_FILES}
    ${FIGURETYPE_FILES}
    ${GAME_FILES}
    ${MAP_FILES}
    ${SCENARIO_FILES}
    ${SOUND_FILES}
    ${EDITOR_FILES}
    ${MP_FILES}
)

file(COPY data/c3.emp DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY data/c32.emp DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

function(add_integration_test name input_sav compare_sav ticks)
    string(REPLACE ".sav" "-actual.sav" output_sav ${compare_sav})
    file(COPY data/${input_sav} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY data/${compare_sav} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    add_test(NAME ${name} COMMAND autopilot ${input_sav} ${output_sav} ${compare_sav} ${ticks})
endfunction(add_integration_test)

add_integration_test(sav_tower tower.sav tower2.sav 1785)
add_integration_test(sav_request1 request_start.sav request_orig.sav 908)
add_integration_test(sav_request2 request_start.sav request_orig2.sav 6556)

# Caesar invasion plus ballista
add_integration_test(sav_caesar1 kknight.sav kknight2.sav 686)
add_integration_test(sav_caesar2 kknight.sav kknight3.sav 1087)
add_integration_test(sav_caesar3 kknight.sav kknight4.sav 1287)
add_integration_test(sav_caesar4 kknight.sav kknight5.sav 1494)

# Invasion
add_integration_test(sav_invasion1 inv0.sav inv1.sav 1973)
add_integration_test(sav_invasion2 inv0.sav inv2.sav 3521)
add_integration_test(sav_invasion3 inv0.sav inv3.sav 5105)
add_integration_test(sav_invasion4 inv0.sav inv4.sav 6777)
add_integration_test(sav_invasion5 inv0.sav inv5.sav 8563)

# Distant battle
add_integration_test(sav_distantbattle1 db-fort1.sav db-fort1-done.sav 6328)
add_integration_test(sav_distantbattle2 db-fort2.sav db-fort2-done.sav 6335)
add_integration_test(sav_distantbattle3 db-fort2.sav db-fort2-done2.sav 11197)

# Routing table full kills figures
add_integration_test(sav_routing_full routing-full.sav routing-full-kill.sav 7)

# God curses
add_integration_test(sav_curses1 curses.sav curses-done.sav 13350)
add_integration_test(sav_curses2 mars-wrath.sav mars-wrath-after.sav 1016)

# Earthquake destroying buildings
add_integration_test(sav_earthquake0 earthquake.sav earthquake-start.sav 371)
add_integration_test(sav_earthquake1 earthquake.sav earthquake-during1.sav 551)
add_integration_test(sav_earthquake2 earthquake.sav earthquake-during2.sav 1071)
add_integration_test(sav_earthquake3 earthquake.sav earthquake-during3.sav 1602)
add_integration_test(sav_earthquake4 earthquake.sav earthquake-during4.sav 2155)
add_integration_test(sav_earthquake5 earthquake.sav earthquake-after.sav 3748)

# Testing map with tile offsets >127
add_integration_test(sav_edge1 edge-start.sav edge-battle-before.sav 835)
add_integration_test(sav_edge2 edge-start.sav edge-battle-start.sav 1278)
add_integration_test(sav_edge3 edge-start.sav edge-battle-during.sav 1513)
add_integration_test(sav_edge4 edge-start.sav edge-battle-after.sav 1890)

# Test with bigger cities
add_integration_test(sav_massilia1 brugle-massilia-start.sav brugle-massilia-1.sav 4)
add_integration_test(sav_massilia2 brugle-massilia-start.sav brugle-massilia-2.sav 57)
add_integration_test(sav_massilia3 brugle-massilia-start.sav brugle-massilia-3.sav 391)

add_integration_test(sav_valentia1 valentia57.sav valentia57-after.sav 1026)
add_integration_test(sav_lugdunum1 brugle-lugdunum.sav brugle-lugdunum-after.sav 1176)

add_integration_test(sav_native1 brugle-lugdunum-native.sav brugle-lugdunum-native-after.sav 1678)
add_integration_test(sav_native2 cicero-lugdunum-trade.sav cicero-lugdunum-trade-after.sav 926)

add_integration_test(sav_palace1 brugle-palacepeaks.sav brugle-palacepeaks-2.sav 2562)

# Simulation tool tests (doc/mp/TESTING.md §4)
add_test(NAME sim_trace_deterministic
    COMMAND ${CMAKE_COMMAND} -DSIMTOOL=$<TARGET_FILE:simtool> -DSAVE=brugle-massilia-start.sav -DTICKS=600 -DSTEP=20
        -P ${CMAKE_CURRENT_SOURCE_DIR}/sim/compare_runs.cmake)

# The same saved game loaded twice in one process must behave the same: no hidden state may leak
# from a previous game (M1.3, game/extra_state.c)
foreach(save tower request_start kknight inv0 db-fort1 db-fort2 routing-full curses mars-wrath earthquake
        edge-start brugle-massilia-start valentia57 brugle-lugdunum brugle-lugdunum-native
        cicero-lugdunum-trade brugle-palacepeaks)
    add_test(NAME sim_idempotence_${save} COMMAND simtool idempotence ${save}.sav 1500 25)
endforeach()

# Game rules (M1.4): local settings change a classic game, but not a multiplayer one
function(add_trace_comparison name args_a args_b expect)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} -DSIMTOOL=$<TARGET_FILE:simtool> "-DARGS_A=${args_a}" "-DARGS_B=${args_b}"
            -DEXPECT=${expect} -P ${CMAKE_CURRENT_SOURCE_DIR}/sim/compare_traces.cmake)
endfunction()
add_trace_comparison(sim_rules_classic_reads_local_settings
    "trace|brugle-massilia-start.sav|1000|50"
    "--difficulty|0|--gods|0|trace|brugle-massilia-start.sav|1000|50" different)
add_trace_comparison(sim_rules_multiplayer_ignores_local_settings
    "--mp|trace|brugle-massilia-start.sav|1000|50"
    "--difficulty|0|--gods|0|--mp|trace|brugle-massilia-start.sav|1000|50" same)

# Unit tests of the multiplayer modules
add_executable(mp_test_command mp/test_command.c ${PROJECT_SOURCE_DIR}/src/core/buffer.c ${PROJECT_SOURCE_DIR}/src/mp/command.c)
add_test(NAME mp_command COMMAND mp_test_command)

# Commands (M2.2): placing buildings through commands equals the former direct user interface calls
foreach(save brugle-lugdunum valentia57)
    add_test(NAME mp_build_equivalence_${save} COMMAND simtool buildequiv ${save}.sav)
endforeach()
foreach(save edge-start valentia57)
    add_test(NAME mp_clear_equivalence_${save} COMMAND simtool clearequiv ${save}.sav)
endforeach()
foreach(save brugle-lugdunum cicero-lugdunum-trade)
    add_test(NAME mp_action_equivalence_${save} COMMAND simtool actionequiv ${save}.sav)
endforeach()

# Network games (M2P.5): headless host and clients on this computer
if(UNIX)
    add_test(NAME mp_lan_2_players COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27431 2 brugle-lugdunum.sav 800)
    add_test(NAME mp_lan_4_players COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27432 4 valentia57.sav 1200)
    add_test(NAME mp_lan_desync_detected COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27433 3 brugle-lugdunum.sav 400 desync)
    # separate cities: every player has its own copy of the city, sent by the host as a .mpsav
    add_test(NAME mp_lan_cities_2_players COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27434 2 brugle-lugdunum.sav 800 cities)
    add_test(NAME mp_lan_cities_4_players COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27435 4 valentia57.sav 1200 cities)
    add_test(NAME mp_lan_cities_desync_detected COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27436 3 tower.sav 400 desync cities)
    add_test(NAME mp_lan_pause COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27437 3 brugle-lugdunum.sav 800 pause cities)
    add_test(NAME mp_lan_player_leaves COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27438 3 brugle-lugdunum.sav 800 leave cities)
    # a network game goes on from a multiplayer saved game (its number of cities sets the number of players)
    add_test(NAME mp_lan_resume_mpsav COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_resume_test.sh $<TARGET_FILE:simtool> 27439)
    add_test(NAME mp_lan_other_game_data_refused COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27440 2 tower.sav 400 baddata)
    set_tests_properties(mp_lan_pause mp_lan_player_leaves mp_lan_resume_mpsav mp_lan_other_game_data_refused
        PROPERTIES TIMEOUT 120)
    set_tests_properties(mp_lan_2_players mp_lan_4_players mp_lan_desync_detected mp_lan_cities_2_players
        mp_lan_cities_4_players mp_lan_cities_desync_detected PROPERTIES TIMEOUT 120)
endif()

# Large grids (M3.3): a city moved to a larger grid behaves exactly as on its original grid
foreach(save tower inv0 earthquake edge-start valentia57 brugle-lugdunum)
    add_test(NAME mp_relocation_${save} COMMAND simtool relocequiv ${save}.sav 1500 400 120 90)
endforeach()
add_test(NAME mp_relocation_max_grid COMMAND simtool relocequiv kknight.sav 1500 512 300 13)

# Several cities (M3.7): a city with a twin city on a large map runs exactly as alone, and the twin is an
# exact copy of it, figure by figure (simtool twins / twinfigures)
foreach(save tower request_start kknight inv0 routing-full curses earthquake edge-start brugle-massilia-start
        valentia57 brugle-lugdunum cicero-lugdunum-trade brugle-palacepeaks)
    add_test(NAME mp_twins_${save} COMMAND simtool twins ${save}.sav 1500)
    add_test(NAME mp_twin_copy_${save} COMMAND simtool twinfigures ${save}.sav 1500)
endforeach()
# long runs where cities used to meet: lost buildings at (0, 0), sentries, combat targets (M4.2, D-027)
foreach(save inv0 routing-full)
    add_test(NAME mp_twin_copy_long_${save} COMMAND simtool twinfigures ${save}.sav 12000)
endforeach()

# Multiplayer saved games (M3.8): saving every city, loading and running on equals running on, with twin
# cities and with one city on a large grid
foreach(save tower kknight inv0 curses earthquake brugle-lugdunum cicero-lugdunum-trade valentia57)
    add_test(NAME mp_resume_twins_${save} COMMAND simtool mpresume ${save}.sav 1500 400)
    add_test(NAME mp_resume_alone_${save} COMMAND simtool mpresume ${save}.sav 700 300)
    set_tests_properties(mp_resume_alone_${save} PROPERTIES ENVIRONMENT MPRESUME_ALONE=1)
endforeach()
foreach(save tower brugle-lugdunum)
    add_test(NAME mp_resume_4_cities_${save} COMMAND simtool mpresume ${save}.sav 800 300)
    set_tests_properties(mp_resume_4_cities_${save} PROPERTIES ENVIRONMENT MPRESUME_CITIES=4)
endforeach()

# No Caesar in multiplayer (M4.1): requests, anger and invasions, salary happen in the classic game only
foreach(save request_start tower)
    add_test(NAME mp_no_caesar_${save} COMMAND simtool caesarfree ${save}.sav 12000)
endforeach()

# Connected cities (M4.3, D-018): the walkers of a city act on none of the buildings of another one, even when
# they walk through it (intruders: the twin's walkers moved into the first city; neighbours: two cities
# side by side joined by roads)
foreach(save brugle-lugdunum valentia57 request_start kknight earthquake)
    add_test(NAME mp_intruders_${save} COMMAND simtool intruders ${save}.sav 1500)
endforeach()
add_test(NAME mp_neighbours_brugle-lugdunum COMMAND simtool neighbours brugle-lugdunum.sav 3000)
# network game maps (M4.4): land between the cities, where a road built by a player joins them
foreach(save brugle-lugdunum tower)
    add_test(NAME mp_open_land_${save} COMMAND simtool openland ${save}.sav)
endforeach()
# AI invasions option (M4.5): enemy armies come only when the rules of the multiplayer game allow them
foreach(save inv0 kknight)
    add_test(NAME mp_ai_invasions_${save} COMMAND simtool aiinvasions ${save}.sav 20000)
endforeach()
# End of the game by score (M4.6): the game ends at the start of the year chosen, with a score for every city
foreach(save brugle-lugdunum tower)
    add_test(NAME mp_end_by_score_${save} COMMAND simtool endscore ${save}.sav 1)
endforeach()
# Permissions to exploit (M4.7, D-020): every raw material for a single city, weapons only with iron
foreach(save brugle-lugdunum valentia57 kknight)
    add_test(NAME mp_permissions_${save} COMMAND simtool permissions ${save}.sav 3)
endforeach()

# Generated maps (M6.2): each player settles at its arrival point and gets immigrants; network game on one
foreach(players 2 4)
    add_test(NAME mp_generated_map_${players}_players COMMAND simtool mapgen brugle-massilia-start.sav ${players} 11 4000)
endforeach()
if(NOT WIN32)
    add_test(NAME mp_lan_generated_map COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27441 3 brugle-massilia-start.sav 600 generate)
    set_tests_properties(mp_lan_generated_map PROPERTIES TIMEOUT 120)
endif()

