#ifndef WINDOW_MP_CAESAR_LETTER_H
#define WINDOW_MP_CAESAR_LETTER_H

/**
 * @file
 * Letters of Caesar to the local player (doc/mp/CESAR.md §11): the rules of his judgement at the start of a game, a
 * new rank. One at a time, over the city, until the player closes it.
 */

/**
 * Shows the next letter waiting, if the city is shown and nothing is being built
 */
void window_mp_caesar_letter_show_pending(void);

#endif // WINDOW_MP_CAESAR_LETTER_H
