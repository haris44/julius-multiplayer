#include "wrath.h"

#include "core/string.h"
#include "figure/formation.h"
#include "figure/properties.h"
#include "game/player_context.h"
#include "game/time.h"
#include "map/grid.h"
#include "mp/battle.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/territory.h"
#include "mp/war.h"
#include "translation/translation.h"

#include <string.h>

#define STATE_VERSION 1
#define MAX_PLAYERS PLAYER_CONTEXT_MAX_PLAYERS
#define TICKS_PER_MONTH (16 * 50)
// legions take the first formations of the slice of their player (formation.c get_free_formation(1)): the losses of
// each are counted under its local id
#define LEGION_SLOTS 10
#define NO_TICK -1

typedef enum {
    LEVEL_CALM = 0,
    LEVEL_WARNING = 1,
    LEVEL_ULTIMATUM = 2,
    LEVEL_EXPEDITION = 3
} wrath_level;

static struct {
    // each pair of players, the same in [a][b] and [b][a]: their war, or their last one at peace
    uint8_t motive[MAX_PLAYERS][MAX_PLAYERS];
    uint8_t redeclared[MAX_PLAYERS][MAX_PLAYERS];  // declared within 12 months of their last peace: wrath doubled
    uint8_t brutal[MAX_PLAYERS][MAX_PLAYERS];
    int8_t aggressor[MAX_PLAYERS][MAX_PLAYERS];    // the declarer, -1 when they never were at war
    uint8_t price_paid[MAX_PLAYERS][MAX_PLAYERS];  // the declarer paid the price of the war when its fighting started
    uint8_t fought[MAX_PLAYERS][MAX_PLAYERS];      // they fought on a day of the current month
    int32_t months_fighting[MAX_PLAYERS][MAX_PLAYERS];
    int32_t power_legions[MAX_PLAYERS][MAX_PLAYERS]; // most legions of the aggressor in the land of the defender on a
                                                     // day of the current month
    // each ordered pair [from][to]
    int32_t hostile_tick[MAX_PLAYERS][MAX_PLAYERS]; // last declaration, fight, building pulled down, caravan taken
    int32_t peace_tick[MAX_PLAYERS][MAX_PLAYERS];   // end of the last war `from` declared on `to`: a new war of his
                                                    // on `to` within 12 months is without motive
    int32_t buildings[MAX_PLAYERS][MAX_PLAYERS];    // damage of the current month, by `from` on `to`
    int32_t caravans[MAX_PLAYERS][MAX_PLAYERS];
    int32_t killed[MAX_PLAYERS][MAX_PLAYERS];
    // each player
    int32_t enemy_until[MAX_PLAYERS];               // absolute tick until which he is an enemy of Rome
    int32_t triumphs[MAX_PLAYERS];
    int32_t added_last_month[MAX_PLAYERS];          // tenths of wrath his wars added at the last month
    uint8_t legion_losses[MAX_PLAYERS][LEGION_SLOTS]; // soldiers killed by enemies since the legion was last destroyed
    // the gauge
    int32_t level;                                  // its stage at the last month (wrath_level)
    int32_t ultimatum_months;                       // months before Caesar imposes peace, 0 without ultimatum
    int32_t ultimatum_culprit;
    int32_t expedition_due;
} data;

// display only, never saved nor read by the simulation: what each player did to another since the process started
static struct {
    int buildings[MAX_PLAYERS][MAX_PLAYERS];
    int caravans[MAX_PLAYERS][MAX_PLAYERS];
    int killed[MAX_PLAYERS][MAX_PLAYERS];
} totals;

static int is_player(int player_id)
{
    return player_id >= 0 && player_id < player_context_num_players() && player_id < MAX_PLAYERS;
}

static int is_pair(int a, int b)
{
    return is_player(a) && is_player(b) && a != b;
}

static int is_active(void)
{
    return mp_caesar_is_active();
}

static int now(void)
{
    return game_time_absolute_tick();
}

// whether the tick is less than `months` months ago
static int within_months(int tick, int months)
{
    return tick != NO_TICK && now() >= tick && now() - tick < months * TICKS_PER_MONTH;
}

