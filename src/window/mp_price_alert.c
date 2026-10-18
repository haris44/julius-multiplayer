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
#include "graphics/image_button.h"
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

// A message of the original game (window/message_dialog.c) as large as the screen of the original game (D-049): the
// title, a line under it, the changes in a sunken panel, then the button of the trade advisor at the bottom left, with
// what it does written beside it, and the close button at the bottom right (D-093)
#define WIDTH_BLOCKS 40
#define HEIGHT_BLOCKS 30
#define PANEL_Y 76
#define PANEL_HEIGHT_BLOCKS 20
#define HEADER_Y 86
#define FIRST_ROW_Y 104
#define ROW_HEIGHT 22
#define MAX_ROWS 13
#define RULE_Y 404
// the columns: the seller, the resource, then the prices on their right edge
#define X_SELLER 32
#define X_ICON 200
#define X_RESOURCE 232
#define X_OLD_PRICE 478
#define X_NEW_PRICE 592
// the buttons, where the original message puts them
#define ADVISOR_X 16
#define ADVISOR_Y (16 * HEIGHT_BLOCKS - 40)
#define CLOSE_X (16 * WIDTH_BLOCKS - 38)
#define CLOSE_Y (16 * HEIGHT_BLOCKS - 36)
#define LABEL_X (ADVISOR_X + 36)

static void button_trade(int param1, int param2);
static void button_close(int param1, int param2);

static image_button image_buttons[] = {
    {ADVISOR_X, ADVISOR_Y, 27, 27, IB_NORMAL, GROUP_MESSAGE_ADVISOR_BUTTONS, 12, button_trade, button_none, 0, 0, 1},
    {CLOSE_X, CLOSE_Y, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4, button_close, button_none, 0, 0, 1},
};

// the text beside the button of the trade advisor opens it too
static generic_button label_button[] = {
    {LABEL_X - 4, ADVISOR_Y, 200, 27, button_trade, button_none, 0, 0},
};

static int focus_image_button_id;
static int focus_label;

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

static void draw_number_right(int value, int right, int y, font_t font, color_t color)
{
    uint8_t text[32] = { 0 };
    string_from_int(text, value, 0);
    text_draw(text, right - text_get_width(text, font), y, font, color);
}

static void draw_header_right(translation_key key, int right)
{
    const uint8_t *text = translation_for(key);
    text_draw(text, right - text_get_width(text, FONT_SMALL_PLAIN), HEADER_Y, FONT_SMALL_PLAIN, COLOR_WHITE);
}

static void draw_alert(const mp_price_alert *alert, int y)
{
    draw_player_name(alert->seller, X_SELLER, y + 5);
    int resource = alert->resource;
    image_draw(image_group(GROUP_RESOURCE_ICONS) + resource + resource_image_offset(resource, RESOURCE_IMAGE_ICON),
        X_ICON, y);
    lang_text_draw(23, resource, X_RESOURCE, y + 5, FONT_NORMAL_WHITE);
    draw_number_right(alert->old_price, X_OLD_PRICE, y + 5, FONT_NORMAL_WHITE, 0);
    // dearer in red, cheaper in green
    if (alert->new_price > alert->old_price) {
        draw_number_right(alert->new_price, X_NEW_PRICE, y + 5, FONT_NORMAL_PLAIN, COLOR_FONT_RED);
    } else {
        draw_number_right(alert->new_price, X_NEW_PRICE, y + 5, FONT_NORMAL_GREEN, 0);
    }
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    outer_panel_draw(0, 0, WIDTH_BLOCKS, HEIGHT_BLOCKS);
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_TITLE), 0, 16, 16 * WIDTH_BLOCKS, FONT_LARGE_BLACK, 0);
    text_draw_centered(translation_for(TR_MP_PRICE_ALERT_INTRO), 0, 50, 16 * WIDTH_BLOCKS, FONT_NORMAL_BLACK, 0);

    inner_panel_draw(16, PANEL_Y, WIDTH_BLOCKS - 2, PANEL_HEIGHT_BLOCKS);
    text_draw(translation_for(TR_MP_PRICE_ALERT_SELLER), X_SELLER, HEADER_Y, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw(translation_for(TR_MP_PRICE_ALERT_RESOURCE), X_RESOURCE, HEADER_Y, FONT_SMALL_PLAIN, COLOR_WHITE);
    draw_header_right(TR_MP_PRICE_ALERT_OLD, X_OLD_PRICE);
    draw_header_right(TR_MP_PRICE_ALERT_NEW, X_NEW_PRICE);
    int count = mp_trade_num_price_alerts();
    for (int i = 0; i < count && i < MAX_ROWS; i++) {
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        if (i == MAX_ROWS - 1 && count > MAX_ROWS) {
            text_draw((const uint8_t *) "...", X_SELLER, y + 5, FONT_NORMAL_WHITE, 0);
        } else {
            draw_alert(mp_trade_price_alert(i), y);
        }
    }
    text_draw_multiline(translation_for(TR_MP_PRICE_ALERT_RULE), 32, RULE_Y, 16 * WIDTH_BLOCKS - 64,
        FONT_NORMAL_BLACK, 0);

    image_buttons_draw(0, 0, image_buttons, 2);
    text_draw(translation_for(TR_MP_PRICE_ALERT_TRADE), LABEL_X, ADVISOR_Y + 8, FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (image_buttons_handle_mouse(m_dialog, 0, 0, image_buttons, 2, &focus_image_button_id)) {
        return;
    }
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, label_button, 1, &focus_label)) {
        return;
    }
    if (focus_label) {
        image_buttons[0].focused = 1; // the text and its button are one
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        button_close(0, 0);
    }
}

// the texts of the original game: the report on trade, leave this panel
static void get_tooltip(tooltip_context *c)
{
    if (focus_image_button_id == 1 || focus_label) {
        c->type = TOOLTIP_BUTTON;
        c->text_id = 74;
    } else if (focus_image_button_id == 2) {
        c->type = TOOLTIP_BUTTON;
        c->text_id = 2;
    }
}

static void button_trade(int param1, int param2)
{
    const mp_price_alert *alert = mp_trade_price_alert(0);
    int seller = alert ? alert->seller : 0;
    int resource = alert ? alert->resource : RESOURCE_MIN;
    mp_trade_clear_price_alerts();
    window_mp_trade_show_resource(seller, resource);
}

static void button_close(int param1, int param2)
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
        handle_input,
        get_tooltip
    };
    focus_image_button_id = 0;
    focus_label = 0;
    window_show(&window);
}
