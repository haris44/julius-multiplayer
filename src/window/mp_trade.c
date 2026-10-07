#include "mp_trade.h"

#include "city/resource.h"
#include "core/image.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "mp/actions.h"
#include "mp/colors.h"
#include "mp/session.h"
#include "mp/trade.h"
#include "translation/translation.h"
#include "window/advisors.h"
#include "window/empire.h"
#include "window/resource_settings.h"
#include "window/trade_prices.h"

#define ADVISOR_HEIGHT 27
#define PRICE_STEP 10
#define FIRST_ROW_Y 90
#define ROW_HEIGHT 18
#define NUM_RESOURCES (RESOURCE_MAX - RESOURCE_MIN)

// columns: the resource and its stock, the empire, the selected player
#define X_STOCK 132
#define X_EMPIRE_STATUS 176 // 72 wide
#define X_ROME_PRICE 252 // 40 wide
#define X_EMPIRE_PRICE 296 // 60 wide: paid on imports, received on exports
#define X_MY_PRICE 360
#define X_HIS_PRICE 436
#define X_BUY 490
#define X_ON_THE_WAY 554
// the stock tab: the stock kept for the city, the stock at which it stops buying (T4.5, D-070)
#define X_KEEP 382 // 72 wide
#define X_LIMIT 506 // 72 wide
#define STOCK_STEP 4 // a space of a warehouse
#define STOCK_TAB -1

// the tabs on the right of the title: one per other player, then the stock tab; narrower, with a smaller font, when
// four of them would cover the title
#define MAX_TAB_STEP 76
#define WIDE_TAB 70

static void button_partner(int index, int param2);
static void button_route(int param1, int param2);
static void button_resource(int resource, int param2);
static void button_empire_status(int resource, int param2);
static void button_price(int resource, int delta);
static void button_buy(int resource, int param2);
static void button_keep(int resource, int delta);
static void button_limit(int resource, int delta);
static void button_empire_map(int param1, int param2);
static void button_empire_prices(int param1, int param2);

// the tabs, the route, up to six buttons per resource, the map and the prices of the empire
#define MAX_BUTTONS_PER_RESOURCE 6
#define MAX_BUTTONS (PLAYER_CONTEXT_MAX_PLAYERS + 2 + MAX_BUTTONS_PER_RESOURCE * NUM_RESOURCES + 2)
static generic_button buttons[MAX_BUTTONS];

