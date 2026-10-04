#ifndef MAP_WATER_SUPPLY_H
#define MAP_WATER_SUPPLY_H

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

#endif // MAP_WATER_SUPPLY_H
