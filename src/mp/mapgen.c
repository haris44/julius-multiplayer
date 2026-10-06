#include "mapgen.h"

#include "building/building.h"
#include "building/storage.h"
#include "core/dir.h"
#include "core/image.h"
#include "city/data.h"
#include "city/map.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/route.h"
#include "figuretype/animal.h"
#include "game/file.h"
#include "game/player_context.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "empire/city.h"
#include "empire/type.h"
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
#include "mp/fog.h"
#include "mp/missionary.h"
#include "mp/permissions.h"
#include "mp/territory.h"
#include "scenario/data.h"
#include "scenario/editor_map.h"
#include "scenario/property.h"
#include "game/resource.h"

#include <string.h>

#define MIN_SIZE 96
#define LATTICE 16
#define CITY_RADIUS 22

typedef struct map_layout map_layout; // plan of a prepared map, defined with the prepared maps below

static struct {
    unsigned int seed;
    int size;
    int num_players; // arrival points on the map (a prepared map for 4 may have 3 players)
    int prepared;
    const map_layout *layout; // prepared maps: their plan
    int slot_of_player[MP_MAPGEN_MAX_PLAYERS]; // prepared maps: arrival point of each player, drawn by lot
    int sea_top[GRID_MAX_SIZE]; // prepared maps: the arm of the sea at each column, both coasts included
    int sea_bottom[GRID_MAX_SIZE];
    int aqueduct_end_x[MP_MAPGEN_MAX_PLAYERS]; // end of the aqueduct of Caesar near a city, -1 without it
    int aqueduct_end_y[MP_MAPGEN_MAX_PLAYERS];
    int center_x[MP_MAPGEN_MAX_PLAYERS];
    int center_y[MP_MAPGEN_MAX_PLAYERS];
    int entry_x[MP_MAPGEN_MAX_PLAYERS];
    int entry_y[MP_MAPGEN_MAX_PLAYERS];
    int mission_x[MP_MAPGEN_MAX_PLAYERS];
    int mission_y[MP_MAPGEN_MAX_PLAYERS];
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

static int in_prepared_settlement(int x, int y);

// land kept clear: around each city, and along the main roads (prepared maps) or on the way from the arrival point
// to the city (random maps of the tests)
static int in_settlement(int x, int y)
{
    if (data.prepared) {
        return in_prepared_settlement(x, y);
    }
    for (int p = 0; p < data.num_players; p++) {
        if (distance(x, y, data.center_x[p], data.center_y[p]) < CITY_RADIUS / 2) {
            return 1;
        }
        int to_x = data.center_x[p];
        int to_y = data.center_y[p];
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

// in the order of the game loading a map: the grass of empty land first, then the meadows over it (the other way
// round, the grass covered the meadows, which showed again only once cleared)
static void update_tile_images(void)
{
    map_random_init();
    map_tiles_update_all_elevation();
    map_tiles_update_all_water();
    map_tiles_update_all_rocks();
    map_tiles_update_region_trees(0, 0, data.size - 1, data.size - 1);
    map_tiles_update_all_empty_land();
    map_tiles_update_all_meadow();
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

// prepared multiplayer maps (D-033, D-047, D-062): two maps (T4.14, D-069), each with one fixed plan for 2 players
// and one for 4. A great arm of the sea crosses the map from the west edge to the east edge; on the maps for 2 both
// players live on the same shore, on the maps for 4 two players live on each shore, joined by the main road of Caesar
// over his bridge. The players of the
// rocks live far from the sea and get no water but the aqueduct of Caesar, one reservoir of his on the nearest coast
// for each of them; the others live on the coast, where the ships of their empire come along the sea. No pond
// anywhere: the sea is the only water of the map, so that cutting the aqueduct of Caesar dries the cities of the rocks
// up (D-055). Each arrival point offers meadows and only the food and materials of its plan (T4.15): wheat and
// vegetables for everybody; pigs, iron and marble inland; fruit, fish, timber and clay on the coast.
#define PREPARED_SEED 2026
#define MAX_SLOT_RESOURCES 8
#define WILD_DISTANCE 30
#define SMALL_CLEARING 16 // a clearing shut in by woods and smaller than this becomes woods
#define WOODS_THRESHOLD 168 // noise above which the wild land is wooded, lower further from the cities
#define SEA_HALF_WIDTH 12
#define ROAD_MARGIN 2
#define MAX_ROADS 8
#define MAX_SEA_POINTS 6

typedef struct {
    int x1, y1, x2, y2; // along a row or a column, ends included
} segment;

typedef struct {
    int resources[MAX_SLOT_RESOURCES]; // food and materials the player may produce, RESOURCE_NONE ends the list
    // the players of the rocks: the reservoir of Caesar that brings them the only water they have stands on the coast
    // at this column, south of the sea (1) or north of it (-1); 0 for the players of the coast
    int reservoir_column;
    int reservoir_side;
    int center_x, center_y;
    int entry_x, entry_y; // on the west or the east edge: the main road runs along the row of the city
    int corners[4]; // corners of the city for its rocks, woods and meadows: those away from the sea first
} map_slot;

struct map_layout {
    int size;
    map_slot slots[MP_MAPGEN_MAX_PLAYERS];
    int num_roads;
    segment roads[MAX_ROADS];
    int bridge_road; // the road that crosses the sea on the bridge of Caesar
    int num_sea_points;
    int sea_x[MAX_SEA_POINTS]; // middle line of the arm of the sea, from the west edge to the east edge
    int sea_y[MAX_SEA_POINTS];
    int woods_threshold; // 0: WOODS_THRESHOLD; higher on a map whose wild land is larger, for as much forest
};

// what the plan gives or refuses to each arrival point: the permissions of its city. Pig farms and wharves both give
// meat; the wharves do not depend on this permission (as in the original game), so meat here means pigs (D-065)
static const int PLAN_RESOURCES[] = {
    RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_MEAT,
    RESOURCE_IRON, RESOURCE_CLAY, RESOURCE_TIMBER, RESOURCE_OLIVES, RESOURCE_VINES, RESOURCE_MARBLE
};
#define NUM_PLAN_RESOURCES (int) (sizeof(PLAN_RESOURCES) / sizeof(PLAN_RESOURCES[0]))

// two players on the south shore: in the west the player of the rocks (iron, marble, olives, pigs), in the east on the
// coast the player of timber, clay, vines, fruit and fish; the main road of Caesar crosses the sea to the wild north
// shore
static const map_layout LAYOUT_2 = {
    200,
    {
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_OLIVES },
            75, 1, 45, 140, 0, 140, { 0, 2, 1, 3 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_TIMBER, RESOURCE_CLAY, RESOURCE_VINES },
            0, 0, 150, 90, 199, 90, { 2, 3, 0, 1 } },
    },
    4,
    { { 0, 140, 100, 140 }, { 100, 90, 199, 90 }, { 100, 90, 100, 140 }, { 100, 12, 100, 90 } },
    3,
    4,
    { 0, 60, 120, 199 },
    { 40, 44, 58, 70 }
};

// two players on each shore (D-062): in the west a player of the rocks on each shore (iron, marble, pigs; olives in
// the north, vines in the south), each with his own reservoir of Caesar on the coast; in the east a player on each
// coast (timber, clay, fruit, fish). Three players leave the south-east free: there is always a player inland and
// one on the coast.
static const map_layout LAYOUT_4 = {
    260,
    {
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_OLIVES },
            95, -1, 55, 55, 0, 55, { 0, 2, 1, 3 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_TIMBER, RESOURCE_CLAY },
            0, 0, 200, 104, 259, 104, { 0, 1, 2, 3 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_VINES },
            95, 1, 60, 210, 0, 210, { 2, 0, 3, 1 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_TIMBER, RESOURCE_CLAY },
            0, 0, 200, 156, 259, 156, { 2, 3, 0, 1 } },
    },
    7,
    {
        { 0, 55, 130, 55 }, { 130, 104, 259, 104 }, { 130, 55, 130, 104 }, { 130, 104, 130, 156 },
        { 0, 210, 130, 210 }, { 130, 156, 259, 156 }, { 130, 156, 130, 210 }
    },
    3,
    5,
    { 0, 60, 130, 200, 259 },
    { 140, 138, 132, 130, 128 }
};

