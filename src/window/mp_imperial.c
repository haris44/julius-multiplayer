#include "mp_imperial.h"

#include "core/image_group.h"
#include "core/string.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/colors.h"
#include "mp/session.h"
#include "scenario/property.h"
#include "translation/translation.h"

#define NOTES_X 48
#define BAR_X 214
#define BAR_WIDTH 100
#define NOTE_VALUE_X 322
#define NOTE_LAURELS_X 360
#define RANKING_X 420
#define RANKING_LAURELS_X 568
#define FIRST_ROW_Y 122
#define ROW_HEIGHT 22

#define COLOR_BAR_BACKGROUND 0x303030
#define COLOR_BAR_LOW 0xc04040
#define COLOR_BAR_MEDIUM 0xd0b030
#define COLOR_BAR_HIGH 0x40a040

static const translation_key NOTE_NAMES[MP_NOTE_MAX] = {
    TR_MP_NOTE_PROSPERITY, TR_MP_NOTE_TRADE, TR_MP_NOTE_HOUSING, TR_MP_NOTE_CULTURE, TR_MP_NOTE_GREATNESS
};

int window_mp_imperial_is_active(void)
{
    return mp_caesar_is_active();
}

int window_mp_imperial_rank(void)
{
    return mp_caesar_rank(mp_session_local_player_id());
}

static int is_judged_to_the_score(void)
{
    return game_rules_end_condition() == GAME_END_CAESAR;
}

static void append(uint8_t *text, int size, const uint8_t *part)
{
    int length = string_length(text);
    string_copy(part, text + length, size - length);
}

static void append_number(uint8_t *text, int size, int value)
{
    int length = string_length(text);
    if (length < size - 16) {
        string_from_int(text + length, value, 0);
    }
}

// "Laurels: 45 of 1000, next rank at 200", then the rule of the judgement
static void draw_header(int player_id)
{
    uint8_t line[200] = { 0 };
    append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_LAURELS));
    append_number(line, sizeof(line), mp_caesar_laurels(player_id) / 10);
    if (is_judged_to_the_score()) {
        append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_OF));
        append_number(line, sizeof(line), game_rules_caesar_score());
    }
    int rank = mp_caesar_rank(player_id);
    if (rank < MP_CAESAR_NUM_RANKS - 1) {
        append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_NEXT_RANK));
        append_number(line, sizeof(line), mp_caesar_rank_laurels(rank + 1));
    }
    text_draw(line, 60, 44, FONT_NORMAL_BLACK, 0);
    uint8_t rule[400];
    if (is_judged_to_the_score()) {
        string_copy(translation_for(TR_MP_IMPERIAL_RULE_1), rule, 200);
        string_from_int(rule + string_length(rule), game_rules_caesar_score(), 0);
        string_copy(translation_for(TR_MP_IMPERIAL_RULE_2), rule + string_length(rule), 390 - string_length(rule));
    } else {
        string_copy(translation_for(TR_MP_IMPERIAL_RULE_ENDLESS), rule, 390);
    }
    text_draw_multiline(rule, 60, 62, 544, FONT_SMALL_PLAIN, 0);
}

static color_t bar_color(int note)
{
    return note < 34 ? COLOR_BAR_LOW : note < 67 ? COLOR_BAR_MEDIUM : COLOR_BAR_HIGH;
}

// the five notes, each with its bar and the laurels it brought; then the laurels of the city and of Caesar
static void draw_notes(int player_id)
{
    text_draw(translation_for(TR_MP_NOTES_TITLE), NOTES_X, 100, FONT_NORMAL_WHITE, 0);
    text_draw(translation_for(TR_MP_NOTES_LAURELS), NOTE_LAURELS_X - 8, 102, FONT_SMALL_PLAIN, COLOR_WHITE);
    for (int note = 0; note < MP_NOTE_MAX; note++) {
        int y = FIRST_ROW_Y + ROW_HEIGHT * note;
        int value = mp_caesar_note(player_id, note);
        text_draw(translation_for(NOTE_NAMES[note]), NOTES_X, y, FONT_NORMAL_WHITE, 0);
        graphics_fill_rect(BAR_X, y + 2, BAR_WIDTH, 10, COLOR_BAR_BACKGROUND);
        if (value > 0) {
            graphics_fill_rect(BAR_X, y + 2, BAR_WIDTH * value / 100, 10, bar_color(value));
        }
        text_draw_number(value, 0, "", NOTE_VALUE_X, y, FONT_NORMAL_WHITE);
        text_draw_number(mp_caesar_laurels_from(player_id, note) / 10, 0, "", NOTE_LAURELS_X, y, FONT_NORMAL_WHITE);
    }
    int y = FIRST_ROW_Y + ROW_HEIGHT * MP_NOTE_MAX + 8;
    int width = text_draw(translation_for(TR_MP_CITY_LAURELS), NOTES_X, y, FONT_NORMAL_WHITE, 0);
    text_draw_number(mp_caesar_city_laurels(player_id) / 10, 0, "", NOTES_X + width, y, FONT_NORMAL_WHITE);
    y += ROW_HEIGHT;
    width = text_draw(translation_for(TR_MP_CAESAR_LAURELS), NOTES_X, y, FONT_NORMAL_WHITE, 0);
    int caesar = mp_caesar_laurels(player_id) - mp_caesar_city_laurels(player_id);
    text_draw_number(caesar / 10, 0, "", NOTES_X + width, y, FONT_NORMAL_WHITE);
    text_draw(translation_for(TR_MP_CAESAR_LAURELS_SOON), NOTES_X, y + 20, FONT_SMALL_PLAIN, COLOR_WHITE);
}

// the ranking of the province, public (D-053): each player in his color, the local one marked
static void draw_ranking(int local_player)
{
    text_draw(translation_for(TR_MP_RANKING), RANKING_X, 100, FONT_NORMAL_WHITE, 0);
    int players[PLAYER_CONTEXT_MAX_PLAYERS];
    int count = mp_caesar_ranking(players);
    for (int i = 0; i < count; i++) {
        int p = players[i];
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        uint8_t name[64];
        int length = string_from_int(name, i + 1, 0);
        name[length++] = '.';
        name[length++] = ' ';
        string_copy(translation_for(TR_MP_PLAYER), name + length, 40);
        string_from_int(name + string_length(name), p + 1, 0);
        int width = text_draw(name, RANKING_X, y, FONT_NORMAL_PLAIN, mp_colors_player(p));
        if (p == local_player) {
            // on the same line: below the name, it would cover the next player
            text_draw(translation_for(TR_MP_YOU), RANKING_X + width + 4, y + 1, FONT_SMALL_PLAIN,
                COLOR_FONT_LIGHT_GRAY);
        }
        text_draw_number(mp_caesar_laurels(p) / 10, 0, "", RANKING_LAURELS_X, y, FONT_NORMAL_WHITE);
    }
}

void window_mp_imperial_draw_background(int height)
{
    outer_panel_draw(0, 0, 40, height);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 2, 10, 10);
    text_draw(scenario_player_name(), 60, 12, FONT_LARGE_BLACK, 0);
    int player_id = mp_session_local_player_id();
    draw_header(player_id);
    inner_panel_draw(32, 90, 36, 14);
    draw_notes(player_id);
    draw_ranking(player_id);
}
