#ifndef MP_LOBBY_H
#define MP_LOBBY_H

#include "game/rules.h"

/**
 * @file
 * Rules of a multiplayer game as chosen in the lobby (window/mp_lobby.c draws them).
 *
 * The host chooses them, also after "Host" while waiting for the players: every change goes to the network game
 * at once (mp_lockstep_set_rules), which sends it to the players already there. A player who joined sees the
 * rules of the host and cannot change them (T4.11).
 */

typedef enum {
    MP_LOBBY_RULE_DIFFICULTY = 0,
    MP_LOBBY_RULE_GODS,
    MP_LOBBY_RULE_END,
    MP_LOBBY_RULE_INVASIONS,
    MP_LOBBY_RULE_FOG,
    MP_LOBBY_RULE_MAP  /**< Prepared map of a new game: map 1, map 2, drawn by lot (T4.14) */
} mp_lobby_rule;

/**
 * The first time: the rules proposed by the lobby (easy difficulty, T4.4; a map drawn by lot, T4.14); later, keeps
 * the choices made
 */
void mp_lobby_rules_init(void);

/**
 * Next value of a rule, when this computer may choose the rules
 */
void mp_lobby_change_rule(mp_lobby_rule rule);

/**
 * Whether this computer chooses the rules: not when it joined a game hosted by another one
 */
int mp_lobby_rules_editable(void);

/**
 * The rules chosen on this computer
 */
void mp_lobby_rules_settings(game_rules_settings *settings);

/**
 * The rules the lobby shows: those of the host once this computer joined a game, else those chosen here
 */
const game_rules_settings *mp_lobby_rules_shown(void);

/**
 * Host: the game starts with the rules chosen at this moment, once every player is there
 */
void mp_lobby_start_game(void);

#endif // MP_LOBBY_H
