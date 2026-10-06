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

static void compose(const mp_caesar_letter *letter, uint8_t *text)
{
    text[0] = 0;
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
