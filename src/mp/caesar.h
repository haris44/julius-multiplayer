#ifndef MP_CAESAR_H
#define MP_CAESAR_H

#include "core/buffer.h"

/**
 * @file
 * Caesar the judge (doc/mp/CESAR.md, D-050, D-053): the laurels of every city and the common gauge of Caesar's
 * wrath. Laurels add up from two sources: the laurels of the city, earned every month from five notes, and the
 * laurels of Caesar, earned by serving him and lost by aggression. The first city to reach the score wins.
 *
 * Laurels are in tenths, the wrath in tenths of a point (mp/caesar_rules.h). Inactive in a classic game: nothing
 * changes, and every classic game starts from an empty state.
 */

typedef enum {
    // laurels of the city, one source per note (CESAR §5)
    MP_LAURELS_PROSPERITY = 0,
    MP_LAURELS_TRADE,
    MP_LAURELS_HOUSING,
    MP_LAURELS_CULTURE,
    MP_LAURELS_GREATNESS,
    // laurels of Caesar (CESAR §6, §7)
    MP_LAURELS_FESTIVALS,
    MP_LAURELS_GIFTS,
    MP_LAURELS_CAMPAIGNS,
    MP_LAURELS_REQUESTS,
    MP_LAURELS_WARS,
    MP_LAURELS_ARMY,
    MP_LAURELS_PUNISHMENT,
    MP_LAURELS_MAX_SOURCE
} mp_laurels_source;

#define MP_LAURELS_FIRST_CAESAR_SOURCE MP_LAURELS_FESTIVALS

/**
 * Whether Caesar judges this game: multiplayer rules, even alone
 */
int mp_caesar_is_active(void);

/**
 * Gives (or takes, when negative) tenths of laurels to a city
 */
void mp_caesar_add_laurels(int player_id, mp_laurels_source source, int tenths);

/**
 * Laurels of a city, in tenths: in all, from one source, from its five notes
 */
int mp_caesar_laurels(int player_id);
int mp_caesar_laurels_from(int player_id, mp_laurels_source source);
int mp_caesar_city_laurels(int player_id);

/**
 * The common gauge of Caesar's wrath, in tenths (0 to MP_CAESAR_WRATH_MAX)
 */
int mp_caesar_wrath(void);

/**
 * A city adds to the wrath of Caesar (or takes from it, when negative): its share, its belligerence, follows
 */
void mp_caesar_add_wrath(int player_id, int tenths);

/**
 * What a city added to the wrath, in tenths: its share of the gauge
 */
int mp_caesar_belligerence(int player_id);

void mp_caesar_reset(void);
void mp_caesar_save_state(buffer *buf);
void mp_caesar_load_state(buffer *buf);

#endif // MP_CAESAR_H
