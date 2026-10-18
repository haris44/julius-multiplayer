#include "mp_ratings.h"

#include "core/lang.h"
#include "core/string.h"
#include "game/player_context.h"
#include "graphics/color.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"
#include "mp/caesar.h"
#include "mp/session.h"
#include "translation/translation.h"

#define TEXT_SIZE 400

int window_mp_ratings_is_active(void)
{
    return mp_caesar_is_active();
}

int window_mp_ratings_pillar_height(void)
{
    return mp_caesar_esteem(mp_session_local_player_id());
}

int window_mp_ratings_goal_reached(void)
{
    return mp_caesar_esteem_goal_reached(mp_session_local_player_id());
}

void window_mp_ratings_draw_pillar_text(int x, int width)
{
    int player_id = mp_session_local_player_id();
    text_draw_centered(translation_for(TR_MP_NOTES_LAURELS), x, 294, width, FONT_NORMAL_BLACK, 0);
    text_draw_number_centered(mp_caesar_laurels(player_id) / 10, x, 309, width - 10, FONT_LARGE_BLACK);
    // the laurels to reach, written as the goals of the other pillars ("@60 Requis"); "1000 Requis" is wider than the
    // pillar box: smaller
    uint8_t goal[64];
    goal[0] = '@';
    int length = 1 + string_from_int(goal + 1, mp_caesar_esteem_goal(player_id), 0);
    goal[length++] = ' ';
    string_copy(lang_get_string(53, 5), goal + length, (int) sizeof(goal) - length);
    if (text_get_width(goal, FONT_NORMAL_BLACK) <= width - 10) {
        text_draw(goal, x + 5, 334, FONT_NORMAL_BLACK, 0);
    } else {
        text_draw_centered(goal + 1, x, 335, width, FONT_SMALL_PLAIN, COLOR_BLACK);
    }
}

static void append(uint8_t *text, const uint8_t *part)
{
    int length = string_length(text);
    string_copy(part, text + length, TEXT_SIZE - length);
}

static void append_number(uint8_t *text, int value)
{
    int length = string_length(text);
    if (length < TEXT_SIZE - 16) {
        string_from_int(text + length, value, 0);
    }
}

// tenths of laurels with their sign and one decimal: "+4,8"
static void append_tenths(uint8_t *text, int tenths)
{
    append(text, string_from_ascii(tenths < 0 ? "-" : "+"));
    if (tenths < 0) {
        tenths = -tenths;
    }
    append_number(text, tenths / 10);
    append(text, translation_for(TR_MP_RATINGS_DECIMAL));
    append_number(text, tenths % 10);
}

static int place_in_province(int player_id, int *num_cities)
{
    int players[PLAYER_CONTEXT_MAX_PLAYERS];
    *num_cities = mp_caesar_ranking(players);
    for (int i = 0; i < *num_cities; i++) {
        if (players[i] == player_id) {
            return i + 1;
        }
    }
    return *num_cities;
}

// as the original box of a rating: its name, then two lines: the laurels gained last month and their trend, the place
// of the city in the province, and what brings laurels when there is room for it
void window_mp_ratings_draw_explanation(int x, int y, int width)
{
    int player_id = mp_session_local_player_id();
    text_draw(translation_for(TR_MP_NOTES_LAURELS), x, y, FONT_NORMAL_WHITE, 0);

    uint8_t text[TEXT_SIZE] = { 0 };
    if (mp_caesar_history_months(player_id) < 2) {
        append(text, translation_for(TR_MP_RATINGS_NO_HISTORY));
    } else {
        append(text, translation_for(TR_MP_RATINGS_LAST_MONTH));
        append_tenths(text, mp_caesar_laurels_gained(player_id, 1));
        switch (mp_caesar_laurels_trend(player_id)) {
            case MP_CAESAR_TREND_UP: append(text, translation_for(TR_MP_RATINGS_TREND_UP)); break;
            case MP_CAESAR_TREND_DOWN: append(text, translation_for(TR_MP_RATINGS_TREND_DOWN)); break;
            default: append(text, translation_for(TR_MP_RATINGS_TREND_STEADY)); break;
        }
    }
    int num_cities;
    int place = place_in_province(player_id, &num_cities);
    if (num_cities > 1) {
        append(text, translation_for(TR_MP_RATINGS_PLACE));
        append_number(text, place);
        append(text, translation_for(TR_MP_IMPERIAL_OF));
        append_number(text, num_cities);
        append(text, string_from_ascii("."));
    } else {
        append(text, translation_for(TR_MP_RATINGS_ALONE));
    }
    // the box holds two lines under the name, as the original texts
    int length = string_length(text);
    append(text, translation_for(TR_MP_RATINGS_SOURCES));
    if (text_measure_multiline(text, width, FONT_NORMAL_WHITE, 0) > 2) {
        text[length] = 0;
    }
    text_draw_multiline(text, x, y + 15, width, FONT_NORMAL_WHITE, 0);
}
