#ifndef MAP_SOLDIER_STRENGTH_H
#define MAP_SOLDIER_STRENGTH_H

#include "core/buffer.h"

void map_soldier_strength_clear(void);

void map_soldier_strength_add(int x, int y, int radius, int amount);

int map_soldier_strength_get(int grid_offset);

int map_soldier_strength_get_max(int x, int y, int radius, int *out_x, int *out_y);

/**
 * Saves the soldier strength grid, which classic saved games do not store
 * @param buf Buffer
 */
void map_soldier_strength_save_state(buffer *buf);

/**
 * Loads the soldier strength grid
 * @param buf Buffer
 */
void map_soldier_strength_load_state(buffer *buf);

#endif // MAP_SOLDIER_STRENGTH_H
