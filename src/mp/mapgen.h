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
 * Builds the prepared multiplayer map for this number of players (doc/mp/DECISIONS.md D-033): always the same map,
 * each arrival point with meadows and only the materials its player may exploit, the main road of Caesar from every
 * arrival point to the middle of the map. Three players play on the map for four.
 * @param template_file Map of the free game giving climate, empire and funds
 * @return 1 on success
 */
int mp_mapgen_create_prepared(const char *template_file, int num_players);

/**
 * On the last prepared map: whether the arrival point of the player offers this material
 */
int mp_mapgen_slot_allows(int player_id, int resource);

/**
 * On the last prepared map: whether the arrival point of the player has water nearby
 */
int mp_mapgen_slot_has_water(int player_id);

/**
 * On the last prepared map: where the aqueduct of Caesar ends near the city of a player without water
 * @return 0 when the player has no aqueduct of Caesar
 */
int mp_mapgen_caesar_aqueduct_end(int player_id, int *x, int *y);

/**
 * On the last prepared map: where the river of the central lake leaves the map (ships come and go there)
 */
void mp_mapgen_river_point(int *x, int *y);

/**
 * Arrival point of a player on the last generated map
 */
void mp_mapgen_entry_point(int player_id, int *x, int *y);

/**
 * Where the city of a player should grow, on the last generated map
 */
void mp_mapgen_city_center(int player_id, int *x, int *y);

#endif // MP_MAPGEN_H
