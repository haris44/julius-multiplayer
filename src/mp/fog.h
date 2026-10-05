#ifndef MP_FOG_H
#define MP_FOG_H

#include "core/buffer.h"

/**
 * @file
 * Fog of war (doc/mp/DECISIONS.md D-038), an option of the lobby. Everything a player owns (buildings, walkers,
 * soldiers, missionaries, his zone) lights MP_FOG_RADIUS tiles around it. A tile once discovered stays discovered:
 * its land and buildings show, but what moves on it only while it is lit.
 * Computed once a day by the simulation, the same on every computer; the computers only show the land of their
 * own player. Everybody computes everything: a cheater could see through the fog, which is accepted among friends.
 */

#define MP_FOG_RADIUS 20

/**
 * Whether the fog applies: a multiplayer game with the fog rule
 */
int mp_fog_is_active(void);

/**
 * Whether the local player has discovered the tile (always when the fog does not apply)
 */
int mp_fog_is_discovered(int grid_offset);

/**
 * Whether the local player sees what moves on the tile now (always when the fog does not apply)
 */
int mp_fog_is_lit(int grid_offset);

/**
 * Daily update of what the current city discovers and lights
 */
void mp_fog_update_city(void);

void mp_fog_clear(void);

void mp_fog_save_state(buffer *buf);

void mp_fog_load_state(buffer *buf);

#endif // MP_FOG_H
