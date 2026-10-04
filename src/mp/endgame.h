#ifndef MP_ENDGAME_H
#define MP_ENDGAME_H

#include "core/buffer.h"

/**
 * @file
 * End of a multiplayer game (doc/mp/ROADMAP.md M4.6). With GAME_END_SCORE, the game ends score_years after
 * the year of its first tick: every city gets a score, the simulation stops on every computer at the same
 * tick, and the interface shows the ranking. Conquest comes with war (M9).
 */

/**
 * Provisional score of the current city (to be validated): culture + prosperity + peace + population / 100
 */
int mp_endgame_city_score(void);

/**
 * Checks the end of the game; called by the simulation after every tick of all cities
 */
void mp_endgame_check(void);

int mp_endgame_is_over(void);

/**
 * Score of a player now, updated every game day, for the interface
 */
int mp_endgame_live_score(int player_id);

/**
 * Player with the best score (the lowest id on a tie), once the game is over
 */
int mp_endgame_winner(void);

int mp_endgame_score(int player_id);

/**
 * Called once by the interface when the game is over (window showing the ranking)
 */
void mp_endgame_set_over_callback(void (*callback)(void));

/**
 * Interface: calls the over callback once, after the ticks of a frame
 */
void mp_endgame_notify(void);

void mp_endgame_reset(void);
void mp_endgame_save_state(buffer *buf);
void mp_endgame_load_state(buffer *buf);

#endif // MP_ENDGAME_H
