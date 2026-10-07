#include "war.h"

#include "building/building.h"
#include "building/destruction.h"
#include "building/warehouse.h"
#include "city/warning.h"
#include "core/calc.h"
#include "core/lang.h"
#include "core/string.h"
#include "figure/formation.h"
#include "figure/formation_legion.h"
#include "figure/properties.h"
#include "figure/route.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "game/rules.h"
#include "game/time.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/owner.h"
#include "map/routing.h"
#include "map/terrain.h"
#include "mp/session.h"
#include "translation/translation.h"

#include <string.h>

#define STATE_VERSION 1
#define MAX_PLAYERS PLAYER_CONTEXT_MAX_PLAYERS

// The state of each pair of players, the same in [a][b] and [b][a]; the proposals of peace go one way
static struct {
    uint8_t status[MAX_PLAYERS][MAX_PLAYERS];
    uint8_t form[MAX_PLAYERS][MAX_PLAYERS];
    int8_t declarer[MAX_PLAYERS][MAX_PLAYERS];
    int32_t fight_tick[MAX_PLAYERS][MAX_PLAYERS]; // absolute tick at which an honourable war starts fighting
    uint8_t peace_proposed[MAX_PLAYERS][MAX_PLAYERS];
} data;

// derived from the state above (recomputed on every change and on loading): not hidden state
static int any_war;
static int any_fighting;

// display only, never read by the simulation
#define MAX_PENDING 8
#define MAX_TEXT 100
static struct {
    int announcements;
    int loads_taken;
    int loads_lost;
    uint8_t pending[MAX_PENDING][MAX_TEXT]; // announcements not yet shown: the player was not on the city view
    int num_pending;
} display;

static int is_player(int player_id)
{
    return player_id >= 0 && player_id < player_context_num_players() && player_id < MAX_PLAYERS;
}

static void update_summary(void)
{
    any_war = 0;
    any_fighting = 0;
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            any_war |= data.status[a][b] != MP_WAR_PEACE;
            any_fighting |= data.status[a][b] == MP_WAR_FIGHTING;
        }
    }
}

void mp_war_reset(void)
{
    display.num_pending = 0;
    memset(&data, 0, sizeof(data));
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            data.declarer[a][b] = -1;
        }
    }
    update_summary();
}

int mp_war_status(int a, int b)
{
    return is_player(a) && is_player(b) && a != b ? data.status[a][b] : MP_WAR_PEACE;
}

int mp_war_form(int a, int b)
{
    return mp_war_status(a, b) != MP_WAR_PEACE ? data.form[a][b] : MP_WAR_HONOURABLE;
}

int mp_war_declarer(int a, int b)
{
    return mp_war_status(a, b) != MP_WAR_PEACE ? data.declarer[a][b] : -1;
}

int mp_war_days_until_fighting(int a, int b)
{
    if (mp_war_status(a, b) != MP_WAR_NOTICE) {
        return 0;
    }
    int ticks = data.fight_tick[a][b] - game_time_absolute_tick();
    return ticks <= 0 ? 0 : (ticks + 49) / 50;
}

int mp_war_peace_proposed(int from, int to)
{
    return mp_war_status(from, to) != MP_WAR_PEACE && data.peace_proposed[from][to];
}

int mp_war_is_fighting(int a, int b)
{
    return any_fighting && mp_war_status(a, b) == MP_WAR_FIGHTING;
}

int mp_war_any_fighting(void)
{
    return any_fighting;
}

// ---------- announcements ----------

static void append(uint8_t *text, const uint8_t *part)
{
    int length = string_length(text);
    string_copy(part, text + length, 200 - length);
}

static void append_number(uint8_t *text, int value)
{
    uint8_t number[16];
    string_from_int(number, value, 0);
    append(text, number);
}

// shown as a warning when the local player is next on the city view (window/city.c): a war must not go unnoticed
// because he was in an advisor
static void announce(const uint8_t *text)
{
    display.announcements++;
    if (display.num_pending == MAX_PENDING) {
        memmove(display.pending[0], display.pending[1], (MAX_PENDING - 1) * MAX_TEXT);
        display.num_pending--;
    }
    string_copy(text, display.pending[display.num_pending++], MAX_TEXT);
}

void mp_war_show_pending_announcements(void)
{
    for (int i = 0; i < display.num_pending; i++) {
        city_warning_show_to_local_player(display.pending[i]);
    }
    display.num_pending = 0;
}

