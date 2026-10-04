#ifndef MAP_ROAD_NETWORK_H
#define MAP_ROAD_NETWORK_H

void map_road_network_clear(void);

int map_road_network_get(int grid_offset);

void map_road_network_update(void);

/**
 * Computes the road network grid of the whole map (shared by all players)
 */
void map_road_network_update_grid(void);

/**
 * Ranks the largest road networks of the current city, from the last grid update
 */
void map_road_network_update_largest(void);

#endif // MAP_ROAD_NETWORK_H
