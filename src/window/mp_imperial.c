#include "mp_imperial.h"

#include "city/population.h"
#include "core/image_group.h"
#include "core/string.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/colors.h"
#include "mp/expedition.h"
#include "mp/session.h"
#include "mp/wrath.h"
#include "scenario/property.h"
#include "translation/translation.h"

// As the tables of the original advisors: the column headers above the inner panel, the rows 20 pixels apart
#define HEADER_Y 80
#define PANEL_Y 94
#define PANEL_HEIGHT_BLOCKS 13
#define FIRST_ROW_Y 104
#define ROW_HEIGHT 20

// left column: the notes of the city (first page), the laurels by source (second page)
#define LEFT_X 48
#define BAR_X 214
#define BAR_WIDTH 100
#define NOTE_VALUE_RIGHT 352
#define SOURCE_VALUE_RIGHT 312

// right column: the ranking (first page); the shares of the wrath and the army (second page), nearer the laurels by
// source, which end sooner than the notes and their bars
#define RIGHT_END 592
#define PLACE_X 384
#define NAME_X 402
#define DETAILS_X 360

// Caesar's wrath, under the notes (first page)
#define WRATH_Y 214

#define PAGE_BUTTON_X 392
#define PAGE_BUTTON_Y 274
#define PAGE_BUTTON_WIDTH 200
#define PAGE_BUTTON_HEIGHT 20

#define COLOR_BAR_BACKGROUND 0x303030
#define COLOR_BAR_LOW 0xc04040
#define COLOR_BAR_MEDIUM 0xd0b030
#define COLOR_BAR_HIGH 0x40a040

enum {
    PAGE_SUMMARY = 0,
    PAGE_DETAILS = 1
};

static void button_page(int param1, int param2);

static generic_button page_buttons[] = {
    {PAGE_BUTTON_X, PAGE_BUTTON_Y, PAGE_BUTTON_WIDTH, PAGE_BUTTON_HEIGHT, button_page, button_none, 0, 0},
};

static struct {
    int page;
    int focus_button_id;
} data;

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

static void draw_right(const uint8_t *text, int right, int y, font_t font, color_t color)
{
    text_draw(text, right - text_get_width(text, font), y, font, color);
}

// laurels from tenths, rounded towards zero, right-aligned: "-15"
static void draw_laurels_right(int tenths, int right, int y, font_t font, color_t color)
{
    uint8_t number[16];
    string_from_int(number, tenths / 10, 0);
    draw_right(number, right, y, font, color);
}

// "Laurels: 45 of 1000, next rank at 200", as the original's "Emperor's favor 71"; then one line of the rule, as the
// original's word of the emperor
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
    uint8_t rule[200] = { 0 };
    if (is_judged_to_the_score()) {
        append(rule, sizeof(rule), translation_for(TR_MP_IMPERIAL_RULE_1));
        append_number(rule, sizeof(rule), game_rules_caesar_score());
        append(rule, sizeof(rule), translation_for(TR_MP_IMPERIAL_RULE_2));
    } else {
        append(rule, sizeof(rule), translation_for(TR_MP_IMPERIAL_RULE_ENDLESS));
    }
    text_draw(rule, 60, 60, FONT_NORMAL_BLACK, 0);
}

static color_t bar_color(int note)
{
    return note < 34 ? COLOR_BAR_LOW : note < 67 ? COLOR_BAR_MEDIUM : COLOR_BAR_HIGH;
}

static void draw_bar(int x, int y, int width, int value, int max, color_t color)
{
    graphics_fill_rect(x, y, width, 10, COLOR_BAR_BACKGROUND);
    int filled = value <= 0 ? 0 : value >= max ? width : width * value / max;
    if (filled > 0) {
        graphics_fill_rect(x, y, filled, 10, color);
    }
    graphics_draw_inset_rect(x - 1, y - 1, width + 2, 12);
}

// "Player 2"
static int draw_player_name(int player_id, int x, int y)
{
    uint8_t name[64];
    string_copy(translation_for(TR_MP_PLAYER), name, 40);
    string_from_int(name + string_length(name), player_id + 1, 0);
    return text_draw(name, x, y, FONT_NORMAL_PLAIN, mp_colors_player(player_id));
}

