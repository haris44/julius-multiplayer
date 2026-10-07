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

// Three kinds of tabs shape the page, each with few columns (T5.6): the empire (what the city does with it, the
// price of Rome, the portorium and the price that applies), one tab per other player (my price to him, his price,
// the empire to compare with, whether I buy from him, what his caravans bring) and the stocks (when to sell and when
// to stop buying). The stock of the city opens every row. Numbers end on the right edge of their column, and the
// headers over them too; one help line below the table says what the tab does.
#define X_STOCK 176 // right edge
#define STOCK_TAB -1
#define EMPIRE_TAB -2
// the empire tab
#define X_EMPIRE_STATUS 204 // 96 wide
#define W_EMPIRE_STATUS 96
#define X_ROME_PRICE 400 // right edges
#define X_PORTORIUM 490
#define X_EMPIRE_PRICE 590
// the player tab
#define X_MY_PRICE 200 // 88 wide: - 1234 +
#define X_HIS_PRICE 368 // right edges
#define X_EMPIRE_COMPARED 450
#define X_BUY 462 // 56 wide
#define W_BUY 56
#define X_ON_THE_WAY 602
// the stock tab: the stock above which the city sells, the stock up to which it buys (T4.5, D-070)
#define X_KEEP 284 // 88 wide
#define X_LIMIT 470
#define W_ARROWS 88
#define STOCK_STEP 4 // a space of a warehouse

// the tabs, in a row under the title
#define TAB_X 60 // after the icon of the advisor, in line with the title
#define TAB_Y 40
#define TAB_HEIGHT 22
#define TAB_STEP 108
#define TAB_WIDTH 104
#define MAX_TABS (PLAYER_CONTEXT_MAX_PLAYERS + 2)
// the route with the selected player, in the corner of the title
#define ROUTE_X 410
#define ROUTE_Y 10
#define ROUTE_WIDTH 200
// the table, its headers, and the help line under it
#define TABLE_Y 66
#define HEADER_Y 71
#define HELP_Y 374
#define HELP_WIDTH 600

static void button_tab(int tab, int param2);
static void button_route(int param1, int param2);
static void button_resource(int resource, int param2);
static void button_empire_status(int resource, int param2);
static void button_price(int resource, int delta);
static void button_buy(int resource, int param2);
static void button_keep(int resource, int delta);
static void button_limit(int resource, int delta);
static void button_empire_map(int param1, int param2);
static void button_empire_prices(int param1, int param2);

// the tabs, the route, up to five buttons per resource, the map and the prices of the empire
#define MAX_BUTTONS_PER_RESOURCE 5
#define MAX_BUTTONS (MAX_TABS + 1 + MAX_BUTTONS_PER_RESOURCE * NUM_RESOURCES + 2)
static generic_button buttons[MAX_BUTTONS];

static struct {
    int tab; // a player, STOCK_TAB or EMPIRE_TAB
    int num_buttons;
    int focus_button_id;
    int num_tabs;
    int tabs[MAX_TABS]; // what each tab shows
    int route_button; // -1 when the tab has no route
    int first_row_button;
    int buttons_per_row;
    int drawn_route_state; // the state of the route when the background was drawn
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
    return data.tab == STOCK_TAB;
}

static int empire_tab_shown(void)
{
    return data.tab == EMPIRE_TAB;
}

static int player_tab_shown(void)
{
    return data.tab >= 0;
}

static int tab_is_valid(int tab)
{
    for (int i = 0; i < data.num_tabs; i++) {
        if (data.tabs[i] == tab) {
            return 1;
        }
    }
    return 0;
}

// What the route button and the help line say about the route with the selected player, or -1 on the other tabs. They
// are drawn over the wooden frame, which only the background redraws: when the state changes, a command from either
// player executed in the meantime, the text of the old state would stay under the new one (T5.6).
static int route_state(void)
{
    int me = local_player();
    if (!player_tab_shown()) {
        return -1;
    }
    return mp_trade_route_is_open(me, data.tab) ? 3 :
        mp_trade_route_is_proposed(me, data.tab) ? 2 : mp_trade_route_is_proposed(data.tab, me) ? 1 : 0;
}

int window_mp_trade_frame_is_current(void)
{
    return route_state() == data.drawn_route_state;
}

