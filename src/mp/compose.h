#ifndef MP_COMPOSE_H
#define MP_COMPOSE_H

/**
 * @file
 * Moving a city on a grid of another size: the base of large multiplayer maps built from
 * classic maps or saved games, and of the tests that prove a city behaves the same there
 * (doc/mp/ROADMAP.md M3.3, M3.7).
 */

/**
 * Moves the current map and city to a grid of side new_stride, shifted by (dx, dy) tiles.
 * Map coordinates do not change (they count from the first tile of the map), every grid offset does.
 * @return 1 on success, 0 when the map does not fit
 */
int mp_compose_relocate(int new_stride, int dx, int dy);

#include <stdint.h>

/**
 * Changes the size of the playable map area, keeping its first tile
 */
int mp_compose_set_map_size(int width, int height);

/**
 * Adds a second player whose city is a copy of the first one, shifted by (dx, dy) tiles
 * @param x_min First column of the copied area (map coordinates)
 * @param y_min First row of the copied area
 * @param width Width of the copied area
 * @param height Height of the copied area
 */
int mp_compose_add_twin(int x_min, int y_min, int width, int height, int dx, int dy);

/**
 * Checksum of every simulation grid in a map area (one tile of border included)
 */
uint64_t mp_compose_region_checksum(int x_min, int y_min, int width, int height);

/**
 * Adds tiles around the playable map area (negative values remove them). The map origin moves,
 * so every map coordinate of the cities is shifted; tiles stay where they are on the grid.
 */
int mp_compose_extend_map(int left, int top, int right, int bottom);

#endif // MP_COMPOSE_H