static int level_of(int wrath)
{
    if (wrath >= MP_CAESAR_WRATH_EXPEDITION) {
        return LEVEL_EXPEDITION;
    } else if (wrath >= MP_CAESAR_WRATH_ULTIMATUM) {
        return LEVEL_ULTIMATUM;
    } else if (wrath >= MP_CAESAR_WRATH_WARNING) {
        return LEVEL_WARNING;
    }
    return LEVEL_CALM;
}

void mp_wrath_reset(void)
{
    memset(&data, 0, sizeof(data));
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            data.aggressor[a][b] = -1;
            data.hostile_tick[a][b] = NO_TICK;
            data.peace_tick[a][b] = NO_TICK;
        }
    }
    data.ultimatum_culprit = -1;
}

void mp_wrath_reset_for_old_game(void)
{
    mp_wrath_reset();
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            if (a != b && mp_war_status(a, b) != MP_WAR_PEACE) {
                data.aggressor[a][b] = (int8_t) mp_war_declarer(a, b);
                data.brutal[a][b] = mp_war_form(a, b) == MP_WAR_BRUTAL;
                data.motive[a][b] = MP_WAR_MOTIVE_UNJUST;
                data.price_paid[a][b] = 1; // declared before Caesar judged the wars: nothing to pay
            }
        }
    }
    data.level = level_of(mp_caesar_wrath()); // no letter for a stage reached before
}

// ---------- motives and price (CESAR §7.1, D-053, D-080) ----------

int mp_wrath_is_enemy_of_rome(int player_id)
{
    return is_player(player_id) && now() < data.enemy_until[player_id];
}

int mp_wrath_enemy_of_rome_months(int player_id)
{
    if (!mp_wrath_is_enemy_of_rome(player_id)) {
        return 0;
    }
    return (data.enemy_until[player_id] - now() + TICKS_PER_MONTH - 1) / TICKS_PER_MONTH;
}

int mp_wrath_motive_if_declared(int declarer, int target, int *redeclared)
{
    if (redeclared) {
        *redeclared = 0;
    }
    if (!is_pair(declarer, target)) {
        return MP_WAR_MOTIVE_UNJUST;
    }
    // a new war within 12 months of a peace that binds the declarer is always without motive (CESAR §7.1)
    if (within_months(data.peace_tick[declarer][target], MP_CAESAR_REDECLARATION_DELAY)) {
        if (redeclared) {
            *redeclared = 1;
        }
        return MP_WAR_MOTIVE_UNJUST;
    }
    // the most lenient of the two just motives first: a quarter of the wrath against half (D-084, to be validated)
    if (mp_wrath_is_enemy_of_rome(target)) {
        return MP_WAR_MOTIVE_MANDATE;
    }
    if (within_months(data.hostile_tick[target][declarer], MP_CAESAR_RETALIATION_MONTHS)) {
        return MP_WAR_MOTIVE_RETALIATION;
    }
    return MP_WAR_MOTIVE_UNJUST;
}

int mp_wrath_war_price(int motive, int form)
{
    int price = motive == MP_WAR_MOTIVE_UNJUST ? MP_CAESAR_UNJUST_WAR_LAURELS : 0;
    // the -40 laurels of a war on a player whose legions are away for Caesar (MP_CAESAR_UNJUST_WAR_CAMPAIGN_LAURELS)
    // waits for the campaigns of Caesar (M9.7)
    if (form == MP_WAR_BRUTAL) {
        price += MP_CAESAR_BRUTAL_WAR_LAURELS;
    }
    return price;
}

int mp_wrath_motive(int a, int b)
{
    return is_pair(a, b) ? data.motive[a][b] : MP_WAR_MOTIVE_UNJUST;
}

int mp_wrath_is_redeclared(int a, int b)
{
    return is_pair(a, b) && data.redeclared[a][b];
}

int mp_wrath_months_fighting(int a, int b)
{
    return is_pair(a, b) ? data.months_fighting[a][b] : 0;
}

int mp_wrath_ultimatum_months(void)
{
    return data.ultimatum_months;
}

int mp_wrath_ultimatum_culprit(void)
{
    return data.ultimatum_months > 0 ? data.ultimatum_culprit : -1;
}

int mp_wrath_expedition_due(void)
{
    return data.expedition_due;
}

