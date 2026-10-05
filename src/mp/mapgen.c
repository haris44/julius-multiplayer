#include "mapgen.h"

#include "building/building.h"
#include "building/storage.h"
#include "core/image.h"
#include "city/data.h"
#include "city/map.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/route.h"
#include "game/file.h"
#include "game/player_context.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "empire/city.h"
#include "map/image.h"
#include "map/owner.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_network.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "map/water_supply.h"
#include "mp/compose.h"
#include "mp/permissions.h"
#include "mp/territory.h"
#include "scenario/editor_map.h"
#include "scenario/property.h"
#include "game/resource.h"

#include <string.h>

#define MIN_SIZE 96
#define LATTICE 16
#define CITY_RADIUS 22

static struct {
    unsigned int seed;
    int size;
    int num_players; // arrival points on the map (a prepared map for 4 may have 3 players)
    int prepared;
    int aqueduct_end_x[MP_MAPGEN_MAX_PLAYERS]; // end of the aqueduct of Caesar near a city, -1 without it
    int aqueduct_end_y[MP_MAPGEN_MAX_PLAYERS];
    int river_x; // where the river of the central lake leaves the map, -1 without it
    int river_y;
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
        // prepared maps: the main road goes on to the middle of the map
        int to_x = data.prepared ? data.size / 2 : data.center_x[p];
        int to_y = data.prepared ? data.size / 2 : data.center_y[p];
        int x_min = data.entry_x[p] < to_x ? data.entry_x[p] : to_x;
        int x_max = data.entry_x[p] > to_x ? data.entry_x[p] : to_x;
        int y_min = data.entry_y[p] < to_y ? data.entry_y[p] : to_y;
        int y_max = data.entry_y[p] > to_y ? data.entry_y[p] : to_y;
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

// loads the template, empties it and grows it to a map of `size` centred on the grid
static int prepare_template(const char *template_file, int size)
{
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
    // the map is centred on the grid, as classic maps are: the camera limits and the minimap expect it
    int x0 = map_data.start_offset % GRID_SIZE;
    int y0 = map_data.start_offset / GRID_SIZE;
    int start = (GRID_MAX_SIZE - size) / 2;
    return mp_compose_relocate(GRID_MAX_SIZE, start - x0, start - y0) &&
        mp_compose_extend_map(0, 0, size - map_data.width, size - map_data.height);
}

static void set_terrain(int (*terrain_at)(int x, int y))
{
    for (int y = 0; y < data.size; y++) {
        for (int x = 0; x < data.size; x++) {
            int grid_offset = map_grid_offset(x, y);
            map_terrain_set(grid_offset, terrain_at(x, y));
            map_elevation_set(grid_offset, 0);
            map_property_set_multi_tile_size(grid_offset, 1);
            map_property_mark_draw_tile(grid_offset);
            map_property_clear_plaza_or_earthquake(grid_offset);
            map_image_set(grid_offset, 0);
        }
    }
}

static void update_tile_images(void)
{
    map_random_init();
    map_tiles_update_all_rocks();
    map_tiles_update_region_trees(0, 0, data.size - 1, data.size - 1);
    map_tiles_update_all_water();
    map_tiles_update_all_meadow();
    map_tiles_update_all_empty_land();
    map_tiles_update_all_elevation();
    map_tiles_update_all_roads();
}

// one empty city per player, each with its arrival point
static int add_cities(int num_players)
{
    for (int p = 1; p < num_players; p++) {
        if (player_context_add_player() != p) {
            return 0;
        }
    }
    for (int p = 0; p < num_players; p++) {
        set_arrival(p);
    }
    player_context_switch(0);
    return 1;
}

static void update_networks(int num_players)
{
    map_routing_update_all();
    map_road_network_update_grid();
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        map_road_network_update_largest();
        map_water_supply_init_ranges_from_terrain(0, 0, data.size - 1, data.size - 1);
    }
    player_context_switch(0);
}

int mp_mapgen_create(const char *template_file, int num_players, int size, unsigned int seed)
{
    if (num_players < 1 || num_players > MP_MAPGEN_MAX_PLAYERS || size < MIN_SIZE || size > GRID_MAX_SIZE - 4) {
        return 0;
    }
    data.seed = seed;
    data.size = size;
    data.num_players = num_players;
    data.prepared = 0;
    if (!prepare_template(template_file, size)) {
        return 0;
    }
    place_arrivals();
    set_terrain(generated_terrain);
    for (int p = 0; p < num_players; p++) {
        place_resources(p);
    }
    update_tile_images();
    if (!add_cities(num_players)) {
        return 0;
    }
    mp_permissions_share_out();
    update_networks(num_players);
    return 1;
}