// "Player 1: brutal war on player 2", on every computer
static void announce_declaration(int declarer, int target, int form)
{
    uint8_t text[200] = { 0 };
    append(text, translation_for(TR_MP_PLAYER));
    append_number(text, declarer + 1);
    append(text, translation_for(form == MP_WAR_BRUTAL ? TR_MP_WAR_ANNOUNCE_BRUTAL : TR_MP_WAR_ANNOUNCE_HONOURABLE));
    append_number(text, target + 1);
    if (form != MP_WAR_BRUTAL) {
        append(text, translation_for(TR_MP_WAR_ANNOUNCE_NOTICE));
    }
    announce(text);
}

// "Players 1 and 2: the fighting begins" / "...: peace signed", on every computer
static void announce_pair(int a, int b, translation_key what)
{
    uint8_t text[200] = { 0 };
    append(text, translation_for(TR_MP_WAR_PLAYERS));
    append_number(text, (a < b ? a : b) + 1);
    append(text, translation_for(TR_MP_WAR_AND));
    append_number(text, (a < b ? b : a) + 1);
    append(text, translation_for(what));
    announce(text);
}

// ---------- commands ----------

static void set_pair(int a, int b, int status, int form, int declarer, int fight_tick)
{
    data.status[a][b] = data.status[b][a] = (uint8_t) status;
    data.form[a][b] = data.form[b][a] = (uint8_t) form;
    data.declarer[a][b] = data.declarer[b][a] = (int8_t) declarer;
    data.fight_tick[a][b] = data.fight_tick[b][a] = fight_tick;
    data.peace_proposed[a][b] = data.peace_proposed[b][a] = 0;
    update_summary();
}

void mp_war_declare(int target, int form)
{
    int self = player_context_current_player;
    if (!game_rules_is_multiplayer() || !is_player(self) || !is_player(target) || target == self ||
        (form != MP_WAR_BRUTAL && form != MP_WAR_HONOURABLE)) {
        return;
    }
    int status = data.status[self][target];
    if (status == MP_WAR_FIGHTING || (status == MP_WAR_NOTICE && form != MP_WAR_BRUTAL)) {
        return; // already at war, or already declared
    }
    if (form == MP_WAR_BRUTAL) {
        // also the answer of the defender during the notice: he strikes first, and the brutality is his
        set_pair(self, target, MP_WAR_FIGHTING, MP_WAR_BRUTAL, self, game_time_absolute_tick());
    } else {
        set_pair(self, target, MP_WAR_NOTICE, MP_WAR_HONOURABLE, self,
            game_time_absolute_tick() + MP_WAR_NOTICE_TICKS);
    }
    announce_declaration(self, target, form);
}

// the figures of a player fighting one of the other player stop: they go on with what they did before the fight
static void stop_fights(int player_id, int other)
{
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE || f->action_state != FIGURE_ACTION_150_ATTACK) {
            continue;
        }
        int against_other = (f->attacker_id1 > 0 && FIGURE_OWNER(f->attacker_id1) == other) ||
            (f->attacker_id2 > 0 && FIGURE_OWNER(f->attacker_id2) == other) ||
            (f->opponent_id > 0 && FIGURE_OWNER(f->opponent_id) == other);
        if (against_other) {
            // as figure_combat_handle_attack does when the fight ends
            f->num_attackers = 0;
            f->action_state = f->action_state_before_attack;
            f->opponent_id = 0;
            f->attacker_id1 = 0;
            f->attacker_id2 = 0;
            figure_route_remove(f);
        }
    }
}

static int is_at_war_with_anyone(int player_id)
{
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (data.status[player_id][p] != MP_WAR_PEACE) {
            return 1;
        }
    }
    return 0;
}

// at peace, the legions of a player out of their fort go back to it, unless he is still at war with another player
static void legions_go_home(int player_id)
{
    if (is_at_war_with_anyone(player_id)) {
        return;
    }
    int previous = player_context_current();
    player_context_switch(player_id);
    for (int i = FORMATION_FIRST; i < FORMATION_END; i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion && !m->is_at_fort && !m->in_distant_battle) {
            formation_legion_return_home(m);
        }
    }
    player_context_switch(previous);
}

void mp_war_propose_peace(int other, int propose)
{
    int self = player_context_current_player;
    if (!is_player(self) || !is_player(other) || other == self || data.status[self][other] == MP_WAR_PEACE ||
        data.peace_proposed[self][other] == (propose ? 1 : 0)) {
        return;
    }
    data.peace_proposed[self][other] = propose ? 1 : 0;
    if (!propose) {
        return;
    }
    if (!data.peace_proposed[other][self]) {
        uint8_t text[200] = { 0 };
        append(text, translation_for(TR_MP_PLAYER));
        append_number(text, self + 1);
        append(text, translation_for(TR_MP_WAR_ANNOUNCE_PROPOSES));
        append_number(text, other + 1);
        announce(text);
        return;
    }
    // both proposed it: peace is signed
    set_pair(self, other, MP_WAR_PEACE, MP_WAR_HONOURABLE, -1, 0);
    stop_fights(self, other);
    stop_fights(other, self);
    legions_go_home(self);
    legions_go_home(other);
    announce_pair(self, other, TR_MP_WAR_ANNOUNCE_PEACE);
}

