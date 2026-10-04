#ifndef CITY_DATA_H
#define CITY_DATA_H

#include "core/buffer.h"

void city_data_init(void);

void city_data_init_scenario(void);

void city_data_init_campaign_mission(void);

void city_data_save_state(buffer *main, buffer *faction, buffer *faction_unknown, buffer *graph_order,
                          buffer *entry_exit_xy, buffer *entry_exit_grid_offset);

void city_data_load_state(buffer *main, buffer *faction, buffer *faction_unknown, buffer *graph_order,
                          buffer *entry_exit_xy, buffer *entry_exit_grid_offset);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void city_data_register_player_state(void);

/**
 * Moves the grid offsets stored in the city data on a grid of another size (doc/mp/ROADMAP.md M3.3)
 */
void city_data_relocate_grid_offsets(int (*remap)(int grid_offset));

#endif // CITY_DATA_H
