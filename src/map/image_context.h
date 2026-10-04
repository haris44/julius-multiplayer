#ifndef MAP_IMAGE_CONTEXT_H
#define MAP_IMAGE_CONTEXT_H

#include "core/buffer.h"

typedef struct {
    int is_valid;
    int group_offset;
    int item_offset;
    int aqueduct_offset;
} terrain_image;

void map_image_context_init(void);

/**
 * Saves the rotation state of image variants, which classic saved games do not store
 * @param buf Buffer
 */
void map_image_context_save_state(buffer *buf);

/**
 * Loads the rotation state of image variants
 * @param buf Buffer
 */
void map_image_context_load_state(buffer *buf);
void map_image_context_reset_water(void);
void map_image_context_reset_elevation(void);

const terrain_image *map_image_context_get_elevation(int grid_offset, int elevation);
const terrain_image *map_image_context_get_earthquake(int grid_offset);
const terrain_image *map_image_context_get_shore(int grid_offset);
const terrain_image *map_image_context_get_wall(int grid_offset);
const terrain_image *map_image_context_get_wall_gatehouse(int grid_offset);
const terrain_image *map_image_context_get_dirt_road(int grid_offset);
const terrain_image *map_image_context_get_paved_road(int grid_offset);
const terrain_image *map_image_context_get_aqueduct(int grid_offset, int include_construction);

#endif // MAP_IMAGE_CONTEXT_H
