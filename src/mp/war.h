#ifndef MP_WAR_H
#define MP_WAR_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * @file
 * War between players, a first playable slice (T5.5, doc/mp/DECISIONS.md D-077, CESAR.md §7, DESIGN §7): a state
 * for each pair of players, changed by commands of the military advisor and told to every player.
 *
 * - A player declares war on another: a brutal war fights at once, an honourable war after a notice of three months
 *   (D-053). During the notice nobody fights; the defender may strike first by declaring a brutal war in turn, and
 *   the declarer may withdraw his declaration: the threat leaves the defender time to give in (D-080).
 * - Peace is signed when both players proposed it; the legions of both out of their fort then go home (unless
 *   their player is still at war with another one).
 * - While two players fight, their soldiers are hostile to each other: they fight each other's soldiers and
 *   citizens (melee, javelins, towers), the soldiers standing by a building of the enemy damage it as invaders do,
 *   and a soldier next to a caravan of the enemy takes its goods. No caravan leaves between them, and one on its way
 *   between them turns back home, unpaid (mp/trade.c). During the notice, they trade as in peace (D-089).
 *
 * Nothing here exists in a classic game, nor in a multiplayer game while nobody is at war: every check below is then
 * a no-op. Caesar judges the wars in mp/wrath.c (M10.4, M10.5): motive, price, triumphs, wrath.
 */

typedef enum {
    MP_WAR_PEACE = 0,
    MP_WAR_NOTICE = 1,   /**< honourable war declared, nobody fights yet */
    MP_WAR_FIGHTING = 2
} mp_war_state;

typedef enum {
    MP_WAR_HONOURABLE = 0,
    MP_WAR_BRUTAL = 1
} mp_war_kind;

/**
 * Ticks between the declaration of an honourable war and the fighting (three months of 16 days of 50 ticks)
 */
#define MP_WAR_NOTICE_TICKS (3 * 16 * 50)

int mp_war_status(int a, int b);
int mp_war_form(int a, int b);
/** The player who declared the war between a and b, or -1 at peace */
int mp_war_declarer(int a, int b);
/** Days before the fighting of an honourable war begins (0 when it fights or at peace) */
int mp_war_days_until_fighting(int a, int b);
/** Whether `from` proposed peace to `to` (during a war) */
int mp_war_peace_proposed(int from, int to);

/**
 * Whether the soldiers of two players fight each other
 */
int mp_war_is_fighting(int a, int b);

/**
 * Whether two players fight somewhere: when not, every check of the combat code is skipped
 */
int mp_war_any_fighting(void);

/**
 * The current player declares war on another (command of its player): MP_WAR_BRUTAL fights at once,
 * MP_WAR_HONOURABLE after the notice. A brutal declaration during the notice starts the fighting.
 */
void mp_war_declare(int target, int form);

/**
 * Whether `declarer` may withdraw his declaration of war on `target`: an honourable war he declared, during its
 * notice (D-080)
 */
int mp_war_may_withdraw(int declarer, int target);

/**
 * The current player withdraws his declaration of an honourable war on another, during the notice (command of its
 * player): peace at once, as when it is signed
 */
void mp_war_withdraw(int target);

/**
 * The current player proposes peace to another, or withdraws the proposal (command of its player); peace is signed
 * when both proposed it
 */
void mp_war_propose_peace(int other, int propose);

/**
 * Every tick, once for the world: the honourable wars whose notice ended start fighting
 */
void mp_war_update(void);

/**
 * Caesar imposes peace on two players at the deadline of his ultimatum (M10.5): as when it is signed
 */
void mp_war_impose_peace(int a, int b);

/**
 * Tells every player, as the other announcements of the war (Caesar's judgement, mp/wrath.c)
 */
void mp_war_announce(const uint8_t *text);

/**
 * Melee: whether the figure attacks the opponent because their players fight each other. A soldier attacks the
 * soldiers, armed walkers and citizens of the enemy (not his caravans, which are taken instead); any other armed
 * walker (prefect, sentry) attacks his soldiers only.
 */
int mp_war_may_attack(const figure *attacker, const figure *opponent);

/**
 * Missiles and target searches: whether the figure is a soldier of a player at war with `player_id`
 */
int mp_war_is_enemy_soldier(int player_id, const figure *f);

/**
 * Searches of soldiers and towers, after those of their own city: the nearest soldier of an enemy of the current
 * player within the distance (strictly nearer than `max_distance` when `strict`), or 0
 */
int mp_war_nearest_enemy_soldier(int x, int y, int max_distance, int strict, int *distance);

/**
 * Whether an enemy of the current player has soldiers (the legions mop up)
 */
int mp_war_current_player_has_enemy_soldiers(void);

/**
 * A soldier standing with his legion damages a building, wall or gatehouse of an enemy next to him, as invaders do;
 * it collapses at the damage of the original
 * @return 1 when he attacks one
 */
int mp_war_soldier_attack_buildings(figure *f);

/**
 * Whether a building, wall or gatehouse of an enemy stands next to the soldier: what mp_war_soldier_attack_buildings
 * would attack (the order "attack" waits for it to fall, mp/assault.c); changes nothing
 */
int mp_war_soldier_has_target_next_to(const figure *f);

/**
 * A caravan between players next to a soldier of an enemy of its seller is intercepted: the soldier's player stores
 * what his warehouses take, the rest is lost
 * @return 1 when intercepted: the caravan carries nothing any more
 */
int mp_war_intercept_caravan(figure *caravan);

/**
 * Announcements to the local player (display only, never saved): declarations, start of the fighting, proposals and
 * peace, caravans taken
 */
int mp_war_announcements(void);
/**
 * Shows the announcements not yet seen as warnings: the city view calls it
 */
void mp_war_show_pending_announcements(void);

/**
 * A proposal of peace to the local player not yet shown in a dialog, and still open (display only, never saved):
 * its proposer, now marked as shown; -1 when there is none (window/mp_war.c)
 */
int mp_war_take_peace_offer(void);

/**
 * Writes a text of the translations whose "%d" stand for players: the first for `first`, the second for `second`
 * (player ids, written from 1)
 */
void mp_war_compose(uint8_t *text, int size, const uint8_t *pattern, int first, int second);

/** Loads taken and lost by interceptions since the start of the process (tests) */
int mp_war_loads_taken(void);
int mp_war_loads_lost(void);

void mp_war_reset(void);
void mp_war_save_state(buffer *buf);
void mp_war_load_state(buffer *buf);

#endif // MP_WAR_H
