#include "mp_results.h"

#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/discovery.h"
#include "mp/endgame.h"
#include "mp/lockstep.h"
#include "mp/session.h"
#include "translation/translation.h"
#include "window/city.h"
#include "window/main_menu.h"

static void button_watch(int param1, int param2);
static void button_main_menu(int param1, int param2);

static generic_button buttons[] = {
    {112, 360, 192, 25, button_watch, button_none, 0, 0},
    {336, 360, 192, 25, button_main_menu, button_none, 0, 0},
};

static int focus_button_id;

static void draw_background(void)
{
    window_draw_underlying_window();
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    outer_panel_draw(80, 80, 30, 21);
    text_draw_centered(translation_for(TR_MP_RESULTS_TITLE), 80, 96, 480, FONT_LARGE_BLACK, 0);
    int local = mp_session_local_player_id();
    // with Caesar: the laurels of each city, the winner is his heir (doc/mp/CESAR.md §4.3)
    int caesar = game_rules_end_condition() == GAME_END_CAESAR;
    for (int p = 0; p < player_context_num_players(); p++) {
        int y = 150 + 32 * p;
        font_t font = p == mp_endgame_winner() ? FONT_NORMAL_WHITE : FONT_NORMAL_BLACK;
        if (p == mp_endgame_winner()) {
            inner_panel_draw(104, y - 6, 27, 2);
        }
        int width = text_draw(translation_for(TR_MP_PLAYER), 120, y, font, 0);
        width += text_draw_number(p + 1, 0, " ", 120 + width, y, font);
        if (p == local) {
            text_draw(translation_for(TR_MP_YOU), 120 + width, y, font, 0);
        }
        width = text_draw(translation_for(caesar ? TR_MP_LAURELS_LABEL : TR_MP_SCORE), 280, y, font, 0);
        text_draw_number(mp_endgame_score(p), 0, "", 280 + width, y, font);
        if (p == mp_endgame_winner()) {
            text_draw(translation_for(caesar ? TR_MP_HEIR : TR_MP_WINNER), 400, y, font, 0);
        }
    }
    text_draw_multiline(translation_for(caesar ? TR_MP_CAESAR_RESULT_RULE : TR_MP_SCORE_RULE), 112, 312, 416,
        FONT_SMALL_PLAIN, 0);
    for (int i = 0; i < 2; i++) {
        button_border_draw(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, focus_button_id == i + 1);
    }
    text_draw_centered(translation_for(TR_MP_WATCH), 112, 367, 192, FONT_NORMAL_BLACK, 0);
    text_draw_centered(translation_for(TR_MP_MAIN_MENU), 336, 367, 192, FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, 2, &focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_watch(0, 0);
    }
}

static void button_watch(int param1, int param2)
{
    window_city_show();
}

static void button_main_menu(int param1, int param2)
{
    mp_lockstep_stop();
    window_main_menu_show(1);
}

void window_mp_results_show(void)
{
    window_type window = {
        WINDOW_MP_RESULTS,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