// prepared multiplayer maps (D-033): each arrival point offers meadows and only the materials its player may
// exploit; provisional share, to be checked by Alexandre
#define PREPARED_SEED 2026
#define MAX_SLOT_RESOURCES 3
#define WILD_DISTANCE 45

typedef struct {
    int resources[MAX_SLOT_RESOURCES]; // materials the player may exploit, RESOURCE_NONE ends the list
    int has_water;
} map_slot;

static const int SHARED_RAW_MATERIALS[] = {
    RESOURCE_IRON, RESOURCE_CLAY, RESOURCE_TIMBER, RESOURCE_OLIVES, RESOURCE_VINES, RESOURCE_MARBLE
};
#define NUM_SHARED_RAW_MATERIALS (int) (sizeof(SHARED_RAW_MATERIALS) / sizeof(SHARED_RAW_MATERIALS[0]))

// west, east: the player of the rocks has iron and marble but no water; the other has the lake, timber and clay,
// which needs water; olives and vines are shared out
static const map_slot SLOTS_2[2] = {
    { { RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_OLIVES }, 0 },
    { { RESOURCE_TIMBER, RESOURCE_CLAY, RESOURCE_VINES }, 1 },
};

// west, east, north, south: one player of the rocks (iron and marble), the others share timber, clay, olives and
// vines; the players without water will have the aqueduct of Caesar
static const map_slot SLOTS_4[4] = {
    { { RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_NONE }, 0 },
    { { RESOURCE_TIMBER, RESOURCE_CLAY, RESOURCE_NONE }, 1 },
    { { RESOURCE_OLIVES, RESOURCE_TIMBER, RESOURCE_NONE }, 0 },
    { { RESOURCE_VINES, RESOURCE_CLAY, RESOURCE_NONE }, 1 },
};

static const map_slot *slot_of(int slot)
{
    return data.num_players <= 2 ? &SLOTS_2[slot] : &SLOTS_4[slot];
}

static int slot_has(int slot, int resource)
{
    for (int i = 0; i < MAX_SLOT_RESOURCES; i++) {
        if (slot_of(slot)->resources[i] == resource) {
            return 1;
        }
    }
    return 0;
}

static int distance_to_nearest_city(int x, int y)
{
    int nearest = data.size;
    for (int p = 0; p < data.num_players; p++) {
        int d = distance(x, y, data.center_x[p], data.center_y[p]);
        nearest = d < nearest ? d : nearest;
    }
    return nearest;
}

// two octaves: shapes less square than the lattice of the noise
static int soft_noise(int x, int y, int layer)
{
    return (2 * noise(x, y, layer) + noise(2 * x + 7, 2 * y + 3, layer + 8)) / 3;
}

static int prepared_terrain(int x, int y)
{
    if (in_settlement(x, y)) {
        return soft_noise(x, y, 4) > 150 ? TERRAIN_MEADOW : 0;
    }
    // far from every city, woods that thin out towards the cities; near them, nothing that one player may exploit
    // and another not
    int wild = distance_to_nearest_city(x, y) - WILD_DISTANCE;
    if (wild > 0 && soft_noise(x, y, 1) > 175 - (wild < 15 ? wild : 15) * 2 + 30) {
        return TERRAIN_TREE;
    }
    return soft_noise(x, y, 3) > 150 ? TERRAIN_MEADOW : 0;
}

// a round patch with a slightly irregular edge
static void set_blob(int x, int y, int radius, int terrain)
{
    for (int yy = y - radius - 2; yy <= y + radius + 2; yy++) {
        for (int xx = x - radius - 2; xx <= x + radius + 2; xx++) {
            if (xx <= 1 || yy <= 1 || xx >= data.size - 2 || yy >= data.size - 2 || in_settlement(xx, yy)) {
                continue;
            }
            int dx = xx - x, dy = yy - y;
            int r = radius * 4 + (soft_noise(xx * 4, yy * 4, 5) - 128) / 24; // in quarter tiles
            if (16 * (dx * dx + dy * dy) <= r * r) {
                map_terrain_set(map_grid_offset(xx, yy), terrain);
            }
        }
    }
}