static struct {
    int partner; // the selected player, or STOCK_TAB
    int num_buttons;
    int focus_button_id;
    int route_button; // -1 on the stock tab
    int first_row_button;
    int buttons_per_row;
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

static int stock_tab_shown(void)
{
    return data.partner == STOCK_TAB;
}

static void init_buttons(void)
{
    data.num_buttons = 0;
    if (data.partner != STOCK_TAB && (data.partner == local_player() || data.partner < 0 ||
        data.partner >= player_context_num_players())) {
        // alone on the map: only the stock tab
        data.partner = player_context_num_players() > 1 ? (local_player() == 0 ? 1 : 0) : STOCK_TAB;
    }
    // tabs on the right of the title, the stock tab against the edge
    int tab = 0;
    int num_tabs = player_context_num_players();
    int step = (608 - 64 - lang_text_get_width(54, 0, FONT_LARGE_BLACK)) / num_tabs;
    step = step > MAX_TAB_STEP ? MAX_TAB_STEP : step;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p != local_player()) {
            add_button(610 - step * (num_tabs - tab++), 10, step - 2, 22, button_partner, p, 0);
        }
    }
    add_button(610 - step, 10, step - 2, 22, button_partner, STOCK_TAB, 0);
    data.route_button = -1;
    if (!stock_tab_shown()) {
        data.route_button = data.num_buttons;
        add_button(440, 34, 168, 20, button_route, 0, 0);
    }
    data.first_row_button = data.num_buttons;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        int resource = RESOURCE_MIN + i;
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        add_button(20, y - 1, X_STOCK + 40 - 20, ROW_HEIGHT - 2, button_resource, resource, 0);
        add_button(X_EMPIRE_STATUS, y - 1, 72, ROW_HEIGHT - 2, button_empire_status, resource, 0);
        if (stock_tab_shown()) {
            add_button(X_KEEP, y - 1, 16, ROW_HEIGHT - 2, button_keep, resource, -STOCK_STEP);
            add_button(X_KEEP + 56, y - 1, 16, ROW_HEIGHT - 2, button_keep, resource, STOCK_STEP);
            add_button(X_LIMIT, y - 1, 16, ROW_HEIGHT - 2, button_limit, resource, -STOCK_STEP);
            add_button(X_LIMIT + 56, y - 1, 16, ROW_HEIGHT - 2, button_limit, resource, STOCK_STEP);
        } else {
            add_button(X_MY_PRICE, y - 1, 16, ROW_HEIGHT - 2, button_price, resource, -PRICE_STEP);
            add_button(X_MY_PRICE + 56, y - 1, 16, ROW_HEIGHT - 2, button_price, resource, PRICE_STEP);
            add_button(X_BUY, y - 1, 56, ROW_HEIGHT - 2, button_buy, resource, 0);
        }
    }
    data.buttons_per_row = (data.num_buttons - data.first_row_button) / NUM_RESOURCES;
    add_button(100, 398, 200, 23, button_empire_map, 0, 0);
    add_button(400, 398, 200, 23, button_empire_prices, 0, 0);
}

int window_mp_trade_is_active(void)
{
    // also alone on the map, for the stock tab (T4.5, D-070)
    return player_context_num_players() > 1 || game_rules_is_multiplayer();
}

int window_mp_trade_draw_background(void)
{
    city_resource_determine_available();
    init_buttons();
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 4, 10, 10);
    lang_text_draw(54, 0, 60, 12, FONT_LARGE_BLACK);
    return ADVISOR_HEIGHT;
}

