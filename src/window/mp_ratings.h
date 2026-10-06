#ifndef WINDOW_MP_RATINGS_H
#define WINDOW_MP_RATINGS_H

/**
 * @file
 * The ratings advisor in a multiplayer game (T4.1, D-071): the fourth pillar shows the laurels of the city, the
 * esteem of Caesar, instead of the favor of the original game. Culture, prosperity and peace stay the original ones.
 */

/**
 * Whether the fourth pillar shows the laurels instead of the favor
 */
int window_mp_ratings_is_active(void);

/**
 * Height of the pillar, from 0 to 100: the laurels of the local city against the score, or against its next rank in
 * a game without score
 */
int window_mp_ratings_pillar_height(void);

/**
 * Under the pillar, in its button: the name, the laurels, and the laurels to reach
 * @param x Left of the button
 * @param width Width of the button
 */
void window_mp_ratings_draw_pillar_text(int x, int width);

/**
 * In the box at the bottom, when the pillar is selected: the laurels gained last month and the trend, the place of
 * the city in the province, and what brings laurels
 */
void window_mp_ratings_draw_explanation(int x, int y, int width);

#endif // WINDOW_MP_RATINGS_H
