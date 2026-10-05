#include "extra_state.h"

#include "building/count.h"
#include "building/granary.h"
#include "building/list.h"
#include "building/maintenance.h"
#include "city/labor.h"
#include "core/random.h"
#include "map/grid.h"
#include "map/image_context.h"
#include "map/point.h"
#include "map/soldier_strength.h"
#include "map/water_supply.h"
#include "mp/territory.h"
#include "mp/trade.h"

#include "core/log.h"

#include <stdlib.h>

// grids of the extra state (soldier strength, water ranges), with room to spare
#define EXTRA_STATE_GRIDS 4

void game_extra_state_reset(void)
{
    random_reset();
    building_count_reset_extra_state();
    building_maintenance_reset_extra_state();
    map_image_context_init();
    map_soldier_strength_clear();
    map_point_reset_last_result();
    building_list_reset_extra_state();
    city_labor_reset_extra_state();
    building_granary_reset_extra_state();
    map_water_supply_reset_extra_state();
    mp_territory_reset_extra_state();
    mp_trade_reset_extra_state();
}

void game_extra_state_save(buffer *buf)
{
    random_save_extra_state(buf);
    building_count_save_extra_state(buf);
    building_maintenance_save_extra_state(buf);
    map_image_context_save_state(buf);
    map_soldier_strength_save_state(buf);
    map_point_save_state(buf);
    building_list_save_extra_state(buf);
    city_labor_save_extra_state(buf);
    building_granary_save_extra_state(buf);
    map_water_supply_save_extra_state(buf);
    mp_territory_save_extra_state(buf);
    mp_trade_save_extra_state(buf);
}

void game_extra_state_load(buffer *buf)
{
    random_load_extra_state(buf);
    building_count_load_extra_state(buf);
    building_maintenance_load_extra_state(buf);
    map_image_context_load_state(buf);
    map_soldier_strength_load_state(buf);
    map_point_load_state(buf);
    building_list_load_extra_state(buf);
    city_labor_load_extra_state(buf);
    building_granary_load_extra_state(buf);
    map_water_supply_load_extra_state(buf);
    mp_territory_load_extra_state(buf);
    mp_trade_load_extra_state(buf);
}

int game_extra_state_size(void)
{
    // depends on the size of the grid (soldier strength): measured on every call
    static void *data;
    static int capacity;
    int needed = 64 * 1024 + EXTRA_STATE_GRIDS * GRID_MAX_TILES;
    if (capacity < needed) {
        free(data);
        data = malloc(needed);
        capacity = data ? needed : 0;
        if (!data) {
            return 0;
        }
    }
    buffer buf;
    buffer_init(&buf, data, capacity);
    game_extra_state_save(&buf);
    if (buf.overflow) {
        log_error("Extra state larger than its buffer: raise EXTRA_STATE_GRIDS", 0, 0);
    }
    return buf.index;
}