// the four corners of a city, away from its main road
static void slot_corner(int slot, int corner, int *x, int *y)
{
    int r = CITY_RADIUS / 2 + 5;
    *x = data.center_x[slot] + (corner & 1 ? r : -r);
    *y = data.center_y[slot] + (corner & 2 ? r : -r);
}

static void place_slot(int slot)
{
    int x, y, corner = 0;
    if (slot_of(slot)->has_water) {
        slot_corner(slot, corner++, &x, &y);
        set_blob(x, y, 6, TERRAIN_WATER);
    }
    if (slot_has(slot, RESOURCE_IRON) || slot_has(slot, RESOURCE_MARBLE)) {
        slot_corner(slot, corner++, &x, &y);
        set_blob(x, y, 4, TERRAIN_ROCK);
    }
    if (slot_has(slot, RESOURCE_TIMBER)) {
        slot_corner(slot, corner++, &x, &y);
        set_blob(x, y, 6, TERRAIN_TREE);
    }
    // farms: wheat, vegetables, fruit, pigs, and olives or vines when allowed
    slot_corner(slot, corner, &x, &y);
    set_blob(x, y, 6, TERRAIN_MEADOW);
}

// a lake in the middle, for the reservoir of Caesar (D-034), joined to the north-east corner by a river: ships of
// the empire sail up to the docks built on its shores. Neither crosses a main road.
static void lake_center(int *x, int *y);

static void place_central_lake(void)
{
    int x, y;
    lake_center(&x, &y);
    set_blob(x, y, 6, TERRAIN_WATER);
    for (int step = 0;; step++) {
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int xx = x + dx, yy = y + dy;
                if (xx >= 0 && yy >= 0 && xx < data.size && yy < data.size && !in_settlement(xx, yy)) {
                    map_terrain_set(map_grid_offset(xx, yy), TERRAIN_WATER);
                }
            }
        }
        if (x == data.size - 1 || y == 0) {
            break;
        }
        // always towards the corner, more often across or up depending on a slow noise: it meanders
        int bias = soft_noise(3 * step, 0, 6);
        if (bias > 145 && x < data.size - 1) {
            x++;
        } else if (bias < 111 && y > 0) {
            y--;
        } else if (step & 1) {
            x++;
        } else {
            y--;
        }
    }
    data.river_x = x;
    data.river_y = y;
}

static void lake_center(int *x, int *y)
{
    *x = data.size / 2 + 14;
    *y = data.size / 2 - 14;
}

static void set_caesar_aqueduct(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        map_terrain_add(grid_offset, TERRAIN_AQUEDUCT); // over the main road, across it
    } else {
        map_terrain_set(grid_offset, TERRAIN_AQUEDUCT);
    }
    map_owner_set(grid_offset, MAP_OWNER_CAESAR);
}

static int is_land_for_reservoir(int x, int y)
{
    for (int yy = y; yy < y + 3; yy++) {
        for (int xx = x; xx < x + 3; xx++) {
            if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_WATER | TERRAIN_ROAD) || in_settlement(xx, yy)) {
                return 0;
            }
        }
    }
    return map_terrain_exists_tile_in_area_with_type(x - 1, y - 1, 5, TERRAIN_WATER);
}

// the reservoir of Caesar on the west shore of the central lake, and his aqueduct to every player without water:
// westwards to the city of the west, northwards to the city of the north (D-034). He never lets them dry up.
static int place_caesar_water(void)
{
    int lx, ly;
    lake_center(&lx, &ly);
    int rx = -1, ry = ly - 1;
    for (int x = lx - 14; x < lx; x++) {
        if (is_land_for_reservoir(x, ry)) {
            rx = x;
        }
    }
    if (rx < 0) {
        return 0;
    }
    for (int yy = ry; yy < ry + 3; yy++) {
        for (int xx = rx; xx < rx + 3; xx++) {
            map_terrain_set(map_grid_offset(xx, yy), 0);
        }
    }
    building *reservoir = building_create_for_caesar(BUILDING_RESERVOIR, rx, ry);
    if (!BUILDING_IS_CAESAR(reservoir->id)) {
        return 0;
    }
    map_building_tiles_add(reservoir->id, rx, ry, 3, image_group(GROUP_BUILDING_RESERVOIR), TERRAIN_BUILDING);
    map_aqueduct_set(map_grid_offset(rx, ry), 0);
    for (int slot = 0; slot < data.num_players; slot++) {
        data.aqueduct_end_x[slot] = data.aqueduct_end_y[slot] = -1;
        if (slot_of(slot)->has_water) {
            continue;
        }
        if (data.entry_x[slot] == 0) {
            // west: from the middle of the west side of the reservoir
            int end = data.center_x[slot] + CITY_RADIUS / 2 + 1;
            for (int x = rx - 1; x >= end; x--) {
                set_caesar_aqueduct(x, ry + 1);
            }
            data.aqueduct_end_x[slot] = end;
            data.aqueduct_end_y[slot] = ry + 1;
        } else if (data.entry_y[slot] == 0) {
            // north: from the middle of the north side
            int end = data.center_y[slot] + CITY_RADIUS / 2 + 1;
            for (int y = ry - 1; y >= end; y--) {
                set_caesar_aqueduct(rx + 1, y);
            }
            data.aqueduct_end_x[slot] = rx + 1;
            data.aqueduct_end_y[slot] = end;
        } else {
            return 0; // the lake is in the north-east: the prepared maps keep water in the east and south
        }
    }
    return 1;
}

