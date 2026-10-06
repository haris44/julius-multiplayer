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

typedef enum {
    GAME_END_NONE = 0,   /**< Endless game */
    GAME_END_SCORE = 1,  /**< After a number of years, the player with the best score wins (provisional, M4.6) */
    GAME_END_CAESAR = 2  /**< The first city to reach the score of laurels wins (doc/mp/CESAR.md §4.3, D-053) */
} game_end_condition;

/** Prepared map of a new multiplayer game, chosen in the lobby (doc/mp/DECISIONS.md D-069) */
typedef enum {
    GAME_MAP_1 = 0,      /**< The first prepared maps (D-047, D-062); also those of games saved before the choice */
    GAME_MAP_2 = 1,      /**< The second prepared maps (T4.14) */
    GAME_MAP_RANDOM = 2  /**< Drawn by lot with the seed of the lobby; the game then keeps the map drawn */
} game_map_choice;

typedef struct {
    int difficulty; /**< One of the set_difficulty values of game/settings.h */
    int gods_enabled;
    int fix_immigration_bug;
    int fix_100_year_ghosts;
    int ai_invasions;      /**< Enemy armies and local uprisings of the map (multiplayer) */
    int end_condition;     /**< game_end_condition */
    int score_years;       /**< Length of a GAME_END_SCORE game */
    int territories;       /**< Players build only in their zone (prepared maps, doc/mp/DECISIONS.md D-036) */
    int fog_of_war;        /**< Players see only what they discovered (doc/mp/DECISIONS.md D-038) */
    int caesar_score;      /**< Laurels that win a GAME_END_CAESAR game (0: the default score) */
    int prepared_map;      /**< game_map_choice of a new game; once it started, the map it is played on */
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

/**
 * Settings of the multiplayer rules last set or loaded
 */
const game_rules_settings *game_rules_multiplayer_settings(void);

int game_rules_is_multiplayer(void);

int game_rules_difficulty(void);

int game_rules_gods_enabled(void);

int game_rules_fix_immigration_bug(void);

int game_rules_fix_100_year_ghosts(void);

/**
 * Whether the enemy armies and local uprisings of the map attack (always in a classic game)
 */
int game_rules_ai_invasions(void);

/**
 * How a multiplayer game ends (GAME_END_NONE in a classic game) and after how many years
 */
game_end_condition game_rules_end_condition(void);

/**
 * Whether players build only in their zone (multiplayer games on prepared maps)
 */
int game_rules_territories(void);

/**
 * Whether players see only what they discovered (multiplayer games)
 */
int game_rules_fog_of_war(void);
int game_rules_score_years(void);

/**
 * Laurels the first city must reach to win a GAME_END_CAESAR game, also the scale of the ranks in every game
 */
int game_rules_caesar_score(void);

/**
 * Whether the rules of the multiplayer maps apply (water of Caesar, reservoirs that hold water, territories, fog,
 * missionaries): a game of several cities, or a prepared map played alone
 */
int game_rules_multiplayer_map(void);

void game_rules_save_state(buffer *buf);

void game_rules_load_state(buffer *buf);

#endif // GAME_RULES_H
