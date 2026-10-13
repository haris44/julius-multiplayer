#ifndef MP_ROME_H
#define MP_ROME_H

/**
 * @file
 * Rome is one for every player (T5.9, D-079): the wages it pays change in every city at once, by the same amount.
 * The wage events of the month are drawn in the turn of the first city only; the other cities receive the result.
 */

/**
 * Whether the current city draws the wage events of Rome: always in a classic game, the first city only in a
 * multiplayer game
 */
int mp_rome_draws_wage_events(void);

/**
 * Gives the wages Rome now pays in the current city (the first one) to every other city, with the same message
 * @param message_type Message of the change (Rome raises or lowers its wages)
 */
void mp_rome_share_wages(int message_type);

#endif // MP_ROME_H
