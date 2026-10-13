#ifndef MP_WRATH_H
#define MP_WRATH_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * @file
 * Caesar judges the wars between players (doc/mp/CESAR.md §7.1 to §7.3, DECISIONS D-053, D-080, D-084): the motive
 * of each war, its price in laurels, the triumphs, and the common gauge of his wrath with its stages.
 *
 * - The motive of a war is set by the game when it is declared: a retaliation when the target declared war on the
 *   declarer, attacked him or took one of his caravans in the last 12 months; a mandate of Caesar when the target is
 *   an enemy of Rome; without motive otherwise, and always when the declarer's last war on the target ended (peace
 *   signed, imposed or declaration withdrawn) in the last 12 months (its wrath then counts double).
 * - The price is paid in laurels of Caesar when the fighting starts: a threat withdrawn during its notice costs
 *   nothing.
 * - Every month, once for the world, each war adds to the gauge (duration, power, damage), weighed by its motive and
 *   form; the gauge then decreases. Crossing a threshold brings a letter of Caesar to every player, an ultimatum
 *   (peace imposed at its deadline, the main culprit an enemy of Rome), then the punitive expedition (M10.6).
 *
 * Nothing here runs in a classic game. While nobody is at war, only the decrease of the gauge runs.
 */

typedef enum {
    MP_WAR_MOTIVE_UNJUST = 0,      /**< without motive: its price in laurels, full wrath, no triumph */
    MP_WAR_MOTIVE_RETALIATION = 1, /**< the target attacked the declarer in the last 12 months: half the wrath */
    MP_WAR_MOTIVE_MANDATE = 2      /**< the target is an enemy of Rome: a quarter of the wrath */
} mp_war_motive;

/**
 * The motive a declaration of war of `declarer` on `target` would have now
 * @param redeclared Set to 1 when it would come within 12 months of the end of the last war `declarer` declared on
 *        `target` (always without motive, wrath doubled); may be 0
 */
int mp_wrath_motive_if_declared(int declarer, int target, int *redeclared);

/**
 * Tenths of laurels (negative) a war of this motive and form costs its declarer when its fighting starts
 */
int mp_wrath_war_price(int motive, int form);

/**
 * The motive of the war between two players (of their last war, at peace), from its declarer's side
 */
int mp_wrath_motive(int a, int b);

/** Whether the war between two players was declared again within 12 months of the end of the last one: wrath x2 */
int mp_wrath_is_redeclared(int a, int b);

/** Months of fighting of the war between two players, counted at each new month */
int mp_wrath_months_fighting(int a, int b);

/** Whether a player is an enemy of Rome: a war against him is a mandate of Caesar */
int mp_wrath_is_enemy_of_rome(int player_id);

/** Months (rounded up) a player stays an enemy of Rome, 0 when he is not */
int mp_wrath_enemy_of_rome_months(int player_id);

/** Months left before Caesar imposes peace on every war, 0 without ultimatum */
int mp_wrath_ultimatum_months(void);

/** The main culprit of the last ultimatum, or -1 */
int mp_wrath_ultimatum_culprit(void);

/**
 * Whether the gauge reached the punitive expedition (M10.6, not done yet: nothing happens, it is only recorded)
 */
int mp_wrath_expedition_due(void);

/** The punitive expedition was sent (M10.6) */
void mp_wrath_clear_expedition(void);

/**
 * Damage of the current month, by `from` on `to`: buildings pulled down, caravans intercepted, inhabitants killed
 */
int mp_wrath_month_buildings(int from, int to);
int mp_wrath_month_caravans(int from, int to);
int mp_wrath_month_killed(int from, int to);

/**
 * Damage `from` did to `to` since the process started (tests; display only, not saved)
 */
void mp_wrath_damage_done(int from, int to, int *buildings, int *caravans, int *killed);

/**
 * Absolute tick of the last hostile act of `from` against `to` (declaration, fight, building pulled down, caravan
 * taken), -1 when none
 */
int mp_wrath_last_hostile_act(int from, int to);

/**
 * Tenths of wrath the wars of a player added at the start of this month (before the decrease)
 */
int mp_wrath_added_last_month(int player_id);

/**
 * Triumphs a player was given (tests)
 */
int mp_wrath_triumphs(int player_id);

/**
 * Legions of `aggressor` whose standard stands in the land of `defender` (power of a war, CESAR §7.2)
 */
int mp_wrath_legions_in_land_of(int aggressor, int defender);

// ---------- hooks of the war (mp/war.c) and of the combat code; no-ops in a classic game ----------

/** `declarer` declared war on `target` (mp_war_declare): its motive; a brutal war starts fighting at once */
void mp_wrath_war_declared(int declarer, int target, int form);

/** The notice of an honourable war ended: the fighting starts, its declarer pays its price */
void mp_wrath_fighting_started(int a, int b);

/** `declarer` withdrew his declaration on `target` during the notice: no price, but it binds him for 12 months */
void mp_wrath_war_withdrawn(int declarer, int target);

/** Peace signed by both (or imposed by Caesar): binds the declarer of the war for 12 months */
void mp_wrath_peace_signed(int a, int b);

/** A figure of `from` started a fight with a figure of `to` */
void mp_wrath_hostile_act(int from, int to);

/** A soldier of `from` pulls down a building or wall of `to` */
void mp_wrath_building_pulled_down(int from, int to);

/** A soldier of `taker` intercepted a caravan of `seller` */
void mp_wrath_caravan_intercepted(int taker, int seller);

/** A figure of `killer` killed `victim` (melee or missile) */
void mp_wrath_figure_killed(int killer, const figure *victim);

/**
 * Every tick, after the cities, once for the world: the power of the wars every day, the wrath of Caesar every month
 */
void mp_wrath_update(void);

void mp_wrath_reset(void);
/**
 * A game saved before the judgement of the wars: the wars in progress are without motive, already paid for
 */
void mp_wrath_reset_for_old_game(void);
void mp_wrath_save_state(buffer *buf);
void mp_wrath_load_state(buffer *buf);

#endif // MP_WRATH_H
