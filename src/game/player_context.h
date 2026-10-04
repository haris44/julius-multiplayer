#ifndef GAME_PLAYER_CONTEXT_H
#define GAME_PLAYER_CONTEXT_H

/**
 * @file
 * Current city: the simulation code of Caesar III works on one city. With several players, every
 * module that keeps per-city state registers it here, and switching the current player swaps that
 * memory in and out (doc/mp/DECISIONS.md D-021). With one player, nothing is ever swapped.
 */

#define PLAYER_CONTEXT_MAX_PLAYERS 4

/**
 * Current player and number of players, read by the id slice macros of buildings, figures...
 * Read-only outside of player_context.c.
 */
extern int player_context_current_player;
extern int player_context_player_count;

/**
 * Registers per-city state; called once by every module at startup (player_context_init)
 */
void player_context_register(void *state, int size, const char *name);

/**
 * Registers the state of all modules; called once at startup
 */
void player_context_init(void);

/**
 * Number of cities simulated: 1 in a classic game. Every city starts with the current state of
 * player 0 copied; the current player becomes player 0.
 */
void player_context_set_num_players(int num_players);

int player_context_num_players(void);

/**
 * Adds a city whose state is a copy of the city of player 0, without changing the others
 * @return the id of the new player, or -1 if there are already PLAYER_CONTEXT_MAX_PLAYERS
 */
int player_context_add_player(void);

int player_context_current(void);

/**
 * Makes another city the current one
 */
void player_context_switch(int player_id);

/**
 * Copies the memory of every registered state of the current city into its slot, so that
 * player_context_copy_slot and saving see up-to-date values
 */
void player_context_flush(void);

/**
 * Total size in bytes of the registered per-city state
 */
int player_context_state_size(void);

/**
 * Direct access to the stored state of a player (after player_context_flush for the current one),
 * for saving and tests
 */
const unsigned char *player_context_slot(int player_id);

/**
 * Name of the first registered state, apart from the ignored ones (0-terminated list), that differs
 * between two copies of a slot, or 0 if none:
 * tests use it to check that loading a saved game restores every city exactly
 */
const char *player_context_first_difference(const unsigned char *a, const unsigned char *b,
    const char **ignored_names);

/**
 * Replaces the stored state of a player (not the current one)
 */
void player_context_set_slot(int player_id, const unsigned char *state);

#endif // GAME_PLAYER_CONTEXT_H
