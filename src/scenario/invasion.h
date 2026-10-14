#ifndef SCENARIO_INVASION_H
#define SCENARIO_INVASION_H

#include "core/buffer.h"

void scenario_invasion_clear(void);
void scenario_invasion_init(void);

int scenario_invasion_exists_upcoming(void);

void scenario_invasion_foreach_warning(void (*callback)(int x, int y, int image_id));

int scenario_invasion_count(void);

int scenario_invasion_start_from_mars(void);

int scenario_invasion_start_from_caesar(int size);

/**
 * Multiplayer: Caesar's punitive expedition lands in the current city (M10.6, D-086), his legions of the original
 * invasion, with exactly this number of soldiers (at most 150): the difficulty does not change it
 * @return 1 when the army landed
 */
int scenario_invasion_start_caesar_expedition(int soldiers);

void scenario_invasion_start_from_cheat(void);

void scenario_invasion_process(void);

void scenario_invasion_save_state(buffer *invasion_id, buffer *warnings);

void scenario_invasion_load_state(buffer *invasion_id, buffer *warnings);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void scenario_invasion_register_player_state(void);

#endif // SCENARIO_INVASION_H
