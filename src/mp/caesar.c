#include "caesar.h"

#include "building/building.h"
#include "city/emperor.h"
#include "city/finance.h"
#include "city/population.h"
#include "city/ratings.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "game/time.h"
#include "mp/caesar_rules.h"
#include "mp/session.h"

#include <string.h>

#define STATE_VERSION 3 // 2: notes, remainders of the laurels of the city, ranks; 3: waiting time of the gifts
#define MAX_LETTERS 8
#define HIGHEST_HOUSE_LEVEL 19

static const int NOTE_WEIGHTS[MP_NOTE_MAX] = {
    MP_CAESAR_WEIGHT_PROSPERITY,
    MP_CAESAR_WEIGHT_TRADE,
    MP_CAESAR_WEIGHT_HOUSING,
    MP_CAESAR_WEIGHT_CULTURE,
    MP_CAESAR_WEIGHT_GREATNESS
};

static struct {
    int laurels[PLAYER_CONTEXT_MAX_PLAYERS][MP_LAURELS_MAX_SOURCE];
    int wrath;
    int belligerence[PLAYER_CONTEXT_MAX_PLAYERS];
    int notes[PLAYER_CONTEXT_MAX_PLAYERS][MP_NOTE_MAX];
    int remainders[PLAYER_CONTEXT_MAX_PLAYERS][MP_NOTE_MAX]; // hundredths of a tenth not yet given
    int ranks[PLAYER_CONTEXT_MAX_PLAYERS];
    int gift_cooldown[PLAYER_CONTEXT_MAX_PLAYERS]; // months before the next gift counts
} data;

// display state of this computer, never saved
static struct {
    mp_caesar_letter items[MAX_LETTERS];
    int count;
} letters;

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

int mp_caesar_note(int player_id, mp_note note)
{
    return is_player(player_id) && note >= 0 && note < MP_NOTE_MAX ? data.notes[player_id][note] : 0;
}

// ---------- gifts and salary (CESAR §6.1) ----------

int mp_caesar_gift_laurels(int size)
{
    static const int GIFT_LAURELS[] = MP_CAESAR_GIFT_LAURELS;
    return size >= GIFT_MODEST && size <= GIFT_LAVISH ? GIFT_LAURELS[size] : 0;
}

int mp_caesar_gift_cooldown(int player_id)
{
    return is_player(player_id) ? data.gift_cooldown[player_id] : 0;
}

int mp_caesar_gift_sent(int player_id, int size)
{
    int tenths = mp_caesar_gift_laurels(size);
    if (!mp_caesar_is_active() || !is_player(player_id) || !tenths || data.gift_cooldown[player_id] > 0) {
        return 0;
    }
    data.laurels[player_id][MP_LAURELS_GIFTS] += tenths;
    data.gift_cooldown[player_id] = MP_CAESAR_GIFT_PERIOD;
    return tenths;
}

int mp_caesar_salary_rank_limit(int player_id)
{
    return mp_caesar_rank(player_id);
}

void mp_caesar_limit_salary(void)
{
    if (!mp_caesar_is_active()) {
        return;
    }
    // the amount is always the one of the table for the rank chosen, never above the rank of the city
    int rank = city_emperor_salary_rank();
    int limit = mp_caesar_salary_rank_limit(player_context_current_player);
    city_emperor_set_salary_rank(rank > limit ? limit : rank);
}

// ---------- the notes of the current city (CESAR §5) ----------

static int clamp_note(int value)
{
    return value < 0 ? 0 : value > 100 ? 100 : value;
}

// 100 * value / (value + reference): 50 at the reference, never quite 100
static int saturate(int value, int reference)
{
    if (value <= 0) {
        return 0;
    }
    return (int) (100LL * value / ((long long) value + reference));
}

