#ifndef WINDOW_MP_IMPERIAL_H
#define WINDOW_MP_IMPERIAL_H

/**
 * @file
 * The imperial advisor in a multiplayer game (doc/mp/CESAR.md §11): the laurels of the city and its rank, its five
 * notes, the laurels of Caesar, and the ranking of the province. Savings, salary and gifts stay those of the original
 * advisor.
 */

/**
 * Whether the imperial advisor shows this page instead of the favor and requests of the original game
 */
int window_mp_imperial_is_active(void);

/**
 * Draws the page above the savings and salary of the original advisor
 * @param height Height of the advisor, in blocks
 */
void window_mp_imperial_draw_background(int height);

/**
 * Rank shown at the bottom of the advisor: the rank given by the laurels
 */
int window_mp_imperial_rank(void);

#endif // WINDOW_MP_IMPERIAL_H
