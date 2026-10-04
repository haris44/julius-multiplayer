#ifndef MP_SESSION_H
#define MP_SESSION_H

#include "mp/command.h"

/**
 * @file
 * Who the local player is and where the commands they issue go.
 *
 * Offline (classic or solo game), a submitted command runs right away, exactly where the user
 * interface used to change the simulation directly. In a network game, commands go to the
 * network layer, which schedules them on every computer (doc/mp/DESIGN.md §4).
 */

typedef void (*mp_command_sink)(mp_command *command);

/**
 * Offline session: the local player is player 0 and commands run immediately
 */
void mp_session_init_offline(void);

/**
 * Network session: commands are handed to the sink instead of running immediately
 * @param local_player_id Id of the player on this computer
 * @param sink Receives every submitted command, with player and sequence set
 */
void mp_session_init_network(int local_player_id, mp_command_sink sink);

int mp_session_local_player_id(void);

int mp_session_is_networked(void);

/**
 * Optional observer of every command executed by the simulation (recording, logs)
 */
void mp_session_set_execution_observer(mp_command_executor observer);

/**
 * Submits a command issued by the local player
 * @param command Command with type and arguments; player, sequence and tick are filled in
 */
void mp_command_submit(mp_command *command);

/**
 * Applies a command to the simulation. Called for commands run right away and for scheduled ones.
 */
void mp_command_execute(const mp_command *command);

/**
 * Runs the scheduled commands due at the current tick; called at the start of every tick
 */
void mp_command_run_scheduled(void);

#endif // MP_SESSION_H