void mp_war_update(void)
{
    if (!any_war) {
        return;
    }
    int now = game_time_absolute_tick();
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = a + 1; b < MAX_PLAYERS; b++) {
            if (data.status[a][b] == MP_WAR_NOTICE && now >= data.fight_tick[a][b]) {
                data.status[a][b] = data.status[b][a] = MP_WAR_FIGHTING;
                update_summary();
                announce_pair(a, b, TR_MP_WAR_ANNOUNCE_FIGHTING);
            }
        }
    }
}

// ---------- combat ----------

static int is_soldier(const figure *f)
{
    return f->type == FIGURE_FORT_LEGIONARY || f->type == FIGURE_FORT_JAVELIN || f->type == FIGURE_FORT_MOUNTED;
}

static int owner_of(const figure *f)
{
    return FIGURE_OWNER(f->id);
}

static int is_alive(const figure *f)
{
    return f->state == FIGURE_STATE_ALIVE && f->action_state != FIGURE_ACTION_149_CORPSE;
}

int mp_war_may_attack(const figure *attacker, const figure *opponent)
{
    if (!any_fighting || !mp_war_is_fighting(owner_of(attacker), owner_of(opponent))) {
        return 0;
    }
    int category = figure_properties_for_type(opponent->type)->category;
    if (is_soldier(attacker)) {
        if (opponent->type == FIGURE_TRADE_CARAVAN || opponent->type == FIGURE_TRADE_CARAVAN_DONKEY) {
            return 0; // taken by mp_war_intercept_caravan
        }
        return category == FIGURE_CATEGORY_ARMED || category == FIGURE_CATEGORY_CITIZEN;
    }
    return is_soldier(opponent);
}

int mp_war_is_enemy_soldier(int player_id, const figure *f)
{
    return any_fighting && is_soldier(f) && is_alive(f) && mp_war_is_fighting(player_id, owner_of(f)) &&
        f->action_state != FIGURE_ACTION_89_SOLDIER_AT_DISTANT_BATTLE;
}

int mp_war_nearest_enemy_soldier(int x, int y, int max_distance, int strict, int *distance)
{
    if (!any_fighting) {
        return 0;
    }
    int self = player_context_current_player;
    int best = 0;
    int best_distance = max_distance + 1;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (!mp_war_is_fighting(self, p)) {
            continue;
        }
        for (int i = p * MAX_FIGURES + 1; i < (p + 1) * MAX_FIGURES; i++) {
            figure *f = figure_get(i);
            if (!mp_war_is_enemy_soldier(self, f)) {
                continue;
            }
            int d = calc_maximum_distance(x, y, f->x, f->y);
            if (d < best_distance && (d < max_distance || (!strict && d == max_distance))) {
                best_distance = d;
                best = i;
            }
        }
    }
    if (best && distance) {
        *distance = best_distance;
    }
    return best;
}

int mp_war_current_player_has_enemy_soldiers(void)
{
    if (!any_fighting) {
        return 0;
    }
    int self = player_context_current_player;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (!mp_war_is_fighting(self, p)) {
            continue;
        }
        for (int i = p * MAX_FIGURES + 1; i < (p + 1) * MAX_FIGURES; i++) {
            if (mp_war_is_enemy_soldier(self, figure_get(i))) {
                return 1;
            }
        }
    }
    return 0;
}

// the player owning what stands on the tile, buildings and walls (-1: nobody, or something not to attack)
static int owner_of_target(int grid_offset, int *max_damage)
{
    int building_id = map_building_at(grid_offset);
    if (building_id > 0) {
        const building *b = building_get(building_id);
        if (b->state != BUILDING_STATE_IN_USE || BUILDING_IS_CAESAR(building_id)) {
            return -1;
        }
        *max_damage = b->type == BUILDING_GATEHOUSE ? 150 : 10;
        return BUILDING_OWNER(building_id);
    }
    if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
        int owner = map_owner_get_claimed(grid_offset);
        *max_damage = 200;
        return owner == MAP_OWNER_NONE || owner == MAP_OWNER_CAESAR ? -1 : owner;
    }
    return -1;
}

