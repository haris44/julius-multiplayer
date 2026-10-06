#include "water_supply.h"

#include "building/building.h"
#include "building/list.h"
#include "core/image.h"
#include "map/aqueduct.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/desirability.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/owner.h"
#include "map/property.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "game/player_context.h"
#include "game/rules.h"

#include <string.h>

#define OFFSET(x,y) (x + GRID_SIZE * y)

#define MAX_QUEUE 1000

// grid side only known at run time: up, right, down, left
#define ADJACENT_OFFSETS(i) ((i) == 0 ? -GRID_SIZE : (i) == 1 ? 1 : (i) == 2 ? GRID_SIZE : -1)

static struct {
    int items[MAX_QUEUE];
    int head;
    int tail;
} queue;

// With several cities, the water ranges of each city: the range bits of the terrain are the union of all
// cities (saved games, overlays), but a city only gets water from its own fountains and reservoirs (D-018)
static grid_u8 ranges[PLAYER_CONTEXT_MAX_PLAYERS];

// With several cities, the aqueducts reached by the water of the city being computed, cleared at the start of each
// city's turn: the aqueduct grid keeps the water of the cities already computed this day, which must not stop the
// water of the next city (the aqueduct of Caesar is the same for all). Never saved: empty between two turns (D-064)
static grid_u8 reached;

// With several cities, reservoirs hold water (D-035): cut off from their source, they still serve their fountains
// and baths until they are empty; joined again, they fill. One update a day: at the normal speed (90%), a day lasts
// about 1.1 second, so a full reservoir lasts about 5 minutes and fills in about 1 minute.
#define RESERVOIR_LEVEL_FULL 270
#define RESERVOIR_FILL_PER_DAY 5
static uint16_t reservoir_levels[PLAYER_CONTEXT_MAX_PLAYERS][MAX_BUILDINGS];

static int several_cities(void)
{
    return game_rules_multiplayer_map();
}

static int range_bits(int terrain)
{
    return (terrain & TERRAIN_FOUNTAIN_RANGE ? 1 : 0) | (terrain & TERRAIN_RESERVOIR_RANGE ? 2 : 0);
}

static void add_range(int x, int y, int size, int radius, int terrain)
{
    map_terrain_add_with_radius(x, y, size, radius, terrain);
    if (several_cities()) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);
        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                ranges[player_context_current_player].items[map_grid_offset(xx, yy)] |= range_bits(terrain);
            }
        }
    }
}

int map_water_supply_has_range(int grid_offset, int terrain)
{
    if (several_cities()) {
        return (ranges[player_context_current_player].items[grid_offset] & range_bits(terrain)) != 0;
    }
    return map_terrain_is(grid_offset, terrain);
}

int map_water_supply_has_range_in_area(int x, int y, int size, int terrain)
{
    if (!several_cities()) {
        return map_terrain_exists_tile_in_area_with_type(x, y, size, terrain);
    }
    for (int yy = y; yy < y + size; yy++) {
        for (int xx = x; xx < x + size; xx++) {
            if (map_grid_is_inside(xx, yy, 1) && map_water_supply_has_range(map_grid_offset(xx, yy), terrain)) {
                return 1;
            }
        }
    }
    return 0;
}

void map_water_supply_init_ranges_from_terrain(int x_min, int y_min, int x_max, int y_max)
{
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            ranges[player_context_current_player].items[grid_offset] = range_bits(map_terrain_get(grid_offset));
        }
    }
}

void map_water_supply_reset_extra_state(void)
{
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        map_grid_clear_u8(ranges[p].items);
    }
    memset(reservoir_levels, 0, sizeof(reservoir_levels));
}

void map_water_supply_save_extra_state(buffer *buf)
{
    map_grid_save_state_u8(ranges[player_context_current_player].items, buf);
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        buffer_write_u16(buf, reservoir_levels[player_context_current_player][i]);
    }
}

void map_water_supply_load_extra_state(buffer *buf)
{
    map_grid_load_state_u8(ranges[player_context_current_player].items, buf);
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        reservoir_levels[player_context_current_player][i] = buffer_read_u16(buf);
    }
}

int map_water_supply_reservoir_level(int building_id)
{
    return reservoir_levels[BUILDING_OWNER(building_id) % PLAYER_CONTEXT_MAX_PLAYERS][BUILDING_LOCAL_ID(building_id)];
}

int map_water_supply_reservoir_level_full(void)
{
    return RESERVOIR_LEVEL_FULL;
}

