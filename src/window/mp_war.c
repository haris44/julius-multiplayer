#include "mp_war.h"

#include "core/string.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/actions.h"
#include "mp/colors.h"
#include "mp/session.h"
#include "mp/war.h"
#include "translation/translation.h"
#include "window/advisors.h"

#define MAX_OTHERS (PLAYER_CONTEXT_MAX_PLAYERS - 1)
#define FIRST_ROW_Y 150
#define ROW_HEIGHT 80
#define BUTTON_WIDTH 130
#define BUTTON_HEIGHT 25
#define BRUTAL_X 206
#define HONOURABLE_X 342
#define PEACE_X 478
#define BUTTONS_DY 32

enum {
    ACTION_BRUTAL = 0,
    ACTION_HONOURABLE = 1,
    ACTION_PEACE = 2
};

static void button_war(int row, int action);
static void button_ok(int param1, int param2);

static generic_button buttons[] = {
    {BRUTAL_X, FIRST_ROW_Y + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 0, ACTION_BRUTAL},
    {HONOURABLE_X, FIRST_ROW_Y + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 0, ACTION_HONOURABLE},
    {PEACE_X, FIRST_ROW_Y + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 0, ACTION_PEACE},
    {BRUTAL_X, FIRST_ROW_Y + ROW_HEIGHT + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 1,
        ACTION_BRUTAL},
    {HONOURABLE_X, FIRST_ROW_Y + ROW_HEIGHT + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 1,
        ACTION_HONOURABLE},
    {PEACE_X, FIRST_ROW_Y + ROW_HEIGHT + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 1,
        ACTION_PEACE},
    {BRUTAL_X, FIRST_ROW_Y + 2 * ROW_HEIGHT + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 2,
        ACTION_BRUTAL},
    {HONOURABLE_X, FIRST_ROW_Y + 2 * ROW_HEIGHT + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 2,
        ACTION_HONOURABLE},
    {PEACE_X, FIRST_ROW_Y + 2 * ROW_HEIGHT + BUTTONS_DY, BUTTON_WIDTH, BUTTON_HEIGHT, button_war, button_none, 2,
        ACTION_PEACE},
    {256, 440, 128, BUTTON_HEIGHT, button_ok, button_none, 0, 0},
};
#define NUM_BUTTONS (sizeof(buttons) / sizeof(generic_button))
#define OK_BUTTON (NUM_BUTTONS - 1)

static struct {
    int focus_button_id;
    int armed_player;  // a declaration waits for its second click: the other player, or -1
    int armed_form;
} data;

int window_mp_war_is_available(void)
{
    return game_rules_is_multiplayer() && player_context_num_players() > 1;
}

// the other player shown on a row, or -1
static int player_of_row(int row)
{
    int local = mp_session_local_player_id();
    int index = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p == local) {
            continue;
        }
        if (index == row) {
            return p;
        }
        index++;
    }
    return -1;
}

static int action_available(int other, int action)
{
    int status = mp_war_status(mp_session_local_player_id(), other);
    switch (action) {
        case ACTION_BRUTAL:
            return status != MP_WAR_FIGHTING; // during the notice too: the defender strikes first
        case ACTION_HONOURABLE:
            return status == MP_WAR_PEACE;
        case ACTION_PEACE:
            return status != MP_WAR_PEACE;
        default:
            return 0;
    }
}

static void draw_status(int local, int other, int y)
{
    int status = mp_war_status(local, other);
    uint8_t text[100] = { 0 };
    color_t color = COLOR_WHITE;
    if (status == MP_WAR_PEACE) {
        string_copy(translation_for(TR_MP_WAR_STATUS_PEACE), text, 100);
    } else if (status == MP_WAR_NOTICE) {
        string_copy(translation_for(TR_MP_WAR_STATUS_NOTICE), text, 100);
        string_from_int(text + string_length(text), mp_war_days_until_fighting(local, other), 0);
        string_copy(translation_for(TR_MP_WAR_DAYS), text + string_length(text), 100 - string_length(text));
        color = COLOR_FONT_ORANGE;
    } else {
        string_copy(translation_for(TR_MP_WAR_STATUS_FIGHTING), text, 100);
        color = COLOR_FONT_RED;
    }
    text_draw(text, 200, y, FONT_NORMAL_PLAIN, color);
    // the proposals of peace, on the right
    if (mp_war_peace_proposed(other, local)) {
        text_draw(translation_for(TR_MP_WAR_PEACE_OFFERED), 420, y, FONT_NORMAL_GREEN, 0);
    } else if (mp_war_peace_proposed(local, other)) {
        text_draw(translation_for(TR_MP_WAR_PEACE_SENT), 420, y, FONT_NORMAL_PLAIN, COLOR_FONT_LIGHT_GRAY);
    }
}

