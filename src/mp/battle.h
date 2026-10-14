#ifndef MP_BATTLE_H
#define MP_BATTLE_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * @file
 * Each side of the wars between players (M10.8, doc/mp/DECISIONS.md D-088, DESIGN §7): the original rules of morale
 * and the military totals of a city, which know only the invaders of the AI, applied to the soldiers of the players
 * who fight each other.
 *
 * - Morale: when a legion breaks (its morale falls to 20), the original lowers the morale of every legion of its city
 *   and raises the morale of its enemies; the legions of the players fighting its owner are its enemies too.
 * - The soldiers of the players fighting a city who stand in its land are enemies in that city, as invaders are
 *   (immigration stops, the advisors and the people speak of them, battle music).
 * - The losses of each side: soldiers and legions killed by each enemy, saved, told by the military advisor.
 * - A triumph (D-084) earns the city a triumphal arch, as the distant battles of the original did.
 *
 * Nothing here runs in a classic game, nor in a multiplayer game while nobody fights.
 */

/** A figure of `killer` killed `victim` while their players fight each other (melee or missile) */
void mp_battle_figure_killed(int killer, const figure *victim);

/** A triumph of the player (mp/wrath.c): his city earns a triumphal arch */
void mp_battle_triumph(int player_id);

/**
 * A legion of the current player broke (formation.c, after the original changes of morale of its own city): the
 * legions of the players fighting him take heart, +10
 */
void mp_battle_legion_broke(void);

/**
 * At the end of the figures of the current city: the soldiers of the players fighting it who stand in its land count
 * as enemies in the city
 */
void mp_battle_count_enemies_in_city(void);

/** Soldiers of the players fighting `player_id` who stand in his land */
int mp_battle_enemy_soldiers_in_land(int player_id);

/** Soldiers of `victim` killed by `killer`, and his legions destroyed by `killer`, since the start of the game */
int mp_battle_soldiers_killed(int killer, int victim);
int mp_battle_legions_destroyed(int killer, int victim);

/** Totals of a player against every other: his losses, and what he killed */
int mp_battle_soldiers_lost(int player_id);
int mp_battle_legions_lost(int player_id);
int mp_battle_enemy_soldiers_killed(int player_id);
int mp_battle_enemy_legions_destroyed(int player_id);

void mp_battle_reset(void);
void mp_battle_save_state(buffer *buf);
void mp_battle_load_state(buffer *buf);

#endif // MP_BATTLE_H
