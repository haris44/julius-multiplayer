#include "siege.h"

#include "building/building.h"
#include "building/destruction.h"
#include "game/player_context.h"
#include "game/time.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/owner.h"
#include "map/property.h"
#include "map/routing.h"
#include "map/terrain.h"
#include "mp/war.h"

// damage of the original game (figure/movement.c, invaders): a gatehouse collapses past 150, any building or
// aqueduct past 10; one point every four ticks for each soldier attacking it
#define GATEHOUSE_MAX_DAMAGE 150
#define AQUEDUCT_MAX_DAMAGE 10

// the player whose soldier is being routed: set during mp_siege_soldier_can_travel only, never between two calls
static int routed_player;

// the aqueduct tiles visited by the search of a network, cleared after each search
static uint8_t visited[GRID_MAX_TILES];
static int network[GRID_MAX_TILES];

static int is_soldier(const figure *f)
{
    return f->type == FIGURE_FORT_LEGIONARY || f->type == FIGURE_FORT_JAVELIN || f->type == FIGURE_FORT_MOUNTED;
}

int mp_siege_is_soldier_at_war(const figure *f)
{
    if (!mp_war_any_fighting() || !is_soldier(f)) {
        return 0;
    }
    int self = FIGURE_OWNER(f->id);
    for (int p = 0; p < player_context_num_players(); p++) {
        if (mp_war_is_fighting(self, p)) {
            return 1;
        }
    }
    return 0;
}

// the gatehouse on the tile when it belongs to a player fighting `player_id`, else 0 (towers too have the terrain of
// gatehouses: never one)
static int gatehouse_of_enemy(int player_id, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        return 0;
    }
    int building_id = map_building_at(grid_offset);
    if (building_id <= 0) {
        return 0;
    }
    const building *b = building_get(building_id);
    if (b->type != BUILDING_GATEHOUSE || b->state != BUILDING_STATE_IN_USE ||
        !mp_war_is_fighting(player_id, BUILDING_OWNER(building_id))) {
        return 0;
    }
    return building_id;
}

static int is_closed_to_routed_soldier(int grid_offset)
{
    return gatehouse_of_enemy(routed_player, grid_offset) != 0;
}

int mp_siege_soldier_can_travel(const figure *f)
{
    // as the invaders of the original game: first a way that breaks nothing, then through what stands in the way
    routed_player = FIGURE_OWNER(f->id);
    if (map_routing_citizen_can_travel_over_land_except(f->x, f->y, f->destination_x, f->destination_y,
            is_closed_to_routed_soldier)) {
        return 1;
    }
    return map_routing_citizen_can_travel_over_land(f->x, f->y, f->destination_x, f->destination_y);
}

// one point of damage every four ticks; past `max_damage` it collapses in the city of its owner (his peace rating,
// his sentries), as in mp_war_soldier_attack_buildings
static void damage(int grid_offset, int max_damage, int victim)
{
    if (!(game_time_tick() & 3) && map_building_damage_increase(grid_offset) > max_damage) {
        int previous = player_context_current();
        player_context_switch(victim);
        building_destroy_by_enemy(map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset), grid_offset);
        player_context_switch(previous);
    }
}

int mp_siege_soldier_breaks_gate(figure *f, int grid_offset)
{
    if (!mp_war_any_fighting() || !is_soldier(f)) {
        return 0;
    }
    int gatehouse_id = gatehouse_of_enemy(FIGURE_OWNER(f->id), grid_offset);
    if (!gatehouse_id) {
        return 0;
    }
    damage(grid_offset, GATEHOUSE_MAX_DAMAGE, BUILDING_OWNER(gatehouse_id));
    return 1;
}

// whether the aqueduct tile joins a reservoir of the player on this tile: the middle of a side, as the water flows in
// (map/water_supply.c)
static int joins_reservoir_of(int player_id, int grid_offset)
{
    int building_id = map_building_at(grid_offset);
    if (building_id <= 0 || BUILDING_OWNER(building_id) != player_id) {
        return 0;
    }
    const building *b = building_get(building_id);
    if (b->type != BUILDING_RESERVOIR || b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    int xy = map_property_multi_tile_xy(grid_offset);
    return xy != EDGE_X0Y0 && xy != EDGE_X2Y0 && xy != EDGE_X0Y2 && xy != EDGE_X2Y2;
}

// whether the aqueduct of Caesar on the tile waters the player: following the aqueducts that carry his water
// (Caesar's, his own, those nobody claimed: map/water_supply.c), it joins a reservoir of his
static int caesar_aqueduct_waters(int grid_offset, int player_id)
{
    int head = 0;
    int tail = 0;
    int found = 0;
    network[tail++] = grid_offset;
    visited[grid_offset] = 1;
    while (head < tail && !found) {
        int offset = network[head++];
        for (int dir = 0; dir < 8 && !found; dir += 2) {
            int next = offset + map_grid_direction_delta(dir);
            if (!map_grid_is_valid_offset(next) || visited[next]) {
                continue;
            }
            if (joins_reservoir_of(player_id, next)) {
                found = 1;
            } else if (map_terrain_is(next, TERRAIN_AQUEDUCT)) {
                int owner = map_owner_get_claimed(next);
                if (owner == MAP_OWNER_CAESAR || owner == MAP_OWNER_NONE || owner == player_id) {
                    visited[next] = 1;
                    network[tail++] = next;
                }
            }
        }
    }
    for (int i = 0; i < tail; i++) {
        visited[network[i]] = 0;
    }
    return found;
}

// the player fighting `self` whose water the aqueduct on the tile carries: its owner, or for an aqueduct of Caesar
// the first player fighting `self` it waters; -1 when none. Never an aqueduct over a road: the road under it would go
// with it (the invaders of the original game walk over these)
static int aqueduct_victim(int self, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_AQUEDUCT) || map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        return -1;
    }
    int owner = map_owner_get_claimed(grid_offset);
    if (owner == MAP_OWNER_CAESAR) {
        for (int p = 0; p < player_context_num_players(); p++) {
            if (mp_war_is_fighting(self, p) && caesar_aqueduct_waters(grid_offset, p)) {
                return p;
            }
        }
        return -1;
    }
    return owner != MAP_OWNER_NONE && mp_war_is_fighting(self, owner) ? owner : -1;
}

int mp_siege_soldier_attack_aqueduct(figure *f)
{
    if (!mp_war_any_fighting() || !is_soldier(f)) {
        return 0;
    }
    int self = FIGURE_OWNER(f->id);
    for (int dir = 0; dir < 8; dir++) {
        int grid_offset = f->grid_offset + map_grid_direction_delta(dir);
        if (!map_grid_is_valid_offset(grid_offset)) {
            continue;
        }
        int victim = aqueduct_victim(self, grid_offset);
        if (victim >= 0) {
            damage(grid_offset, AQUEDUCT_MAX_DAMAGE, victim);
            return 1;
        }
    }
    return 0;
}