// the five notes, each with its bar
static void draw_notes(int player_id)
{
    for (int note = 0; note < MP_NOTE_MAX; note++) {
        int y = FIRST_ROW_Y + ROW_HEIGHT * note;
        int value = mp_caesar_note(player_id, note);
        text_draw(translation_for(NOTE_NAMES[note]), LEFT_X, y, FONT_NORMAL_WHITE, 0);
        draw_bar(BAR_X, y + 1, BAR_WIDTH, value, 100, bar_color(value));
        uint8_t number[16];
        string_from_int(number, value, 0);
        draw_right(number, NOTE_VALUE_RIGHT, y, FONT_NORMAL_WHITE, 0);
    }
}

// the ranking of the province, public (D-053): each player in his color, the local one marked
static void draw_ranking(int local_player)
{
    int players[PLAYER_CONTEXT_MAX_PLAYERS];
    int count = mp_caesar_ranking(players);
    for (int i = 0; i < count; i++) {
        int p = players[i];
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        uint8_t place[8];
        int length = string_from_int(place, i + 1, 0);
        place[length++] = '.';
        place[length] = 0;
        text_draw(place, PLACE_X, y, FONT_NORMAL_WHITE, 0);
        int width = draw_player_name(p, NAME_X, y);
        if (p == local_player) {
            text_draw(translation_for(TR_MP_YOU), NAME_X + width + 4, y + 1, FONT_SMALL_PLAIN, COLOR_FONT_LIGHT_GRAY);
        }
        draw_laurels_right(mp_caesar_laurels(p), RIGHT_END, y, FONT_NORMAL_WHITE, 0);
    }
}

// "Enemy of Rome: Player 2, Player 3" (M10.4), or nothing
static void append_enemies_of_rome(uint8_t *line, int size)
{
    int found = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (!mp_wrath_is_enemy_of_rome(p)) {
            continue;
        }
        append(line, size, found ? (const uint8_t *) ", " : translation_for(TR_MP_STATS_ENEMY_OF_ROME));
        append(line, size, translation_for(TR_MP_PLAYER));
        append_number(line, size, p + 1);
        found = 1;
    }
    if (found) {
        append(line, size, (const uint8_t *) ".");
    }
}

// Caesar's wrath (M10.5) under the notes, as one more row: its gauge with the steps of the warning and of the
// ultimatum; then what threatens: the deadline of an ultimatum, the enemies of Rome, the disgrace after a punitive
// expedition (M10.6). The shares are on the second page.
static void draw_wrath(void)
{
    int wrath = mp_caesar_wrath();
    color_t color = wrath >= MP_CAESAR_WRATH_ULTIMATUM ? COLOR_BAR_LOW :
        wrath >= MP_CAESAR_WRATH_WARNING ? COLOR_BAR_MEDIUM : COLOR_BAR_HIGH;
    text_draw(translation_for(TR_MP_STATS_WRATH), LEFT_X, WRATH_Y, FONT_NORMAL_WHITE, 0);
    draw_bar(BAR_X, WRATH_Y + 1, BAR_WIDTH, wrath, MP_CAESAR_WRATH_MAX, color);
    int warning_x = BAR_X + BAR_WIDTH * MP_CAESAR_WRATH_WARNING / MP_CAESAR_WRATH_MAX;
    int ultimatum_x = BAR_X + BAR_WIDTH * MP_CAESAR_WRATH_ULTIMATUM / MP_CAESAR_WRATH_MAX;
    graphics_draw_vertical_line(warning_x, WRATH_Y + 1, WRATH_Y + 10, COLOR_BLACK);
    graphics_draw_vertical_line(ultimatum_x, WRATH_Y + 1, WRATH_Y + 10, COLOR_BLACK);
    uint8_t number[16];
    string_from_int(number, wrath / 10, 0);
    if (wrath >= MP_CAESAR_WRATH_WARNING) {
        draw_right(number, NOTE_VALUE_RIGHT, WRATH_Y, FONT_NORMAL_PLAIN,
            wrath >= MP_CAESAR_WRATH_ULTIMATUM ? COLOR_FONT_RED : COLOR_FONT_ORANGE);
    } else {
        draw_right(number, NOTE_VALUE_RIGHT, WRATH_Y, FONT_NORMAL_WHITE, 0);
    }

    int y = WRATH_Y + ROW_HEIGHT + 2;
    uint8_t line[200] = { 0 };
    if (mp_wrath_ultimatum_months() > 0) {
        append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_ULTIMATUM));
        append_number(line, sizeof(line), mp_wrath_ultimatum_months());
        append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_MONTHS));
        append(line, sizeof(line), (const uint8_t *) ". ");
    }
    append_enemies_of_rome(line, sizeof(line));
    if (line[0]) {
        y += text_draw_multiline(line, LEFT_X, y, RIGHT_END - LEFT_X, FONT_NORMAL_PLAIN, COLOR_FONT_RED);
    }
    if (mp_expedition_disgrace_months() > 0) {
        line[0] = 0;
        append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_DISGRACE));
        append_number(line, sizeof(line), mp_expedition_disgrace_months());
        append(line, sizeof(line), translation_for(TR_MP_IMPERIAL_MONTHS));
        // left of the page button
        text_draw_multiline(line, LEFT_X, y, PAGE_BUTTON_X - 16 - LEFT_X, FONT_NORMAL_PLAIN, COLOR_FONT_RED);
    }
}

