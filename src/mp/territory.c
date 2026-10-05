#include "territory.h"

#include "building/building.h"
#include "building/destruction.h"
#include "city/warning.h"
#include "mp/audit.h"
#include "translation/translation.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "map/data.h"
#include "map/grid.h"
#include "mp/missionary.h"

#include <string.h>

// player id + 1, 0 when nobody got the tile
static grid_u8 territory;

// a building left outside the zone of its player collapses after this many days (3 months), unless covered again
#define GRACE_DAYS 48

// days each building of each city has been outside its zone
static uint8_t days_outside[PLAYER_CONTEXT_MAX_PLAYERS][MAX_BUILDINGS];

// coverage of the current city, one row of differences per map row (map coordinates)
static int16_t coverage[GRID_MAX_SIZE][GRID_MAX_SIZE + 1];

int mp_territory_is_active(void)
{
    return player_context_num_players() > 1 && game_rules_territories();
}

int mp_territory_owner(int grid_offset)
{
    return territory.items[grid_offset] - 1;
}

static int needs_zone(building_type type)
{
    switch (type) {
        case BUILDING_ROAD:
        case BUILDING_AQUEDUCT:
        case BUILDING_WALL:
        case BUILDING_LOW_BRIDGE:
        case BUILDING_SHIP_BRIDGE:
        case BUILDING_PLAZA:
        case BUILDING_CLEAR_LAND:
            return 0;
        default:
            return 1;
    }
}

int mp_territory_allows_tile(building_type type, int grid_offset)
{
    if (!mp_territory_is_active() || !needs_zone(type)) {
        return 1;
    }
    return mp_territory_owner(grid_offset) == player_context_current_player;
}

int mp_territory_allows_building(building_type type, int x, int y, int size)
{
    if (!mp_territory_is_active() || !needs_zone(type)) {
        return 1;
    }
    if (type == BUILDING_MISSION_POST) {
        return mp_mission_allows_place(x, y, size); // missions open a zone near a missionary (D-037)
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            if (!map_grid_is_inside(x + dx, y + dy, 1) ||
                mp_territory_owner(map_grid_offset(x + dx, y + dy)) != player_context_current_player) {
                return 0;
            }
        }
    }
    return 1;
}

// a settled building gives a zone: an inhabited house, a building with employees, a mission
static int gives_zone(const building *b)
{
    if (b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    if (b->type == BUILDING_MISSION_POST) {
        return 1;
    }
    if (b->house_size) {
        return b->house_population > 0;
    }
    return b->num_workers > 0;
}

static void add_coverage(int x_min, int y_min, int x_max, int y_max)
{
    x_min = x_min < 0 ? 0 : x_min;
    y_min = y_min < 0 ? 0 : y_min;
    x_max = x_max >= map_data.width ? map_data.width - 1 : x_max;
    y_max = y_max >= map_data.height ? map_data.height - 1 : y_max;
    for (int y = y_min; y <= y_max; y++) {
        coverage[y][x_min]++;
        coverage[y][x_max + 1]--;
    }
}

static void check_buildings_outside(void);

void mp_territory_update_city(void)
{
    if (!mp_territory_is_active()) {
        return;
    }
    for (int y = 0; y < map_data.height; y++) {
        memset(coverage[y], 0, sizeof(int16_t) * (map_data.width + 1));
    }
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (gives_zone(b)) {
            int size = b->size > 0 ? b->size : 1;
            add_coverage(b->x - MP_TERRITORY_RADIUS, b->y - MP_TERRITORY_RADIUS,
                b->x + size - 1 + MP_TERRITORY_RADIUS, b->y + size - 1 + MP_TERRITORY_RADIUS);
        }
    }
    uint8_t own = player_context_current_player + 1;
    for (int y = 0; y < map_data.height; y++) {
        int covered = 0;
        for (int x = 0; x < map_data.width; x++) {
            covered += coverage[y][x];
            uint8_t *tile = &territory.items[map_grid_offset(x, y)];
            if (covered > 0 && !*tile) {
                *tile = own; // the first player keeps it
            } else if (covered <= 0 && *tile == own) {
                *tile = 0;
            }
        }
    }
    check_buildings_outside();
}

void mp_territory_reset_extra_state(void)
{
    memset(days_outside, 0, sizeof(days_outside));
}

void mp_territory_save_extra_state(buffer *buf)
{
    buffer_write_raw(buf, days_outside[player_context_current_player], MAX_BUILDINGS);
}

void mp_territory_load_extra_state(buffer *buf)
{
    buffer_read_raw(buf, days_outside[player_context_current_player], MAX_BUILDINGS);
}

// buildings outside the zone of their player: a warning, then they collapse (D-036)
static void check_buildings_outside(void)
{
    uint8_t *days = days_outside[player_context_current_player];
    int warn = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        uint8_t *d = &days[BUILDING_LOCAL_ID(i)];
        if (b->state != BUILDING_STATE_IN_USE || mp_territory_owner(b->grid_offset) == player_context_current_player ||
            b->type == BUILDING_MISSION_POST) {
            *d = 0;
            continue;
        }
        (*d)++;
        if (*d == 1 || *d == GRACE_DAYS - 16) {
            warn = 1; // when it happens, and one month before the collapse
        }
        if (*d >= GRACE_DAYS) {
            *d = 0;
            mp_audit_effect(i, "outside territory");
            building_destroy_by_collapse(b);
        }
    }
    if (warn) {
        city_warning_show_custom(translation_for(TR_MP_BUILDINGS_OUTSIDE_TERRITORY));
    }
}

int mp_territory_owns_land(void)
{
    uint8_t own = player_context_current_player + 1;
    for (int y = 0; y < map_data.height; y++) {
        for (int x = 0; x < map_data.width; x++) {
            if (territory.items[map_grid_offset(x, y)] == own) {
                return 1;
            }
        }
    }
    return 0;
}

void mp_territory_clear(void)
{
    map_grid_clear_u8(territory.items);
}

void mp_territory_save_state(buffer *buf)
{
    map_grid_save_state_u8(territory.items, buf);
}

void mp_territory_load_state(buffer *buf)
{
    map_grid_load_state_u8(territory.items, buf);
}