// map 2 for two players (T4.14): the arm of the sea runs diagonally from the north-west to the south-east and both
// players live south-west of it. The player of the rocks (iron, marble, olives, pigs) is in the south-west corner,
// his reservoir of Caesar on the coast north-east of him; the player of the coast (timber, clay, vines, fruit, fish)
// is on the shore in the south, his arrival point on the south edge. The main road of Caesar runs along the row of
// each city and crosses the sea to the wild north-east
static const map_layout LAYOUT_2_MAP2 = {
    220,
    {
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_OLIVES },
            75, 1, 45, 180, 0, 180, { 2, 0, 3, 1 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_TIMBER, RESOURCE_CLAY, RESOURCE_VINES },
            0, 0, 140, 156, 150, 219, { 2, 3, 0, 1 } },
    },
    5,
    {
        { 0, 180, 100, 180 }, { 100, 156, 100, 180 }, { 100, 156, 150, 156 }, { 150, 156, 150, 219 },
        { 90, 40, 90, 180 }
    },
    4,
    4,
    { 0, 70, 140, 219 },
    { 40, 80, 130, 182 },
    170 // the wild north-east is half of the map: fewer woods there
};

// map 2 for four players (T4.14): a winding arm of the sea in the south. The two players of the rocks live on the
// wide north shore, joined by a main road from the west edge to the east edge (iron, marble, pigs; olives in the
// west, vines in the east), each with his reservoir of Caesar on the coast south-east of him; the two players of the
// coast live on the south shore (timber, clay, fruit, fish). Three players leave the south-east free (D-062)
static const map_layout LAYOUT_4_MAP2 = {
    240,
    {
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_OLIVES },
            95, -1, 55, 90, 0, 90, { 0, 2, 1, 3 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_TIMBER, RESOURCE_CLAY },
            0, 0, 70, 204, 0, 204, { 2, 3, 0, 1 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_MARBLE, RESOURCE_VINES },
            215, -1, 175, 90, 239, 90, { 1, 3, 0, 2 } },
        { { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_TIMBER, RESOURCE_CLAY },
            0, 0, 180, 208, 239, 208, { 3, 2, 1, 0 } },
    },
    4,
    { { 0, 90, 239, 90 }, { 0, 204, 120, 204 }, { 120, 208, 239, 208 }, { 120, 90, 120, 208 } },
    3,
    5,
    { 0, 60, 120, 180, 239 },
    { 168, 180, 166, 182, 170 }
};