static void draw_summary(int player_id)
{
    text_draw(translation_for(TR_MP_NOTES_TITLE), LEFT_X, HEADER_Y, FONT_SMALL_PLAIN, 0);
    text_draw(translation_for(TR_MP_RANKING), PLACE_X, HEADER_Y, FONT_SMALL_PLAIN, 0);
    draw_right(translation_for(TR_MP_NOTES_LAURELS), RIGHT_END, HEADER_Y, FONT_SMALL_PLAIN, 0);
    inner_panel_draw(32, PANEL_Y, 36, PANEL_HEIGHT_BLOCKS);
    draw_notes(player_id);
    draw_ranking(player_id);
    draw_wrath();
}

// the laurels of the city by source, since the start: its five notes, then those of Caesar
static void draw_sources(int player_id)
{
    static const struct {
        mp_laurels_source source;
        translation_key name;
        int always;
    } SOURCES[] = {
        {MP_LAURELS_PROSPERITY, TR_MP_NOTE_PROSPERITY, 1},
        {MP_LAURELS_TRADE, TR_MP_NOTE_TRADE, 1},
        {MP_LAURELS_HOUSING, TR_MP_NOTE_HOUSING, 1},
        {MP_LAURELS_CULTURE, TR_MP_NOTE_CULTURE, 1},
        {MP_LAURELS_GREATNESS, TR_MP_NOTE_GREATNESS, 1},
        {MP_LAURELS_GIFTS, TR_MP_STATS_GIFTS, 1},
        {MP_LAURELS_WARS, TR_MP_STATS_WARS, 1},
        {MP_LAURELS_ARMY, TR_MP_STATS_ARMY, 1},
        {MP_LAURELS_PUNISHMENT, TR_MP_STATS_EXPEDITION, 0},
    };
    int y = FIRST_ROW_Y;
    for (int i = 0; i < (int) (sizeof(SOURCES) / sizeof(SOURCES[0])); i++) {
        int tenths = mp_caesar_laurels_from(player_id, SOURCES[i].source);
        if (!SOURCES[i].always && !tenths) {
            continue;
        }
        text_draw(translation_for(SOURCES[i].name), LEFT_X, y, FONT_NORMAL_WHITE, 0);
        draw_laurels_right(tenths, SOURCE_VALUE_RIGHT, y, FONT_NORMAL_WHITE, 0);
        y += ROW_HEIGHT;
    }
}

// each player's share of Caesar's wrath (M10.5), and the enemies of Rome
static void draw_shares(void)
{
    for (int p = 0; p < player_context_num_players(); p++) {
        int y = FIRST_ROW_Y + ROW_HEIGHT * p;
        int width = draw_player_name(p, DETAILS_X, y);
        if (mp_wrath_is_enemy_of_rome(p)) {
            text_draw(translation_for(TR_MP_IMPERIAL_ENEMY_OF_ROME), DETAILS_X + width + 6, y + 1, FONT_SMALL_PLAIN,
                COLOR_FONT_RED);
        }
        draw_laurels_right(mp_caesar_belligerence(p), RIGHT_END, y, FONT_NORMAL_WHITE, 0);
    }
}

