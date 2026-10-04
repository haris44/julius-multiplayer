#ifndef GAME_EXTRA_STATE_H
#define GAME_EXTRA_STATE_H

#include "core/buffer.h"

/**
 * @file
 * Simulation state that classic saved games do not store: current random values and pool,
 * building counters of most types, fire spread direction, image variant rotation, soldier
 * strength grid, last stored map point, building list sizes and the water workers cursor.
 *
 * Left alone, it leaks from one game to the next in the same process, so a game behaves
 * differently depending on what was played before (doc/mp/code-map/01 §3.4).
 */

/**
 * Puts the extra state in the state of a newly started process.
 * Called before a saved game is read or a scenario is started, so that the game only depends
 * on its file. Partly saved modules then get their saved part back from the file.
 */
void game_extra_state_reset(void);

/**
 * Saves the extra state (multiplayer saved games, checksums)
 * @param buf Buffer of at least game_extra_state_size() bytes
 */
void game_extra_state_save(buffer *buf);

/**
 * Loads the extra state saved by game_extra_state_save
 * @param buf Buffer
 */
void game_extra_state_load(buffer *buf);

/**
 * @return Size in bytes of the saved extra state
 */
int game_extra_state_size(void);

#endif // GAME_EXTRA_STATE_H