// the arrival point of a player: the plan numbers them, a draw at the start of the game gives them out
static const map_slot *slot_of(int player_id)
{
    return &data.layout->slots[data.slot_of_player[player_id]];
}

// players draw their arrival points by lot, among those of the players there are: on the map for 4, three players
// always leave the south-east free, a place on the coast, so that someone lives on the rocks and someone on the coast
// (D-062). Seed 0 keeps the order of the plan (tests).
static void draw_arrival_points(int num_players, unsigned int seed)
{
    for (int p = 0; p < MP_MAPGEN_MAX_PLAYERS; p++) {
        data.slot_of_player[p] = p;
    }
    unsigned int state = seed;
    for (int i = seed ? num_players - 1 : 0; i > 0; i--) {
        state = state * 1103515245u + 12345u;
        int j = (int) ((state >> 16) % (unsigned int) (i + 1));
        int swap = data.slot_of_player[i];
        data.slot_of_player[i] = data.slot_of_player[j];
        data.slot_of_player[j] = swap;
    }
}

static int slot_has(int player_id, int resource)
{
    for (int i = 0; i < MAX_SLOT_RESOURCES; i++) {
        if (slot_of(player_id)->resources[i] == resource) {
            return 1;
        }
    }
    return 0;
}

static void place_prepared_arrivals(void)
{
    for (int p = 0; p < data.num_players; p++) {
        data.center_x[p] = slot_of(p)->center_x;
        data.center_y[p] = slot_of(p)->center_y;
        data.entry_x[p] = slot_of(p)->entry_x;
        data.entry_y[p] = slot_of(p)->entry_y;
    }
}

static int in_city_area(int x, int y)
{
    for (int p = 0; p < data.num_players; p++) {
        if (distance(x, y, data.center_x[p], data.center_y[p]) < CITY_RADIUS / 2) {
            return 1;
        }
    }
    return 0;
}

static int near_road(int x, int y, int ignored_road)
{
    for (int i = 0; i < data.layout->num_roads; i++) {
        const segment *r = &data.layout->roads[i];
        if (i == ignored_road) {
            continue;
        }
        int x_min = r->x1 < r->x2 ? r->x1 : r->x2;
        int x_max = r->x1 > r->x2 ? r->x1 : r->x2;
        int y_min = r->y1 < r->y2 ? r->y1 : r->y2;
        int y_max = r->y1 > r->y2 ? r->y1 : r->y2;
        if (x >= x_min - ROAD_MARGIN && x <= x_max + ROAD_MARGIN && y >= y_min - ROAD_MARGIN &&
            y <= y_max + ROAD_MARGIN) {
            return 1;
        }
    }
    return 0;
}