void mp_wrath_clear_expedition(void)
{
    data.expedition_due = 0;
}

int mp_wrath_month_buildings(int from, int to)
{
    return is_pair(from, to) ? data.buildings[from][to] : 0;
}

int mp_wrath_month_caravans(int from, int to)
{
    return is_pair(from, to) ? data.caravans[from][to] : 0;
}

int mp_wrath_month_killed(int from, int to)
{
    return is_pair(from, to) ? data.killed[from][to] : 0;
}

void mp_wrath_damage_done(int from, int to, int *buildings, int *caravans, int *killed)
{
    int pair = is_pair(from, to);
    *buildings = pair ? totals.buildings[from][to] : 0;
    *caravans = pair ? totals.caravans[from][to] : 0;
    *killed = pair ? totals.killed[from][to] : 0;
}

int mp_wrath_last_hostile_act(int from, int to)
{
    return is_pair(from, to) ? data.hostile_tick[from][to] : NO_TICK;
}

int mp_wrath_added_last_month(int player_id)
{
    return is_player(player_id) ? data.added_last_month[player_id] : 0;
}

int mp_wrath_triumphs(int player_id)
{
    return is_player(player_id) ? data.triumphs[player_id] : 0;
}

// not announced: the war window tells it to whoever would declare war on him, the imperial advisor to everyone, and
// the letter of an ultimatum names him
static void make_enemy_of_rome(int player_id)
{
    int until = now() + MP_CAESAR_ENEMY_OF_ROME_MONTHS * TICKS_PER_MONTH;
    if (data.enemy_until[player_id] < until) {
        data.enemy_until[player_id] = until;
    }
}

static void set_pair_u8(uint8_t values[MAX_PLAYERS][MAX_PLAYERS], int a, int b, int value)
{
    values[a][b] = values[b][a] = (uint8_t) value;
}

void mp_wrath_war_declared(int declarer, int target, int form)
{
    if (!is_active() || !is_pair(declarer, target)) {
        return;
    }
    int redeclared = 0;
    int motive = mp_wrath_motive_if_declared(declarer, target, &redeclared);
    set_pair_u8(data.motive, declarer, target, motive);
    set_pair_u8(data.redeclared, declarer, target, redeclared);
    set_pair_u8(data.brutal, declarer, target, form == MP_WAR_BRUTAL);
    set_pair_u8(data.price_paid, declarer, target, 0);
    data.aggressor[declarer][target] = data.aggressor[target][declarer] = (int8_t) declarer;
    data.months_fighting[declarer][target] = data.months_fighting[target][declarer] = 0;
    // a threat gives the target a motive of retaliation, even when it is withdrawn (D-080)
    data.hostile_tick[declarer][target] = now();
    if (mp_war_is_fighting(declarer, target)) {
        mp_wrath_fighting_started(declarer, target); // a brutal war: at once
    }
}

void mp_wrath_fighting_started(int a, int b)
{
    if (!is_active() || !is_pair(a, b) || data.price_paid[a][b]) {
        return;
    }
    set_pair_u8(data.price_paid, a, b, 1);
    int aggressor = data.aggressor[a][b] >= 0 ? data.aggressor[a][b] : mp_war_declarer(a, b);
    if (!is_player(aggressor)) {
        return;
    }
    int price = mp_wrath_war_price(data.motive[a][b], data.brutal[a][b] ? MP_WAR_BRUTAL : MP_WAR_HONOURABLE);
    if (price) {
        mp_caesar_add_laurels(aggressor, MP_LAURELS_WARS, price);
    }
    if (data.motive[a][b] == MP_WAR_MOTIVE_UNJUST) {
        // he declared a war without motive: a war against him is a mandate of Caesar for 12 months
        make_enemy_of_rome(aggressor);
    }
}

// the end of a war binds its declarer: declaring it again on the same player within 12 months is without motive, and
// its wrath counts double; his enemy keeps his motive of retaliation (D-084, to be validated)
static void bind_declarer(int a, int b)
{
    int aggressor = data.aggressor[a][b];
    if (is_player(aggressor)) {
        data.peace_tick[aggressor][aggressor == a ? b : a] = now();
    }
}

