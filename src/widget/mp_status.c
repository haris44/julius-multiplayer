#include "mp_status.h"

#include "city/view.h"
#include "core/encoding.h"
#include "core/image_group.h"
#include "core/string.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/colors.h"
#include "mp/fog.h"
#include "mp/endgame.h"
#include "mp/lockstep.h"
#include "mp/missionary.h"
#include "mp/session.h"
#include "mp/territory.h"
#include "translation/translation.h"


// D-091: the banner is drawn as the top menu of the original game, at the bottom of the city view: its stone strip,
// the labels in its font, the values in its dark panels
#define TOP_MENU_TILE 24
#define TOP_MENU_TILES 8
#define TOP_MENU_DARK_PANEL 14
#define BANNER_Y 28
#define TEXT_Y 5
#define PANEL_PADDING 10
#define GAP 8
#define ENTRY_GAP 12
#define WRATH_BAR_WIDTH 50
#define COLOR_WRATH_BACKGROUND 0x484848
#define COLOR_WRATH_LOW 0x40a040
#define COLOR_WRATH_WARNING 0xd0b030
#define COLOR_WRATH_ULTIMATUM 0xc04040

// a dark panel of the top menu (Dn, Pop, date), of any width: its rounded ends, its middle repeated in between
static void draw_dark_panel(int x, int y, int width)
{
    int image_id = image_group(GROUP_TOP_MENU) + TOP_MENU_DARK_PANEL;
    const image *img = image_get(image_id);
    int image_width = img->width;
    int height = img->height;
    if (width <= image_width) {
        int half = width / 2;
        graphics_set_clip_rectangle(x, y, half, height);
        image_draw(image_id, x, y);
        graphics_set_clip_rectangle(x + half, y, width - half, height);
        image_draw(image_id, x + width - image_width, y);
    } else {
        int end = image_width / 4; // the ends are narrower than a quarter of the image: its middle is plain
        graphics_set_clip_rectangle(x, y, image_width / 2, height);
        image_draw(image_id, x, y);
        for (int middle = x + image_width / 2; middle < x + width - end; middle += image_width / 2) {
            int part = x + width - end - middle;
            graphics_set_clip_rectangle(middle, y, part < image_width / 2 ? part : image_width / 2, height);
            image_draw(image_id, middle - end, y);
        }
        graphics_set_clip_rectangle(x + width - end, y, end, height);
        image_draw(image_id, x + width - image_width, y);
    }
    graphics_reset_clip_rectangle();
}

// the stone strip of the top menu, from the left of the screen
static void draw_strip(int y, int width)
{
    int image_base = image_group(GROUP_TOP_MENU);
    graphics_set_clip_rectangle(0, y, width, TOP_MENU_TILE);
    for (int i = 0; i * TOP_MENU_TILE < width; i++) {
        image_draw(image_base + i % TOP_MENU_TILES, i * TOP_MENU_TILE, y);
    }
    graphics_reset_clip_rectangle();
}

// Caesar's wrath (M10.5, CESAR §11): "Wrath 12" and a gauge, green, then yellow from the warning, red from the
// ultimatum
static void draw_wrath(const uint8_t *label, int x, int y)
{
    text_draw(label, x, y + TEXT_Y, FONT_NORMAL_PLAIN, COLOR_WHITE);
    int width = text_get_width(label, FONT_NORMAL_PLAIN) + 4;
    int wrath = mp_caesar_wrath();
    color_t color = wrath >= MP_CAESAR_WRATH_ULTIMATUM ? COLOR_WRATH_ULTIMATUM :
        wrath >= MP_CAESAR_WRATH_WARNING ? COLOR_WRATH_WARNING : COLOR_WRATH_LOW;
    graphics_fill_rect(x + width, y + 8, WRATH_BAR_WIDTH, 9, COLOR_WRATH_BACKGROUND);
    int filled = WRATH_BAR_WIDTH * wrath / MP_CAESAR_WRATH_MAX;
    if (filled > 0) {
        graphics_fill_rect(x + width, y + 8, filled, 9, color);
    }
    // its whole length shows on the dark panel
    graphics_draw_inset_rect(x + width - 1, y + 7, WRATH_BAR_WIDTH + 2, 11);
}

