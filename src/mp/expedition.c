#include "expedition.h"

#include "core/log.h"
#include "core/string.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "game/time.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/endgame.h"
#include "mp/session.h"
#include "mp/war.h"
#include "mp/wrath.h"
#include "scenario/invasion.h"
#include "translation/translation.h"

#include <stdio.h>
#include <string.h>

#define STATE_VERSION 1
#define MAX_PLAYERS PLAYER_CONTEXT_MAX_PLAYERS

static struct {
    int32_t count;                      // expeditions sent in the game
    int32_t disgrace_months;            // monthly laurels of the notes halved while above 0
    int32_t state[MAX_PLAYERS];         // mp_expedition_state of the last expedition in each city
    int32_t months_left[MAX_PLAYERS];   // before the legions in the city leave
    int32_t landed[MAX_PLAYERS];        // soldiers of the last expedition who landed in the city
    int32_t laurels_lost[MAX_PLAYERS];  // tenths of laurels lost at the last expedition
} data;

static int is_active(void)
{
    return mp_caesar_is_active();
}

static int is_player(int player_id)
{
    return player_id >= 0 && player_id < player_context_num_players() && player_id < MAX_PLAYERS;
}

int mp_expedition_count(void)
{
    return data.count;
}

int mp_expedition_disgrace_months(void)
{
    return is_active() ? data.disgrace_months : 0;
}

int mp_expedition_state_of(int player_id)
{
    return is_player(player_id) ? data.state[player_id] : MP_EXPEDITION_NONE;
}

int mp_expedition_soldiers_landed(int player_id)
{
    return is_player(player_id) ? data.landed[player_id] : 0;
}

int mp_expedition_months_left(int player_id)
{
    return is_player(player_id) && data.state[player_id] == MP_EXPEDITION_IN_CITY ? data.months_left[player_id] : 0;
}

int mp_expedition_laurels_lost(int player_id)
{
    return is_player(player_id) ? data.laurels_lost[player_id] : 0;
}

int mp_expedition_soldiers_alive(int player_id)
{
    if (!is_player(player_id)) {
        return 0;
    }
    int count = 0;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        const figure *f = figure_get(i);
        count += f->state == FIGURE_STATE_ALIVE && f->type == FIGURE_ENEMY_CAESAR_LEGIONARY &&
            f->action_state != FIGURE_ACTION_149_CORPSE;
    }
    return count;
}

// the figures the city may still create: the soldiers of Caesar take them like any figure of the city
static int free_figures(int player_id)
{
    int count = 0;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        count += !figure_get(i)->state; // as figure_create looks for a free figure
    }
    return count;
}

// ---------- the shares of the wrath (CESAR §7.4) ----------

static int total_shares(void)
{
    int total = 0;
    for (int p = 0; p < player_context_num_players() && p < MAX_PLAYERS; p++) {
        total += mp_caesar_belligerence(p);
    }
    return total;
}

int mp_expedition_soldiers_for(int player_id)
{
    if (!is_player(player_id)) {
        return 0;
    }
    int share = mp_caesar_belligerence(player_id);
    int culprit = mp_caesar_main_culprit();
    int total = total_shares();
    if (share <= 0 || culprit < 0 || total <= 0) {
        return MP_CAESAR_EXPEDITION_SOLDIERS_INNOCENT;
    }
    // the main culprit: from 96 (a share of nothing) to 144 (all the wrath), by his share of the wrath
    int main_share = mp_caesar_belligerence(culprit);
    int main_soldiers = MP_CAESAR_EXPEDITION_SOLDIERS_CULPRIT_MIN + (int) ((long long)
        (MP_CAESAR_EXPEDITION_SOLDIERS_CULPRIT_MAX - MP_CAESAR_EXPEDITION_SOLDIERS_CULPRIT_MIN) * main_share / total);
    if (player_id == culprit) {
        return main_soldiers;
    }
    // another culprit: from 32 to as many as the main culprit, by his share against the share of the main culprit
    return MP_CAESAR_EXPEDITION_SOLDIERS_INNOCENT + (int) ((long long)
        (main_soldiers - MP_CAESAR_EXPEDITION_SOLDIERS_INNOCENT) * share / main_share);
}