// has_water_access of the reservoirs of the city now says whether water flows in: the level follows, and the
// reservoir has water as long as some is left
static void update_reservoir_levels(void)
{
    uint16_t *levels = reservoir_levels[player_context_current_player];
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        uint16_t *level = &levels[BUILDING_LOCAL_ID(i)];
        if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_RESERVOIR) {
            *level = 0; // a new reservoir starts empty
            continue;
        }
        if (b->has_water_access) {
            *level = *level + RESERVOIR_FILL_PER_DAY > RESERVOIR_LEVEL_FULL ?
                RESERVOIR_LEVEL_FULL : *level + RESERVOIR_FILL_PER_DAY;
        } else if (*level > 0) {
            (*level)--;
        }
        b->has_water_access = *level > 0;
    }
}

static void mark_well_access(int well_id, int radius)
{
    building *well = building_get(well_id);
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(well->x, well->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int building_id = map_building_at(map_grid_offset(xx, yy));
            if (building_id && BUILDING_OWNER(building_id) == player_context_current_player) {
                building_get(building_id)->has_well_access = 1;
            }
        }
    }
}

void map_water_supply_update_houses(void)
{
    building_list_small_clear();
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        if (b->type == BUILDING_WELL) {
            building_list_small_add(i);
        } else if (b->house_size) {
            b->has_water_access = 0;
            b->has_well_access = 0;
            if (map_water_supply_has_range_in_area(b->x, b->y, b->size, TERRAIN_FOUNTAIN_RANGE)) {
                b->has_water_access = 1;
            }
        }
    }
    int total_wells = building_list_small_size();
    const int *wells = building_list_small_items();
    for (int i = 0; i < total_wells; i++) {
        mark_well_access(wells[i], 2);
    }
}

static void set_all_aqueducts_to_no_water(void)
{
    int image_without_water = image_group(GROUP_BUILDING_AQUEDUCT_NO_WATER);
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
                map_aqueduct_set(grid_offset, 0);
                int image_id = map_image_at(grid_offset);
                if (image_id < image_without_water) {
                    map_image_set(grid_offset, image_id + 15);
                }
            }
        }
    }
}

static void fill_aqueducts_from_offset(int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        return;
    }
    memset(&queue, 0, sizeof(queue));
    int guard = 0;
    int next_offset;
    int multiple = several_cities();
    int image_without_water = image_group(GROUP_BUILDING_AQUEDUCT_NO_WATER);
    do {
        if (++guard >= GRID_SIZE * GRID_SIZE) {
            break;
        }
        map_aqueduct_set(grid_offset, 1);
        if (multiple) {
            reached.items[grid_offset] = 1;
        }
        int image_id = map_image_at(grid_offset);
        if (image_id >= image_without_water) {
            map_image_set(grid_offset, image_id - 15);
        }
        next_offset = -1;
        for (int i = 0; i < 4; i++) {
            int new_offset = grid_offset + ADJACENT_OFFSETS(i);
            building *b = building_get(map_building_at(new_offset));
            int owner = map_owner_get_claimed(new_offset);
            if (owner != MAP_OWNER_NONE && owner != player_context_current_player && owner != MAP_OWNER_CAESAR) {
                continue; // the aqueducts of another city do not carry the water of this one; those of Caesar do
            }
            if (b->id && b->type == BUILDING_RESERVOIR && BUILDING_IS_OWN(b->id)) {
                // check if aqueduct connects to reservoir --> doesn't connect to corner
                int xy = map_property_multi_tile_xy(new_offset);
                if (xy != EDGE_X0Y0 && xy != EDGE_X2Y0 && xy != EDGE_X0Y2 && xy != EDGE_X2Y2) {
                    if (!b->has_water_access) {
                        b->has_water_access = 2;
                    }
                }
            } else if (map_terrain_is(new_offset, TERRAIN_AQUEDUCT)) {
                if (multiple ? !reached.items[new_offset] : !map_aqueduct_at(new_offset)) {
                    if (next_offset == -1) {
                        next_offset = new_offset;
                    } else {
                        queue.items[queue.tail++] = new_offset;
                        if (queue.tail >= MAX_QUEUE) {
                            queue.tail = 0;
                        }
                    }
                }
            }
        }
        if (next_offset == -1) {
            if (queue.head == queue.tail) {
                return;
            }
            next_offset = queue.items[queue.head++];
            if (queue.head >= MAX_QUEUE) {
                queue.head = 0;
            }
        }
        grid_offset = next_offset;
    } while (next_offset > -1);
}