static int in_prepared_settlement(int x, int y)
{
    return in_city_area(x, y) || near_road(x, y, -1);
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
    // far from every city, forests with clearings, thicker further away, and no pond (D-055); near the cities,
    // nothing that one player may exploit and another not (D-044)
    int wild = distance_to_nearest_city(x, y) - WILD_DISTANCE;
    int threshold = data.layout->woods_threshold ? data.layout->woods_threshold : WOODS_THRESHOLD;
    if (wild > 0 && soft_noise(x, y, 1) > threshold - (wild < 20 ? wild : 20)) {
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
static void slot_corner(int player_id, int corner, int *x, int *y)
{
    int r = CITY_RADIUS / 2 + 5;
    *x = data.center_x[player_id] + (corner & 1 ? r : -r);
    *y = data.center_y[player_id] + (corner & 2 ? r : -r);
}

static void place_slot(int player_id)
{
    int x, y, corner = 0;
    // rocks inland only, woods on the coast only (D-062)
    if (slot_has(player_id, RESOURCE_IRON) || slot_has(player_id, RESOURCE_MARBLE)) {
        slot_corner(player_id, slot_of(player_id)->corners[corner++], &x, &y);
        set_blob(x, y, 4, TERRAIN_ROCK);
    }
    if (slot_has(player_id, RESOURCE_TIMBER)) {
        slot_corner(player_id, slot_of(player_id)->corners[corner++], &x, &y);
        set_blob(x, y, 6, TERRAIN_TREE);
    }
    // farms: wheat and vegetables, pigs inland or fruit on the coast, and olives or vines when allowed
    slot_corner(player_id, slot_of(player_id)->corners[corner], &x, &y);
    set_blob(x, y, 6, TERRAIN_MEADOW);
}

// the middle line of the sea at a column, between the points of the plan
static int sea_middle(int x)
{
    const map_layout *l = data.layout;
    for (int i = 1; i < l->num_sea_points; i++) {
        if (x <= l->sea_x[i]) {
            int dx = l->sea_x[i] - l->sea_x[i - 1];
            return l->sea_y[i - 1] + (l->sea_y[i] - l->sea_y[i - 1]) * (x - l->sea_x[i - 1]) / (dx > 0 ? dx : 1);
        }
    }
    return l->sea_y[l->num_sea_points - 1];
}

// the arm of the sea, which winds a little and whose coasts are not straight; it never reaches a city nor a main
// road, except the one that crosses it on the bridge of Caesar. Under the bridge, a straight channel of three tiles
// between two clear shores, as bridges need
static void place_sea(void)
{
    const map_layout *l = data.layout;
    for (int x = 0; x < data.size; x++) {
        int middle = sea_middle(x) + (soft_noise(2 * x, 1, 11) - 128) / 32;
        data.sea_top[x] = middle - SEA_HALF_WIDTH + (soft_noise(3 * x, 2, 12) - 128) / 40;
        data.sea_bottom[x] = middle + SEA_HALF_WIDTH + (soft_noise(3 * x, 3, 13) - 128) / 40;
        for (int y = data.sea_top[x]; y <= data.sea_bottom[x]; y++) {
            if (y >= 0 && y < data.size && !in_city_area(x, y) && !near_road(x, y, l->bridge_road)) {
                map_terrain_set(map_grid_offset(x, y), TERRAIN_WATER);
            }
        }
    }
    int bridge_x = l->roads[l->bridge_road].x1;
    int top = data.sea_top[bridge_x], bottom = data.sea_bottom[bridge_x];
    for (int x = bridge_x - 1; x <= bridge_x + 1; x++) {
        data.sea_top[x] = top;
        data.sea_bottom[x] = bottom;
        for (int y = top - 1; y <= bottom + 1; y++) {
            map_terrain_set(map_grid_offset(x, y), y < top || y > bottom ? 0 : TERRAIN_WATER);
        }
    }
}

// the main road of Caesar, on land: his bridge carries it over the sea
static void place_main_road(void)
{
    const map_layout *l = data.layout;
    for (int i = 0; i < l->num_roads; i++) {
        const segment *r = &l->roads[i];
        int dx = r->x2 > r->x1 ? 1 : r->x2 < r->x1 ? -1 : 0;
        int dy = r->y2 > r->y1 ? 1 : r->y2 < r->y1 ? -1 : 0;
        for (int x = r->x1, y = r->y1;; x += dx, y += dy) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
                map_terrain_set(grid_offset, TERRAIN_ROAD);
                map_owner_set(grid_offset, MAP_OWNER_CAESAR);
            }
            if (x == r->x2 && y == r->y2) {
                break;
            }
        }
    }
}

