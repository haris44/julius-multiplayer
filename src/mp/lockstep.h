#ifndef MP_LOCKSTEP_H
#define MP_LOCKSTEP_H

#include "game/rules.h"

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
 * @param separate_cities 1: every player gets a copy of the city of the saved game, on a shared map
 *                        (mp_compose_separate_cities); 0: all players share the city of the saved game
 * @return 1 when listening
 */
int mp_lockstep_host(int port, int num_players, const char *saved_game, int separate_cities);

/**
 * Rules of the next hosted game (default multiplayer rules otherwise); while the host waits for the players, the
 * rules of its game, sent at once to the players already there
 */
void mp_lockstep_set_rules(const game_rules_settings *rules);

/**
 * Rules of the game being prepared, as known on this computer: the host's own, or those the host sent to this
 * client (0 before they come, or without a network game)
 */
const game_rules_settings *mp_lockstep_lobby_rules(void);

/**
 * Host: with a manual start (lobby), the game starts on mp_lockstep_start_game once every player is there;
 * otherwise as soon as they are all there
 */
void mp_lockstep_set_manual_start(int manual);

/**
 * Host: the game takes place on a generated map (mp/mapgen), the chosen map giving climate, empire and funds
 */
void mp_lockstep_set_generated_map(int generate, unsigned int seed);

/**
 * Tests: this computer pretends to have other game data than the host, which must refuse it
 */
void mp_lockstep_test_alter_game_data(void);
void mp_lockstep_start_game(void);

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

/**
 * Pauses or resumes the game for every player: the host decides, a client asks the host
 */
void mp_lockstep_toggle_pause(void);

/**
 * Asks for the game to be paused (1) or resumed (0); requests that change nothing are ignored
 */
void mp_lockstep_request_pause(int paused);

int mp_lockstep_is_paused(void);

/**
 * The host sets the speed of the game; clients run as fast as the turns of the host come
 */
int mp_lockstep_is_host(void);

/**
 * Ticks this computer may run before it needs a new turn of the host
 */
int mp_lockstep_ticks_available(void);

/**
 * Absolute tick at which the network game started
 */
int mp_lockstep_base_tick(void);

/**
 * Tests: stops the game after this number of ticks (0: no limit), once it runs
 */
void mp_lockstep_set_tick_limit(int ticks_in_game);

/**
 * Host: players connected so far, the host included
 */
int mp_lockstep_connected_players(void);

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
