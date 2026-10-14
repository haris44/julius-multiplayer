#ifndef MP_EXPEDITION_H
#define MP_EXPEDITION_H

#include "core/buffer.h"

/**
 * @file
 * Caesar's punitive expedition (doc/mp/CESAR.md §7.4, DECISIONS D-053, D-086), when his wrath reaches 100:
 * - his legions of the original invasion (attack 13, defence 2) land in every city, at its point of arrival, with no
 *   regard to the difficulty: an innocent city (no share of the wrath) gets 32 soldiers, the main culprit 96 to 144
 *   by his share of the wrath, another culprit from 32 to the soldiers of the main culprit by his share against the
 *   share of the main culprit;
 * - they fight in their city as the invaders of the original, until destroyed, or 12 months at most, then they go back
 *   where they landed; destroying them (or putting them to flight) gives the city 20 laurels;
 * - the main culprit loses 25 % of his laurels, another culprit a part of these 25 % by his share against the share
 *   of the main culprit; an innocent city loses nothing but the disgrace and the damage;
 * - every war ends, as a signed peace; the gauge falls to 30, each share in proportion;
 * - for 12 months the province is in disgrace: every monthly laurel of the notes of every city is halved.
 * A second expedition in the same game strikes like the first one, or, with the lobby option, ends the game without
 * winner: Rome takes the province back. "AI invasions: no" does not concern it.
 *
 * It runs once for the world, after the wrath of the month. Nothing here runs in a classic game.
 */

typedef enum {
    MP_EXPEDITION_NONE = 0,     /**< Caesar never sent his legions to the city */
    MP_EXPEDITION_IN_CITY = 1,  /**< his legions are in the city */
    MP_EXPEDITION_LEAVING = 2,  /**< their months are over: they go back where they landed */
    MP_EXPEDITION_DEFEATED = 3, /**< the city destroyed them, or put them to flight */
    MP_EXPEDITION_LEFT = 4      /**< they left the city after their months */
} mp_expedition_state;

/** Expeditions sent in this game */
int mp_expedition_count(void);

/** Months the province stays in disgrace: while above 0, the monthly laurels of the notes are halved */
int mp_expedition_disgrace_months(void);

/** Soldiers Caesar would send to the city of a player now, from the shares of the wrath */
int mp_expedition_soldiers_for(int player_id);

/** Tenths of laurels a player would lose now, from the shares of the wrath and his laurels */
int mp_expedition_loss_for(int player_id);

/** What became of the last expedition in the city of a player (mp_expedition_state) */
int mp_expedition_state_of(int player_id);

/** Soldiers of the last expedition who landed in the city of a player */
int mp_expedition_soldiers_landed(int player_id);

/** Caesar's soldiers alive in the city of a player */
int mp_expedition_soldiers_alive(int player_id);

/** Months before the legions in the city of a player leave (0 when there are none, or once they leave) */
int mp_expedition_months_left(int player_id);

/** Tenths of laurels a player lost at the last expedition */
int mp_expedition_laurels_lost(int player_id);

/**
 * Every tick, after the wrath, once for the world: the expedition when it is due, then, every day, the legions of each
 * city (destroyed, leaving, gone) and, every month, the disgrace and the months of the legions
 */
void mp_expedition_update(void);

void mp_expedition_reset(void);
void mp_expedition_save_state(buffer *buf);
void mp_expedition_load_state(buffer *buf);

#endif // MP_EXPEDITION_H
