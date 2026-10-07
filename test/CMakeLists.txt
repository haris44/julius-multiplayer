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
    ${PROJECT_SOURCE_DIR}/src/translation/french.c
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
    ${PROJECT_SOURCE_DIR}/src/translation/french.c
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

# the network code of the multiplayer game (src/platform/net.c) needs Winsock on Windows
if(WIN32)
    target_link_libraries(autopilot ws2_32)
    target_link_libraries(simtool ws2_32)
endif()

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
    # T4.11: the rules changed in the lobby after "Host" are those of the game, for the host and the players
    add_test(NAME mp_lan_lobby_rules COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27443 3 tower.sav 400 rules cities)
    set_tests_properties(mp_lan_pause mp_lan_player_leaves mp_lan_resume_mpsav mp_lan_other_game_data_refused
        mp_lan_lobby_rules PROPERTIES TIMEOUT 120)
    set_tests_properties(mp_lan_2_players mp_lan_4_players mp_lan_desync_detected mp_lan_cities_2_players
        mp_lan_cities_4_players mp_lan_cities_desync_detected PROPERTIES TIMEOUT 120)
    # T5.5: two players at war over the network: a brutal war, a legion of player 1 in the city of player 2, soldiers
    # lost, then peace; the same war and the same checksum on both computers (port 27550)
    add_test(NAME mp_lan_war COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27550 2 inv0.sav 3000 war cities)
    set_tests_properties(mp_lan_war PROPERTIES TIMEOUT 300)
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

# Popups and sounds of a city are for its own player only (T1.1): every computer runs every city
foreach(save tower brugle-lugdunum valentia57)
    add_test(NAME mp_private_popups_${save} COMMAND simtool privatepopups ${save}.sav 3000)
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
# T4.11: the status texts of the network game are translated, with the same numbers in French and in English
add_test(NAME mp_status_translations COMMAND translationcheck)
# T4.4, T4.11: the lobby proposes an easy game; a game hosted alone starts with the rules changed after "Host"
add_test(NAME mp_lobby_rules COMMAND simtool lobbyrules tower.sav)
# K-review-fixes (T4.11): a player refuses rules of the host out of their bounds (lobby and welcome message)
add_test(NAME mp_lobby_bad_rules COMMAND simtool badrules 27490)
set_tests_properties(mp_lobby_bad_rules PROPERTIES TIMEOUT 60)
# K-review-fixes (T4.11, D-073): a multiplayer game resumed from the lobby keeps its saved rules
add_test(NAME mp_lobby_resume_rules COMMAND simtool resumerules brugle-massilia-start.sav 27491)
# K-review-fixes (T4.11): the same with games saved before the score of Caesar (64 bytes of header) and before
# territories (56 bytes): the missing rules read as 0, the others are kept
add_test(NAME mp_lobby_resume_rules_before_score COMMAND simtool resumerules brugle-massilia-start.sav 27492 64)
add_test(NAME mp_lobby_resume_rules_before_territories COMMAND simtool resumerules brugle-massilia-start.sav 27493 56)
# T4.11: "AI invasions: no" stops every source of attack (armies and local uprisings of the scenario, the uprising
# sent by an angry Mars), which all come in a classic game and with the option on
add_test(NAME mp_attack_source_army COMMAND simtool attacksource inv0.sav army 20000)
add_test(NAME mp_attack_source_uprising COMMAND simtool attacksource tower.sav uprising 2000)
add_test(NAME mp_attack_source_mars COMMAND simtool attacksource kknight.sav mars 3200)
# End of the game by score (M4.6): the game ends at the start of the year chosen, with a score for every city
foreach(save brugle-lugdunum tower)
    add_test(NAME mp_end_by_score_${save} COMMAND simtool endscore ${save}.sav 1)
endforeach()
# Permissions to exploit (M4.7, D-020): every raw material for a single city, weapons only with iron
foreach(save brugle-lugdunum valentia57 kknight)
    add_test(NAME mp_permissions_${save} COMMAND simtool permissions ${save}.sav 3)
endforeach()

# Generated maps (M6.2): each player settles at its arrival point and gets immigrants; network game on one
# Prepared multiplayer maps (MC.2, D-033): land reached from the main road, materials and food of each arrival point,
# permissions (also after the map is saved and read), pig farms inland only (T4.15), the aqueduct of Caesar to every
# player inland, the same map every time, cities that grow; three players play on the map for four
foreach(players 2 3 4)
    add_test(NAME mp_prepared_map_${players}_players COMMAND simtool preparedmap brugle-massilia-start.sav ${players} 4000)
    # T4.14: the same rules on the prepared map 2
    add_test(NAME mp_prepared_map2_${players}_players COMMAND simtool --map 2 preparedmap brugle-massilia-start.sav ${players} 4000)
