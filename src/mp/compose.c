#include "compose.h"

#include "building/building.h"
#include "building/granary.h"
#include "empire/city.h"
#include "scenario/scenario.h"
#include "scenario/earthquake.h"
#include "map/point.h"
#include "map/owner.h"
#include "game/player_context.h"
#include "game/player_clone.h"
#include "figure/route.h"
#include "figure/formation.h"
#include "figure/enemy_army.h"
#include "city/labor.h"
#include "building/storage.h"
#include "building/list.h"
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

int mp_compose_set_map_size(int width, int height)
{
    int x0 = map_data.start_offset % GRID_SIZE;
    int y0 = map_data.start_offset / GRID_SIZE;
    if (x0 + width >= GRID_SIZE || y0 + height >= GRID_SIZE) {
        return 0;
    }
    map_grid_init(width, height, map_data.start_offset, GRID_SIZE - width);
    scenario_map_set_size(width, height);
    scenario_map_set_grid_position(map_data.start_offset, GRID_SIZE - width);
    return 1;
}

static void copy_region(int x_min, int y_min, int width, int height, const player_clone *c)
{
    int stride = GRID_SIZE;
    // map-relative region of the copied city, one tile of border included
    for (int y = y_min - 1; y <= y_min + height; y++) {
        for (int x = x_min - 1; x <= x_min + width; x++) {
            int from = map_grid_offset(x, y);
            int to = from + c->grid_delta;
            if (from < 0 || to < 0 || to >= stride * stride) {
                continue;
            }
            for (int g = 0; g < NUM_GRIDS; g++) {
                int size = ITEM_SIZE[g];
                memcpy(&relocation.data[g][to * size], &relocation.data[g][from * size], size);
            }
            // ids stored in the building and figure grids belong to the copied slices
            int building_id = relocation.data[GRID_BUILDING][2 * to] | (relocation.data[GRID_BUILDING][2 * to + 1] << 8);
            building_id = player_clone_id(c, building_id, MAX_BUILDINGS);
            relocation.data[GRID_BUILDING][2 * to] = building_id & 0xff;
            relocation.data[GRID_BUILDING][2 * to + 1] = building_id >> 8;
            int figure_id = relocation.data[GRID_FIGURE][2 * to] | (relocation.data[GRID_FIGURE][2 * to + 1] << 8);
            figure_id = player_clone_id(c, figure_id, MAX_FIGURES);
            relocation.data[GRID_FIGURE][2 * to] = figure_id & 0xff;
            relocation.data[GRID_FIGURE][2 * to + 1] = figure_id >> 8;
            map_owner_set(to, c->to);
        }
    }
}

int mp_compose_add_twin(int x_min, int y_min, int width, int height, int dx, int dy)
{
    int stride = GRID_SIZE;
    if (player_context_num_players() != 1 || x_min + dx + width > map_data.width ||
        y_min + dy + height > map_data.height) {
        return 0;
    }
    player_clone c = { 0, 1, dx, dy, dx + dy * stride };
    for (int g = 0; g < NUM_GRIDS; g++) {
        relocation.data[g] = malloc(GRID_MAX_TILES * ITEM_SIZE[g]);
    }
    init_buffers(stride);
    save_grids();
    copy_region(x_min, y_min, width, height, &c);
    init_buffers(stride);
    load_grids();
    for (int g = 0; g < NUM_GRIDS; g++) {
        free(relocation.data[g]);
    }

    // the per-city state of the twin starts as a copy of the first city
    player_context_set_num_players(2);
    building_clone_player(&c);
    figure_clone_player(&c);
    formation_clone_player(&c);
    building_storage_clone_player(&c);
    figure_route_clone_player(&c);

    player_context_switch(1);
    city_data_clone_fixup(&c);
    building_clone_player_counters(&c);
    formation_clone_player_counters(&c);
    city_labor_clone_fixup(&c);
    building_list_clone_fixup(&c);
    enemy_army_clone_fixup(&c);
    scenario_clone_fixup(&c);
    scenario_earthquake_clone_fixup(&c);
    map_point_clone_fixup(&c);
    empire_city_clone_fixup(&c);
    building_granary_clone_fixup(&c);
    player_context_switch(0);

    map_routing_update_all();
    map_road_network_update_grid();
    for (int p = 0; p < 2; p++) {
        player_context_switch(p);
        map_road_network_update_largest();
    }
    player_context_switch(0);

    // road network numbers kept in the buildings: those of the copy's own networks
    static int network_of_copy[65536];
    memset(network_of_copy, 0, sizeof(network_of_copy));
    for (int y = y_min - 1; y <= y_min + height; y++) {
        for (int x = x_min - 1; x <= x_min + width; x++) {
            int offset = map_grid_offset(x, y);
            int network = map_road_network_get(offset);
            if (network) {
                network_of_copy[network] = map_road_network_get(offset + c.grid_delta);
            }
        }
    }
    for (int i = MAX_BUILDINGS; i < 2 * MAX_BUILDINGS; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_UNUSED && b->road_network_id) {
            b->road_network_id = network_of_copy[b->road_network_id];
        }
    }
    return 1;
}

