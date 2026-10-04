#ifndef BUILDING_MAINTENANCE_H
#define BUILDING_MAINTENANCE_H

#include "core/buffer.h"

void building_maintenance_update_fire_direction(void);
void building_maintenance_update_burning_ruins(void);
void building_maintenance_check_fire_collapse(void);
int building_maintenance_get_closest_burning_ruin(int x, int y, int *distance);

void building_maintenance_check_rome_access(void);

/**
 * State not stored in classic saved games: the fire spread direction
 */
void building_maintenance_reset_extra_state(void);
void building_maintenance_save_extra_state(buffer *buf);
void building_maintenance_load_extra_state(buffer *buf);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void building_maintenance_register_player_state(void);

#endif // BUILDING_MAINTENANCE_H
