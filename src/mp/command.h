#ifndef MP_COMMAND_H
#define MP_COMMAND_H

#include "core/buffer.h"

#include <stdint.h>

/**
 * @file
 * Commands: every player action that changes the simulation.
 *
 * The user interface never changes the simulation directly: it submits a command, with all its
 * parameters as absolute values. In a classic or solo game the command runs right away; in a
 * network game it runs on every computer at the same tick, before anything else in that tick.
 * See doc/mp/DESIGN.md §3.7.
 */

#define MP_COMMAND_MAX_ARGS 8
#define MP_COMMAND_SERIALIZED_SIZE (4 * 4 + 4 * MP_COMMAND_MAX_ARGS)
#define MP_COMMAND_QUEUE_SIZE 1024

typedef enum {
    MP_COMMAND_NONE = 0,
    MP_COMMAND_BUILD = 1,        /**< type, sub_type, x_start, y_start, x_end, y_end, road_orientation */
    MP_COMMAND_CLEAR_LAND = 2,   /**< x_start, y_start, x_end, y_end, confirmed (fort/bridge) */
    MP_COMMAND_TEST = 99,        /**< for tests only: no effect */
    MP_COMMAND_MAX
} mp_command_type;

typedef struct {
    int type;          /**< mp_command_type */
    int player_id;     /**< Player who issued the command */
    int sequence;      /**< Per player sequence number: keeps the order of commands issued in the same tick */
    int tick;          /**< Absolute tick at which the command runs (game_time_absolute_tick) */
    int32_t args[MP_COMMAND_MAX_ARGS];
} mp_command;

typedef void (*mp_command_executor)(const mp_command *command);

/**
 * Serializes a command (explicit little endian, MP_COMMAND_SERIALIZED_SIZE bytes)
 */
void mp_command_write(const mp_command *command, buffer *buf);

/**
 * Deserializes a command
 * @return 1 when the command is valid
 */
int mp_command_read(mp_command *command, buffer *buf);

/**
 * Empties the queue of scheduled commands
 */
void mp_command_queue_clear(void);

/**
 * Schedules a command; commands run ordered by tick, then player, then sequence
 * @return 1 on success, 0 when the queue is full
 */
int mp_command_queue_add(const mp_command *command);

/**
 * @return Number of scheduled commands
 */
int mp_command_queue_size(void);

/**
 * Runs, in order, every scheduled command whose tick is at most the given tick
 * @param tick Current absolute tick
 * @param executor Function that applies a command to the simulation
 * @return Number of commands run
 */
int mp_command_queue_run_due(int tick, mp_command_executor executor);

#endif // MP_COMMAND_H
