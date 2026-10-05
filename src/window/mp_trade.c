#include "mp_trade.h"

#include "core/image.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/actions.h"
#include "mp/colors.h"
#include "mp/session.h"
#include "mp/trade.h"
#include "translation/translation.h"
#include "window/advisors.h"

#define PRICE_STEP 10
#define ROW_HEIGHT 19
#define FIRST_ROW_Y 122
#define NUM_RESOURCES (RESOURCE_MAX - RESOURCE_MIN)

static void button_partner(int index, int param2);
static void button_route(int param1, int param2);
static void button_price(int resource, int delta);
static void button_buy(int resource, int param2);
static void button_back(int param1, int param2);

// tabs of the other players, the route, three buttons per resource, back
#define MAX_BUTTONS (PLAYER_CONTEXT_MAX_PLAYERS + 2 + 3 * NUM_RESOURCES)
static generic_button buttons[MAX_BUTTONS];

static struct {
    int partner;
    int num_buttons;
    int focus_button_id;
} data;

static int local_player(void)
{
    return mp_session_local_player_id();
}

static void add_button(int x, int y, int width, int height, void (*callback)(int, int), int p1, int p2)
{
    generic_button b = { x, y, width, height, callback, button_none, p1, p2 };
    buttons[data.num_buttons++] = b;
}

static void init(void)
{
    data.num_buttons = 0;
    if (data.partner == local_player() || data.partner >= player_context_num_players()) {
        data.partner = local_player() == 0 ? 1 : 0;
    }
    int tab = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p != local_player()) {
            add_button(32 + 150 * tab++, 44, 140, 22, button_partner, p, 0);
        }
    }
    add_button(400, 72, 208, 22, button_route, 0, 0);
    for (int i = 0; i < NUM_RESOURCES; i++) {
        int resource = RESOURCE_MIN + i;
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        add_button(196, y, 20, 18, button_price, resource, -PRICE_STEP);
        add_button(266, y, 20, 18, button_price, resource, PRICE_STEP);
        add_button(500, y, 90, 18, button_buy, resource, 0);
    }
    add_button(240, 448, 160, 22, button_back, 0, 0);
}

static void draw_background(void)
{
    window_draw_underlying_window();
}

static void draw_player_name(int player_id, int x, int y, int width, int centered)
{
    uint8_t text[64] = { 0 };
    int length = string_length(translation_for(TR_MP_PLAYER));
    string_copy(translation_for(TR_MP_PLAYER), text, 60);
    string_from_int(text + length, player_id + 1, 0);
    if (centered) {
        text_draw_centered(text, x, y, width, FONT_NORMAL_PLAIN, mp_colors_player(player_id));
    } else {
        text_draw(text, x, y, FONT_NORMAL_PLAIN, mp_colors_player(player_id));
    }
}

