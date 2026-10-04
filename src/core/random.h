#ifndef CORE_RANDOM_H
#define CORE_RANDOM_H

#include <stdint.h>

#include "core/buffer.h"

/**
 * @file
 * Random number generation.
 */

/**
 * Initializes the pseudo-random number generator
 */
void random_init(void);

/**
 * Generates the next pseudo-random random_byte
 */
void random_generate_next(void);

/**
 * Generates the pool of random bytes
 */
void random_generate_pool(void);

/**
 * Gets the current random 7-bit byte
 * @return Random 7-bit byte
 */
int8_t random_byte(void);

/**
 * Gets the current alternative random 7-bit byte
 * @return Alternative random 7-bits byte
 */
int8_t random_byte_alt(void);

/**
 * Gets the current random 15-bit short
 * @return Random 15-bits short
 */
int16_t random_short(void);

/**
 * Gets a random integer from the pool at the specified index
 * @param index Index to use
 * @return Random integer from the pool
 */
int32_t random_from_pool(int index);

/**
 * Save data to buffer
 * @param buf Buffer to save to
 */
void random_save_state(buffer *buf);

/**
 * Load data from buffer
 * @param buf Buffer to read from
 */
void random_load_state(buffer *buf);

/**
 * Puts the generator in the state it has in a newly started process, before a game is loaded
 */
void random_reset(void);

/**
 * Saves the generator state that classic saved games do not store: current values and pool
 * @param buf Buffer
 */
void random_save_extra_state(buffer *buf);

/**
 * Loads the generator state saved by random_save_extra_state
 * @param buf Buffer
 */
void random_load_extra_state(buffer *buf);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void random_register_player_state(void);

#endif // CORE_RANDOM_H