int mp_expedition_loss_for(int player_id)
{
    if (!is_player(player_id)) {
        return 0;
    }
    int share = mp_caesar_belligerence(player_id);
    int culprit = mp_caesar_main_culprit();
    int laurels = mp_caesar_laurels(player_id);
    if (share <= 0 || culprit < 0 || laurels <= 0) {
        return 0; // an innocent city, or nothing to lose
    }
    // the main culprit loses 25 % of his laurels; another culprit, a part of these 25 % by his share against the share
    // of the main culprit; rounded down
    return (int) ((long long) laurels * MP_CAESAR_CULPRIT_LOSS_PERCENT * share /
        (100LL * mp_caesar_belligerence(culprit)));
}

// ---------- landing, and leaving ----------

static void log_event(const char *what, int player_id, int value)
{
    char text[100];
    snprintf(text, sizeof(text), "%s, player %d, year %d month %d", what, player_id + 1, game_time_year(),
        game_time_month());
    log_info("Caesar's expedition:", text, value);
}

// "Player 2: Caesar's legions defeated, 20 laurels" / "Player 2: Caesar's legions leave his city", on every computer
static void announce(int player_id, translation_key what)
{
    uint8_t text[150];
    string_copy(translation_for(TR_MP_PLAYER), text, 150);
    string_from_int(text + string_length(text), player_id + 1, 0);
    string_copy(translation_for(what), text + string_length(text), 150 - string_length(text));
    mp_war_announce(text);
}

static void land(void)
{
    int soldiers[MAX_PLAYERS];
    int losses[MAX_PLAYERS];
    int culprit = mp_caesar_main_culprit();
    int num_players = player_context_num_players() < MAX_PLAYERS ? player_context_num_players() : MAX_PLAYERS;
    // from the shares of the wrath as the month left them, before the gauge falls
    for (int p = 0; p < num_players; p++) {
        soldiers[p] = mp_expedition_soldiers_for(p);
        losses[p] = mp_expedition_loss_for(p);
    }
    int previous = player_context_current();
    for (int p = 0; p < num_players; p++) {
        data.laurels_lost[p] = losses[p];
        if (losses[p]) {
            mp_caesar_add_laurels(p, MP_LAURELS_PUNISHMENT, -losses[p]);
        }
        // in the context of the city, as its own invaders; never more soldiers than the figures it may create
        player_context_switch(p);
        int wanted = soldiers[p];
        int room = free_figures(p);
        if (wanted > room) {
            wanted = room;
        }
        int before = mp_expedition_soldiers_alive(p);
        scenario_invasion_start_caesar_expedition(wanted);
        data.landed[p] = mp_expedition_soldiers_alive(p) - before;
        if (data.landed[p] > 0) {
            data.state[p] = MP_EXPEDITION_IN_CITY;
            data.months_left[p] = MP_CAESAR_EXPEDITION_MONTHS;
        }
        log_event("landed", p, data.landed[p]);
    }
    player_context_switch(previous);
    // the gauge falls to 30, every share in proportion (each rounded down, as the monthly decrease)
    int wrath = mp_caesar_wrath();
    if (wrath > MP_CAESAR_WRATH_AFTER_EXPEDITION) {
        mp_caesar_decay_wrath(wrath - MP_CAESAR_WRATH_AFTER_EXPEDITION);
    }
    mp_wrath_expedition_sent(); // peace on every war
    data.disgrace_months = MP_CAESAR_DISGRACE_MONTHS;
    data.count++;
    mp_caesar_add_letter(MP_CAESAR_LETTER_EXPEDITION, culprit);
}

// a second expedition, with the lobby option: the game ends without winner (D-053)
static void take_province_back(void)
{
    mp_wrath_clear_expedition();
    mp_endgame_rome_takes_province();
    log_info("Caesar's expedition: Rome takes the province back, no winner; expeditions before", 0, data.count);
}

static void send_expedition(void)
{
    if (data.count > 0 && game_rules_rome_takes_province()) {
        take_province_back();
    } else {
        land();
    }
}

// the legions of a city whose months are over go back where they landed, as the legions of the original game that
// retreat (city/emperor.c); every day, for the soldiers still fighting when it was first said
static void retreat(int player_id)
{
    int previous = player_context_current();
    player_context_switch(player_id);
    formation_caesar_retreat();
    player_context_switch(previous);
}

