#ifndef MAP_OWNER_H
#define MAP_OWNER_H

#include "core/buffer.h"

/**
 * @file
 * Owner of the infrastructure built on the terrain (roads, walls, aqueducts, gardens, plazas,
 * bridges), which are not buildings: doc/mp/DECISIONS.md D-018.
 * Tiles nobody claimed belong to player 0, so classic games never need this grid.
 */

#define MAP_OWNER_NONE -1

/**
 * @return Player owning the tile; player 0 when nobody claimed it
 */
int map_owner_get(int grid_offset);

/**
 * @return Player who claimed the tile, or MAP_OWNER_NONE
 */
int map_owner_get_claimed(int grid_offset);

void map_owner_set(int grid_offset, int player_id);

void map_owner_clear_all(void);

/**
 * Player whose construction commands run (mp_command_execute): only they claim free tiles, never the
 * updates of the whole map that run in the turn of a city
 * @param player_id Player, or MAP_OWNER_NONE once the command is done
 */
void map_owner_set_builder(int player_id);

int map_owner_builder(void);

void map_owner_save_state(buffer *buf);

void map_owner_load_state(buffer *buf);

#endif // MAP_OWNER_H
