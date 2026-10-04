#include "mapgen.h"

#include "building/building.h"
#include "building/storage.h"
#include "city/data.h"
#include "city/map.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/route.h"
#include "game/file.h"
#include "game/player_context.h"
#include "map/building.h"
#include "map/data.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_network.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "map/water_supply.h"
#include "mp/compose.h"
#include "mp/permissions.h"
#include "scenario/editor_map.h"
#include "scenario/property.h"

#include <string.h>

#define MIN_SIZE 96
#define LATTICE 16
#define CITY_RADIUS 22

static struct {
    unsigned int seed;
    int size;
    int num_players;
    int center_x[MP_MAPGEN_MAX_PLAYERS];
    int center_y[MP_MAPGEN_MAX_PLAYERS];
    int entry_x[MP_MAPGEN_MAX_PLAYERS];
    int entry_y[MP_MAPGEN_MAX_PLAYERS];
} data;

// deterministic integer hash: the same seed gives the same map on every computer
static unsigned int hash(int x, int y, int layer)
{
    unsigned int h = data.seed * 0x9e3779b1u + (unsigned int) x * 0x85ebca6bu + (unsigned int) y * 0xc2b2ae35u +
        (unsigned int) layer * 0x27d4eb2fu;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

// smooth value noise from 0 to 255, in cells of LATTICE tiles
static int noise(int x, int y, int layer)
{
    int cx = x / LATTICE, cy = y / LATTICE;
    int fx = x % LATTICE, fy = y % LATTICE;
    int v00 = hash(cx, cy, layer) & 0xff;
    int v10 = hash(cx + 1, cy, layer) & 0xff;
    int v01 = hash(cx, cy + 1, layer) & 0xff;
    int v11 = hash(cx + 1, cy + 1, layer) & 0xff;
    int top = v00 * (LATTICE - fx) + v10 * fx;
    int bottom = v01 * (LATTICE - fx) + v11 * fx;
    return (top * (LATTICE - fy) + bottom * fy) / (LATTICE * LATTICE);
}

static int distance(int x1, int y1, int x2, int y2)
{
    int dx = x1 > x2 ? x1 - x2 : x2 - x1;
    int dy = y1 > y2 ? y1 - y2 : y2 - y1;
    return dx > dy ? dx : dy;
}

// arrival points on the middle of the edges: left, right, top, bottom
static void place_arrivals(void)
{
    static const int EDGE_X[MP_MAPGEN_MAX_PLAYERS] = { 0, 2, 1, 1 };
    static const int EDGE_Y[MP_MAPGEN_MAX_PLAYERS] = { 1, 1, 0, 2 };
    int last = data.size - 1;
    for (int p = 0; p < data.num_players; p++) {
        data.entry_x[p] = EDGE_X[p] == 0 ? 0 : EDGE_X[p] == 2 ? last : data.size / 2;
        data.entry_y[p] = EDGE_Y[p] == 0 ? 0 : EDGE_Y[p] == 2 ? last : data.size / 2;
        data.center_x[p] = EDGE_X[p] == 0 ? data.size / 5 : EDGE_X[p] == 2 ? last - data.size / 5 : data.size / 2;
        data.center_y[p] = EDGE_Y[p] == 0 ? data.size / 5 : EDGE_Y[p] == 2 ? last - data.size / 5 : data.size / 2;
    }
}

// land kept clear: around each city and on the way from its arrival point to it
static int in_settlement(int x, int y)
{
    for (int p = 0; p < data.num_players; p++) {
        if (distance(x, y, data.center_x[p], data.center_y[p]) < CITY_RADIUS / 2) {
            return 1;
        }
        int x_min = data.entry_x[p] < data.center_x[p] ? data.entry_x[p] : data.center_x[p];
        int x_max = data.entry_x[p] > data.center_x[p] ? data.entry_x[p] : data.center_x[p];
        int y_min = data.entry_y[p] < data.center_y[p] ? data.entry_y[p] : data.center_y[p];
        int y_max = data.entry_y[p] > data.center_y[p] ? data.entry_y[p] : data.center_y[p];
        if (x >= x_min - 2 && x <= x_max + 2 && y >= y_min - 2 && y <= y_max + 2) {
            return 1;
        }
    }
    return 0;
}

static int generated_terrain(int x, int y)
{
    if (in_settlement(x, y)) {
        return noise(x, y, 4) > 150 ? TERRAIN_MEADOW : 0;
    }
    if (noise(x, y, 0) < 60) {
        return TERRAIN_WATER;
    }
    if (noise(x, y, 1) > 175) {
        return TERRAIN_TREE;
    }
    if (noise(x, y, 2) > 195) {
        return TERRAIN_ROCK;
    }
    if (noise(x, y, 3) > 150) {
        return TERRAIN_MEADOW;
    }
    return 0;
}

static void set_patch(int x, int y, int radius, int terrain)
{
    for (int yy = y - radius; yy <= y + radius; yy++) {
        for (int xx = x - radius; xx <= x + radius; xx++) {
            if (xx > 0 && yy > 0 && xx < data.size - 1 && yy < data.size - 1 &&
                distance(xx, yy, x, y) <= radius) {
                map_terrain_set(map_grid_offset(xx, yy), terrain);
            }
        }
    }
}

// every city finds the land it needs near it: forest, rocks, a pond and meadows
static void place_resources(int p)
{
    int cx = data.center_x[p], cy = data.center_y[p];
    int r = CITY_RADIUS / 2 + 4;
    set_patch(cx - r, cy - r, 3, TERRAIN_TREE);
    set_patch(cx + r, cy - r, 2, TERRAIN_ROCK);
    set_patch(cx - r, cy + r, 2, TERRAIN_WATER);
    set_patch(cx + r, cy + r, 3, TERRAIN_MEADOW);
    set_patch(cx, cy + r + 2, 2, TERRAIN_MEADOW);
}

static void remove_scenario_entities(void)
{
    // natives, herds and fishing boats of the template belong to its own terrain, which is replaced
    building_clear_all();
    building_storage_clear_all();
    figure_init_scenario();
    formations_clear();
    enemy_armies_clear();
    figure_route_clear_all();
    map_building_clear();
    map_figure_clear();
    scenario_editor_clear_herd_points();
    scenario_editor_clear_fishing_points();
    scenario_editor_set_river_entry_point(-1, -1);
    scenario_editor_set_river_exit_point(-1, -1);
    scenario_editor_set_earthquake_point(-1, -1);
}

static void set_arrival(int p)
{
    player_context_switch(p);
    scenario_editor_set_entry_point(data.entry_x[p], data.entry_y[p]);
    scenario_editor_set_exit_point(data.entry_x[p], data.entry_y[p]);
    city_map_set_entry_point(data.entry_x[p], data.entry_y[p]);
    city_map_set_exit_point(data.entry_x[p], data.entry_y[p]);
    map_tiles_add_entry_exit_flags();
    // invaders come from the edges
    scenario_editor_clear_invasion_points();
    scenario_editor_set_invasion_point(0, data.size / 2, 1);
    scenario_editor_set_invasion_point(1, data.size / 2, data.size - 2);
    scenario_editor_set_invasion_point(2, 1, data.size / 3);
    scenario_editor_set_invasion_point(3, data.size - 2, 2 * data.size / 3);
}

int mp_mapgen_default_size(int num_players)
{
    return num_players <= 2 ? 200 : 260;
}

int mp_mapgen_create(const char *template_file, int num_players, int size, unsigned int seed)
{
    if (num_players < 1 || num_players > MP_MAPGEN_MAX_PLAYERS || size < MIN_SIZE || size > GRID_MAX_SIZE - 4) {
        return 0;
    }
    data.seed = seed;
    data.size = size;
    data.num_players = num_players;

    // climate, empire, start year and funds of a map of the free game (or of a saved game: tests)
    size_t length = strlen(template_file);
    int is_map = length > 4 && (strcmp(template_file + length - 4, ".map") == 0 ||
        strcmp(template_file + length - 4, ".MAP") == 0);
    if (is_map) {
        scenario_set_custom(2);
    }
    if (!(is_map ? game_file_start_scenario(template_file) : game_file_load_saved_game(template_file)) ||
        player_context_num_players() != 1) {
        return 0;
    }
    remove_scenario_entities();
    if (!is_map) {
        // a saved game as template: the cities start empty, with the funds of its scenario
        city_data_init();
        city_data_init_scenario();
    }
    int x0 = map_data.start_offset % GRID_SIZE;
    int y0 = map_data.start_offset / GRID_SIZE;
    if (!mp_compose_relocate(GRID_MAX_SIZE, 1 - x0, 1 - y0) ||
        !mp_compose_extend_map(0, 0, size - map_data.width, size - map_data.height)) {
        return 0;
    }

    // the terrain of the whole map
    place_arrivals();
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int grid_offset = map_grid_offset(x, y);
            map_terrain_set(grid_offset, generated_terrain(x, y));
            map_elevation_set(grid_offset, 0);
            map_property_set_multi_tile_size(grid_offset, 1);
            map_property_mark_draw_tile(grid_offset);
            map_property_clear_plaza_or_earthquake(grid_offset);
            map_image_set(grid_offset, 0);
        }
    }
    for (int p = 0; p < num_players; p++) {
        place_resources(p);
    }
    map_random_init();
    map_tiles_update_all_rocks();
    map_tiles_update_region_trees(0, 0, size - 1, size - 1);
    map_tiles_update_all_water();
    map_tiles_update_all_meadow();
    map_tiles_update_all_empty_land();
    map_tiles_update_all_elevation();

    // one empty city per player, each with its arrival point
    for (int p = 1; p < num_players; p++) {
        if (player_context_add_player() != p) {
            return 0;
        }
    }
    for (int p = 0; p < num_players; p++) {
        set_arrival(p);
    }
    player_context_switch(0);
    mp_permissions_share_out();
    map_routing_update_all();
    map_road_network_update_grid();
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        map_road_network_update_largest();
        map_water_supply_init_ranges_from_terrain(0, 0, size - 1, size - 1);
    }
    player_context_switch(0);
    return 1;
}

void mp_mapgen_city_center(int player_id, int *x, int *y)
{
    *x = data.center_x[player_id];
    *y = data.center_y[player_id];
}