static void draw_button(const generic_button *b, translation_key key, int focused)
{
    button_border_draw(b->x, b->y, b->width, b->height, focused);
    text_draw_centered(translation_for(key), b->x, b->y + 6, b->width, FONT_NORMAL_WHITE, 0);
}

static void draw_row(int row, int other, int local)
{
    int y = FIRST_ROW_Y + ROW_HEIGHT * row;
    uint8_t name[64] = { 0 };
    string_copy(translation_for(TR_MP_PLAYER), name, 50);
    string_from_int(name + string_length(name), other + 1, 0);
    text_draw(name, 32, y + 6, FONT_NORMAL_PLAIN, mp_colors_player(other));
    draw_status(local, other, y + 6);
    if (action_available(other, ACTION_BRUTAL) || action_available(other, ACTION_HONOURABLE)) {
        text_draw(translation_for(TR_MP_WAR_DECLARE), 32, y + BUTTONS_DY + 6, FONT_NORMAL_WHITE, 0);
    }
    for (int action = ACTION_BRUTAL; action <= ACTION_PEACE; action++) {
        if (!action_available(other, action)) {
            continue;
        }
        int index = 3 * row + action;
        translation_key key;
        if (action != ACTION_PEACE && data.armed_player == other &&
            data.armed_form == (action == ACTION_BRUTAL ? MP_WAR_BRUTAL : MP_WAR_HONOURABLE)) {
            key = TR_MP_WAR_CONFIRM;
        } else if (action == ACTION_BRUTAL) {
            key = TR_MP_WAR_BRUTAL;
        } else if (action == ACTION_HONOURABLE) {
            key = TR_MP_WAR_HONOURABLE;
        } else {
            key = mp_war_peace_proposed(local, other) ? TR_MP_WAR_WITHDRAW_PEACE : TR_MP_WAR_PROPOSE_PEACE;
        }
        draw_button(&buttons[index], key, data.focus_button_id == index + 1);
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
    graphics_in_dialog();
    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MP_WAR_TITLE), 0, 16, 640, FONT_LARGE_BLACK, 0);
    text_draw_multiline(translation_for(TR_MP_WAR_RULE), 32, 54, 576, FONT_NORMAL_BLACK, 0);
    text_draw_multiline(translation_for(TR_MP_WAR_RULE_2), 32, 94, 576, FONT_NORMAL_BLACK, 0);

    inner_panel_draw(16, FIRST_ROW_Y - 10, 38, 16);
    int local = mp_session_local_player_id();
    int rows = 0;
    for (int row = 0; row < MAX_OTHERS; row++) {
        int other = player_of_row(row);
        if (other >= 0) {
            draw_row(row, other, local);
            rows++;
        }
    }
    if (!rows) {
        text_draw(translation_for(TR_MP_WAR_ALONE), 32, FIRST_ROW_Y + 6, FONT_NORMAL_WHITE, 0);
    }
    const generic_button *ok = &buttons[OK_BUTTON];
    button_border_draw(ok->x, ok->y, ok->width, ok->height, data.focus_button_id == OK_BUTTON + 1);
    text_draw_centered(translation_for(TR_BUTTON_OK), ok->x, ok->y + 6, ok->width, FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, NUM_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        button_ok(0, 0);
    }
}

static void button_war(int row, int action)
{
    int other = player_of_row(row);
    if (other < 0 || !action_available(other, action)) {
        return;
    }
    if (action == ACTION_PEACE) {
        data.armed_player = -1;
        mp_action_propose_peace(other, !mp_war_peace_proposed(mp_session_local_player_id(), other));
    } else {
        int form = action == ACTION_BRUTAL ? MP_WAR_BRUTAL : MP_WAR_HONOURABLE;
        if (data.armed_player == other && data.armed_form == form) {
            data.armed_player = -1; // second click: declared
            mp_action_declare_war(other, form);
        } else {
            data.armed_player = other;
            data.armed_form = form;
        }
    }
    window_invalidate();
}

static void button_ok(int param1, int param2)
{
    data.armed_player = -1;
    window_advisors_show_advisor(ADVISOR_MILITARY);
}

void window_mp_war_show(void)
{
    window_type window = {
        WINDOW_MP_WAR,
        draw_background,
        draw_foreground,
        handle_input
    };
    data.focus_button_id = 0;
    data.armed_player = -1;
    window_show(&window);
}