static void init_buttons(void)
{
    data.num_buttons = 0;
    // the empire, the other players, the stocks
    data.num_tabs = 0;
    data.tabs[data.num_tabs++] = EMPIRE_TAB;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p != local_player()) {
            data.tabs[data.num_tabs++] = p;
        }
    }
    data.tabs[data.num_tabs++] = STOCK_TAB;
    if (!tab_is_valid(data.tab)) {
        // the first other player, or the empire when alone on the map
        data.tab = data.num_tabs > 2 ? data.tabs[1] : EMPIRE_TAB;
    }
    for (int i = 0; i < data.num_tabs; i++) {
        add_button(TAB_X + TAB_STEP * i, TAB_Y, TAB_WIDTH, TAB_HEIGHT, button_tab, data.tabs[i], 0);
    }
    data.route_button = -1;
    if (player_tab_shown()) {
        data.route_button = data.num_buttons;
        add_button(ROUTE_X, ROUTE_Y, ROUTE_WIDTH, TAB_HEIGHT, button_route, 0, 0);
    }
    data.first_row_button = data.num_buttons;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        int resource = RESOURCE_MIN + i;
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        add_button(20, y - 1, X_STOCK - 20, ROW_HEIGHT - 2, button_resource, resource, 0);
        if (empire_tab_shown()) {
            add_button(X_EMPIRE_STATUS, y - 1, W_EMPIRE_STATUS, ROW_HEIGHT - 2, button_empire_status, resource, 0);
        } else if (stock_tab_shown()) {
            add_button(X_KEEP, y - 1, 16, ROW_HEIGHT - 2, button_keep, resource, -STOCK_STEP);
            add_button(X_KEEP + W_ARROWS - 16, y - 1, 16, ROW_HEIGHT - 2, button_keep, resource, STOCK_STEP);
            add_button(X_LIMIT, y - 1, 16, ROW_HEIGHT - 2, button_limit, resource, -STOCK_STEP);
            add_button(X_LIMIT + W_ARROWS - 16, y - 1, 16, ROW_HEIGHT - 2, button_limit, resource, STOCK_STEP);
        } else {
            add_button(X_MY_PRICE, y - 1, 16, ROW_HEIGHT - 2, button_price, resource, -PRICE_STEP);
            add_button(X_MY_PRICE + W_ARROWS - 16, y - 1, 16, ROW_HEIGHT - 2, button_price, resource, PRICE_STEP);
            add_button(X_BUY, y - 1, W_BUY, ROW_HEIGHT - 2, button_buy, resource, 0);
        }
    }
    data.buttons_per_row = (data.num_buttons - data.first_row_button) / NUM_RESOURCES;
    add_button(100, 398, 200, 23, button_empire_map, 0, 0);
    add_button(400, 398, 200, 23, button_empire_prices, 0, 0);
}

int window_mp_trade_is_active(void)
{
    // also alone on the map, for the empire and stock tabs (T4.5, D-070)
    return player_context_num_players() > 1 || game_rules_is_multiplayer();
}

int window_mp_trade_draw_background(void)
{
    city_resource_determine_available();
    init_buttons();
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 4, 10, 10);
    lang_text_draw(54, 0, 60, 12, FONT_LARGE_BLACK);
    data.drawn_route_state = route_state();
    return ADVISOR_HEIGHT;
}

static void draw_player_name(int player_id, int x, int y, int width, font_t font)
{
    uint8_t text[64] = { 0 };
    string_copy(translation_for(TR_MP_PLAYER), text, 60);
    string_from_int(text + string_length(text), player_id + 1, 0);
    text_draw_centered(text, x, y, width, font, mp_colors_player(player_id));
}

// a number whose last digit is on the given edge, so that the digits of a column line up
static void draw_number_right(int value, int force_sign, int right, int y, font_t font)
{
    uint8_t text[32] = { 0 };
    string_from_int(text, value, force_sign);
    text_draw(text, right - text_get_width(text, font), y, font, 0);
}

static void draw_dash_right(int right, int y)
{
    const uint8_t *dash = (const uint8_t *) "-";
    text_draw(dash, right - text_get_width(dash, FONT_NORMAL_WHITE), y, FONT_NORMAL_WHITE, 0);
}

// a header over a column of numbers: its last letter on their right edge
static void draw_header_right(translation_key text, int right)
{
    const uint8_t *str = translation_for(text);
    text_draw(str, right - text_get_width(str, FONT_SMALL_PLAIN), HEADER_Y, FONT_SMALL_PLAIN, COLOR_WHITE);
}