// a bridge for ships, from the south shore to the north one: ships sail on under it
static int place_caesar_bridge(void)
{
    int x = data.layout->roads[data.layout->bridge_road].x1;
    int length, direction;
    if (!map_bridge_calculate_length_direction(x, data.sea_bottom[x], &length, &direction) ||
        length != data.sea_bottom[x] - data.sea_top[x] + 1 || map_bridge_add(x, data.sea_bottom[x], 1) != length) {
        return 0;
    }
    for (int y = data.sea_top[x]; y <= data.sea_bottom[x]; y++) {
        map_owner_set(map_grid_offset(x, y), MAP_OWNER_CAESAR);
    }
    return 1;
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

// a reservoir of Caesar on the coast at this column, south of the sea (side 1) or north of it (-1)
static int place_caesar_reservoir(int column, int side, int *rx, int *ry)
{
    *rx = column - 1;
    *ry = -1;
    for (int y = side > 0 ? data.sea_bottom[column] - 1 : data.sea_top[column] - 1; y > 2 && y < data.size - 3; y += side) {
        int top = side > 0 ? y : y - 2;
        if (is_land_for_reservoir(*rx, top)) {
            *ry = top;
            break;
        }
    }
    if (*ry < 0) {
        return 0;
    }
    for (int yy = *ry; yy < *ry + 3; yy++) {
        for (int xx = *rx; xx < *rx + 3; xx++) {
            map_terrain_set(map_grid_offset(xx, yy), 0);
        }
    }
    building *reservoir = building_create_for_caesar(BUILDING_RESERVOIR, *rx, *ry);
    if (!BUILDING_IS_CAESAR(reservoir->id)) {
        return 0;
    }
    map_building_tiles_add(reservoir->id, *rx, *ry, 3, image_group(GROUP_BUILDING_RESERVOIR), TERRAIN_BUILDING);
    map_aqueduct_set(map_grid_offset(*rx, *ry), 0);
    return 1;
}

// for each player of the rocks, a reservoir of Caesar on the nearest coast and his aqueduct to the city: along a
// column away from the sea, then westwards along a row to the east side of the city (D-034, D-047, D-062). He never
// lets it dry up. The arrival points left free on the map for 4 have theirs too: the map is the same for 3 and 4.
static int place_caesar_water(void)
{
    for (int p = 0; p < data.num_players; p++) {
        data.aqueduct_end_x[p] = data.aqueduct_end_y[p] = -1;
        int side = slot_of(p)->reservoir_side;
        int rx, ry;
        if (!side) {
            continue;
        }
        if (!place_caesar_reservoir(slot_of(p)->reservoir_column, side, &rx, &ry)) {
            return 0;
        }
        // from the middle of the side of the reservoir away from the sea
        int x = rx + 1;
        int y = side > 0 ? ry + 3 : ry - 1;
        int end_x = data.center_x[p] + CITY_RADIUS / 2 + 1;
        int end_y = data.center_y[p] - 4;
        if (end_x >= x || (side > 0 ? end_y < y : end_y > y)) {
            return 0; // the plan puts the player of the rocks west of the reservoir, away from the sea
        }
        for (; y != end_y; y += side) {
            set_caesar_aqueduct(x, y);
        }
        for (; x >= end_x; x--) {
            set_caesar_aqueduct(x, y);
        }
        data.aqueduct_end_x[p] = end_x;
        data.aqueduct_end_y[p] = end_y;
    }
    return 1;
}

// every player starts with his mission, built for him beside the main road through the place of his city (D-045),
// north of it
static int place_start_mission(int p)
{
    int x = data.center_x[p] - 1, y = data.center_y[p] - 2;
    for (int yy = y; yy < y + 2; yy++) {
        for (int xx = x; xx < x + 2; xx++) {
            map_terrain_set(map_grid_offset(xx, yy), 0);
        }
    }
    building *b = building_create(BUILDING_MISSION_POST, x, y);
    if (!b->id) {
        return 0;
    }
    data.mission_x[p] = x;
    data.mission_y[p] = y;
    b->state = BUILDING_STATE_IN_USE;
    map_building_tiles_add(b->id, x, y, b->size, image_group(GROUP_BUILDING_MISSION_POST), TERRAIN_BUILDING);
    return 1;
}

// land cut off from the main road by the sea and rocks becomes rocks, so that every player reaches all the land (the
// bridge of Caesar leads to the other shore); never a pond, which would water the city of the rocks (D-055)
static void fill_cut_off_land(void)
{
    static uint8_t reached[GRID_MAX_SIZE * GRID_MAX_SIZE];
    static int queue[GRID_MAX_SIZE * GRID_MAX_SIZE];
    memset(reached, 0, sizeof(reached));
    int head = 0, tail = 0;
    queue[tail++] = data.entry_y[0] * data.size + data.entry_x[0];
    reached[queue[0]] = 1;
    while (head < tail) {
        int i = queue[head++];
        int x = i % data.size, y = i / data.size;
        static const int DX[] = { 1, -1, 0, 0 };
        static const int DY[] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            int n = ny * data.size + nx;
            if (nx < 0 || ny < 0 || nx >= data.size || ny >= data.size || reached[n]) {
                continue;
            }
            int grid_offset = map_grid_offset(nx, ny);
            if (map_terrain_is(grid_offset, TERRAIN_WATER | TERRAIN_ROCK) &&
                !map_terrain_is(grid_offset, TERRAIN_ROAD)) {
                continue;
            }
            reached[n] = 1;
            queue[tail++] = n;
        }
    }
    for (int y = 0; y < data.size; y++) {
        for (int x = 0; x < data.size; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!reached[y * data.size + x] && !map_terrain_is(grid_offset, TERRAIN_WATER | TERRAIN_ROCK)) {
                map_terrain_set(grid_offset, TERRAIN_ROCK);
            }
        }
    }
}