static void draw_player_name(int player_id, int x, int y, int width, font_t font)
{
    uint8_t text[64] = { 0 };
    string_copy(translation_for(TR_MP_PLAYER), text, 60);
    string_from_int(text + string_length(text), player_id + 1, 0);
    text_draw_centered(text, x, y, width, font, mp_colors_player(player_id));
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

// button_border_draw needs two blocks of 16 px to draw a bottom border (and the right one with a single block of
// width): the buttons of a row, 16 px high, get a rectangle in the colours of the borders instead (T4.5)
static void draw_button(int index)
{
    const generic_button *b = &buttons[index];
    int focus = data.focus_button_id == index + 1;
    if (b->height > 16) {
        button_border_draw(b->x, b->y, b->width, b->height, focus);
        return;
    }
    graphics_draw_rect(b->x, b->y, b->width, b->height, focus ? COLOR_FONT_YELLOW : COLOR_INSET_DARK);
    graphics_draw_rect(b->x + 1, b->y + 1, b->width - 2, b->height - 2, focus ? COLOR_FONT_YELLOW : COLOR_WHITE);
}

// the empire: what the city does with it, the price of Rome, and the price that applies, portorium included (paid
// on imports, received on exports, D-060)
static void draw_empire(int resource, int y, int index, int cheaper_player)
{
    int can_import = empire_can_import_resource(resource);
    int can_export = empire_can_export_resource(resource);
    int status = city_resource_trade_status(resource);
    if (city_resource_is_stockpiled(resource)) {
        lang_text_draw_centered(54, 3, X_EMPIRE_STATUS, y + 3, 72, FONT_NORMAL_WHITE);
    } else if (can_import || can_export) {
        draw_button(index);
        int text = status == TRADE_STATUS_IMPORT ? TR_MP_EMPIRE_IMPORTS :
            status == TRADE_STATUS_EXPORT ? TR_MP_EMPIRE_EXPORTS : TR_MP_EMPIRE_NO_TRADE;
        text_draw_centered(translation_for(text), X_EMPIRE_STATUS, y + 3, 72, FONT_NORMAL_WHITE, 0);
    } else {
        text_draw_centered((const uint8_t *) "-", X_EMPIRE_STATUS, y + 3, 72, FONT_NORMAL_WHITE, 0);
    }
    text_draw_number_centered(trade_price_rome(resource), X_ROME_PRICE, y + 3, 40, FONT_NORMAL_WHITE);
    if (status == TRADE_STATUS_EXPORT && can_export) {
        text_draw_number_centered(trade_price_sell(resource), X_EMPIRE_PRICE, y + 3, 60, FONT_NORMAL_WHITE);
    } else if (empire_can_import_resource_potentially(resource)) {
        // green when the empire is where the city buys it
        int supplies = status == TRADE_STATUS_IMPORT && can_import && cheaper_player < 0;
        text_draw_number_centered(trade_price_buy(resource), X_EMPIRE_PRICE, y + 3, 60,
            supplies ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
    } else {
        text_draw_centered((const uint8_t *) "-", X_EMPIRE_PRICE, y + 3, 60, FONT_NORMAL_WHITE, 0);
    }
}

static void draw_arrows(int index, int x, int y, int value, const uint8_t *text)
{
    draw_button(index);
    text_draw_centered((const uint8_t *) "-", x, y + 3, 16, FONT_NORMAL_WHITE, 0);
    if (text) {
        text_draw_centered(text, x + 16, y + 3, 40, FONT_NORMAL_WHITE, 0);
    } else {
        text_draw_number_centered(value, x + 16, y + 3, 40, FONT_NORMAL_WHITE);
    }
    draw_button(index + 1);
    text_draw_centered((const uint8_t *) "+", x + 56, y + 3, 16, FONT_NORMAL_WHITE, 0);
}

// the stock tab: the stock kept, below which nothing is sold, and the stock at which the city stops buying, both for
// the empire and for the players (T4.5, D-070)
static void draw_bounds(int resource, int y, int index)
{
    draw_arrows(index, X_KEEP, y, city_resource_export_over(resource), 0);
    int limit = mp_trade_buy_limit(local_player(), resource);
    draw_arrows(index + 2, X_LIMIT, y, limit, limit > 0 ? 0 : translation_for(TR_MP_TRADE_NO_LIMIT));
}

// the selected player: my price to him, his price to me, whether I buy from him, what his caravans bring me
static void draw_partner(int resource, int y, int index, int cheaper_player)
{
    int me = local_player();
    draw_button(index);
    text_draw_centered((const uint8_t *) "-", X_MY_PRICE, y + 3, 16, FONT_NORMAL_WHITE, 0);
    text_draw_number_centered(mp_trade_price(me, data.partner, resource), X_MY_PRICE + 16, y + 3, 40,
        FONT_NORMAL_WHITE);
    draw_button(index + 1);
    text_draw_centered((const uint8_t *) "+", X_MY_PRICE + 56, y + 3, 16, FONT_NORMAL_WHITE, 0);
    text_draw_number_centered(mp_trade_price(data.partner, me, resource), X_HIS_PRICE, y + 3, 50,
        cheaper_player == data.partner ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
    draw_button(index + 2);
    text_draw_centered(translation_for(mp_trade_buys_from(me, data.partner, resource) ? TR_MP_YES : TR_MP_NO),
        X_BUY, y + 3, 56, FONT_NORMAL_WHITE, 0);
    int loads = mp_trade_loads_on_the_way(data.partner, me, resource);
    if (loads > 0) {
        text_draw_number_centered(loads, X_ON_THE_WAY, y + 3, 50, FONT_NORMAL_WHITE);
    }
}

static void draw_headers(void)
{
    // "Empire, portorium 50 %"
    uint8_t empire[64] = { 0 };
    string_copy(translation_for(TR_MP_TRADE_EMPIRE), empire, 60);
    string_copy(translation_for(TR_MP_TRADE_PORTORIUM), empire + string_length(empire), 60 - string_length(empire));
    string_from_int(empire + string_length(empire), trade_price_portorium_percent(), 0);
    string_copy((const uint8_t *) " %", empire + string_length(empire), 60 - string_length(empire));
    text_draw_centered(empire, X_EMPIRE_STATUS, 62, X_MY_PRICE - 8 - X_EMPIRE_STATUS, FONT_NORMAL_WHITE, 0);
    text_draw_centered(translation_for(TR_MP_TRADE_STOCK), X_STOCK, 78, 40, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_STATUS), X_EMPIRE_STATUS, 78, 72, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_ROME), X_ROME_PRICE, 78, 40, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_PRICE), X_EMPIRE_PRICE, 78, 60, FONT_SMALL_PLAIN, COLOR_WHITE);
    if (stock_tab_shown()) {
        // "Sell / above" and "Buy / up to", on two lines over their arrows
        text_draw_centered(translation_for(TR_MP_TRADE_KEEP), X_KEEP, 66, 72, FONT_SMALL_PLAIN, COLOR_WHITE);
        text_draw_centered(translation_for(TR_MP_TRADE_KEEP_2), X_KEEP, 78, 72, FONT_SMALL_PLAIN, COLOR_WHITE);
        text_draw_centered(translation_for(TR_MP_TRADE_BUY_LIMIT), X_LIMIT, 66, 72, FONT_SMALL_PLAIN, COLOR_WHITE);
        text_draw_centered(translation_for(TR_MP_TRADE_BUY_LIMIT_2), X_LIMIT, 78, 72, FONT_SMALL_PLAIN,
            COLOR_WHITE);
        return;
    }
    draw_player_name(data.partner, X_MY_PRICE, 62, X_ON_THE_WAY + 50 - X_MY_PRICE, FONT_NORMAL_PLAIN);
    text_draw_centered(translation_for(TR_MP_TRADE_I_SELL), X_MY_PRICE, 78, 72, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_HE_SELLS), X_HIS_PRICE, 78, 50, FONT_SMALL_PLAIN, COLOR_WHITE);
    text_draw_centered(translation_for(TR_MP_TRADE_I_BUY), X_BUY, 78, 56, FONT_SMALL_PLAIN, COLOR_WHITE);
    // wider than its column in English: centered over a wider box, within the panel
    text_draw_centered(translation_for(TR_MP_RESOURCE_ON_THE_WAY), X_ON_THE_WAY - 8, 78, 66, FONT_SMALL_PLAIN,
        COLOR_WHITE);
}