endforeach()
# T4.14: the lobby chooses map 1, map 2 or one drawn by lot; the game keeps the map it is played on
add_test(NAME mp_prepared_map_choice COMMAND simtool mapchoice brugle-massilia-start.sav)

# Reservoirs hold water (ME.1, D-035): cut off, a reservoir empties in about 270 days; joined again, it fills
add_test(NAME mp_reservoir_level COMMAND simtool reservoirlevel brugle-massilia-start.sav)
# No pond on the prepared maps (D-055): the aqueduct of Caesar is the only water of the city of the rocks, which dries
# up when it is cut and fills again when it is mended; alone on the map too
foreach(players 1 2 4)
    add_test(NAME mp_prepared_map_drying_${players}_players COMMAND simtool drying brugle-massilia-start.sav ${players})
    add_test(NAME mp_prepared_map2_drying_${players}_players COMMAND simtool --map 2 drying brugle-massilia-start.sav ${players})
endforeach()

# The aqueduct of Caesar waters the player inland wherever his arrival point is drawn, players 2 to 4 included (T4.9)
foreach(players 2 3 4)
    add_test(NAME mp_caesar_aqueduct_${players}_players COMMAND simtool inlandwater brugle-massilia-start.sav ${players})
    add_test(NAME mp_caesar_aqueduct_map2_${players}_players COMMAND simtool --map 2 inlandwater brugle-massilia-start.sav ${players})
endforeach()

# The build menu is the one of the local player: a route opened by another player does not change it (T4.13, D-061)
add_test(NAME mp_build_menu_owner COMMAND simtool menuowner brugle-massilia-start.sav)

# Long routes on the large map (T4.17): paths within the limit of a figure, a caravan between the farthest players,
# ships on a sea as large as the grid
add_test(NAME mp_long_routes COMMAND simtool longroutes brugle-massilia-start.sav)
# K-review-fixes (T4.17): an enemy army from the edge farthest from a city finds its way over the land and reaches it
add_test(NAME mp_far_invasion COMMAND simtool farinvasion brugle-massilia-start.sav)
# T4.14: every prepared map keeps its longest path under 400 steps (D-064)
add_test(NAME mp_long_routes_2_players COMMAND simtool longroutes brugle-massilia-start.sav 2)
foreach(players 2 4)
    add_test(NAME mp_long_routes_map2_${players}_players COMMAND simtool --map 2 longroutes brugle-massilia-start.sav ${players})
endforeach()

# Territories (MT.1, D-036): build only in one's zone, which follows the settled buildings and the missions
add_test(NAME mp_territory COMMAND simtool territory brugle-massilia-start.sav)
# Missions and missionaries (MT.2, MT.3, D-037): near a missionary, free then marble, trained for money
add_test(NAME mp_missions COMMAND simtool missions brugle-massilia-start.sav)
# Buildings left outside the zone collapse after three months, unless covered again (MT.4)
add_test(NAME mp_outside_territory COMMAND simtool outside brugle-massilia-start.sav)
# Fog of war (MB.1, D-038): discovered land stays discovered, what moves shows only where the player sees
add_test(NAME mp_fog_of_war COMMAND simtool fog brugle-massilia-start.sav)
# The missionary crosses the bridge of Caesar (Alexandre, 2026-10-06)
add_test(NAME mp_missionary_bridge COMMAND simtool missionarybridge brugle-massilia-start.sav)
add_test(NAME mp_missionary_bridge_map2 COMMAND simtool --map 2 missionarybridge brugle-massilia-start.sav)
# The players draw their arrival points by lot (D-052); two players inland on the map for 4, food and materials of
# the plan, no timber inland (D-062, T4.15)
add_test(NAME mp_prepared_map_placement COMMAND simtool placement brugle-massilia-start.sav)
add_test(NAME mp_prepared_map2_placement COMMAND simtool --map 2 placement brugle-massilia-start.sav)
# We go everywhere (D-052): fewer woods, which stay impassable, and no clearing shut in by them
foreach(players 2 4)
    add_test(NAME mp_prepared_map_reachable_${players}_players COMMAND simtool reachable brugle-massilia-start.sav ${players})
    add_test(NAME mp_prepared_map2_reachable_${players}_players COMMAND simtool --map 2 reachable brugle-massilia-start.sav ${players})
