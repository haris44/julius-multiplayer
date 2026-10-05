#include "missionary.h"

#include "building/building.h"
#include "building/warehouse.h"
#include "city/finance.h"
#include "city/resource.h"
#include "core/image.h"
#include "figure/action.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "mp/territory.h"

#define ACTION_WAITING FIGURE_ACTION_220_MP_MISSIONARY_WAITING
#define ACTION_WALKING FIGURE_ACTION_221_MP_MISSIONARY_WALKING

int mp_missionary_is_scout(const figure *f)
{
    return f->type == FIGURE_MISSIONARY &&
        (f->action_state == ACTION_WAITING || f->action_state == ACTION_WALKING);
}

static int is_own_living_missionary(const figure *f)
{
    return f->state == FIGURE_STATE_ALIVE && mp_missionary_is_scout(f);
}

int mp_missionary_first(void)
{
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        if (is_own_living_missionary(figure_get(i))) {
            return i;
        }
    }
    return 0;
}

int mp_mission_exists(void)
{
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->type == BUILDING_MISSION_POST &&
            (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED)) {
            return 1;
        }
    }
    return 0;
}

figure *mp_missionary_create(int mission_id, int x, int y)
{
    figure *f = figure_create(FIGURE_MISSIONARY, x, y, DIR_4_BOTTOM);
    if (!f->id) {
        return f;
    }
    f->action_state = ACTION_WAITING;
    f->building_id = mission_id;
    f->terrain_usage = TERRAIN_USAGE_ANY;
    return f;
}

void mp_missionary_action(figure *f)
{
    // he walks where soldiers walk: anywhere but water, rocks and buildings
    f->terrain_usage = TERRAIN_USAGE_ANY;
    f->use_cross_country = 0;
    figure_image_increase_offset(f, 12);
    if (f->action_state == ACTION_WALKING) {
        figure_movement_move_ticks(f, 1);
        if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
            f->action_state = ACTION_WAITING;
            figure_route_remove(f);
        } else if (f->direction == DIR_FIGURE_REROUTE) {
            figure_route_remove(f);
        }
    }
    if (f->action_state == ACTION_WAITING) {
        f->image_offset = 0;
    }
    figure_image_update(f, image_group(GROUP_FIGURE_MISSIONARY));
}

void mp_missionary_move(int figure_id, int x, int y)
{
    if (figure_id < FIGURE_FIRST || figure_id >= FIGURE_END || !map_grid_is_inside(x, y, 1)) {
        return; // only the missionaries of the player who sent the command
    }
    figure *f = figure_get(figure_id);
    if (!is_own_living_missionary(f)) {
        return;
    }
    figure_route_remove(f);
    f->destination_x = x;
    f->destination_y = y;
    f->destination_grid_offset = map_grid_offset(x, y);
    f->action_state = ACTION_WALKING;
}

int mp_missionary_is_near(int x, int y, int size)
{
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        figure *f = figure_get(i);
        if (!is_own_living_missionary(f)) {
            continue;
        }
        int dx = f->x < x ? x - f->x : f->x >= x + size ? f->x - (x + size - 1) : 0;
        int dy = f->y < y ? y - f->y : f->y >= y + size ? f->y - (y + size - 1) : 0;
        if (dx <= MP_MISSIONARY_RANGE && dy <= MP_MISSIONARY_RANGE) {
            return 1;
        }
    }
    return 0;
}

int mp_mission_allows_place(int x, int y, int size)
{
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            if (!map_grid_is_inside(x + dx, y + dy, 1)) {
                return 0;
            }
            int owner = mp_territory_owner(map_grid_offset(x + dx, y + dy));
            if (owner >= 0 && owner != player_context_current_player) {
                return 0; // never in the zone of another player
            }
        }
    }
    return mp_missionary_is_near(x, y, size);
}

int mp_mission_marble_cost(void)
{
    return mp_territory_owns_land() ? MP_MISSION_MARBLE_LOADS : 0;
}

int mp_mission_pay(void)
{
    int loads = mp_mission_marble_cost();
    if (!loads) {
        return 1;
    }
    if (city_resource_count(RESOURCE_MARBLE) < loads) {
        return 0;
    }
    building_warehouses_remove_resource(RESOURCE_MARBLE, loads);
    return 1;
}

int mp_mission_missionary(int mission_id)
{
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        figure *f = figure_get(i);
        if (is_own_living_missionary(f) && f->building_id == mission_id) {
            return i;
        }
    }
    return 0;
}

void mp_mission_train_missionary(int mission_id)
{
    if (mission_id < BUILDING_FIRST || mission_id >= BUILDING_END) {
        return;
    }
    building *b = building_get(mission_id);
    if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_MISSION_POST || mp_mission_missionary(mission_id) ||
        city_finance_treasury() < MP_MISSIONARY_TRAINING_COST) {
        return;
    }
    city_finance_process_construction(MP_MISSIONARY_TRAINING_COST);
    // he comes out at the front corner of the mission
    mp_missionary_create(mission_id, b->x + b->size, b->y + b->size);
}