// the legions of the city and those Caesar tolerates (M10.7)
static void draw_army(int player_id)
{
    int legions = mp_caesar_army_legions(player_id);
    int population = city_population();
    int cost = mp_caesar_army_cost(legions, population);
    int y = FIRST_ROW_Y + ROW_HEIGHT * PLAYER_CONTEXT_MAX_PLAYERS + 12;
    uint8_t number[16];
    text_draw(translation_for(TR_MP_STATS_LEGIONS), DETAILS_X, y, FONT_NORMAL_WHITE, 0);
    string_from_int(number, legions, 0);
    draw_right(number, RIGHT_END, y, FONT_NORMAL_WHITE, 0);
    y += ROW_HEIGHT;
    text_draw(translation_for(TR_MP_STATS_TOLERATED), DETAILS_X, y, FONT_NORMAL_WHITE, 0);
    string_from_int(number, mp_caesar_tolerated_legions(population), 0);
    draw_right(number, RIGHT_END, y, FONT_NORMAL_WHITE, 0);
    y += ROW_HEIGHT;
    text_draw(translation_for(TR_MP_STATS_MONTHLY_COST), DETAILS_X, y, cost ? FONT_NORMAL_PLAIN : FONT_NORMAL_WHITE,
        cost ? COLOR_FONT_ORANGE : 0);
    if (cost) {
        draw_laurels_right(cost, RIGHT_END, y, FONT_NORMAL_PLAIN, COLOR_FONT_ORANGE);
    } else {
        draw_right(string_from_ascii("0"), RIGHT_END, y, FONT_NORMAL_WHITE, 0);
    }
}

static void draw_details(int player_id)
{
    text_draw(translation_for(TR_MP_STATS_SOURCES), LEFT_X, HEADER_Y, FONT_SMALL_PLAIN, 0);
    draw_right(translation_for(TR_MP_NOTES_LAURELS), SOURCE_VALUE_RIGHT, HEADER_Y, FONT_SMALL_PLAIN, 0);
    text_draw(translation_for(TR_MP_IMPERIAL_SHARES), DETAILS_X, HEADER_Y, FONT_SMALL_PLAIN, 0);
    inner_panel_draw(32, PANEL_Y, 36, PANEL_HEIGHT_BLOCKS);
    draw_sources(player_id);
    draw_shares();
    draw_army(player_id);
}

void window_mp_imperial_draw_background(int height)
{
    outer_panel_draw(0, 0, 40, height);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 2, 10, 10);
    text_draw(scenario_player_name(), 60, 12, FONT_LARGE_BLACK, 0);
    int player_id = mp_session_local_player_id();
    draw_header(player_id);
    if (data.page == PAGE_DETAILS) {
        draw_details(player_id);
    } else {
        draw_summary(player_id);
    }
}

void window_mp_imperial_draw_foreground(void)
{
    button_border_draw(PAGE_BUTTON_X, PAGE_BUTTON_Y, PAGE_BUTTON_WIDTH, PAGE_BUTTON_HEIGHT,
        data.focus_button_id == 1);
    translation_key label = data.page == PAGE_DETAILS ? TR_MP_STATS_PAGE_SUMMARY : TR_MP_STATS_PAGE_DETAILS;
    text_draw_centered(translation_for(label), PAGE_BUTTON_X, PAGE_BUTTON_Y + 5, PAGE_BUTTON_WIDTH,
        FONT_NORMAL_WHITE, 0);
}

int window_mp_imperial_handle_mouse(const mouse *m)
{
    return generic_buttons_handle_mouse(m, 0, 0, page_buttons, 1, &data.focus_button_id);
}

static void button_page(int param1, int param2)
{
    data.page = data.page == PAGE_DETAILS ? PAGE_SUMMARY : PAGE_DETAILS;
    window_invalidate();
}
