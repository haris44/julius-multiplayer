#ifndef MP_MAPGEN_H
#define MP_MAPGEN_H

/**
 * @file
 * Generated multiplayer maps (doc/mp/ROADMAP.md M6.2): a large map of land, forests, rocks, lakes and
 * meadows, with one arrival point per player on the middle of an edge. Every player starts an empty city
 * near its arrival point, which offers forest, rocks, water and meadows; what each city may exploit follows
 * mp/permissions. Climate, empire, start year and funds come from a map of the free game.
 */

#define MP_MAPGEN_MAX_PLAYERS 4

/**
 * Side of the map for this number of players
 */
int mp_mapgen_default_size(int num_players);

/**
 * Builds the starting game in memory
 * @param template_file Map of the free game (.map) giving climate, empire and funds, or a saved game
 * @param size Side of the square map, in tiles
 * @param seed Same seed, same map
 * @return 1 on success
 */
int mp_mapgen_create(const char *template_file, int num_players, int size, unsigned int seed);

/**
 * Side of the prepared map for this number of players: the map for 2, or the map for 4 (3 players included)
 */
int mp_mapgen_prepared_size(int num_players);

/**
 * Map of the free game giving the empire, start year and funds of the prepared maps: the first one found in the
 * data of the game among those whose empire trades by land and by sea (doc/mp/DECISIONS.md D-044)
 * @return Its file name, or 0 when the data holds none
 */
const char *mp_mapgen_prepared_template(void);

/**
 * Builds the prepared multiplayer map for this number of players (doc/mp/DECISIONS.md D-033, D-047): always the same
 * map, crossed from west to east by an arm of the sea; both players on the south shore of the map for 2, two on each
 * shore of the map for 4, joined by the main road of Caesar over his bridge. Each arrival point offers meadows and
 * only the materials its player may exploit; the player of the rocks lives far from water and gets the aqueduct of
 * Caesar, the others live on the coast. Three players play on the map for four.
 * @param template_file Map of the free game giving climate, empire and funds
 * @param placement_seed Draw of the arrival points among the players; 0 keeps the order of the plan
 * @return 1 on success
 */
int mp_mapgen_create_prepared(const char *template_file, int num_players, unsigned int placement_seed);

/**
 * On the last prepared map: whether the arrival point of the player offers this material
 */
int mp_mapgen_slot_allows(int player_id, int resource);

/**
 * On the last prepared map: whether the player lives on the coast of the sea (all but the player of the rocks)
 */
int mp_mapgen_slot_is_coastal(int player_id);

/**
 * On the last prepared map: where the aqueduct of Caesar ends, coming westwards along a row, near the city of the
 * player of the rocks
 * @return 0 when the player has no aqueduct of Caesar
 */
int mp_mapgen_caesar_aqueduct_end(int player_id, int *x, int *y);

/**
 * On the last prepared map: where the arm of the sea meets the west edge (east = 0) or the east edge (east = 1)
 */
void mp_mapgen_sea_end(int east, int *x, int *y);

/**
 * On the last prepared map: where the ships of the empire of a player come in, the end of the sea nearest to him
 */
void mp_mapgen_player_river_point(int player_id, int *x, int *y);

/**
 * On the last prepared map: the column of the bridge of Caesar over the sea and its two ends
 */
void mp_mapgen_caesar_bridge(int *x, int *y_north, int *y_south);

/**
 * Arrival point of a player on the last generated map
 */
void mp_mapgen_entry_point(int player_id, int *x, int *y);

/**
 * Whether the last prepared map was refused because the empire of its template does not trade by land and by sea
 */
int mp_mapgen_lacks_trade_routes(void);

/**
 * On the last prepared map: the mission every player starts with (top-left tile)
 */
void mp_mapgen_start_mission(int player_id, int *x, int *y);

/**
 * Where the city of a player should grow, on the last generated map
 */
void mp_mapgen_city_center(int player_id, int *x, int *y);

#endif // MP_MAPGEN_H