void mp_wrath_war_withdrawn(int declarer, int target)
{
    if (!is_active() || !is_pair(declarer, target)) {
        return;
    }
    // no price, no wrath (D-080)
    bind_declarer(declarer, target);
}

void mp_wrath_peace_signed(int a, int b)
{
    if (!is_active() || !is_pair(a, b)) {
        return;
    }
    bind_declarer(a, b);
}

// ---------- acts of war (CESAR §7.2) ----------

void mp_wrath_hostile_act(int from, int to)
{
    if (is_active() && is_pair(from, to)) {
        data.hostile_tick[from][to] = now();
    }
}

void mp_wrath_building_pulled_down(int from, int to)
{
    if (is_active() && is_pair(from, to)) {
        data.hostile_tick[from][to] = now();
        data.buildings[from][to]++;
        totals.buildings[from][to]++;
    }
}

void mp_wrath_caravan_intercepted(int taker, int seller)
{
    // HOOK (M9.8): a caravan carrying the resource of a request of Caesar in progress adds nothing to the wrath
    if (is_active() && is_pair(taker, seller)) {
        data.hostile_tick[taker][seller] = now();
        data.caravans[taker][seller]++;
        totals.caravans[taker][seller]++;
    }
}

static int is_soldier(const figure *f)
{
    return f->type == FIGURE_FORT_LEGIONARY || f->type == FIGURE_FORT_JAVELIN || f->type == FIGURE_FORT_MOUNTED;
}

// whether a soldier of the legion other than `except` is still alive
static int legion_has_soldiers(int player_id, int formation_id, int except)
{
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        const figure *f = figure_get(i);
        if (i != except && f->state == FIGURE_STATE_ALIVE && f->action_state != FIGURE_ACTION_149_CORPSE &&
            f->formation_id == formation_id && is_soldier(f)) {
            return 1;
        }
    }
    return 0;
}

// "Player 2 triumphs: a legion of player 1 destroyed", on every computer
static void announce_triumph(int winner, int loser)
{
    uint8_t text[100];
    string_copy(translation_for(TR_MP_PLAYER), text, 100);
    string_from_int(text + string_length(text), winner + 1, 0);
    string_copy(translation_for(TR_MP_WRATH_ANNOUNCE_TRIUMPH), text + string_length(text), 100 - string_length(text));
    string_from_int(text + string_length(text), loser + 1, 0);
    mp_war_announce(text);
}

// a soldier of a legion of `loser` killed by `killer`: when it was the last one, the legion is destroyed, and a
// triumph goes to `killer` unless he declared a war without motive on `loser`
static void legion_soldier_killed(int killer, int loser, const figure *victim)
{
    int formation_id = victim->formation_id;
    int slot = FORMATION_LOCAL_ID(formation_id);
    if (formation_id <= 0 || FORMATION_OWNER(formation_id) != loser || slot >= LEGION_SLOTS ||
        !formation_get(formation_id)->is_legion) {
        return;
    }
    if (data.legion_losses[loser][slot] < 255) {
        data.legion_losses[loser][slot]++;
    }
    if (legion_has_soldiers(loser, formation_id, victim->id)) {
        return;
    }
    int losses = data.legion_losses[loser][slot];
    data.legion_losses[loser][slot] = 0;
    int unjust_declarer = data.aggressor[killer][loser] == killer && data.motive[killer][loser] == MP_WAR_MOTIVE_UNJUST;
    if (losses < MP_CAESAR_TRIUMPH_MIN_SOLDIERS || unjust_declarer) {
        return;
    }
    mp_caesar_add_laurels(killer, MP_LAURELS_WARS, MP_CAESAR_TRIUMPH_LAURELS);
    data.triumphs[killer]++;
    mp_battle_triumph(killer); // and a triumphal arch for his city (M10.8)
    announce_triumph(killer, loser);
}