static void letter_to_city(int player_id, mp_caesar_letter_type type, int param)
{
    if (player_id == mp_session_local_player_id()) {
        mp_caesar_add_letter(type, param);
    }
}

// every day: the legions of the city destroyed (or put to flight), or gone after their months
static void follow_city(int player_id)
{
    int state = data.state[player_id];
    if (state != MP_EXPEDITION_IN_CITY && state != MP_EXPEDITION_LEAVING) {
        return;
    }
    if (mp_expedition_soldiers_alive(player_id) > 0) {
        if (state == MP_EXPEDITION_LEAVING) {
            retreat(player_id);
        }
        return;
    }
    data.months_left[player_id] = 0;
    if (state == MP_EXPEDITION_IN_CITY) {
        data.state[player_id] = MP_EXPEDITION_DEFEATED;
        mp_caesar_add_laurels(player_id, MP_LAURELS_PUNISHMENT, MP_CAESAR_EXPEDITION_DEFEATED_LAURELS);
        letter_to_city(player_id, MP_CAESAR_LETTER_EXPEDITION_DEFEATED, MP_CAESAR_EXPEDITION_DEFEATED_LAURELS);
        announce(player_id, TR_MP_EXPEDITION_ANNOUNCE_DEFEATED);
        log_event("defeated", player_id, MP_CAESAR_EXPEDITION_DEFEATED_LAURELS);
    } else {
        data.state[player_id] = MP_EXPEDITION_LEFT;
        letter_to_city(player_id, MP_CAESAR_LETTER_EXPEDITION_LEFT, 0);
        announce(player_id, TR_MP_EXPEDITION_ANNOUNCE_LEFT);
        log_event("left", player_id, 0);
    }
}

// every month, before an expedition that would land this month: the disgrace, and the months of the legions
static void update_month(void)
{
    if (data.disgrace_months > 0) {
        data.disgrace_months--;
    }
    for (int p = 0; p < player_context_num_players() && p < MAX_PLAYERS; p++) {
        if (data.state[p] == MP_EXPEDITION_IN_CITY && data.months_left[p] > 0 && --data.months_left[p] == 0) {
            data.state[p] = MP_EXPEDITION_LEAVING;
            retreat(p);
            log_event("leaving", p, mp_expedition_soldiers_alive(p));
        }
    }
}

void mp_expedition_update(void)
{
    if (!is_active() || game_time_tick() != 0) {
        return;
    }
    if (game_time_day() == 0) {
        update_month();
    }
    if (mp_wrath_expedition_due()) {
        send_expedition();
    }
    for (int p = 0; p < player_context_num_players() && p < MAX_PLAYERS; p++) {
        follow_city(p);
    }
}

// ---------- saved game ----------

void mp_expedition_reset(void)
{
    memset(&data, 0, sizeof(data));
}

void mp_expedition_save_state(buffer *buf)
{
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, MAX_PLAYERS);
    buffer_write_i32(buf, data.count);
    buffer_write_i32(buf, data.disgrace_months);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        buffer_write_i32(buf, data.state[p]);
        buffer_write_i32(buf, data.months_left[p]);
        buffer_write_i32(buf, data.landed[p]);
        buffer_write_i32(buf, data.laurels_lost[p]);
    }
}

void mp_expedition_load_state(buffer *buf)
{
    mp_expedition_reset();
    int version = buffer_read_i32(buf);
    int players = buffer_read_i32(buf);
    if (version < 1 || version > STATE_VERSION || players != MAX_PLAYERS) {
        return;
    }
    data.count = buffer_read_i32(buf);
    data.disgrace_months = buffer_read_i32(buf);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        data.state[p] = buffer_read_i32(buf);
        data.months_left[p] = buffer_read_i32(buf);
        data.landed[p] = buffer_read_i32(buf);
        data.laurels_lost[p] = buffer_read_i32(buf);
        if (data.state[p] < MP_EXPEDITION_NONE || data.state[p] > MP_EXPEDITION_LEFT) {
            data.state[p] = MP_EXPEDITION_NONE; // nonsense
        }
    }
    if (buf->overflow) {
        mp_expedition_reset();
    }
}