void window_mp_trade_draw_foreground(void)
{
    int me = local_player();

    // tabs of the other players and the stock tab, the route with the selected player
    int button = 0;
    for (; button < data.first_row_button && button != data.route_button; button++) {
        const generic_button *b = &buttons[button];
        if (b->parameter1 == data.partner) {
            inner_panel_draw(b->x, b->y, b->width / 16, 1);
        }
        draw_button(button);
        font_t font = b->width >= WIDE_TAB ? FONT_NORMAL_PLAIN : FONT_SMALL_PLAIN;
        int text_y = b->y + (b->width >= WIDE_TAB ? 6 : 7);
        if (b->parameter1 == STOCK_TAB) {
            text_draw_centered(translation_for(TR_MP_TRADE_STOCK_TAB), b->x, text_y, b->width, font,
                stock_tab_shown() ? COLOR_WHITE : COLOR_BLACK);
        } else {
            draw_player_name(b->parameter1, b->x, text_y, b->width, font);
        }
    }
    if (stock_tab_shown()) {
        text_draw(translation_for(TR_MP_TRADE_BOUNDS_INTRO), 60, 40, FONT_NORMAL_BLACK, 0);
    } else {
        int width = text_draw(translation_for(TR_MP_ROUTE), 60, 40, FONT_NORMAL_BLACK, 0);
        text_draw(translation_for(route_status_text()), 60 + width, 40, FONT_NORMAL_BLACK, 0);
        const generic_button *route = &buttons[button];
        draw_button(button++);
        text_draw_centered(translation_for(mp_trade_route_is_proposed(me, data.partner) ?
            TR_MP_ROUTE_WITHDRAW : TR_MP_ROUTE_PROPOSE), route->x, route->y + 5, route->width, FONT_NORMAL_BLACK, 0);
    }

    // one row per resource: the empire on the left, the selected player on the right
    inner_panel_draw(16, 56, 38, 19);
    draw_headers();
    for (int i = 0; i < NUM_RESOURCES; i++) {
        int resource = RESOURCE_MIN + i;
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        if (data.focus_button_id == button + 1) {
            draw_button(button);
        }
        image_draw(image_group(GROUP_RESOURCE_ICONS) + resource + resource_image_offset(resource, RESOURCE_IMAGE_ICON),
            24, y - 4);
        lang_text_draw(23, resource, 52, y + 3, FONT_NORMAL_WHITE);
        text_draw_number_centered(city_resource_count(resource), X_STOCK, y + 3, 40, FONT_NORMAL_WHITE);
        // in green, the cheaper source: the empire or this player (the empire always sells, D-060)
        int cheaper_player = mp_trade_cheaper_player(resource);
        draw_empire(resource, y, button + 1, cheaper_player);
        if (stock_tab_shown()) {
            draw_bounds(resource, y, button + 2);
        } else {
            draw_partner(resource, y, button + 2, cheaper_player);
        }
        button += data.buttons_per_row;
    }
    // two lines of rules between the table and the buttons, apart enough not to touch each other nor the buttons
    text_draw(translation_for(stock_tab_shown() ? TR_MP_TRADE_BOUNDS_RULE : TR_MP_TRADE_RULE), 32, 366,
        FONT_SMALL_PLAIN, 0);
    text_draw(translation_for(stock_tab_shown() ? TR_MP_TRADE_BOUNDS_RULE_2 : TR_MP_TRADE_RULE_2), 32, 380,
        FONT_SMALL_PLAIN, 0);

    draw_button(button);
    lang_text_draw_centered(54, 30, 100, 404, 200, FONT_NORMAL_BLACK);
    draw_button(button + 1);
    lang_text_draw_centered(54, 2, 400, 404, 200, FONT_NORMAL_BLACK);
}