// a header over a button or a row of controls
static void draw_header_centered(translation_key text, int x, int width)
{
    text_draw_centered(translation_for(text), x, HEADER_Y, width, FONT_SMALL_PLAIN, COLOR_WHITE);
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

// the price of the empire that applies to the city: what it receives on an export, what it pays on an import, the
// portorium included (D-060); 0 when the empire does not trade this resource
static int empire_price(int resource, int *is_export)
{
    *is_export = 0;
    if (city_resource_trade_status(resource) == TRADE_STATUS_EXPORT && empire_can_export_resource(resource)) {
        *is_export = 1;
        return trade_price_sell(resource);
    } else if (empire_can_import_resource_potentially(resource)) {
        return trade_price_buy(resource);
    }
    return 0;
}

// green when the empire is where the city buys it
static font_t empire_price_font(int resource, int is_export, int cheaper_player)
{
    int supplies = !is_export && city_resource_trade_status(resource) == TRADE_STATUS_IMPORT &&
        empire_can_import_resource(resource) && cheaper_player < 0;
    return supplies ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE;
}

// the empire tab: what the city does with it, the price of Rome, the portorium (added on an import, taken off on an
// export) and the price that applies (D-060)
static void draw_empire(int resource, int y, int index, int cheaper_player)
{
    int can_import = empire_can_import_resource(resource);
    int can_export = empire_can_export_resource(resource);
    int status = city_resource_trade_status(resource);
    if (city_resource_is_stockpiled(resource)) {
        lang_text_draw_centered(54, 3, X_EMPIRE_STATUS, y + 3, W_EMPIRE_STATUS, FONT_NORMAL_WHITE);
    } else if (can_import || can_export) {
        draw_button(index);
        int text = status == TRADE_STATUS_IMPORT ? TR_MP_EMPIRE_IMPORTS :
            status == TRADE_STATUS_EXPORT ? TR_MP_EMPIRE_EXPORTS : TR_MP_EMPIRE_NO_TRADE;
        text_draw_centered(translation_for(text), X_EMPIRE_STATUS, y + 3, W_EMPIRE_STATUS, FONT_NORMAL_WHITE, 0);
    } else {
        text_draw_centered((const uint8_t *) "-", X_EMPIRE_STATUS, y + 3, W_EMPIRE_STATUS, FONT_NORMAL_WHITE, 0);
    }
    draw_number_right(trade_price_rome(resource), 0, X_ROME_PRICE, y + 3, FONT_NORMAL_WHITE);
    int is_export;
    int price = empire_price(resource, &is_export);
    if (price) {
        int duty = trade_price_duty(resource);
        draw_number_right(is_export ? -duty : duty, 1, X_PORTORIUM, y + 3, FONT_NORMAL_WHITE);
        draw_number_right(price, 0, X_EMPIRE_PRICE, y + 3, empire_price_font(resource, is_export, cheaper_player));
    } else {
        draw_dash_right(X_PORTORIUM, y + 3);
        draw_dash_right(X_EMPIRE_PRICE, y + 3);
    }
}

static void draw_arrows(int index, int x, int y, int value, const uint8_t *text)
{
    draw_button(index);
    text_draw_centered((const uint8_t *) "-", x, y + 3, 16, FONT_NORMAL_WHITE, 0);
    if (text) {
        text_draw_centered(text, x + 16, y + 3, W_ARROWS - 32, FONT_NORMAL_WHITE, 0);
    } else {
        text_draw_number_centered(value, x + 16, y + 3, W_ARROWS - 32, FONT_NORMAL_WHITE);
    }
    draw_button(index + 1);
    text_draw_centered((const uint8_t *) "+", x + W_ARROWS - 16, y + 3, 16, FONT_NORMAL_WHITE, 0);
}

// the stock tab: the stock kept, below which nothing is sold, and the stock at which the city stops buying, both for
// the empire and for the players (T4.5, D-070)
static void draw_bounds(int resource, int y, int index)
{
    draw_arrows(index, X_KEEP, y, city_resource_export_over(resource), 0);
    int limit = mp_trade_buy_limit(local_player(), resource);
    draw_arrows(index + 2, X_LIMIT, y, limit, limit > 0 ? 0 : translation_for(TR_MP_TRADE_NO_LIMIT));
}

// the selected player: my price to him, his price to me, the empire to compare with, whether I buy from him, and
// what his caravans bring me
static void draw_partner(int resource, int y, int index, int cheaper_player)
{
    int me = local_player();
    draw_button(index);
    text_draw_centered((const uint8_t *) "-", X_MY_PRICE, y + 3, 16, FONT_NORMAL_WHITE, 0);
    text_draw_number_centered(mp_trade_price(me, data.tab, resource), X_MY_PRICE + 16, y + 3, W_ARROWS - 32,
        FONT_NORMAL_WHITE);
    draw_button(index + 1);
    text_draw_centered((const uint8_t *) "+", X_MY_PRICE + W_ARROWS - 16, y + 3, 16, FONT_NORMAL_WHITE, 0);
    draw_number_right(mp_trade_price(data.tab, me, resource), 0, X_HIS_PRICE, y + 3,
        cheaper_player == data.tab ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
    // what the city would pay the empire for it, the portorium included
    if (empire_can_import_resource_potentially(resource)) {
        int supplies = city_resource_trade_status(resource) == TRADE_STATUS_IMPORT &&
            empire_can_import_resource(resource) && cheaper_player < 0;
        draw_number_right(trade_price_buy(resource), 0, X_EMPIRE_COMPARED, y + 3,
            supplies ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
    } else {
        draw_dash_right(X_EMPIRE_COMPARED, y + 3);
    }
    draw_button(index + 2);
    text_draw_centered(translation_for(mp_trade_buys_from(me, data.tab, resource) ? TR_MP_YES : TR_MP_NO),
        X_BUY, y + 3, W_BUY, FONT_NORMAL_WHITE, 0);
    int loads = mp_trade_loads_on_the_way(data.tab, me, resource);
    if (loads > 0) {
        draw_number_right(loads, 0, X_ON_THE_WAY, y + 3, FONT_NORMAL_WHITE);
    }
}

static void draw_headers(void)
{
    draw_header_right(TR_MP_TRADE_STOCK, X_STOCK);
    if (empire_tab_shown()) {
        draw_header_centered(TR_MP_TRADE_STATUS, X_EMPIRE_STATUS, W_EMPIRE_STATUS);
        draw_header_right(TR_MP_TRADE_ROME, X_ROME_PRICE);
        draw_header_right(TR_MP_TRADE_PORTORIUM, X_PORTORIUM);
        draw_header_right(TR_MP_TRADE_PRICE, X_EMPIRE_PRICE);
    } else if (stock_tab_shown()) {
        draw_header_centered(TR_MP_TRADE_KEEP, X_KEEP - 60, W_ARROWS + 120);
        draw_header_centered(TR_MP_TRADE_BUY_LIMIT, X_LIMIT - 60, W_ARROWS + 120);
    } else {
        draw_header_centered(TR_MP_TRADE_I_SELL, X_MY_PRICE - 20, W_ARROWS + 40);
        draw_header_right(TR_MP_TRADE_HE_SELLS, X_HIS_PRICE);
        draw_header_right(TR_MP_TRADE_EMPIRE, X_EMPIRE_COMPARED);
        draw_header_centered(TR_MP_TRADE_I_BUY, X_BUY - 12, W_BUY + 24);
        draw_header_right(TR_MP_RESOURCE_ON_THE_WAY, X_ON_THE_WAY);
    }
}

// the route with the selected player: the button says what a click does
static int route_button_text(void)
{
    int me = local_player();
    if (mp_trade_route_is_open(me, data.tab)) {
        return TR_MP_ROUTE_CLOSE;
    } else if (mp_trade_route_is_proposed(me, data.tab)) {
        return TR_MP_ROUTE_WITHDRAW;
    } else if (mp_trade_route_is_proposed(data.tab, me)) {
        return TR_MP_ROUTE_ACCEPT;
    }
    return TR_MP_ROUTE_PROPOSE;
}

// the one help line of the tab; on the tab of a player, the state of the route while it is not open
static void draw_help(void)
{
    uint8_t composed[160] = { 0 };
    const uint8_t *text;
    int me = local_player();
    if (stock_tab_shown()) {
        text = translation_for(TR_MP_TRADE_BOUNDS_RULE);
    } else if (empire_tab_shown()) {
        // "Portorium of 50 %: added to the price of Rome when buying, taken off when selling."
        string_copy(translation_for(TR_MP_TRADE_EMPIRE_RULE), composed, 100);
        int length = string_length(composed);
        length += string_from_int(composed + length, trade_price_portorium_percent(), 0);
        string_copy(translation_for(TR_MP_TRADE_EMPIRE_RULE_2), composed + length, 150 - length);
        text = composed;
    } else if (mp_trade_route_is_open(me, data.tab)) {
        text = translation_for(TR_MP_TRADE_RULE);
    } else if (mp_trade_route_is_proposed(me, data.tab)) {
        text = translation_for(TR_MP_ROUTE_STATUS_WAITING);
    } else if (mp_trade_route_is_proposed(data.tab, me)) {
        text = translation_for(TR_MP_ROUTE_STATUS_OFFERED);
    } else {
        text = translation_for(TR_MP_ROUTE_STATUS_CLOSED);
    }
    text_draw_multiline(text, 20, HELP_Y, HELP_WIDTH, FONT_SMALL_PLAIN, 0);
}

// the empire, the other players and the stocks, in a row under the title; the selected one on a dark ground
static void draw_tabs(void)
{
    for (int i = 0; i < data.num_tabs; i++) {
        const generic_button *b = &buttons[i];
        int tab = data.tabs[i];
        int selected = tab == data.tab;
        if (selected) {
            inner_panel_draw(b->x, b->y, b->width / 16, 1);
        }
        draw_button(i);
        int text_y = b->y + 6;
        if (tab == STOCK_TAB || tab == EMPIRE_TAB) {
            text_draw_centered(translation_for(tab == STOCK_TAB ? TR_MP_TRADE_STOCK_TAB : TR_MP_TRADE_EMPIRE),
                b->x, text_y, b->width, FONT_NORMAL_PLAIN, selected ? COLOR_WHITE : COLOR_BLACK);
        } else {
            draw_player_name(tab, b->x, text_y, b->width, FONT_NORMAL_PLAIN);
        }
    }
}

void window_mp_trade_draw_foreground(void)
{
    if (route_state() != data.drawn_route_state) {
        // draw the frame again, and the page on the next frame
        window_invalidate();
        return;
    }
    draw_tabs();
    if (data.route_button >= 0) {
        const generic_button *route = &buttons[data.route_button];
        draw_button(data.route_button);
        text_draw_centered(translation_for(route_button_text()), route->x, route->y + 5, route->width,
            FONT_NORMAL_BLACK, 0);
    }

    // one row per resource
    inner_panel_draw(16, TABLE_Y, 38, 19);
    draw_headers();
    int button = data.first_row_button;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        int resource = RESOURCE_MIN + i;
        int y = FIRST_ROW_Y + ROW_HEIGHT * i;
        if (data.focus_button_id == button + 1) {
            draw_button(button);
        }
        image_draw(image_group(GROUP_RESOURCE_ICONS) + resource + resource_image_offset(resource, RESOURCE_IMAGE_ICON),
            24, y - 4);
        lang_text_draw(23, resource, 52, y + 3, FONT_NORMAL_WHITE);
        draw_number_right(city_resource_count(resource), 0, X_STOCK, y + 3, FONT_NORMAL_WHITE);
        // in green, the cheaper source: the empire or this player (the empire always sells, D-060)
        int cheaper_player = mp_trade_cheaper_player(resource);
        if (empire_tab_shown()) {
            draw_empire(resource, y, button + 1, cheaper_player);
        } else if (stock_tab_shown()) {
            draw_bounds(resource, y, button + 1);
        } else {
            draw_partner(resource, y, button + 1, cheaper_player);
        }
        button += data.buttons_per_row;
    }
    draw_help();

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

static void button_tab(int tab, int param2)
{
    data.tab = tab;
    window_invalidate();
}

static void button_route(int param1, int param2)
{
    mp_action_propose_route(data.tab, !mp_trade_route_is_proposed(local_player(), data.tab));
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
    int price = mp_trade_price(local_player(), data.tab, resource) + delta;
    mp_action_set_sell_price(data.tab, resource, price < 1 ? 1 : price);
}

static void button_buy(int resource, int param2)
{
    mp_action_set_buys_from(data.tab, resource, !mp_trade_buys_from(local_player(), data.tab, resource));
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
    data.tab = player_id;
    window_advisors_show_advisor(ADVISOR_TRADE);
}