uint64_t mp_compose_region_checksum(int x_min, int y_min, int width, int height)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (int g = 0; g < NUM_GRIDS; g++) {
        relocation.data[g] = malloc(GRID_MAX_TILES * ITEM_SIZE[g]);
    }
    init_buffers(GRID_SIZE);
    save_grids();
    for (int g = 0; g < NUM_GRIDS; g++) {
        if (g == GRID_IMAGE || g == GRID_SPRITE || g == GRID_SPRITE_BACKUP) {
            continue; // written by the user interface, as in mp_checksum_state
        }
        for (int y = y_min - 1; y <= y_min + height; y++) {
            for (int x = x_min - 1; x <= x_min + width; x++) {
                int offset = map_grid_offset(x, y);
                for (int b = 0; b < ITEM_SIZE[g]; b++) {
                    uint8_t value = relocation.data[g][offset * ITEM_SIZE[g] + b];
                    if (g == GRID_BITFIELDS) {
                        value &= 0xaf; // construction previews
                    }
                    hash = (hash ^ value) * 0x100000001b3ULL;
                }
            }
        }
        free(relocation.data[g]);
    }
    return hash;
}

static void shift_city_in_place(int player_id, int dx, int dy)
{
    // tiles stay where they are on the grid; their map coordinates change with the map origin
    player_clone c = { player_id, player_id, dx, dy, 0 };
    building_clone_player(&c);
    figure_clone_player(&c);
    formation_clone_player(&c);
    int previous = player_context_current();
    player_context_switch(player_id);
    city_data_clone_fixup(&c);
    enemy_army_clone_fixup(&c);
    scenario_clone_fixup(&c);
    scenario_earthquake_clone_fixup(&c);
    map_point_clone_fixup(&c);
    player_context_switch(previous);
}

int mp_compose_extend_map(int left, int top, int right, int bottom)
{
    int stride = GRID_SIZE;
    int x0 = map_data.start_offset % stride - left;
    int y0 = map_data.start_offset / stride - top;
    int width = map_data.width + left + right;
    int height = map_data.height + top + bottom;
    if (x0 < 1 || y0 < 1 || x0 + width >= stride || y0 + height >= stride || width <= 0 || height <= 0) {
        return 0;
    }
    int start = x0 + y0 * stride;
    // tiles added to the map are impassable rock: like the outside of a classic map, they block
    // walkers, boats and floating debris alike (forest and water inside the map would be navigable)
    int old_x0 = map_data.start_offset % stride;
    int old_y0 = map_data.start_offset / stride;
    for (int y = y0; y < y0 + height; y++) {
        for (int x = x0; x < x0 + width; x++) {
            int was_inside = x >= old_x0 && x < old_x0 + map_data.width && y >= old_y0 && y < old_y0 + map_data.height;
            if (!was_inside) {
                map_terrain_set(x + y * stride, TERRAIN_ROCK);
            }
        }
    }
    map_grid_init(width, height, start, stride - width);
    scenario_map_set_size(width, height);
    scenario_map_set_grid_position(start, stride - width);
    for (int p = 0; p < player_context_num_players(); p++) {
        shift_city_in_place(p, left, top);
    }
    map_routing_update_all();
    map_road_network_update();
    return 1;
}