void mp_wrath_figure_killed(int killer, const figure *victim)
{
    int loser = FIGURE_OWNER(victim->id);
    if (!is_active() || !is_pair(killer, loser) || !mp_war_is_fighting(killer, loser)) {
        return;
    }
    // the people of the city and its soldiers; not an animal, an invader or a criminal in its slice of figures
    int category = figure_properties_for_type(victim->type)->category;
    if (category != FIGURE_CATEGORY_CITIZEN && category != FIGURE_CATEGORY_ARMED) {
        return;
    }
    data.hostile_tick[killer][loser] = now();
    if (is_soldier(victim)) {
        legion_soldier_killed(killer, loser, victim);
    } else {
        // an inhabitant of the city: walkers, prefects, sentries (D-084)
        data.killed[killer][loser]++;
        totals.killed[killer][loser]++;
    }
}

// ---------- power (CESAR §7.2) ----------

// HOOK (M10.2, D-068, D-082): Caesar's land around the bridge counts as the land of the defender. M10.2 added the
// function telling whether a tile is on it on another branch: at the merge, return mp_territory_is_caesar_land(offset).
static int stands_on_caesar_land(int grid_offset)
{
    return mp_territory_is_caesar_land(grid_offset); // around the bridge (D-082)
}

int mp_wrath_legions_in_land_of(int aggressor, int defender)
{
    if (!is_pair(aggressor, defender)) {
        return 0;
    }
    int count = 0;
    for (int i = aggressor * MAX_FORMATIONS + 1; i < (aggressor + 1) * MAX_FORMATIONS; i++) {
        const formation *m = formation_get(i);
        if (!m->in_use || !m->is_legion || m->is_at_fort || m->in_distant_battle || m->num_figures <= 0) {
            continue;
        }
        int grid_offset = map_grid_offset(m->standard_x, m->standard_y);
        if (!map_grid_is_valid_offset(grid_offset)) {
            continue;
        }
        if (mp_territory_owner(grid_offset) == defender || stands_on_caesar_land(grid_offset)) {
            count++;
        }
    }
    return count;
}

// every day: the pairs that fight, and the most legions of each aggressor in the land of his enemy this month
static void sample_day(void)
{
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = a + 1; b < MAX_PLAYERS; b++) {
            if (!mp_war_is_fighting(a, b)) {
                continue;
            }
            set_pair_u8(data.fought, a, b, 1);
            int aggressor = data.aggressor[a][b];
            if (!is_player(aggressor)) {
                continue;
            }
            int legions = mp_wrath_legions_in_land_of(aggressor, aggressor == a ? b : a);
            if (legions > data.power_legions[a][b]) {
                data.power_legions[a][b] = data.power_legions[b][a] = legions;
            }
        }
    }
}

// ---------- the monthly wrath (CESAR §7.2, §7.3) ----------

// damage of the month by `from` on `to`, in hundredths of a point
static long long damage_hundredths(int from, int to)
{
    return 10LL * MP_CAESAR_WRATH_BUILDING * data.buildings[from][to] +
        10LL * MP_CAESAR_WRATH_CARAVAN * data.caravans[from][to] +
        (long long) MP_CAESAR_WRATH_TEN_KILLED * data.killed[from][to];
}

static int motive_quarters(int motive)
{
    switch (motive) {
        case MP_WAR_MOTIVE_RETALIATION:
            return MP_CAESAR_WRATH_RETALIATION_QUARTERS;
        case MP_WAR_MOTIVE_MANDATE:
            return MP_CAESAR_WRATH_MANDATE_QUARTERS;
        default:
            return MP_CAESAR_WRATH_UNJUST_QUARTERS;
    }
}

// tenths of a point, rounded to the nearest (half up), from hundredths weighed in quarters and halves
static int weigh(long long hundredths, int quarters, int halves, int times)
{
    long long numerator = hundredths * quarters * halves * times;
    return (int) ((numerator + 40) / 80); // hundredths -> tenths: / 10; quarters: / 4; halves: / 2
}