static int route_status_text(void)
{
    int me = local_player();
    if (mp_trade_route_is_open(me, data.partner)) {
        return TR_MP_ROUTE_STATUS_OPEN;
    } else if (mp_trade_route_is_proposed(me, data.partner)) {
        return TR_MP_ROUTE_STATUS_WAITING;
    } else if (mp_trade_route_is_proposed(data.partner, me)) {
        return TR_MP_ROUTE_STATUS_OFFERED;
    }
    return TR_MP_ROUTE_STATUS_CLOSED;
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MP_TRADE_TITLE), 0, 14, 640, FONT_LARGE_BLACK, 0);
    int me = local_player();

    // tabs
    int button = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p == me) {
            continue;
        }
        const generic_button *b = &buttons[button++];
        if (p == data.partner) {
            inner_panel_draw(b->x, b->y, b->width / 16, 1);
        }
        button_border_draw(b->x, b->y, b->width, b->height, data.focus_button_id == button);
        draw_player_name(p, b->x, b->y + 6, b->width, 1);
    }

    // route
    int width = text_draw(translation_for(TR_MP_ROUTE), 32, 78, FONT_NORMAL_BLACK, 0);
    text_draw(translation_for(route_status_text()), 32 + width, 78, FONT_NORMAL_BLACK, 0);
    const generic_button *route = &buttons[button++];
    button_border_draw(route->x, route->y, route->width, route->height, data.focus_button_id == button);
    text_draw_centered(translation_for(mp_trade_route_is_proposed(me, data.partner) ?
        TR_MP_ROUTE_WITHDRAW : TR_MP_ROUTE_PROPOSE), route->x, route->y + 6, route->width, FONT_NORMAL_BLACK, 0);

    // prices
    inner_panel_draw(16, 100, 38, 20);
    text_draw_centered(translation_for(TR_MP_TRADE_I_SELL), 196, 106, 90, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_HE_SELLS), 296, 106, 90, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_EMPIRE), 396, 106, 90, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_I_BUY), 500, 106, 90, FONT_SMALL_PLAIN, COLOR_WHITE);
    for (int i = 0; i < NUM_RESOURCES; i++) {
        int resource = RESOURCE_MIN + i;
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        image_draw(image_group(GROUP_RESOURCE_ICONS) + resource + resource_image_offset(resource, RESOURCE_IMAGE_ICON),
            24, y - 2);
        lang_text_draw(23, resource, 56, y + 4, FONT_NORMAL_WHITE);
        for (int k = 0; k < 3; k++) {
            const generic_button *b = &buttons[button++];
            button_border_draw(b->x, b->y, b->width, b->height, data.focus_button_id == button);
            if (k == 0) {
                text_draw_centered((const uint8_t *) "-", b->x, b->y + 4, b->width, FONT_NORMAL_WHITE, 0);
            } else if (k == 1) {
                text_draw_centered((const uint8_t *) "+", b->x, b->y + 4, b->width, FONT_NORMAL_WHITE, 0);
            } else {
                text_draw_centered(translation_for(mp_trade_buys_from(me, data.partner, resource) ? TR_MP_YES :
                    TR_MP_NO), b->x, b->y + 4, b->width, FONT_NORMAL_WHITE, 0);
            }
        }
        text_draw_number_centered(mp_trade_price(me, data.partner, resource), 216, y + 4, 50, FONT_NORMAL_WHITE);
        // his price, and the one of the empire with its surcharge: the cheaper source supplies (D-048)
        int his_price = mp_trade_price(data.partner, me, resource);
        int cheaper = mp_trade_cheaper_player(resource) == data.partner;
        text_draw_number_centered(his_price, 296, y + 4, 90, cheaper ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
        if (empire_can_import_resource_potentially(resource)) {
            text_draw_number_centered(trade_price_buy(resource), 396, y + 4, 90,
                mp_trade_buys_from(me, data.partner, resource) && !cheaper ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
        } else {
            text_draw_centered((const uint8_t *) "-", 396, y + 4, 90, FONT_NORMAL_WHITE, 0);
        }
    }
    text_draw(translation_for(TR_MP_TRADE_RULE), 24, 424, FONT_SMALL_PLAIN, 0);
    text_draw(translation_for(TR_MP_TRADE_RULE_2), 24, 436, FONT_SMALL_PLAIN, 0);

    const generic_button *back = &buttons[button++];
    button_border_draw(back->x, back->y, back->width, back->height, data.focus_button_id == button);
    text_draw_centered(translation_for(TR_MP_BACK), back->x, back->y + 6, back->width, FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, data.num_buttons, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_back(0, 0);
    }
}

static void button_partner(int index, int param2)
{
    data.partner = index;
    window_invalidate();
}

static void button_route(int param1, int param2)
{
    mp_action_propose_route(data.partner, !mp_trade_route_is_proposed(local_player(), data.partner));
}

static void button_price(int resource, int delta)
{
    int price = mp_trade_price(local_player(), data.partner, resource) + delta;
    mp_action_set_sell_price(data.partner, resource, price < 1 ? 1 : price);
}

static void button_buy(int resource, int param2)
{
    mp_action_set_buys_from(data.partner, resource, !mp_trade_buys_from(local_player(), data.partner, resource));
}

static void button_back(int param1, int param2)
{
    window_advisors_show_advisor(ADVISOR_TRADE);
}

void window_mp_trade_show(void)
{
    window_type window = {
        WINDOW_MP_TRADE,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}
