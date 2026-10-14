#include "battle.h"

#include "building/menu.h"
#include "city/buildings.h"
#include "city/figures.h"
#include "figure/formation.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "map/grid.h"
#include "mp/territory.h"
#include "mp/war.h"

#include <string.h>

#define STATE_VERSION 1
#define MAX_PLAYERS PLAYER_CONTEXT_MAX_PLAYERS

// each ordered pair [killer][victim], since the start of the game
static struct {
    int32_t soldiers[MAX_PLAYERS][MAX_PLAYERS]; // soldiers of `victim` killed by `killer`
    int32_t legions[MAX_PLAYERS][MAX_PLAYERS];  // legions of `victim` whose last soldier `killer` killed
} data;

static int is_player(int player_id)
{
    return player_id >= 0 && player_id < player_context_num_players() && player_id < MAX_PLAYERS;
}

static int is_soldier(const figure *f)
{
    return f->type == FIGURE_FORT_LEGIONARY || f->type == FIGURE_FORT_JAVELIN || f->type == FIGURE_FORT_MOUNTED;
}

static int is_alive(const figure *f)
{
    return f->state == FIGURE_STATE_ALIVE && f->action_state != FIGURE_ACTION_149_CORPSE;
}

void mp_battle_reset(void)
{
    memset(&data, 0, sizeof(data));
}

// ---------- losses of each side ----------

// whether a soldier of the legion other than `except` is still alive
static int legion_has_soldiers(int player_id, int formation_id, int except)
{
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        const figure *f = figure_get(i);
        if (i != except && f->formation_id == formation_id && is_soldier(f) && is_alive(f)) {
            return 1;
        }
    }
    return 0;
}

void mp_battle_figure_killed(int killer, const figure *victim)
{
    int loser = FIGURE_OWNER(victim->id);
    if (!game_rules_is_multiplayer() || !is_player(killer) || !is_player(loser) || killer == loser ||
        !mp_war_is_fighting(killer, loser) || !is_soldier(victim)) {
        return;
    }
    data.soldiers[killer][loser]++;
    int formation_id = victim->formation_id;
    if (formation_id > 0 && FORMATION_OWNER(formation_id) == loser && formation_get(formation_id)->is_legion &&
        !legion_has_soldiers(loser, formation_id, victim->id)) {
        data.legions[killer][loser]++;
    }
}

int mp_battle_soldiers_killed(int killer, int victim)
{
    return is_player(killer) && is_player(victim) ? data.soldiers[killer][victim] : 0;
}

int mp_battle_legions_destroyed(int killer, int victim)
{
    return is_player(killer) && is_player(victim) ? data.legions[killer][victim] : 0;
}

int mp_battle_soldiers_lost(int player_id)
{
    int total = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        total += mp_battle_soldiers_killed(p, player_id);
    }
    return total;
}

int mp_battle_legions_lost(int player_id)
{
    int total = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        total += mp_battle_legions_destroyed(p, player_id);
    }
    return total;
}

int mp_battle_enemy_soldiers_killed(int player_id)
{
    int total = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        total += mp_battle_soldiers_killed(player_id, p);
    }
    return total;
}

int mp_battle_enemy_legions_destroyed(int player_id)
{
    int total = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        total += mp_battle_legions_destroyed(player_id, p);
    }
    return total;
}

// ---------- triumphal arches ----------

void mp_battle_triumph(int player_id)
{
    if (!game_rules_is_multiplayer() || !is_player(player_id)) {
        return;
    }
    // as a distant battle won in the original (city/military.c): the arch is earned in the city of the victor, and
    // his build menu offers it when he is the local player
    int previous = player_context_current();
    player_context_switch(player_id);
    city_buildings_earn_triumphal_arch();
    building_menu_update();
    player_context_switch(previous);
}

// ---------- morale ----------

void mp_battle_legion_broke(void)
{
    if (!mp_war_any_fighting()) {
        return;
    }
    int self = player_context_current_player;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (!mp_war_is_fighting(self, p)) {
            continue;
        }
        // as change_all_morale (figure/formation.c) does for the enemies of the city in the original
        for (int i = p * MAX_FORMATIONS + 1; i < (p + 1) * MAX_FORMATIONS; i++) {
            formation *m = formation_get(i);
            if (m->in_use && m->is_legion && !m->is_herd) {
                formation_change_morale(m, 10);
            }
        }
    }
}

// ---------- enemies in the city ----------

int mp_battle_enemy_soldiers_in_land(int player_id)
{
    if (!mp_war_any_fighting() || !is_player(player_id)) {
        return 0;
    }
    int count = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (!mp_war_is_fighting(player_id, p)) {
            continue;
        }
        for (int i = p * MAX_FIGURES + 1; i < (p + 1) * MAX_FIGURES; i++) {
            const figure *f = figure_get(i);
            if (mp_war_is_enemy_soldier(player_id, f) && map_grid_is_valid_offset(f->grid_offset) &&
                mp_territory_owner(f->grid_offset) == player_id) {
                count++;
            }
        }
    }
    return count;
}

void mp_battle_count_enemies_in_city(void)
{
    int enemies = mp_battle_enemy_soldiers_in_land(player_context_current_player);
    for (int i = 0; i < enemies; i++) {
        city_figures_add_enemy();
    }
}

// ---------- saved game ----------

void mp_battle_save_state(buffer *buf)
{
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, MAX_PLAYERS);
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            buffer_write_i32(buf, data.soldiers[a][b]);
            buffer_write_i32(buf, data.legions[a][b]);
        }
    }
}

void mp_battle_load_state(buffer *buf)
{
    mp_battle_reset();
    int version = buffer_read_i32(buf);
    int players = buffer_read_i32(buf);
    if (version < 1 || version > STATE_VERSION || players != MAX_PLAYERS) {
        return;
    }
    for (int a = 0; a < MAX_PLAYERS; a++) {
        for (int b = 0; b < MAX_PLAYERS; b++) {
            data.soldiers[a][b] = buffer_read_i32(buf);
            data.legions[a][b] = buffer_read_i32(buf);
        }
    }
    if (buf->overflow) {
        mp_battle_reset();
    }
}