int mp_war_soldier_attack_buildings(figure *f)
{
    if (!any_fighting || !is_soldier(f)) {
        return 0;
    }
    int self = owner_of(f);
    for (int dir = 0; dir < 8; dir += 1) {
        int grid_offset = f->grid_offset + map_grid_direction_delta(dir);
        if (!map_grid_is_valid_offset(grid_offset)) {
            continue;
        }
        int max_damage = 0;
        int victim = owner_of_target(grid_offset, &max_damage);
        if (victim < 0 || !mp_war_is_fighting(self, victim)) {
            continue;
        }
        if (!(game_time_tick() & 3) && map_building_damage_increase(grid_offset) > max_damage) {
            // the building collapses in the city of its owner: his peace rating, his walls and sentries
            int previous = player_context_current();
            player_context_switch(victim);
            building_destroy_by_enemy(map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset),
                grid_offset);
            player_context_switch(previous);
        }
        return 1;
    }
    return 0;
}

// ---------- caravans ----------

static int soldier_of_enemy_near(int seller, int grid_offset)
{
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int offset = grid_offset + map_grid_delta(dx, dy);
            if (!map_grid_is_valid_offset(offset)) {
                continue;
            }
            int guard = 0;
            for (int id = map_figure_at(offset); id > 0 && guard < MAX_FIGURES; guard++) {
                figure *f = figure_get(id);
                if (is_soldier(f) && is_alive(f) && f->action_state != FIGURE_ACTION_89_SOLDIER_AT_DISTANT_BATTLE &&
                    mp_war_is_fighting(seller, owner_of(f))) {
                    return id;
                }
                id = f->next_figure_id_on_same_tile;
            }
        }
    }
    return 0;
}

static void announce_interception(int seller, int taker, int resource, int taken)
{
    int local = mp_session_local_player_id();
    uint8_t text[200] = { 0 };
    if (local == taker) {
        append(text, translation_for(TR_MP_WAR_ANNOUNCE_TAKEN));
        append_number(text, taken);
        append(text, (const uint8_t *) " ");
        append(text, lang_get_string(23, resource));
    } else if (local == seller) {
        append(text, translation_for(TR_MP_PLAYER));
        append_number(text, taker + 1);
        append(text, translation_for(TR_MP_WAR_ANNOUNCE_INTERCEPTED));
    } else {
        return;
    }
    announce(text);
}

int mp_war_intercept_caravan(figure *caravan)
{
    if (!any_fighting || caravan->loads_sold_or_carrying <= 0) {
        return 0;
    }
    int seller = owner_of(caravan);
    int soldier_id = soldier_of_enemy_near(seller, caravan->grid_offset);
    if (!soldier_id) {
        return 0;
    }
    int taker = FIGURE_OWNER(soldier_id);
    int resource = caravan->resource_id;
    int loads = caravan->loads_sold_or_carrying;
    int taken = 0;
    int previous = player_context_current();
    player_context_switch(taker);
    for (int i = BUILDING_FIRST; i < BUILDING_END && taken < loads; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_WAREHOUSE) {
            while (taken < loads && building_warehouse_add_resource(b, resource)) {
                taken++;
            }
        }
    }
    player_context_switch(previous);
    caravan->loads_sold_or_carrying = 0;
    display.loads_taken += taken;
    display.loads_lost += loads - taken;
    announce_interception(seller, taker, resource, taken);
    return 1;
}

int mp_war_announcements(void)
{
    return display.announcements;
}

int mp_war_loads_taken(void)
{
    return display.loads_taken;
}

int mp_war_loads_lost(void)
{
    return display.loads_lost;
}

// ---------- saved game ----------

void mp_war_save_state(buffer *buf)
{
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, MAX_PLAYERS);
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            buffer_write_u8(buf, data.status[a][b]);
            buffer_write_u8(buf, data.form[a][b]);
            buffer_write_i8(buf, data.declarer[a][b]);
            buffer_write_u8(buf, data.peace_proposed[a][b]);
            buffer_write_i32(buf, data.fight_tick[a][b]);
        }
    }
}

void mp_war_load_state(buffer *buf)
{
    mp_war_reset();
    int version = buffer_read_i32(buf);
    int players = buffer_read_i32(buf);
    if (version < 1 || version > STATE_VERSION || players != MAX_PLAYERS) {
        return;
    }
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            int status = buffer_read_u8(buf);
            int form = buffer_read_u8(buf);
            int declarer = buffer_read_i8(buf);
            int proposed = buffer_read_u8(buf);
            int fight_tick = buffer_read_i32(buf);
            if (a == b || status > MP_WAR_FIGHTING || form > MP_WAR_BRUTAL || declarer < -1 || declarer >= MAX_PLAYERS) {
                continue; // nonsense: peace
            }
            data.status[a][b] = (uint8_t) status;
            data.form[a][b] = (uint8_t) form;
            data.declarer[a][b] = (int8_t) declarer;
            data.peace_proposed[a][b] = proposed ? 1 : 0;
            data.fight_tick[a][b] = fight_tick;
        }
    }
    if (buf->overflow) {
        mp_war_reset();
        return;
    }
    update_summary();
}