endforeach()
# Is the multiplayer game more restrictive than the original? The same small city built by commands and played two years
# under classic and multiplayer rules, alone and with four cities, at two difficulties: the numbers stay alike (T4.16, D-072).
# "blank" is a free classic map made by the test (80 x 80, a lake): the multiplayer cities are no poorer than the city of
# the same plan on it. The maps of the game itself (Lugdunum.map...) can be added by hand: they are not in the repository
add_test(NAME mp_restrictiveness COMMAND simtool restrictiveness brugle-massilia-start.sav blank)
# The message of a disaster far on a large map goes to it (Alexandre, 2026-10-06)
add_test(NAME mp_message_location COMMAND simtool messagelocation brugle-massilia-start.sav)

# The prepared maps need an empire trading by land and by sea (D-041)
add_test(NAME mp_prepared_map_needs_trade_routes COMMAND simtool notrade brugle-lugdunum.sav)

# The empire trades at the price of Rome: buying costs it + 50% of portorium, selling earns it - 50% (T4.12, D-060)
add_test(NAME mp_empire_import_price COMMAND simtool importprice brugle-massilia-start.sav)

# Prices between players, notice to the buyer (M8.3, D-043)
add_test(NAME mp_trade_prices COMMAND simtool tradeprices brugle-massilia-start.sav)
# Trade routes and caravans between players (M8.4, M8.5): open by both, delivered by road, paid at the seller's price
add_test(NAME mp_trade_caravans COMMAND simtool caravans brugle-massilia-start.sav)
# Trade between players makes and loses nothing, and survives a save while caravans travel (M8.8)
add_test(NAME mp_trade_conservation COMMAND simtool tradeconservation brugle-massilia-start.sav)
# The empire always sells, even what a player sells cheaper (T4.3, D-060, replaces D-048)
add_test(NAME mp_trade_empire_always_sells COMMAND simtool empiresells brugle-massilia-start.sav)
# Each player trades with the empire on his own: nothing of it leaks to another player (T4.12, D-061)
add_test(NAME mp_trade_empire_isolation COMMAND simtool tradeisolation brugle-massilia-start.sav)
add_test(NAME mp_trade_resume COMMAND simtool traderesume brugle-massilia-start.sav)
# Stock minimum and maximum: a buyer stops buying from players and the empire at his limit, a seller keeps his
# threshold; network commands, saved, an older game loads with no limit (T4.5, D-070)
add_test(NAME mp_trade_bounds COMMAND simtool tradebounds brugle-massilia-start.sav)
# Caesar the judge (M9.1, D-050): laurels and wrath in the checksum, saved and resumed, none in a classic game
add_test(NAME mp_caesar_state COMMAND simtool caesarstate brugle-massilia-start.sav)
# Monthly laurels of the five notes, ranks, the first city to the score wins (M9.2 provisional, M9.5, D-053)
add_test(NAME mp_caesar_laurels COMMAND simtool caesarlaurels valentia57.sav)
# Gifts, salary and donations to Caesar are network commands; a gift gives laurels, one counted a year (T4.2, D-067)
add_test(NAME mp_caesar_gifts COMMAND simtool caesargifts brugle-massilia-start.sav)
# Monthly history of the laurels of each city, for the ratings advisor: gain, trend, saved (T4.1, D-071)
add_test(NAME mp_caesar_history COMMAND simtool caesarhistory brugle-massilia-start.sav)
# War between players (T5.5, D-077): a state per pair of players changed by commands, in the checksum and the saved
# game; a brutal war fights at once, an honourable one after 3 months; the legion of player 1 goes to the city of player 2,
# pulls down his buildings and fights his legion; his soldiers take the caravans of the enemy; peace signed by both stops
# it and sends the legions home; the same war played twice gives the same checksum; nothing in a classic game
add_test(NAME mp_war COMMAND simtool war brugle-massilia-start.sav)

# T1.5 (D-034): the roads of Caesar stay, untinted, and serve every city
add_test(NAME mp_caesar_roads COMMAND simtool caesarroads brugle-massilia-start.sav)
foreach(players 2 4)
    add_test(NAME mp_generated_map_${players}_players COMMAND simtool mapgen brugle-massilia-start.sav ${players} 11 4000)
    # T1.2: the camera reaches the four corners (the map was in a corner of the grid, out of reach)
    add_test(NAME mp_generated_map_view_${players}_players COMMAND simtool viewcorners brugle-massilia-start.sav ${players})
endforeach()
if(NOT WIN32)
    add_test(NAME mp_lan_generated_map COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27441 3 brugle-massilia-start.sav 600 generate)
    # T4.14: three players on the prepared map 2 chosen in the lobby
    add_test(NAME mp_lan_generated_map2 COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/sim/lan_test.sh $<TARGET_FILE:simtool> 27451 3 brugle-massilia-start.sav 600 generate map2)
    set_tests_properties(mp_lan_generated_map mp_lan_generated_map2 PROPERTIES TIMEOUT 120)
endif()