// average quality of the houses, weighed by their inhabitants: a level is worth 100 * (level / 19)^2
static int housing_note(void)
{
    long long points = 0;
    long long people = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->house_population <= 0) {
            continue;
        }
        int level = b->subtype.house_level;
        level = level < 0 ? 0 : level > HIGHEST_HOUSE_LEVEL ? HIGHEST_HOUSE_LEVEL : level;
        points += (long long) b->house_population * 100 * level * level;
        people += b->house_population;
    }
    if (!people) {
        return 0;
    }
    return clamp_note((int) (points / (people * HIGHEST_HOUSE_LEVEL * HIGHEST_HOUSE_LEVEL)));
}

// provisional until M9.2: exports of about the last twelve months, the year begun plus the part of the last one
static int trade_note(void)
{
    int months_this_year = game_time_month();
    int exports = city_finance_overview_this_year()->income.exports +
        city_finance_overview_last_year()->income.exports * (12 - months_this_year) / 12;
    return saturate(exports, MP_CAESAR_TRADE_REFERENCE);
}

static void compute_notes(int *notes)
{
    notes[MP_NOTE_PROSPERITY] = clamp_note(city_rating_prosperity());
    notes[MP_NOTE_TRADE] = trade_note();
    notes[MP_NOTE_HOUSING] = housing_note();
    notes[MP_NOTE_CULTURE] = clamp_note(city_rating_culture());
    notes[MP_NOTE_GREATNESS] = saturate(city_population(), MP_CAESAR_GREATNESS_REFERENCE);
}

// ---------- ranks ----------

int mp_caesar_rank_laurels(int rank)
{
    return game_rules_caesar_score() * rank / (MP_CAESAR_NUM_RANKS - 1);
}

static int rank_of_laurels(int tenths)
{
    int rank = 0;
    while (rank < MP_CAESAR_NUM_RANKS - 1 && tenths >= 10 * mp_caesar_rank_laurels(rank + 1)) {
        rank++;
    }
    return rank;
}

int mp_caesar_rank(int player_id)
{
    return is_player(player_id) ? rank_of_laurels(mp_caesar_laurels(player_id)) : 0;
}

static int ranks_before(int a, int b)
{
    int laurels_a = mp_caesar_laurels(a), laurels_b = mp_caesar_laurels(b);
    if (laurels_a != laurels_b) {
        return laurels_a > laurels_b;
    }
    int city_a = mp_caesar_city_laurels(a), city_b = mp_caesar_city_laurels(b);
    if (city_a != city_b) {
        return city_a > city_b;
    }
    return a < b;
}

int mp_caesar_ranking(int *players)
{
    int count = player_context_num_players();
    for (int i = 0; i < count; i++) {
        int p = i;
        int j = i;
        while (j > 0 && ranks_before(p, players[j - 1])) {
            players[j] = players[j - 1];
            j--;
        }
        players[j] = p;
    }
    return count;
}

void mp_caesar_update_city_month(void)
{
    if (!mp_caesar_is_active()) {
        return;
    }
    int p = player_context_current_player;
    if (data.gift_cooldown[p] > 0) {
        data.gift_cooldown[p]--;
    }
    compute_notes(data.notes[p]);
    for (int note = 0; note < MP_NOTE_MAX; note++) {
        // tenths of a laurel: weight * note / 100, the remainder kept for the next months
        int hundredths = NOTE_WEIGHTS[note] * data.notes[p][note] + data.remainders[p][note];
        data.laurels[p][note] += hundredths / 100;
        data.remainders[p][note] = hundredths % 100;
    }
    int rank = rank_of_laurels(mp_caesar_laurels(p));
    if (rank > data.ranks[p] && p == mp_session_local_player_id()) {
        mp_caesar_add_letter(MP_CAESAR_LETTER_PROMOTION, rank);
    }
    data.ranks[p] = rank;
}

// ---------- wrath ----------

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

// ---------- letters to the local player ----------

int mp_caesar_num_letters(void)
{
    return letters.count;
}