// what a war added during the month: duration and power to its aggressor, the damage to whoever did it
static void add_war_wrath(int a, int b)
{
    int aggressor = data.aggressor[a][b];
    if (!is_player(aggressor)) {
        return;
    }
    int defender = aggressor == a ? b : a;
    long long base = damage_hundredths(aggressor, defender);
    if (data.fought[a][b]) {
        int month = ++data.months_fighting[a][b];
        data.months_fighting[b][a] = month;
        int legions = data.power_legions[a][b];
        base += 10LL * (month * MP_CAESAR_WRATH_MONTH + MP_CAESAR_WRATH_POWER * legions * legions);
    }
    int halves = data.brutal[a][b] ? MP_CAESAR_WRATH_BRUTAL_HALVES : 2;
    int added = weigh(base, motive_quarters(data.motive[a][b]), halves, data.redeclared[a][b] ? 2 : 1);
    if (added > 0) {
        mp_caesar_add_wrath(aggressor, added);
        data.added_last_month[aggressor] += added;
    }
    // the acts of the defender weigh as a retaliation (D-084, to be validated)
    int answered = weigh(damage_hundredths(defender, aggressor), MP_CAESAR_WRATH_RETALIATION_QUARTERS, 2, 1);
    if (answered > 0) {
        mp_caesar_add_wrath(defender, answered);
        data.added_last_month[defender] += answered;
    }
}

// a letter to every player naming the culprits: those who added to the wrath
static void warn(void)
{
    int culprits = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (mp_caesar_belligerence(p) > 0) {
            culprits |= 1 << p;
        }
    }
    mp_caesar_add_letter(MP_CAESAR_LETTER_WARNING, culprits);
}

// every war must end before the deadline; the main culprit is an enemy of Rome
static void issue_ultimatum(void)
{
    if (data.ultimatum_months > 0) {
        return; // one at a time
    }
    data.ultimatum_months = MP_CAESAR_ULTIMATUM_DELAY;
    data.ultimatum_culprit = mp_caesar_main_culprit();
    if (data.ultimatum_culprit >= 0) {
        make_enemy_of_rome(data.ultimatum_culprit);
    }
    mp_caesar_add_letter(MP_CAESAR_LETTER_ULTIMATUM, data.ultimatum_culprit);
}

// the deadline of the ultimatum: Caesar imposes peace on every war still declared, as if it were signed
static void impose_peace(void)
{
    int imposed = 0;
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = a + 1; b < MAX_PLAYERS; b++) {
            if (is_pair(a, b) && mp_war_status(a, b) != MP_WAR_PEACE) {
                mp_war_impose_peace(a, b);
                mp_wrath_peace_signed(a, b);
                imposed |= (1 << a) | (1 << b);
            }
        }
    }
    if (imposed) {
        mp_caesar_add_letter(MP_CAESAR_LETTER_PEACE_IMPOSED, imposed);
    }
}

static void check_stages(void)
{
    int level = level_of(mp_caesar_wrath());
    if (data.level < LEVEL_WARNING && level >= LEVEL_WARNING) {
        warn();
    }
    if (data.level < LEVEL_ULTIMATUM && level >= LEVEL_ULTIMATUM) {
        issue_ultimatum();
    }
    if (data.level < LEVEL_EXPEDITION && level >= LEVEL_EXPEDITION) {
        // HOOK (M10.6): the punitive expedition; only recorded, nothing is announced yet
        data.expedition_due = 1;
    }
    data.level = level;
}

// once for the world, at the start of each month, in a fixed order: what each war added (pairs by increasing ids),
// the deadline of an ultimatum given before, the stages the gauge reached, then its decrease
static void update_month(void)
{
    int fought_any = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        data.added_last_month[p] = 0;
    }
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = a + 1; b < MAX_PLAYERS; b++) {
            if (!is_pair(a, b)) {
                continue;
            }
            add_war_wrath(a, b);
            fought_any |= data.fought[a][b];
        }
    }
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            data.buildings[a][b] = 0;
            data.caravans[a][b] = 0;
            data.killed[a][b] = 0;
            data.power_legions[a][b] = 0;
            data.fought[a][b] = 0;
        }
    }
    if (data.ultimatum_months > 0 && --data.ultimatum_months == 0) {
        impose_peace();
    }
    // before the decrease: the gauge reaches its top, 100, only so
    check_stages();
    mp_caesar_decay_wrath(fought_any ? MP_CAESAR_WRATH_DECAY_WAR : MP_CAESAR_WRATH_DECAY_PEACE);
}

void mp_wrath_update(void)
{
    if (!is_active() || game_time_tick() != 0) {
        return;
    }
    // a sample at the end of each of the 16 days of the month, the last one in the tick that ends it
    if (mp_war_any_fighting()) {
        sample_day();
    }
    if (game_time_day() == 0) {
        update_month();
    }
}