// a player without any mission nor land is told how to found his city again (D-037, D-045)
static void draw_objective(int banner_y)
{
    static const int LINES[] = { TR_MP_OBJECTIVE_MISSION_1, TR_MP_OBJECTIVE_MISSION_2 };
    int num_lines = sizeof(LINES) / sizeof(LINES[0]);
    int width = 0;
    for (int i = 0; i < num_lines; i++) {
        int line_width = text_get_width(translation_for(LINES[i]), FONT_NORMAL_PLAIN);
        width = line_width > width ? line_width : width;
    }
    int height = 16 * num_lines + 8;
    int y = banner_y - height - 4;
    graphics_fill_rect(4, y, width + 16, height, COLOR_BLACK);
    for (int i = 0; i < num_lines; i++) {
        text_draw(translation_for(LINES[i]), 12, y + 6 + 16 * i, FONT_NORMAL_PLAIN, COLOR_FONT_YELLOW);
    }
}

// before the game and after it stopped: the state of the network, at the top of the screen
static void draw_network_state(mp_lockstep_state state)
{
    uint8_t text[200] = { 0 };
    encoding_from_utf8(mp_lockstep_status(), text, sizeof(text));
    color_t color = state == MP_LOCKSTEP_WAITING_FOR_PLAYERS ? COLOR_FONT_YELLOW : COLOR_FONT_RED;
    int width = text_get_width(text, FONT_NORMAL_PLAIN) + 2 * PANEL_PADDING;
    int x = (screen_width() - width) / 2;
    draw_dark_panel(x, BANNER_Y, width);
    text_draw(text, x + PANEL_PADDING, BANNER_Y + TEXT_Y, FONT_NORMAL_PLAIN, color);
}