const mp_caesar_letter *mp_caesar_get_letter(int index)
{
    return index >= 0 && index < letters.count ? &letters.items[index] : 0;
}

void mp_caesar_add_letter(mp_caesar_letter_type type, int param)
{
    if (letters.count == MAX_LETTERS) {
        return;
    }
    // a newer promotion replaces the one not read yet
    for (int i = 0; i < letters.count; i++) {
        if (type == MP_CAESAR_LETTER_PROMOTION && letters.items[i].type == type) {
            letters.items[i].param = param;
            return;
        }
    }
    letters.items[letters.count].type = type;
    letters.items[letters.count].param = param;
    letters.count++;
}

void mp_caesar_remove_first_letter(void)
{
    if (letters.count > 0) {
        letters.count--;
        memmove(&letters.items[0], &letters.items[1], letters.count * sizeof(mp_caesar_letter));
    }
}

// ---------- state ----------

void mp_caesar_reset(void)
{
    memset(&data, 0, sizeof(data));
    letters.count = 0;
}

static void write_table(buffer *buf, const int *values, int count)
{
    for (int i = 0; i < count; i++) {
        buffer_write_i32(buf, values[i]);
    }
}

static void read_table(buffer *buf, int *values, int count)
{
    for (int i = 0; i < count; i++) {
        values[i] = buffer_read_i32(buf);
    }
}

void mp_caesar_save_state(buffer *buf)
{
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, PLAYER_CONTEXT_MAX_PLAYERS);
    buffer_write_i32(buf, MP_LAURELS_MAX_SOURCE);
    write_table(buf, &data.laurels[0][0], PLAYER_CONTEXT_MAX_PLAYERS * MP_LAURELS_MAX_SOURCE);
    buffer_write_i32(buf, data.wrath);
    write_table(buf, data.belligerence, PLAYER_CONTEXT_MAX_PLAYERS);
    write_table(buf, &data.notes[0][0], PLAYER_CONTEXT_MAX_PLAYERS * MP_NOTE_MAX);
    write_table(buf, &data.remainders[0][0], PLAYER_CONTEXT_MAX_PLAYERS * MP_NOTE_MAX);
    write_table(buf, data.ranks, PLAYER_CONTEXT_MAX_PLAYERS);
    write_table(buf, data.gift_cooldown, PLAYER_CONTEXT_MAX_PLAYERS);
}

void mp_caesar_load_state(buffer *buf)
{
    memset(&data, 0, sizeof(data));
    letters.count = 0;
    int version = buffer_read_i32(buf);
    int num_players = buffer_read_i32(buf);
    int num_sources = buffer_read_i32(buf);
    if (version < 1 || version > STATE_VERSION || num_players != PLAYER_CONTEXT_MAX_PLAYERS ||
        num_sources != MP_LAURELS_MAX_SOURCE) {
        return;
    }
    read_table(buf, &data.laurels[0][0], PLAYER_CONTEXT_MAX_PLAYERS * MP_LAURELS_MAX_SOURCE);
    data.wrath = buffer_read_i32(buf);
    read_table(buf, data.belligerence, PLAYER_CONTEXT_MAX_PLAYERS);
    if (version >= 2) {
        read_table(buf, &data.notes[0][0], PLAYER_CONTEXT_MAX_PLAYERS * MP_NOTE_MAX);
        read_table(buf, &data.remainders[0][0], PLAYER_CONTEXT_MAX_PLAYERS * MP_NOTE_MAX);
        read_table(buf, data.ranks, PLAYER_CONTEXT_MAX_PLAYERS);
        if (version >= 3) {
            read_table(buf, data.gift_cooldown, PLAYER_CONTEXT_MAX_PLAYERS);
        }
    } else {
        for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
            data.ranks[p] = rank_of_laurels(sum_laurels(p, 0, MP_LAURELS_MAX_SOURCE));
        }
    }
}
