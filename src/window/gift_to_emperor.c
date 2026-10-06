#include "gift_to_emperor.h"

#include "city/emperor.h"
#include "game/resource.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/actions.h"
#include "mp/caesar.h"
#include "mp/session.h"
#include "translation/translation.h"
#include "window/advisors.h"

static void button_set_gift(int gift_id, int param2);
static void button_send_gift(int param1, int param2);
static void button_cancel(int param1, int param2);

static generic_button buttons[] = {
    {208, 213, 300, 20, button_set_gift, button_none, 1, 0},
    {208, 233, 300, 20, button_set_gift, button_none, 2, 0},
    {208, 253, 300, 20, button_set_gift, button_none, 3, 0},
    {118, 336, 260, 20, button_send_gift, button_none, 0, 0},
    {400, 336, 160, 20, button_cancel, button_none, 0, 0},
};

static int focus_button_id;
// multiplayer: the size chosen is the state of the window, never written in the simulation
static int mp_selected_size;

static int is_multiplayer(void)
{
    return mp_caesar_is_active();
}

static int selected_size(void)
{
    return is_multiplayer() ? mp_selected_size : city_emperor_selected_gift_size();
}

static int gift_cost(int size)
{
    return is_multiplayer() ? city_emperor_gift_cost(size) : city_emperor_get_gift(size)->cost;
}

static int can_send(int size)
{
    return is_multiplayer() ? gift_cost(size) <= city_emperor_personal_savings() : city_emperor_can_send_gift(size);
}

static void init(void)
{
    if (is_multiplayer()) {
        if (mp_selected_size == GIFT_LAVISH && !can_send(GIFT_LAVISH)) {
            mp_selected_size = GIFT_GENEROUS;
        }
        if (mp_selected_size == GIFT_GENEROUS && !can_send(GIFT_GENEROUS)) {
            mp_selected_size = GIFT_MODEST;
        }
        return;
    }
    city_emperor_init_selected_gift();
}

// "+4 laurels" after the cost of a gift that counts (multiplayer)
static void draw_laurels(int size, int x, int y, font_t font)
{
    if (is_multiplayer() && !mp_caesar_gift_cooldown(mp_session_local_player_id())) {
        int width = text_draw_number(mp_caesar_gift_laurels(size) / 10, '+', "", x, y, font);
        text_draw(translation_for(TR_MP_GIFT_LAURELS), x + width, y, font, 0);
    }
}

static void draw_background(void)
{
    window_advisors_draw_dialog_background();

    graphics_in_dialog();

    outer_panel_draw(96, 144, 30, 15);
    image_draw(image_group(GROUP_RESOURCE_ICONS) + RESOURCE_DENARII, 112, 160);
    lang_text_draw_centered(52, 69, 144, 160, 416, FONT_LARGE_BLACK);

    if (is_multiplayer()) {
        // one gift counts every 12 months (CESAR §6.1): say when
        int months = mp_caesar_gift_cooldown(mp_session_local_player_id());
        if (months > 0) {
            int width = text_draw(translation_for(TR_MP_GIFT_NEXT_COUNTS), 144, 304, FONT_NORMAL_BLACK, 0);
            lang_text_draw_amount(8, 4, months, 144 + width, 304, FONT_NORMAL_BLACK);
        } else {
            text_draw_multiline(translation_for(TR_MP_GIFT_COUNTS_NOW), 144, 296, 400, FONT_NORMAL_BLACK, 0);
        }
    } else {
        int width = lang_text_draw(52, 50, 144, 304, FONT_NORMAL_BLACK);
        lang_text_draw_amount(8, 4, city_emperor_months_since_gift(), 144 + width, 304, FONT_NORMAL_BLACK);
    }
    lang_text_draw_centered(13, 4, 400, 341, 160, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    inner_panel_draw(112, 208, 28, 5);

    if (can_send(GIFT_MODEST)) {
        const emperor_gift *gift = city_emperor_get_gift(GIFT_MODEST);
        lang_text_draw(52, 63, 128, 218, FONT_NORMAL_WHITE);
        font_t font = focus_button_id == 1 ? FONT_NORMAL_RED : FONT_NORMAL_WHITE;
        int width = lang_text_draw(52, 51 + gift->id, 224, 218, font);
        width += text_draw_money(gift_cost(GIFT_MODEST), 224 + width, 218, font);
        draw_laurels(GIFT_MODEST, 232 + width, 218, font);
    } else {
        lang_text_draw_multiline(52, 70, 160, 224, 352, FONT_NORMAL_WHITE);
    }
    if (can_send(GIFT_GENEROUS)) {
        const emperor_gift *gift = city_emperor_get_gift(GIFT_GENEROUS);
        lang_text_draw(52, 64, 128, 238, FONT_NORMAL_WHITE);
        font_t font = focus_button_id == 2 ? FONT_NORMAL_RED : FONT_NORMAL_WHITE;
        int width = lang_text_draw(52, 55 + gift->id, 224, 238, font);
        width += text_draw_money(gift_cost(GIFT_GENEROUS), 224 + width, 238, font);
        draw_laurels(GIFT_GENEROUS, 232 + width, 238, font);
    }
    if (can_send(GIFT_LAVISH)) {
        const emperor_gift *gift = city_emperor_get_gift(GIFT_LAVISH);
        lang_text_draw(52, 65, 128, 258, FONT_NORMAL_WHITE);
        font_t font = focus_button_id == 3 ? FONT_NORMAL_RED : FONT_NORMAL_WHITE;
        int width = lang_text_draw(52, 59 + gift->id, 224, 258, font);
        width += text_draw_money(gift_cost(GIFT_LAVISH), 224 + width, 258, font);
        draw_laurels(GIFT_LAVISH, 232 + width, 258, font);
    }
    // can give at least one type
    if (can_send(GIFT_MODEST)) {
        lang_text_draw_centered(52, 66 + selected_size(), 118, 341, 260, FONT_NORMAL_BLACK);
        button_border_draw(118, 336, 260, 20, focus_button_id == 4);
    }
    button_border_draw(400, 336, 160, 20, focus_button_id == 5);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, 5, &focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_advisors_show();
    }
}

static void button_set_gift(int gift_id, int param2)
{
    if (is_multiplayer()) {
        if (can_send(gift_id - 1)) {
            mp_selected_size = gift_id - 1;
            window_invalidate();
        }
    } else if (city_emperor_set_gift_size(gift_id - 1)) {
        window_invalidate();
    }
}

static void button_send_gift(int param1, int param2)
{
    if (can_send(GIFT_MODEST)) {
        if (is_multiplayer()) {
            mp_action_send_gift(mp_selected_size);
        } else {
            city_emperor_send_gift();
        }
        window_advisors_show();
    }
}

static void button_cancel(int param1, int param2)
{
    window_advisors_show();
}

void window_gift_to_emperor_show(void)
{
    window_type window = {
        WINDOW_GIFT_TO_EMPEROR,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}
