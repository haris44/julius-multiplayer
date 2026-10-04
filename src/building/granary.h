#ifndef BUILDING_GRANARY_H
#define BUILDING_GRANARY_H

#include "building/building.h"
#include "core/buffer.h"
#include "map/point.h"

enum {
    GRANARY_TASK_NONE = -1,
    GRANARY_TASK_GETTING = 0
};

int building_granary_add_resource(building *granary, int resource, int is_produced);

int building_granary_remove_resource(building *granary, int resource, int amount);

int building_granary_remove_for_getting_deliveryman(building *src, building *dst, int *resource);

int building_granary_determine_worker_task(building *granary);

void building_granaries_calculate_stocks(void);

/**
 * Stocks computed every 50 ticks and not stored in saved games (game/extra_state.h)
 */
void building_granary_reset_extra_state(void);
void building_granary_save_extra_state(buffer *buf);
void building_granary_load_extra_state(buffer *buf);

int building_granary_for_storing(int x, int y, int resource, int distance_from_entry, int road_network_id,
                                 int force_on_stockpile, int *understaffed, map_point *dst);

int building_getting_granary_for_storing(int x, int y, int resource, int distance_from_entry, int road_network_id,
                                         map_point *dst);

int building_granary_for_getting(building *src, map_point *dst);

void building_granary_bless(void);

void building_granary_warehouse_curse(int big);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void building_granary_register_player_state(void);

#include "game/player_clone.h"
void building_granary_clone_fixup(const player_clone *c);

#endif // BUILDING_GRANARY_H
