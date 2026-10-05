#include "fog.h"

#include "building/building.h"
#include "figure/figure.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "map/data.h"
#include "map/grid.h"
#include "mp/session.h"
#include "mp/territory.h"

#include <string.h>

// bit p: discovered by player p; bit 4 + p: lit for player p
static grid_u8 fog;

// light of the current city, one row of differences per map row (map coordinates)
static int16_t light[GRID_MAX_SIZE][GRID_MAX_SIZE + 1];

int mp_fog_is_active(void)
{
    return game_rules_multiplayer_map() && game_rules_fog_of_war();
}

int mp_fog_is_discovered(int grid_offset)
{
    if (!mp_fog_is_active() || grid_offset < 0) {
        return 1;
    }
    return (fog.items[grid_offset] >> mp_session_local_player_id()) & 1;
}

int mp_fog_is_lit(int grid_offset)
{
    if (!mp_fog_is_active() || grid_offset < 0) {
        return 1;
    }
    return (fog.items[grid_offset] >> (4 + mp_session_local_player_id())) & 1;
}

static void add_light(int x, int y, int size)
{
    int x_min = x - MP_FOG_RADIUS;
    int y_min = y - MP_FOG_RADIUS;
    int x_max = x + size - 1 + MP_FOG_RADIUS;
    int y_max = y + size - 1 + MP_FOG_RADIUS;
    x_min = x_min < 0 ? 0 : x_min;
    y_min = y_min < 0 ? 0 : y_min;
    x_max = x_max >= map_data.width ? map_data.width - 1 : x_max;
    y_max = y_max >= map_data.height ? map_data.height - 1 : y_max;
    for (int yy = y_min; yy <= y_max; yy++) {
        light[yy][x_min]++;
        light[yy][x_max + 1]--;
    }
}

void mp_fog_update_city(void)
{
    if (mp_fog_is_active()) {
        mp_fog_start_city();
    }
}

void mp_fog_start_city(void)
{
    for (int y = 0; y < map_data.height; y++) {
        memset(light[y], 0, sizeof(int16_t) * (map_data.width + 1));
    }
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE) {
            add_light(b->x, b->y, b->size > 0 ? b->size : 1);
        }
    }
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        figure *f = figure_get(i);
        // the gulls over the fish of the map belong to the first city: they light nothing
        if (f->state == FIGURE_STATE_ALIVE && f->type != FIGURE_FISH_GULLS) {
            add_light(f->x, f->y, 1);
        }
    }
    int player = player_context_current_player;
    uint8_t discovered = 1 << player;
    uint8_t lit = 1 << (4 + player);
    for (int y = 0; y < map_data.height; y++) {
        int count = 0;
        for (int x = 0; x < map_data.width; x++) {
            count += light[y][x];
            int grid_offset = map_grid_offset(x, y);
            uint8_t *tile = &fog.items[grid_offset];
            // the zone of the player is lit too
            if (count > 0 || mp_territory_owner(grid_offset) == player) {
                *tile |= discovered | lit;
            } else {
                *tile &= ~lit;
            }
        }
    }
}

void mp_fog_clear(void)
{
    map_grid_clear_u8(fog.items);
}

void mp_fog_save_state(buffer *buf)
{
    map_grid_save_state_u8(fog.items, buf);
}

void mp_fog_load_state(buffer *buf)
{
    map_grid_load_state_u8(fog.items, buf);
}
