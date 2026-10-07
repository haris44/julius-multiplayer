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
    // laurels of the city, one source per note (CESAR §5), in the order of mp_note
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
 * The five notes of a city (CESAR §5), from 0 to 100
 */
typedef enum {
    MP_NOTE_PROSPERITY = 0,
    MP_NOTE_TRADE,
    MP_NOTE_HOUSING,
    MP_NOTE_CULTURE,
    MP_NOTE_GREATNESS,
    MP_NOTE_MAX
} mp_note;

/**
 * A letter of Caesar to the local player (window/mp_caesar_letter): display state of this computer only, never
 * saved, never read by the simulation
 */
typedef enum {
    MP_CAESAR_LETTER_WELCOME = 0, // the rules of the judgement, at the start of a game
    MP_CAESAR_LETTER_PROMOTION    // param: the new rank
} mp_caesar_letter_type;

typedef struct {
    mp_caesar_letter_type type;
    int param;
} mp_caesar_letter;

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
 * A note of a city, from 0 to 100, as computed at the start of the month
 */
int mp_caesar_note(int player_id, mp_note note);

/**
 * Monthly, for the current city: its five notes, then the laurels they bring (CESAR §4.1), then its rank, with a
 * letter of Caesar to the local player when he rises
 */
void mp_caesar_update_city_month(void);

/**
 * Monthly history of the laurels of each city (T4.1, D-071): at the end of each of its months, a city records its
 * laurels; the last MP_CAESAR_HISTORY_MONTHS records are kept, in the saved game
 */
#define MP_CAESAR_HISTORY_MONTHS 12

typedef enum {
    MP_CAESAR_TREND_DOWN = -1,
    MP_CAESAR_TREND_STEADY = 0,
    MP_CAESAR_TREND_UP = 1
} mp_caesar_trend;

/**
 * Number of monthly records of a city (0 to MP_CAESAR_HISTORY_MONTHS)
 */
int mp_caesar_history_months(int player_id);

/**
 * Laurels of a city, in tenths, at a monthly record: 0 is the last one, 1 the one before...
 */
int mp_caesar_history(int player_id, int months_ago);

/**
 * Tenths of laurels a city gained over its last months (negative when it lost some), between its monthly records;
 * over fewer months when the history is shorter
 */
int mp_caesar_laurels_gained(int player_id, int months);

/**
 * Whether a city gains more laurels lately: its last three months against the three before (fewer while the history
 * is short), steady within a tenth or a laurel a month
 */
mp_caesar_trend mp_caesar_laurels_trend(int player_id);

/**
 * The esteem of Caesar for a city, from 0 to 100, what the player sees instead of the favor of the original game
 * (T4.1, D-071): its laurels against the score of the game, or against its next rank in a game without score
 */
int mp_caesar_esteem(int player_id);

/**
 * Laurels (whole ones) the esteem is measured against: the score, or the next rank in a game without score
 */
int mp_caesar_esteem_goal(int player_id);

/**
 * Whether the laurels of a city have reached the laurels it is measured against (esteem goal): what the capital of the
 * laurels pillar shows, instead of the height of the pillar alone
 */
int mp_caesar_esteem_goal_reached(int player_id);

/**
 * Rank of a city (0 to MP_CAESAR_NUM_RANKS - 1): one for each tenth of the score, the last one at the score
 */
int mp_caesar_rank(int player_id);

/**
 * Laurels (whole ones) where a rank starts
 */
int mp_caesar_rank_laurels(int rank);

/**
 * Players by laurels, the most first; on a tie, the most laurels of the city, then the lowest id
 * @param players Filled with the ids of the players
 * @return Number of players
 */
int mp_caesar_ranking(int *players);

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

/**
 * Gifts to Caesar (CESAR §6.1): one gift counts every MP_CAESAR_GIFT_PERIOD months, whatever its size. The others
 * are paid all the same and bring nothing.
 */

/**
 * Tenths of laurels a gift of this size (GIFT_MODEST to GIFT_LAVISH) brings when it counts, 0 for another size
 */
int mp_caesar_gift_laurels(int size);

/**
 * Months before the next gift of a city counts (0: the next one counts)
 */
int mp_caesar_gift_cooldown(int player_id);

/**
 * A city has sent a gift of this size, already paid: gives the laurels if one counts now, and starts the wait
 * @return Tenths of laurels given
 */
int mp_caesar_gift_sent(int player_id, int size);

/**
 * Highest salary rank of a city: its rank (CESAR §6.1)
 */
int mp_caesar_salary_rank_limit(int player_id);

/**
 * Monthly, before the salary is paid: Rome pays the current city the salary of its rank, the same table for every
 * player (D-076), and the rank of the original game follows it
 */
void mp_caesar_limit_salary(void);

/**
 * Letters of Caesar waiting for the local player
 */
int mp_caesar_num_letters(void);
const mp_caesar_letter *mp_caesar_get_letter(int index);
void mp_caesar_add_letter(mp_caesar_letter_type type, int param);
void mp_caesar_remove_first_letter(void);

void mp_caesar_reset(void);
void mp_caesar_save_state(buffer *buf);
void mp_caesar_load_state(buffer *buf);

#endif // MP_CAESAR_H