void widget_mp_status_draw(void)
{
    mp_lockstep_state state = mp_lockstep_get_state();
    if (state == MP_LOCKSTEP_OFF) {
        return;
    }
    // the lobby shows the state of the network itself, under its rules: no banner over its title on a small screen
    if (state != MP_LOCKSTEP_RUNNING && window_is(WINDOW_MP_LOBBY)) {
        return;
    }
    if (state != MP_LOCKSTEP_RUNNING) {
        draw_network_state(state);
        return;
    }
    // on a small screen, the windows over the city (advisors, build menus) reach the bottom: no banner over them
    if (screen_height() < 600 && !window_is(WINDOW_CITY) && !window_is(WINDOW_CITY_MILITARY)) {
        return;
    }
    // who we are, on the stone as "Pop" in the top menu of a small screen
    uint8_t reminder[64] = { 0 };
    string_copy(translation_for(TR_MP_PLAYER), reminder, 40);
    string_from_int(reminder + string_length(reminder), mp_session_local_player_id() + 1, 0);
    int reminder_width = text_get_width(reminder, FONT_NORMAL_GREEN) + GAP;

    // the laurels of the players in one dark panel, each in its color, public (D-053); the provisional score of a game
    // ended by years is hidden by the fog of war (D-038)
    int num_players = player_context_num_players();
    uint8_t scores[PLAYER_CONTEXT_MAX_PLAYERS][32];
    int scores_width = 0;
    int by_years = game_rules_end_condition() == GAME_END_SCORE;
    if (num_players > 1 || mp_caesar_is_active()) {
        for (int p = 0; p < num_players; p++) {
            scores[p][0] = 0;
            if (by_years && mp_fog_is_active() && p != mp_session_local_player_id()) {
                continue;
            }
            string_copy(translation_for(TR_MP_STATS_PLAYER_SHORT), scores[p], 8);
            int length = string_length(scores[p]);
            length += string_from_int(scores[p] + length, p + 1, 0);
            scores[p][length++] = ' ';
            string_from_int(scores[p] + length, by_years ? mp_endgame_live_score(p) : mp_caesar_laurels(p) / 10, 0);
            scores_width += text_get_width(scores[p], FONT_NORMAL_PLAIN) + (scores_width ? ENTRY_GAP : 0);
        }
    }
    int scores_panel = scores_width ? scores_width + 2 * PANEL_PADDING + GAP : 0;

    // the score of Caesar's heir, on the stone
    uint8_t target[64] = { 0 };
    int target_width = 0;
    int target_short_length = 0; // the target without its unit
    if (game_rules_end_condition() == GAME_END_CAESAR) {
        string_copy(translation_for(TR_MP_BANNER_TARGET), target, 40);
        string_from_int(target + string_length(target), game_rules_caesar_score(), 0);
        target_short_length = string_length(target);
        string_copy(translation_for(TR_MP_LAURELS), target + target_short_length, 60 - target_short_length);
        target_width = text_get_width(target, FONT_NORMAL_GREEN) + GAP;
    }
    // the wrath and its gauge, in a dark panel
    uint8_t wrath[32] = { 0 };
    int wrath_panel = 0;
    if (mp_caesar_is_active()) {
        string_copy(translation_for(TR_MP_BANNER_WRATH), wrath, 20);
        string_from_int(wrath + string_length(wrath), mp_caesar_wrath() / 10, 0);
        wrath_panel = text_get_width(wrath, FONT_NORMAL_PLAIN) + 4 + WRATH_BAR_WIDTH + 2 * PANEL_PADDING + GAP;
    }
    uint8_t paused[32] = { 0 };
    int paused_panel = 0;
    if (mp_lockstep_is_paused()) {
        encoding_from_utf8("PAUSE", paused, sizeof(paused));
        paused_panel = text_get_width(paused, FONT_NORMAL_PLAIN) + 2 * PANEL_PADDING + GAP;
    }

    // never under the sidebar, whose width depends on the screen: first the target without its unit, then without the
    // target, then without the reminder of who we are (the colors of the laurels tell it)
    int ignored, strip_width;
    city_view_get_viewport(&ignored, &ignored, &strip_width, &ignored);
    int available = strip_width - GAP;
    int fixed = GAP + scores_panel + wrath_panel + paused_panel;
    if (fixed + reminder_width + target_width > available && target_short_length) {
        target[target_short_length] = 0;
        target_width = text_get_width(target, FONT_NORMAL_GREEN) + GAP;
    }
    if (fixed + reminder_width + target_width > available) {
        target[0] = 0;
        target_width = 0;
    }
    if (fixed + reminder_width + target_width > available) {
        reminder[0] = 0;
        reminder_width = 0;
    }

    // during the game, at the bottom of the view: the warnings of the game use the top
    int y = screen_height() - TOP_MENU_TILE;
    draw_strip(y, strip_width);
    int x = GAP;
    if (reminder[0]) {
        text_draw(reminder, x, y + TEXT_Y, FONT_NORMAL_GREEN, 0);
        x += reminder_width;
    }
    if (scores_panel) {
        draw_dark_panel(x, y, scores_panel - GAP);
        int x_score = x + PANEL_PADDING;
        for (int p = 0; p < num_players; p++) {
            if (scores[p][0]) {
                // text_draw returns the width with a space after the text: the measured width keeps the panel exact
                text_draw(scores[p], x_score, y + TEXT_Y, FONT_NORMAL_PLAIN, mp_colors_player(p));
                x_score += text_get_width(scores[p], FONT_NORMAL_PLAIN) + ENTRY_GAP;
            }
        }
        x += scores_panel;
    }
    if (target[0]) {
        text_draw(target, x, y + TEXT_Y, FONT_NORMAL_GREEN, 0);
        x += target_width;
    }
    if (wrath_panel) {
        draw_dark_panel(x, y, wrath_panel - GAP);
        draw_wrath(wrath, x + PANEL_PADDING, y);
        x += wrath_panel;
    }
    if (paused_panel) {
        draw_dark_panel(x, y, paused_panel - GAP);
        text_draw(paused, x + PANEL_PADDING, y + TEXT_Y, FONT_NORMAL_PLAIN, COLOR_FONT_YELLOW);
    }
    if (mp_territory_is_active() && !mp_mission_exists() && !mp_territory_owns_land()) {
        draw_objective(y);
    }
}