// ---------- saved game ----------

void mp_wrath_save_state(buffer *buf)
{
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, MAX_PLAYERS);
    buffer_write_i32(buf, LEGION_SLOTS);
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            buffer_write_u8(buf, data.motive[a][b]);
            buffer_write_u8(buf, data.redeclared[a][b]);
            buffer_write_u8(buf, data.brutal[a][b]);
            buffer_write_i8(buf, data.aggressor[a][b]);
            buffer_write_u8(buf, data.price_paid[a][b]);
            buffer_write_u8(buf, data.fought[a][b]);
            buffer_write_i32(buf, data.months_fighting[a][b]);
            buffer_write_i32(buf, data.power_legions[a][b]);
            buffer_write_i32(buf, data.hostile_tick[a][b]);
            buffer_write_i32(buf, data.peace_tick[a][b]);
            buffer_write_i32(buf, data.buildings[a][b]);
            buffer_write_i32(buf, data.caravans[a][b]);
            buffer_write_i32(buf, data.killed[a][b]);
        }
    }
    for (int p = 0; p < MAX_PLAYERS; p++) {
        buffer_write_i32(buf, data.enemy_until[p]);
        buffer_write_i32(buf, data.triumphs[p]);
        buffer_write_i32(buf, data.added_last_month[p]);
        for (int i = 0; i < LEGION_SLOTS; i++) {
            buffer_write_u8(buf, data.legion_losses[p][i]);
        }
    }
    buffer_write_i32(buf, data.level);
    buffer_write_i32(buf, data.ultimatum_months);
    buffer_write_i32(buf, data.ultimatum_culprit);
    buffer_write_i32(buf, data.expedition_due);
}

void mp_wrath_load_state(buffer *buf)
{
    mp_wrath_reset_for_old_game();
    int version = buffer_read_i32(buf);
    int players = buffer_read_i32(buf);
    int slots = buffer_read_i32(buf);
    if (version < 1 || version > STATE_VERSION || players != MAX_PLAYERS || slots != LEGION_SLOTS) {
        return;
    }
    mp_wrath_reset();
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            data.motive[a][b] = buffer_read_u8(buf);
            data.redeclared[a][b] = buffer_read_u8(buf) ? 1 : 0;
            data.brutal[a][b] = buffer_read_u8(buf) ? 1 : 0;
            data.aggressor[a][b] = buffer_read_i8(buf);
            data.price_paid[a][b] = buffer_read_u8(buf) ? 1 : 0;
            data.fought[a][b] = buffer_read_u8(buf) ? 1 : 0;
            data.months_fighting[a][b] = buffer_read_i32(buf);
            data.power_legions[a][b] = buffer_read_i32(buf);
            data.hostile_tick[a][b] = buffer_read_i32(buf);
            data.peace_tick[a][b] = buffer_read_i32(buf);
            data.buildings[a][b] = buffer_read_i32(buf);
            data.caravans[a][b] = buffer_read_i32(buf);
            data.killed[a][b] = buffer_read_i32(buf);
            if (data.motive[a][b] > MP_WAR_MOTIVE_MANDATE || data.aggressor[a][b] < -1 ||
                data.aggressor[a][b] >= MAX_PLAYERS) {
                data.motive[a][b] = MP_WAR_MOTIVE_UNJUST; // nonsense
                data.aggressor[a][b] = -1;
            }
        }
    }
    for (int p = 0; p < MAX_PLAYERS; p++) {
        data.enemy_until[p] = buffer_read_i32(buf);
        data.triumphs[p] = buffer_read_i32(buf);
        data.added_last_month[p] = buffer_read_i32(buf);
        for (int i = 0; i < LEGION_SLOTS; i++) {
            data.legion_losses[p][i] = buffer_read_u8(buf);
        }
    }
    data.level = buffer_read_i32(buf);
    data.ultimatum_months = buffer_read_i32(buf);
    data.ultimatum_culprit = buffer_read_i32(buf);
    data.expedition_due = buffer_read_i32(buf) ? 1 : 0;
    if (buf->overflow) {
        mp_wrath_reset_for_old_game();
    }
}
