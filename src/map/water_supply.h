#ifndef MAP_WATER_SUPPLY_H
#define MAP_WATER_SUPPLY_H

#include "core/buffer.h"

void map_water_supply_update_houses(void);
void map_water_supply_update_reservoir_fountain(void);

enum {
    WELL_NECESSARY = 0,
    WELL_UNNECESSARY_FOUNTAIN = 1,
    WELL_UNNECESSARY_NO_HOUSES = 2
};

int map_water_supply_is_well_unnecessary(int well_id, int radius);

/**
 * Water ranges and aqueducts are shared grids: cleared once for the whole map, then filled by every
 * city (map_water_supply_update_reservoir_fountain does both, as in the original)
 */
void map_water_supply_clear(void);
void map_water_supply_update_reservoir_fountain_of_city(void);

/**
 * Whether the current city brings water to the tile (TERRAIN_FOUNTAIN_RANGE or TERRAIN_RESERVOIR_RANGE):
 * the range bits of the terrain in a classic game, the ranges of the current city with several cities
 */
int map_water_supply_has_range(int grid_offset, int terrain);
int map_water_supply_has_range_in_area(int x, int y, int size, int terrain);

/**
 * Water ranges of the current city, not stored in classic saved games (game/extra_state.h)
 */
void map_water_supply_reset_extra_state(void);

/**
 * Water ranges of the current city taken from the range bits of the terrain on its area (composition)
 */
void map_water_supply_init_ranges_from_terrain(int x_min, int y_min, int x_max, int y_max);
void map_water_supply_save_extra_state(buffer *buf);
void map_water_supply_load_extra_state(buffer *buf);

#endif // MAP_WATER_SUPPLY_H
