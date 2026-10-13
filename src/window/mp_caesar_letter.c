#include "mp_caesar_letter.h"

#include "building/construction.h"
#include "core/image_group.h"
#include "core/lang.h"
#include "core/string.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/session.h"
#include "translation/translation.h"
#include "window/city.h"

#define TEXT_SIZE 1200
#define RANK_NAMES_GROUP 32

static void button_ok(int param1, int param2);

static generic_button buttons[] = {
    {224, 376, 192, 25, button_ok, button_none, 0, 0},
};

static int focus_button_id;

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

static int is_judged_to_the_score(void)
{
    return game_rules_end_condition() == GAME_END_CAESAR;
}

// "player 1, player 2 and player 3"
static void append_players(uint8_t *text, int players)
{
    int count = 0;
    for (int p = 0; p < 8; p++) {
        count += (players >> p) & 1;
    }
    int written = 0;
    for (int p = 0; p < 8; p++) {
        if (!((players >> p) & 1)) {
            continue;
        }
        if (written) {
            append(text, written == count - 1 ? translation_for(TR_MP_WAR_AND) : (const uint8_t *) ", ");
        }
        append(text, translation_for(TR_MP_LETTER_PLAYER));
        append_number(text, p + 1);
        written++;
    }
}

// the letters of Caesar's wrath (M10.5): the warning, the ultimatum, the peace he imposes
static int compose_wrath(const mp_caesar_letter *letter, uint8_t *text)
{
    switch (letter->type) {
        case MP_CAESAR_LETTER_WARNING:
            append(text, translation_for(TR_MP_LETTER_WARNING_1));
            append_players(text, letter->param);
            append(text, translation_for(TR_MP_LETTER_WARNING_2));
            return 1;
        case MP_CAESAR_LETTER_ULTIMATUM:
            append(text, translation_for(TR_MP_LETTER_ULTIMATUM_1));
            append_number(text, MP_CAESAR_ULTIMATUM_DELAY);
            append(text, translation_for(TR_MP_LETTER_ULTIMATUM_2));
            if (letter->param >= 0) {
                append(text, translation_for(TR_MP_LETTER_ULTIMATUM_3));
                append_number(text, letter->param + 1);
                append(text, translation_for(TR_MP_LETTER_ULTIMATUM_4));
            }
            return 1;
        case MP_CAESAR_LETTER_PEACE_IMPOSED:
            append(text, translation_for(TR_MP_LETTER_PEACE_IMPOSED_1));
            append_players(text, letter->param);
            append(text, translation_for(TR_MP_LETTER_PEACE_IMPOSED_2));
            return 1;
        default:
            return 0;
    }
}

static void compose(const mp_caesar_letter *letter, uint8_t *text)
{
    text[0] = 0;
    if (compose_wrath(letter, text)) {
        return;
    }
    if (letter->type == MP_CAESAR_LETTER_WELCOME) {
        if (is_judged_to_the_score()) {
            append(text, translation_for(TR_MP_LETTER_WELCOME_1));
            append_number(text, game_rules_caesar_score());
            append(text, translation_for(TR_MP_LETTER_WELCOME_2));
        } else {
            append(text, translation_for(TR_MP_LETTER_WELCOME_ENDLESS));
        }
        return;
    }
    // a new rank: its name in the texts of the game, then what is left to the score
    append(text, translation_for(TR_MP_LETTER_PROMOTION_1));
    append(text, lang_get_string(RANK_NAMES_GROUP, letter->param));
    if (is_judged_to_the_score()) {
        int left = game_rules_caesar_score() - mp_caesar_laurels(mp_session_local_player_id()) / 10;
        append(text, translation_for(TR_MP_LETTER_PROMOTION_2));
        append_number(text, left > 0 ? left : 0);
        append(text, translation_for(TR_MP_LETTER_PROMOTION_3));
    } else {
        append(text, translation_for(TR_MP_LETTER_PROMOTION_ENDLESS));
    }
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_reset_clip_rectangle();
    graphics_shade_rect(0, 0, screen_width(), screen_height(), 0);
}

static void draw_foreground(void)
{
    const mp_caesar_letter *letter = mp_caesar_get_letter(0);
    if (!letter) {
        return;
    }
    graphics_in_dialog();
    outer_panel_draw(64, 48, 32, 24);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 2, 84, 64);
    text_draw_centered(translation_for(TR_MP_LETTER_TITLE), 64, 70, 512, FONT_LARGE_BLACK, 0);
    static uint8_t text[TEXT_SIZE];
    compose(letter, text);
    text_draw_multiline(text, 96, 128, 448, FONT_NORMAL_BLACK, 0);
    text_draw(translation_for(TR_MP_LETTER_SIGNATURE), 448, 336, FONT_LARGE_BLACK, 0);
    button_border_draw(buttons[0].x, buttons[0].y, buttons[0].width, buttons[0].height, focus_button_id == 1);
    text_draw_centered(translation_for(TR_BUTTON_OK), buttons[0].x, buttons[0].y + 7, buttons[0].width,
        FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, 1, &focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        button_ok(0, 0);
    }
}

static void button_ok(int param1, int param2)
{
    mp_caesar_remove_first_letter();
    window_city_show();
}

void window_mp_caesar_letter_show_pending(void)
{
    if (!mp_caesar_num_letters() || !window_is(WINDOW_CITY) || building_construction_in_progress()) {
        return;
    }
    window_type window = {
        WINDOW_MP_CAESAR_LETTER,
        draw_background,
        draw_foreground,
        handle_input
    };
    focus_button_id = 0;
    window_show(&window);
}
