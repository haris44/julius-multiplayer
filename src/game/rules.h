#ifndef GAME_RULES_H
#define GAME_RULES_H

#include "core/buffer.h"

/**
 * @file
 * Rules of the game: every setting the simulation reads.
 *
 * In a classic game they come from the local settings (c3.inf, julius.ini), exactly as before.
 * In a multiplayer game they are chosen in the lobby and are the same on every computer, so a
 * local settings file can never make the simulations diverge (doc/mp/DECISIONS.md D-016).
 */

typedef enum {
    GAME_MODE_CLASSIC = 0,
    GAME_MODE_MULTIPLAYER = 1
} game_mode;

typedef struct {
    int difficulty; /**< One of the set_difficulty values of game/settings.h */
    int gods_enabled;
    int fix_immigration_bug;
    int fix_100_year_ghosts;
} game_rules_settings;

/**
 * Classic game: rules come from the local settings
 */
void game_rules_set_classic(void);

/**
 * Multiplayer game with the given rules
 * @param settings Rules chosen for the game
 */
void game_rules_set_multiplayer(const game_rules_settings *settings);

/**
 * Default rules of a multiplayer game: hard difficulty, gods enabled, original bugs kept
 * @param settings Filled with the defaults
 */
void game_rules_default_multiplayer_settings(game_rules_settings *settings);

game_mode game_rules_mode(void);

int game_rules_is_multiplayer(void);

int game_rules_difficulty(void);

int game_rules_gods_enabled(void);

int game_rules_fix_immigration_bug(void);

int game_rules_fix_100_year_ghosts(void);

void game_rules_save_state(buffer *buf);

void game_rules_load_state(buffer *buf);

#endif // GAME_RULES_H
