#include "endgame.h"

#include "city/population.h"
#include "city/ratings.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "game/time.h"

static struct {
    int end_year; // 0 until the first tick of the game
    int over;
    int winner;
    int scores[PLAYER_CONTEXT_MAX_PLAYERS];
    int notified;
    void (*over_callback)(void);
} data;

int mp_endgame_city_score(void)
{
    return city_rating_culture() + city_rating_prosperity() + city_rating_peace() + city_population() / 100;
}

void mp_endgame_check(void)
{
    if (data.over || !game_rules_is_multiplayer() || game_rules_end_condition() != GAME_END_SCORE) {
        return;
    }
    if (!data.end_year) {
        // counted from the start of the multiplayer game, which may go on with a saved game
        data.end_year = game_time_year() + game_rules_score_years();
    }
    if (game_time_year() < data.end_year) {
        return;
    }
    int previous = player_context_current();
    data.winner = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        player_context_switch(p);
        data.scores[p] = mp_endgame_city_score();
        if (data.scores[p] > data.scores[data.winner]) {
            data.winner = p;
        }
    }
    player_context_switch(previous);
    data.over = 1;
}

int mp_endgame_is_over(void)
{
    return data.over;
}

int mp_endgame_winner(void)
{
    return data.winner;
}

int mp_endgame_score(int player_id)
{
    return player_id >= 0 && player_id < PLAYER_CONTEXT_MAX_PLAYERS ? data.scores[player_id] : 0;
}

void mp_endgame_set_over_callback(void (*callback)(void))
{
    data.over_callback = callback;
}

void mp_endgame_notify(void)
{
    if (data.over && !data.notified) {
        data.notified = 1;
        if (data.over_callback) {
            data.over_callback();
        }
    }
}

void mp_endgame_reset(void)
{
    data.end_year = 0;
    data.over = 0;
    data.winner = 0;
    data.notified = 0;
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        data.scores[p] = 0;
    }
}

void mp_endgame_save_state(buffer *buf)
{
    buffer_write_i32(buf, data.end_year);
    buffer_write_i32(buf, data.over);
    buffer_write_i32(buf, data.winner);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        buffer_write_i32(buf, data.scores[p]);
    }
}

void mp_endgame_load_state(buffer *buf)
{
    mp_endgame_reset();
    data.end_year = buffer_read_i32(buf);
    data.over = buffer_read_i32(buf);
    data.winner = buffer_read_i32(buf);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        data.scores[p] = buffer_read_i32(buf);
    }
    data.notified = data.over; // a finished game loaded again does not announce its end twice
}
