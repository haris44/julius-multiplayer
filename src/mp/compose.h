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

#endif // MP_COMPOSE_H