int window_mp_trade_handle_mouse(const mouse *m)
{
    return generic_buttons_handle_mouse(m, 0, 0, buttons, data.num_buttons, &data.focus_button_id);
}

int window_mp_trade_get_tooltip_text(void)
{
    if (data.focus_button_id && data.focus_button_id == data.num_buttons - 1) {
        return 41; // map of the empire
    } else if (data.focus_button_id && data.focus_button_id == data.num_buttons) {
        return 106; // prices of the empire
    }
    return 0;
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

static void button_resource(int resource, int param2)
{
    window_resource_settings_show(resource);
}

static void button_empire_status(int resource, int param2)
{
    if (!city_resource_is_stockpiled(resource) &&
        (empire_can_import_resource(resource) || empire_can_export_resource(resource))) {
        mp_action_cycle_trade_status(resource);
    }
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

static void button_keep(int resource, int delta)
{
    mp_action_change_export_over(resource, delta);
}

static void button_limit(int resource, int delta)
{
    mp_action_change_buy_limit(resource, delta);
}

static void button_empire_map(int param1, int param2)
{
    window_empire_show();
}

static void button_empire_prices(int param1, int param2)
{
    window_trade_prices_show();
}

void window_mp_trade_show_partner(int player_id)
{
    data.partner = player_id;
    window_advisors_show_advisor(ADVISOR_TRADE);
}
