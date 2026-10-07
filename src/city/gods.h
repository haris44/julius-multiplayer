#ifndef CITY_GODS_H
#define CITY_GODS_H

#define MAX_GODS 5

void city_gods_reset(void);
void city_gods_reset_neptune_blessing(void);

void city_gods_calculate_moods(int update_moods);

int city_gods_calculate_least_happy(void);

int city_god_happiness(int god_id);

int city_god_wrath_bolts(int god_id);

int city_god_months_since_festival(int god_id);

/**
 * @return God ID or -1 if no single god is the least happy
 */
int city_god_least_happy(void);

/**
 * The least happy god now, as city_gods_calculate_least_happy would find it, without storing it in the city (the
 * religion advisor of a network game: the interface of one player must not change the simulated state, T5.3)
 * @return god id, or -1 when no god is unhappy
 */
int city_gods_least_happy_now(void);

/**
 * The least happy god as the religion advisor shows it: in a network game as city_gods_least_happy_now (the state is
 * not written), otherwise as the original does (calculated and stored in the city). Kept apart from the advisor so
 * that a test without graphics can prove a window does not write into the game (T5.3)
 * @return god id, or -1 when no god is unhappy
 */
int city_gods_least_happy_shown(void);

int city_god_spirit_of_mars_power(void);
void city_god_spirit_of_mars_mark_used(void);

int city_god_neptune_create_shipwreck_flotsam(void);

#endif // CITY_GODS_H
