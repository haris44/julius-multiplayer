#ifndef MP_LOCKSTEP_H
#define MP_LOCKSTEP_H

#include <stdint.h>

/**
 * @file
 * Deterministic lockstep over TCP, star topology (doc/mp/DESIGN.md §4).
 *
 * The host sends the saved game to every client, then everybody simulates the same ticks.
 * Time is cut into turns of a few ticks; a command issued during turn T runs at the start of turn
 * T + delay on every computer. After each turn every computer sends the state checksum; the host
 * compares them and stops the game when they differ (desynchronisation).
 */

typedef enum {
    MP_LOCKSTEP_OFF = 0,
    MP_LOCKSTEP_WAITING_FOR_PLAYERS,
    MP_LOCKSTEP_RUNNING,
    MP_LOCKSTEP_DESYNC,
    MP_LOCKSTEP_DISCONNECTED
} mp_lockstep_state;

#define MP_LOCKSTEP_DEFAULT_PORT 27400
#define MP_LOCKSTEP_MAX_PLAYERS 4

/**
 * Hosts a game: waits for the other players, then starts from the saved game
 * @return 1 when listening
 */
int mp_lockstep_host(int port, int num_players, const char *saved_game);

/**
 * Joins a hosted game; the game starts when the host sends the saved game
 * @return 1 when connected
 */
int mp_lockstep_join(const char *address, int port);

/**
 * Called once the shared saved game is loaded on this computer (for example to show the city)
 */
void mp_lockstep_set_started_callback(void (*callback)(void));

/**
 * Network input and output; call every frame
 */
void mp_lockstep_poll(void);

/**
 * @return Whether the next tick may run: the commands of its turn are known on this computer
 */
int mp_lockstep_can_run_tick(void);

/**
 * Call after each tick: sends and checks state checksums at the end of each turn
 */
void mp_lockstep_after_tick(void);

int mp_lockstep_is_active(void);

mp_lockstep_state mp_lockstep_get_state(void);

/**
 * @return Short description of the current state, for display and logs
 */
const char *mp_lockstep_status(void);

/**
 * @return Number of the last turn whose checksum was verified identical on every computer (host only)
 */
int mp_lockstep_last_verified_turn(void);

void mp_lockstep_stop(void);

#endif // MP_LOCKSTEP_H