// where one walks: clear land, meadows and roads (woods, water, rocks and buildings stop walkers, D-052)
static int is_open_land(int grid_offset)
{
    return !map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR) || map_terrain_is(grid_offset, TERRAIN_ROAD);
}

static int is_wood(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_TREE | TERRAIN_SHRUB) &&
        !map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR & ~(TERRAIN_TREE | TERRAIN_SHRUB));
}

// clearings shut in by the woods: the small ones become woods, a path opens to the others through as few trees as
// possible, so that we go everywhere on the map while the woods stay impassable (D-052)
static void open_shut_in_clearings(void)
{
    static int cost[GRID_MAX_SIZE * GRID_MAX_SIZE]; // trees crossed from the main road, -1 not yet reached
    static int from[GRID_MAX_SIZE * GRID_MAX_SIZE];
    static int deque[2 * GRID_MAX_SIZE * GRID_MAX_SIZE];
    static const int DX[] = { 1, -1, 0, 0 };
    static const int DY[] = { 0, 0, 1, -1 };
    int tiles = data.size * data.size;

    // the small clearings first: a clearing of the main area has no cost after the walk below
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < tiles; i++) {
            cost[i] = -1;
        }
        // 0-1 walk from the entry of the first player: open land costs nothing, a tree costs one
        int head = tiles, tail = tiles;
        int start = data.entry_y[0] * data.size + data.entry_x[0];
        cost[start] = 0;
        from[start] = -1;
        deque[tail++] = start;
        while (head < tail) {
            int i = deque[head++];
            int x = i % data.size, y = i / data.size;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (nx < 0 || ny < 0 || nx >= data.size || ny >= data.size) {
                    continue;
                }
                int n = ny * data.size + nx;
                int grid_offset = map_grid_offset(nx, ny);
                int step = is_open_land(grid_offset) ? 0 : is_wood(grid_offset) ? 1 : -1;
                if (step < 0 || (cost[n] >= 0 && cost[n] <= cost[i] + step)) {
                    continue;
                }
                cost[n] = cost[i] + step;
                from[n] = i;
                if (step == 0) {
                    deque[--head] = n;
                } else {
                    deque[tail++] = n;
                }
            }
        }
        if (pass == 0) {
            // clearings out of reach of the main area without trees: too small to matter, they join the woods
            static uint8_t seen[GRID_MAX_SIZE * GRID_MAX_SIZE];
            static int component[GRID_MAX_SIZE * GRID_MAX_SIZE];
            memset(seen, 0, sizeof(seen));
            for (int i = 0; i < tiles; i++) {
                if (seen[i] || cost[i] <= 0 || !is_open_land(map_grid_offset(i % data.size, i / data.size))) {
                    continue;
                }
                int size = 0;
                component[size++] = i;
                seen[i] = 1;
                for (int k = 0; k < size; k++) {
                    int x = component[k] % data.size, y = component[k] / data.size;
                    for (int d = 0; d < 4; d++) {
                        int nx = x + DX[d], ny = y + DY[d];
                        int n = ny * data.size + nx;
                        if (nx < 0 || ny < 0 || nx >= data.size || ny >= data.size || seen[n] ||
                            !is_open_land(map_grid_offset(nx, ny))) {
                            continue;
                        }
                        seen[n] = 1;
                        component[size++] = n;
                    }
                }
                if (size < SMALL_CLEARING) {
                    for (int k = 0; k < size; k++) {
                        map_terrain_set(map_grid_offset(component[k] % data.size, component[k] / data.size),
                            TERRAIN_TREE);
                    }
                }
            }
        }
    }
    // a path to each clearing left: the trees on its cheapest way from the main area are cut
    for (int i = 0; i < tiles; i++) {
        if (cost[i] <= 0 || !is_open_land(map_grid_offset(i % data.size, i / data.size))) {
            continue;
        }
        for (int t = from[i]; t >= 0 && cost[t] > 0; t = from[t]) {
            int grid_offset = map_grid_offset(t % data.size, t / data.size);
            if (is_wood(grid_offset)) {
                map_terrain_set(grid_offset, 0);
            }
        }
    }
}

