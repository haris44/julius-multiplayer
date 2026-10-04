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
 * Where the city of a player should grow, on the last generated map
 */
void mp_mapgen_city_center(int player_id, int *x, int *y);

#endif // MP_MAPGEN_H