void map_water_supply_clear(void)
{
    map_terrain_remove_all(TERRAIN_FOUNTAIN_RANGE | TERRAIN_RESERVOIR_RANGE);
    // reservoirs
    set_all_aqueducts_to_no_water();
}

void map_water_supply_update_reservoir_fountain_of_city(void)
{
    if (several_cities()) {
        map_grid_clear_u8(ranges[player_context_current_player].items);
        map_grid_clear_u8(reached.items);
    }
    building_list_large_clear(1);
    // mark reservoirs next to water
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_RESERVOIR) {
            building_list_large_add(i);
            if (map_terrain_exists_tile_in_area_with_type(b->x - 1, b->y - 1, 5, TERRAIN_WATER)) {
                b->has_water_access = 2;
            } else {
                b->has_water_access = 0;
            }
        }
    }
    if (several_cities()) {
        // the reservoirs of Caesar fill his aqueduct for every city (D-034)
        for (int i = BUILDING_CAESAR_FIRST; i < BUILDING_CAESAR_END; i++) {
            building *b = building_get(i);
            if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_RESERVOIR) {
                building_list_large_add(i);
                b->has_water_access =
                    map_terrain_exists_tile_in_area_with_type(b->x - 1, b->y - 1, 5, TERRAIN_WATER) ? 2 : 0;
            }
        }
    }
    int total_reservoirs = building_list_large_size();
    const int *reservoirs = building_list_large_items();
    // fill reservoirs from full ones
    int changed = 1;
    const int CONNECTOR_OFFSETS[] = {OFFSET(1,-1), OFFSET(3,1), OFFSET(1,3), OFFSET(-1,1)};
    while (changed == 1) {
        changed = 0;
        for (int i = 0; i < total_reservoirs; i++) {
            building *b = building_get(reservoirs[i]);
            if (b->has_water_access == 2) {
                b->has_water_access = 1;
                changed = 1;
                for (int d = 0; d < 4; d++) {
                    fill_aqueducts_from_offset(b->grid_offset + CONNECTOR_OFFSETS[d]);
                }
            }
        }
    }
    if (several_cities()) {
        update_reservoir_levels();
    }
    // mark reservoir ranges
    for (int i = 0; i < total_reservoirs; i++) {
        building *b = building_get(reservoirs[i]);
        if (b->has_water_access) {
            add_range(b->x, b->y, 3, 10, TERRAIN_RESERVOIR_RANGE);
        }
    }
    // fountains
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_FOUNTAIN) {
            continue;
        }
        int des = map_desirability_get(b->grid_offset);
        int image_id;
        if (des > 60) {
            image_id = image_group(GROUP_BUILDING_FOUNTAIN_4);
        } else if (des > 40) {
            image_id = image_group(GROUP_BUILDING_FOUNTAIN_3);
        } else if (des > 20) {
            image_id = image_group(GROUP_BUILDING_FOUNTAIN_2);
        } else {
            image_id = image_group(GROUP_BUILDING_FOUNTAIN_1);
        }
        map_building_tiles_add(i, b->x, b->y, 1, image_id, TERRAIN_BUILDING);
        if (map_water_supply_has_range(b->grid_offset, TERRAIN_RESERVOIR_RANGE) && b->num_workers) {
            b->has_water_access = 1;
            add_range(b->x, b->y, 1,
                scenario_property_climate() == CLIMATE_DESERT ? 3 : 4,
                TERRAIN_FOUNTAIN_RANGE);
        } else {
            b->has_water_access = 0;
        }
    }
}

int map_water_supply_is_well_unnecessary(int well_id, int radius)
{
    building *well = building_get(well_id);
    int num_houses = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(well->x, well->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            int building_id = map_building_at(grid_offset);
            if (building_id && BUILDING_IS_OWN(building_id) && building_get(building_id)->house_size) {
                num_houses++;
                if (!map_water_supply_has_range(grid_offset, TERRAIN_FOUNTAIN_RANGE)) {
                    return WELL_NECESSARY;
                }
            }
        }
    }
    return num_houses ? WELL_UNNECESSARY_FOUNTAIN : WELL_UNNECESSARY_NO_HOUSES;
}

void map_water_supply_update_reservoir_fountain(void)
{
    map_water_supply_clear();
    map_water_supply_update_reservoir_fountain_of_city();
}