static void set_slot_permissions(int num_players)
{
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        for (int i = 0; i < NUM_PLAN_RESOURCES; i++) {
            empire_city_set_our_production_allowed(PLAN_RESOURCES[i], slot_has(p, PLAN_RESOURCES[i]));
        }
    }
    player_context_switch(0);
}

int mp_mapgen_slot_allows(int player_id, int resource)
{
    return data.prepared && slot_has(player_id, resource);
}

int mp_mapgen_slot_is_coastal(int player_id)
{
    return data.prepared && !slot_of(player_id)->reservoir_side;
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

void mp_mapgen_sea_end(int east, int *x, int *y)
{
    *x = data.prepared ? (east ? data.size - 1 : 0) : -1;
    *y = data.prepared ? (data.sea_top[*x] + data.sea_bottom[*x]) / 2 : -1;
}

void mp_mapgen_player_river_point(int player_id, int *x, int *y)
{
    mp_mapgen_sea_end(data.center_x[player_id] >= data.size / 2, x, y);
}

void mp_mapgen_caesar_bridge(int *x, int *y_north, int *y_south)
{
    *x = data.prepared ? data.layout->roads[data.layout->bridge_road].x1 : -1;
    *y_north = data.prepared ? data.sea_top[*x] : -1;
    *y_south = data.prepared ? data.sea_bottom[*x] : -1;
}

// the prepared maps (T4.14, D-069): for each, its plan for 2 players and its plan for 3 or 4
static const map_layout *const PREPARED_LAYOUTS[MP_MAPGEN_NUM_PREPARED_MAPS][2] = {
    { &LAYOUT_2, &LAYOUT_4 },
    { &LAYOUT_2_MAP2, &LAYOUT_4_MAP2 },
};

static const map_layout *prepared_layout(int map, int num_players)
{
    if (map < 0 || map >= MP_MAPGEN_NUM_PREPARED_MAPS) {
        return 0;
    }
    return PREPARED_LAYOUTS[map][num_players <= 2 ? 0 : 1];
}

int mp_mapgen_prepared_map_size(int map, int num_players)
{
    const map_layout *layout = prepared_layout(map, num_players);
    return layout ? layout->size : 0;
}

int mp_mapgen_prepared_size(int num_players)
{
    return mp_mapgen_prepared_map_size(0, num_players);
}

int mp_mapgen_choose_prepared_map(int choice, unsigned int seed)
{
    if (choice >= 0 && choice < MP_MAPGEN_NUM_PREPARED_MAPS) {
        return choice;
    }
    if (!seed) {
        return 0; // the tests keep map 1
    }
    // a draw of its own from the seed of the lobby, which also draws the arrival points
    unsigned int h = seed * 0x9e3779b1u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return (int) ((h >> 8) % MP_MAPGEN_NUM_PREPARED_MAPS);
}

static int lacks_trade_routes;

// the empire of the template must trade by land and by sea: the prepared maps offer both ways out (D-041)
static int empire_trades_by_land_and_sea(void)
{
    int land = 0, sea = 0;
    for (int i = 0; i < 41; i++) { // the empire holds 41 cities
        empire_city *c = empire_city_get(i);
        if (c && c->in_use && (c->type == EMPIRE_CITY_TRADE || c->type == EMPIRE_CITY_FUTURE_TRADE)) {
            sea += c->is_sea_trade ? 1 : 0;
            land += c->is_sea_trade ? 0 : 1;
        }
    }
    return land > 0 && sea > 0;
}

int mp_mapgen_lacks_trade_routes(void)
{
    return lacks_trade_routes;
}

// maps of the free game whose empire trades by land and by sea, those of the north first: their empire is the one
// of the forests of Britannia (D-044)
static const char *PREPARED_TEMPLATES[] = {
    "Lindum.map", "Londinium.map", "Valentia.map", "Tarraco.map", "Caesarea.map", "Cyrene.map", "Carthago.map"
};

const char *mp_mapgen_prepared_template(void)
{
    for (unsigned int i = 0; i < sizeof(PREPARED_TEMPLATES) / sizeof(PREPARED_TEMPLATES[0]); i++) {
        const char *file = dir_get_file(PREPARED_TEMPLATES[i], NOT_LOCALIZED);
        if (file) {
            return file;
        }
    }
    return 0;
}

int mp_mapgen_create_prepared(const char *template_file, int num_players, unsigned int placement_seed)
{
    return mp_mapgen_create_prepared_map(template_file, num_players, 0, placement_seed);
}

int mp_mapgen_create_prepared_map(const char *template_file, int num_players, int map, unsigned int placement_seed)
{
    lacks_trade_routes = 0;
    if (num_players < 1 || num_players > MP_MAPGEN_MAX_PLAYERS || !prepared_layout(map, num_players)) {
        return 0;
    }
    data.seed = PREPARED_SEED;
    data.layout = prepared_layout(map, num_players);
    data.size = data.layout->size;
    // three players play on the map for four, one arrival point stays free
    data.num_players = num_players <= 2 ? 2 : 4;
    data.prepared = 1;
    draw_arrival_points(num_players, placement_seed);
    if (!prepare_template(template_file, data.size)) {
        return 0;
    }
    if (!empire_trades_by_land_and_sea()) {
        lacks_trade_routes = 1;
        return 0;
    }
    // whatever the template, the cities grow in the forest
    scenario.climate = CLIMATE_NORTHERN;
    place_prepared_arrivals();
    set_terrain(prepared_terrain);
    for (int p = 0; p < data.num_players; p++) {
        place_slot(p);
    }
    place_sea();
    map_owner_clear_all();
    mp_territory_clear();
    mp_fog_clear();
    place_main_road();
    if (!place_caesar_water() || !place_caesar_bridge()) {
        return 0;
    }
    fill_cut_off_land();
    open_shut_in_clearings();
    update_tile_images();
    map_tiles_update_all_aqueducts(0);
    int west_x, west_y;
    mp_mapgen_sea_end(0, &west_x, &west_y);
    scenario_editor_set_river_entry_point(west_x, west_y);
    scenario_editor_set_river_exit_point(west_x, west_y);
    // fish in the sea off the coast of every player who lives on it, for his wharves
    int fish = 0;
    for (int p = 0; p < data.num_players; p++) {
        int fx = data.center_x[p];
        int fy = (data.sea_top[fx] + data.sea_bottom[fx]) / 2;
        if (mp_mapgen_slot_is_coastal(p) && map_terrain_is(map_grid_offset(fx, fy), TERRAIN_WATER)) {
            scenario_editor_set_fishing_point(fish++, fx, fy);
        }
    }
    // every player settles with missions (D-037), farms the food of his plan, and fishes on the coast (T4.15)
    scenario.allowed_buildings[ALLOWED_BUILDING_MISSION_POST] = 1;
    scenario.allowed_buildings[ALLOWED_BUILDING_FARMS] = 1;
    scenario.allowed_buildings[ALLOWED_BUILDING_WHARF] = 1;
    if (!add_cities(num_players)) {
        return 0;
    }
    data.num_players = num_players;
    set_slot_permissions(num_players);
    // and starts with his mission, its zone, and a missionary to found the next ones; the ships of his empire come
    // along the sea from the nearest edge
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        int river_x, river_y;
        mp_mapgen_player_river_point(p, &river_x, &river_y);
        scenario_editor_set_river_entry_point(river_x, river_y);
        scenario_editor_set_river_exit_point(river_x, river_y);
        if (!place_start_mission(p)) {
            return 0;
        }
        mp_missionary_create(0, data.center_x[p] + 3, data.center_y[p] + 3);
        mp_territory_start_city();
        mp_fog_start_city(); // the players see their land from the start
    }
    player_context_switch(0);
    figure_create_fishing_points(); // the gulls over the fish, once for everybody
    update_networks(num_players);
    return 1;
}

void mp_mapgen_start_mission(int player_id, int *x, int *y)
{
    *x = data.mission_x[player_id];
    *y = data.mission_y[player_id];
}

void mp_mapgen_city_center(int player_id, int *x, int *y)
{
    *x = data.center_x[player_id];
    *y = data.center_y[player_id];
}
