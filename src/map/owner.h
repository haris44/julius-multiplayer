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
 * Caesar, the neutral owner of the roads and aqueducts placed by a multiplayer map (doc/mp/DECISIONS.md D-034):
 * he never plays, nobody may clear what he owns, every city may use it. Not a player id.
 */
#define MAP_OWNER_CAESAR 7

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

/**
 * Ticks of the simulation run: infrastructure that disappears frees its tile. Outside ticks and commands, terrain
 * changes come from the interface (construction previews) and never touch ownership.
 */
void map_owner_set_simulating(int simulating);
int map_owner_is_simulating(void);

void map_owner_save_state(buffer *buf);

void map_owner_load_state(buffer *buf);

#endif // MAP_OWNER_H
