#include "mp_price_alert.h"

#include "building/construction.h"
#include "core/image.h"
#include "core/string.h"
#include "game/resource.h"
#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/colors.h"
#include "mp/trade.h"
#include "translation/translation.h"
#include "window/city.h"
#include "window/mp_trade.h"

#define FIRST_ROW_Y 106
#define ROW_HEIGHT 22
#define MAX_ROWS 13

static void button_trade(int param1, int param2);
static void button_ok(int param1, int param2);

static generic_button buttons[] = {
    {112, 440, 192, 25, button_trade, button_none, 0, 0},
    {336, 440, 192, 25, button_ok, button_none, 0, 0},
};

static int focus_button_id;

static void draw_background(void)
{
    window_draw_underlying_window();
    // the whole city greys out behind the alert
    graphics_reset_clip_rectangle();
    graphics_shade_rect(0, 0, screen_width(), screen_height(), 0);
}

static void draw_player_name(int player_id, int x, int y)
{
    uint8_t text[64] = { 0 };
    string_copy(translation_for(TR_MP_PLAYER), text, 60);
    string_from_int(text + string_length(text), player_id + 1, 0);
    text_draw(text, x, y, FONT_NORMAL_PLAIN, mp_colors_player(player_id));
}

static void draw_alert(const mp_price_alert *alert, int y)
{
    draw_player_name(alert->seller, 32, y + 4);
    int resource = alert->resource;
    image_draw(image_group(GROUP_RESOURCE_ICONS) + resource + resource_image_offset(resource, RESOURCE_IMAGE_ICON),
        186, y - 2);
    lang_text_draw(23, resource, 218, y + 4, FONT_NORMAL_WHITE);
    text_draw_number_centered(alert->old_price, 380, y + 4, 110, FONT_NORMAL_WHITE);
    // dearer in red, cheaper in green
    if (alert->new_price > alert->old_price) {
        text_draw_number_centered_colored(alert->new_price, 490, y + 4, 110, FONT_NORMAL_PLAIN, COLOR_FONT_RED);
    } else {
        text_draw_number_centered(alert->new_price, 490, y + 4, 110, FONT_NORMAL_GREEN);
    }
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_TITLE), 0, 16, 640, FONT_LARGE_BLACK, 0);
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_INTRO), 0, 54, 640, FONT_NORMAL_BLACK, 0);

    inner_panel_draw(16, 76, 38, 20);
    text_draw(translation_for(TR_MP_PRICE_ALERT_SELLER), 32, 86, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw(translation_for(TR_MP_PRICE_ALERT_RESOURCE), 186, 86, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_OLD), 380, 86, 110, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_NEW), 490, 86, 110, FONT_SMALL_PLAIN, COLOR_WHITE);
    int count = mp_trade_num_price_alerts();
    for (int i = 0; i < count && i < MAX_ROWS; i++) {
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        if (i == MAX_ROWS - 1 && count > MAX_ROWS) {
            text_draw((const uint8_t *) "...", 32, y + 4, FONT_NORMAL_WHITE, 0);
        } else {
            draw_alert(mp_trade_price_alert(i), y);
        }
    }
    text_draw_multiline(translation_for(TR_MP_PRICE_ALERT_RULE), 32, 404, 576, FONT_NORMAL_BLACK, 0);

    for (int i = 0; i < 2; i++) {
        button_border_draw(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, focus_button_id == i + 1);
    }
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_TRADE), 112, 447, 192, FONT_NORMAL_BLACK, 0);
    text_draw_centered(translation_for(TR_BUTTON_OK), 336, 447, 192, FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, 2, &focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        button_ok(0, 0);
    }
}

static void button_trade(int param1, int param2)
{
    const mp_price_alert *alert = mp_trade_price_alert(0);
    int seller = alert ? alert->seller : 0;
    mp_trade_clear_price_alerts();
    window_mp_trade_show_partner(seller);
}

static void button_ok(int param1, int param2)
{
    mp_trade_clear_price_alerts();
    window_city_show();
}

void window_mp_price_alert_show_pending(void)
{
    if (!mp_trade_num_price_alerts() || !window_is(WINDOW_CITY) || building_construction_in_progress()) {
        return;
    }
    window_type window = {
        WINDOW_MP_PRICE_ALERT,
        draw_background,
        draw_foreground,
        handle_input
    };
    focus_button_id = 0;
    window_show(&window);
}