// the main road of Caesar: from every arrival point to the middle of the map
static void place_main_road(void)
{
    int middle = data.size / 2;
    for (int p = 0; p < data.num_players; p++) {
        int x = data.entry_x[p], y = data.entry_y[p];
        while (1) {
            int grid_offset = map_grid_offset(x, y);
            map_terrain_set(grid_offset, TERRAIN_ROAD);
            map_owner_set(grid_offset, MAP_OWNER_CAESAR);
            if (x == middle && y == middle) {
                break;
            }
            x += x < middle ? 1 : x > middle ? -1 : 0;
            y += y < middle ? 1 : y > middle ? -1 : 0;
        }
    }
}

static void set_slot_permissions(int num_players)
{
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        for (int i = 0; i < NUM_SHARED_RAW_MATERIALS; i++) {
            empire_city_set_our_production_allowed(SHARED_RAW_MATERIALS[i], slot_has(p, SHARED_RAW_MATERIALS[i]));
        }
    }
    player_context_switch(0);
}

int mp_mapgen_slot_allows(int player_id, int resource)
{
    return data.prepared && slot_has(player_id, resource);
}

int mp_mapgen_slot_has_water(int player_id)
{
    return data.prepared && slot_of(player_id)->has_water;
}

void mp_mapgen_entry_point(int player_id, int *x, int *y)
{
    *x = data.entry_x[player_id];
    *y = data.entry_y[player_id];
}

int mp_mapgen_caesar_aqueduct_end(int player_id, int *x, int *y)
{
    *x = data.prepared ? data.aqueduct_end_x[player_id] : -1;
    *y = data.prepared ? data.aqueduct_end_y[player_id] : -1;
    return *x >= 0;
}

void mp_mapgen_river_point(int *x, int *y)
{
    *x = data.prepared ? data.river_x : -1;
    *y = data.prepared ? data.river_y : -1;
}

int mp_mapgen_prepared_size(int num_players)
{
    return num_players <= 2 ? 200 : 260;
}

int mp_mapgen_create_prepared(const char *template_file, int num_players)
{
    if (num_players < 1 || num_players > MP_MAPGEN_MAX_PLAYERS) {
        return 0;
    }
    data.seed = PREPARED_SEED;
    data.size = mp_mapgen_prepared_size(num_players);
    // three players play on the map for four, one arrival point stays free
    data.num_players = num_players <= 2 ? 2 : 4;
    data.prepared = 1;
    if (!prepare_template(template_file, data.size)) {
        return 0;
    }
    place_arrivals();
    set_terrain(prepared_terrain);
    for (int slot = 0; slot < data.num_players; slot++) {
        place_slot(slot);
    }
    place_central_lake();
    map_owner_clear_all();
    mp_territory_clear();
    place_main_road();
    if (!place_caesar_water()) {
        return 0;
    }
    update_tile_images();
    map_tiles_update_all_aqueducts(0);
    scenario_editor_set_river_entry_point(data.river_x, data.river_y);
    scenario_editor_set_river_exit_point(data.river_x, data.river_y);
    if (!add_cities(num_players)) {
        return 0;
    }
    data.num_players = num_players;
    set_slot_permissions(num_players);
    update_networks(num_players);
    return 1;
}

void mp_mapgen_city_center(int player_id, int *x, int *y)
{
    *x = data.center_x[player_id];
    *y = data.center_y[player_id];
}
