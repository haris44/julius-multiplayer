#ifndef MAP_NATIVES_H
#define MAP_NATIVES_H

void map_natives_init(void);
void map_natives_init_editor(void);

void map_natives_check_land(void);

/**
 * Native land is a shared grid: cleared once for the whole map, then marked by every city
 * (map_natives_check_land does both, as in the original)
 */
void map_natives_clear_land(void);
void map_natives_check_land_of_city(void);

#endif // MAP_NATIVES_H
