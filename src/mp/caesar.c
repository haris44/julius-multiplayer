#include "caesar.h"

#include "game/player_context.h"
#include "game/rules.h"
#include "mp/caesar_rules.h"

#include <string.h>

#define STATE_VERSION 1

static struct {
    int laurels[PLAYER_CONTEXT_MAX_PLAYERS][MP_LAURELS_MAX_SOURCE];
    int wrath;
    int belligerence[PLAYER_CONTEXT_MAX_PLAYERS];
} data;

static int is_player(int player_id)
{
    return player_id >= 0 && player_id < player_context_num_players();
}

int mp_caesar_is_active(void)
{
    return game_rules_is_multiplayer();
}

void mp_caesar_add_laurels(int player_id, mp_laurels_source source, int tenths)
{
    if (mp_caesar_is_active() && is_player(player_id) && source >= 0 && source < MP_LAURELS_MAX_SOURCE) {
        data.laurels[player_id][source] += tenths;
    }
}

int mp_caesar_laurels_from(int player_id, mp_laurels_source source)
{
    if (!is_player(player_id) || source < 0 || source >= MP_LAURELS_MAX_SOURCE) {
        return 0;
    }
    return data.laurels[player_id][source];
}

static int sum_laurels(int player_id, int from, int to)
{
    int total = 0;
    for (int source = from; source < to; source++) {
        total += mp_caesar_laurels_from(player_id, source);
    }
    return total;
}

int mp_caesar_laurels(int player_id)
{
    return sum_laurels(player_id, 0, MP_LAURELS_MAX_SOURCE);
}

int mp_caesar_city_laurels(int player_id)
{
    return sum_laurels(player_id, 0, MP_LAURELS_FIRST_CAESAR_SOURCE);
}

int mp_caesar_wrath(void)
{
    return data.wrath;
}

void mp_caesar_add_wrath(int player_id, int tenths)
{
    if (!mp_caesar_is_active() || !is_player(player_id)) {
        return;
    }
    data.wrath += tenths;
    if (data.wrath < 0) {
        data.wrath = 0;
    } else if (data.wrath > MP_CAESAR_WRATH_MAX) {
        data.wrath = MP_CAESAR_WRATH_MAX;
    }
    data.belligerence[player_id] += tenths;
    if (data.belligerence[player_id] < 0) {
        data.belligerence[player_id] = 0;
    }
}

int mp_caesar_belligerence(int player_id)
{
    return is_player(player_id) ? data.belligerence[player_id] : 0;
}

void mp_caesar_reset(void)
{
    memset(&data, 0, sizeof(data));
}

void mp_caesar_save_state(buffer *buf)
{
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, PLAYER_CONTEXT_MAX_PLAYERS);
    buffer_write_i32(buf, MP_LAURELS_MAX_SOURCE);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        for (int source = 0; source < MP_LAURELS_MAX_SOURCE; source++) {
            buffer_write_i32(buf, data.laurels[p][source]);
        }
    }
    buffer_write_i32(buf, data.wrath);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        buffer_write_i32(buf, data.belligerence[p]);
    }
}

void mp_caesar_load_state(buffer *buf)
{
    mp_caesar_reset();
    int version = buffer_read_i32(buf);
    int num_players = buffer_read_i32(buf);
    int num_sources = buffer_read_i32(buf);
    if (version != STATE_VERSION || num_players != PLAYER_CONTEXT_MAX_PLAYERS ||
        num_sources != MP_LAURELS_MAX_SOURCE) {
        return;
    }
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        for (int source = 0; source < MP_LAURELS_MAX_SOURCE; source++) {
            data.laurels[p][source] = buffer_read_i32(buf);
        }
    }
    data.wrath = buffer_read_i32(buf);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        data.belligerence[p] = buffer_read_i32(buf);
    }
}
