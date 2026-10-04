#include "compose.h"

#include "building/building.h"
#include "city/data.h"
#include "core/buffer.h"
#include "figure/figure.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/data.h"
#include "map/desirability.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_network.h"
#include "map/routing_terrain.h"
#include "map/soldier_strength.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "scenario/map.h"

#include <stdlib.h>
#include <string.h>

// Every grid of the map, saved and loaded through the functions of its module
enum {
    GRID_IMAGE, GRID_BUILDING, GRID_BUILDING_DAMAGE, GRID_TERRAIN, GRID_AQUEDUCT, GRID_AQUEDUCT_BACKUP,
    GRID_FIGURE, GRID_SPRITE, GRID_SPRITE_BACKUP, GRID_BITFIELDS, GRID_EDGE, GRID_RANDOM,
    GRID_DESIRABILITY, GRID_ELEVATION, GRID_SOLDIER_STRENGTH, NUM_GRIDS
};

static const int ITEM_SIZE[NUM_GRIDS] = { 2, 2, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1 };

static struct {
    int old_stride;
    int new_stride;
    int dx;
    int dy;
    uint8_t *data[NUM_GRIDS];
    buffer buf[NUM_GRIDS];
} relocation;

static void init_buffers(int stride)
{
    for (int g = 0; g < NUM_GRIDS; g++) {
        buffer_init(&relocation.buf[g], relocation.data[g], stride * stride * ITEM_SIZE[g]);
    }
}

static void save_grids(void)
{
    map_image_save_state(&relocation.buf[GRID_IMAGE]);
    map_building_save_state(&relocation.buf[GRID_BUILDING], &relocation.buf[GRID_BUILDING_DAMAGE]);
    map_terrain_save_state(&relocation.buf[GRID_TERRAIN]);
    map_aqueduct_save_state(&relocation.buf[GRID_AQUEDUCT], &relocation.buf[GRID_AQUEDUCT_BACKUP]);
    map_figure_save_state(&relocation.buf[GRID_FIGURE]);
    map_sprite_save_state(&relocation.buf[GRID_SPRITE], &relocation.buf[GRID_SPRITE_BACKUP]);
    map_property_save_state(&relocation.buf[GRID_BITFIELDS], &relocation.buf[GRID_EDGE]);
    map_random_save_state(&relocation.buf[GRID_RANDOM]);
    map_desirability_save_state(&relocation.buf[GRID_DESIRABILITY]);
    map_elevation_save_state(&relocation.buf[GRID_ELEVATION]);
    map_soldier_strength_save_state(&relocation.buf[GRID_SOLDIER_STRENGTH]);
}

static void load_grids(void)
{
    map_image_load_state(&relocation.buf[GRID_IMAGE]);
    map_building_load_state(&relocation.buf[GRID_BUILDING], &relocation.buf[GRID_BUILDING_DAMAGE]);
    map_terrain_load_state(&relocation.buf[GRID_TERRAIN]);
    map_aqueduct_load_state(&relocation.buf[GRID_AQUEDUCT], &relocation.buf[GRID_AQUEDUCT_BACKUP]);
    map_figure_load_state(&relocation.buf[GRID_FIGURE]);
    map_sprite_load_state(&relocation.buf[GRID_SPRITE], &relocation.buf[GRID_SPRITE_BACKUP]);
    map_property_load_state(&relocation.buf[GRID_BITFIELDS], &relocation.buf[GRID_EDGE]);
    map_random_load_state(&relocation.buf[GRID_RANDOM]);
    map_desirability_load_state(&relocation.buf[GRID_DESIRABILITY]);
    map_elevation_load_state(&relocation.buf[GRID_ELEVATION]);
    map_soldier_strength_load_state(&relocation.buf[GRID_SOLDIER_STRENGTH]);
}

static int remap(int grid_offset)
{
    // only offsets of the old grid move; 0 means "none" (it is always in the border) and stays 0
    if (grid_offset <= 0 || grid_offset >= relocation.old_stride * relocation.old_stride) {
        return grid_offset;
    }
    int x = grid_offset % relocation.old_stride + relocation.dx;
    int y = grid_offset / relocation.old_stride + relocation.dy;
    return x + y * relocation.new_stride;
}

static void move_tiles(uint8_t *old_data[NUM_GRIDS])
{
    int old_stride = relocation.old_stride;
    int new_stride = relocation.new_stride;
    for (int g = 0; g < NUM_GRIDS; g++) {
        int size = ITEM_SIZE[g];
        memset(relocation.data[g], 0, new_stride * new_stride * size);
        if (g == GRID_TERRAIN) {
            // outside the map, as map_terrain_init_outside_map does (little endian u16)
            for (int i = 0; i < new_stride * new_stride; i++) {
                relocation.data[g][2 * i] = TERRAIN_TREE | TERRAIN_WATER;
            }
        }
        for (int y = 0; y < old_stride; y++) {
            int new_y = y + relocation.dy;
            for (int x = 0; x < old_stride; x++) {
                int new_x = x + relocation.dx;
                if (new_x < 0 || new_y < 0 || new_x >= new_stride || new_y >= new_stride) {
                    continue;
                }
                memcpy(&relocation.data[g][(new_x + new_y * new_stride) * size],
                    &old_data[g][(x + y * old_stride) * size], size);
            }
        }
    }
}

int mp_compose_relocate(int new_stride, int dx, int dy)
{
    int old_stride = GRID_SIZE;
    int x0 = map_data.start_offset % old_stride;
    int y0 = map_data.start_offset / old_stride;
    if (new_stride > GRID_MAX_SIZE || x0 + dx < 1 || y0 + dy < 1 ||
        x0 + dx + map_data.width >= new_stride || y0 + dy + map_data.height >= new_stride) {
        return 0; // the map must stay inside the grid, with a border
    }
    relocation.old_stride = old_stride;
    relocation.new_stride = new_stride;
    relocation.dx = dx;
    relocation.dy = dy;

    uint8_t *old_data[NUM_GRIDS];
    for (int g = 0; g < NUM_GRIDS; g++) {
        old_data[g] = malloc(GRID_MAX_TILES * ITEM_SIZE[g]);
        relocation.data[g] = old_data[g];
    }
    init_buffers(old_stride);
    save_grids();
    for (int g = 0; g < NUM_GRIDS; g++) {
        relocation.data[g] = malloc(GRID_MAX_TILES * ITEM_SIZE[g]);
    }
    move_tiles(old_data);

    int new_start = remap(map_data.start_offset);
    int new_border = new_stride - map_data.width;
    map_grid_init(map_data.width, map_data.height, new_start, new_border);
    scenario_map_set_grid_position(new_start, new_border);
    init_buffers(new_stride);
    load_grids();

    building_relocate_grid_offsets(remap);
    figure_relocate_grid_offsets(remap);
    city_data_relocate_grid_offsets(remap);

    // grids derived from the terrain, recomputed as when a saved game is loaded
    map_routing_update_all();
    map_road_network_update();

    for (int g = 0; g < NUM_GRIDS; g++) {
        free(old_data[g]);
        free(relocation.data[g]);
    }
    return 1;
}
