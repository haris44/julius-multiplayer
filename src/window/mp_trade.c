#include "mp_trade.h"

#include "city/resource.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "game/rules.h"
#include "graphics/arrow_button.h"
#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
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
#define STOCK_STEP 4 // a space of a warehouse
#define NUM_RESOURCES (RESOURCE_MAX - RESOURCE_MIN)

// The page looks like the original trade advisor (T5.6, D-093): a row per resource, its icon and its name, then a few
// values in columns, in a sunken panel. Three kinds of tabs, on the top edge of the panel: the empire (what the city
// does with it, the price of Rome, the price that applies), one tab per other player (my price to him, his price,
// whether I buy from him, what his caravans bring), the stocks (when to sell, when to stop buying). Prices and limits
// change with the arrows of the original game, 24 pixels high: twelve rows are shown, the list scrolls like the
// original lists. Details go in tooltips.
#define STOCK_TAB -1
#define EMPIRE_TAB -2

// the title band: on the tab of a player, the route with him
#define ROUTE_X 400
#define ROUTE_Y 10
#define ROUTE_WIDTH 224
#define ROUTE_HEIGHT 24

// the tabs, standing on the top edge of the table, after the icon of the advisor
#define TAB_X 60
#define TAB_Y 42
#define TAB_HEIGHT 22
#define TAB_WIDTH_BLOCKS 6
#define TAB_STEP 102
#define MAX_TABS (PLAYER_CONTEXT_MAX_PLAYERS + 2)

// the table and its scrollbar (as in the original lists, src/window/hotkey_config.c)
#define TABLE_X 20
#define TABLE_Y 64
#define TABLE_WIDTH_BLOCKS 35
#define TABLE_HEIGHT_BLOCKS 20
#define TABLE_RIGHT (TABLE_X + 16 * TABLE_WIDTH_BLOCKS)
#define HEADER_Y 70
#define FIRST_ROW_Y 86
#define ROW_HEIGHT 24
#define ROWS_SHOWN 12
#define HELP_X 44 // after the help button of the advisor
#define HELP_Y 387

// the columns: x of the left edge of a control, or right edge of a column of numbers
#define X_ICON 26
#define X_NAME 54
#define X_STOCK 192 // right edge
#define ARROWS_WIDTH 48 // the down and up arrows of the original game
#define VALUE_WIDTH 44 // the value after the arrows, up to four digits
// the empire tab
#define X_EMPIRE_STATUS 214
#define W_EMPIRE_STATUS 112
#define X_ROME_PRICE 446 // right edges
#define X_EMPIRE_PRICE 556
// the tab of a player
#define X_MY_PRICE 210
#define X_HIS_PRICE 380 // right edge
#define X_BUY 400
#define W_BUY 64
#define X_ON_THE_WAY 562 // right edge
// the stock tab: the stock above which the city sells, the stock up to which it buys (T4.5, D-070)
#define X_KEEP 228
#define X_LIMIT 408

#define BUTTON_HEIGHT 20 // the buttons of a row, as in the orders of a warehouse
#define COLOR_SHADOW 0xff181818 // not 0: a text in colour 0 keeps the colours of its font

static void button_tab(int tab, int param2);
static void button_route(int param1, int param2);
static void button_resource(int resource, int param2);
static void button_empire_status(int resource, int param2);
static void button_buy(int resource, int param2);
static void button_empire_map(int param1, int param2);
static void button_empire_prices(int param1, int param2);
static void arrow_price(int resource, int is_down);
static void arrow_keep(int resource, int is_down);
static void arrow_limit(int resource, int is_down);
static void on_scroll(void);

// the tabs, the route, two buttons per row shown, the map and the prices of the empire
#define MAX_BUTTONS (MAX_TABS + 1 + 2 * ROWS_SHOWN + 2)
static generic_button buttons[MAX_BUTTONS];
// up to two pairs of arrows per row shown
#define MAX_ARROWS (4 * ROWS_SHOWN)
static arrow_button arrows[MAX_ARROWS];

static scrollbar_type scrollbar = {
    TABLE_RIGHT, TABLE_Y, 16 * TABLE_HEIGHT_BLOCKS, 16 * TABLE_WIDTH_BLOCKS, ROWS_SHOWN, on_scroll, 1, 0, 1
};

static struct {
    int tab; // a player, STOCK_TAB or EMPIRE_TAB
    int num_buttons;
    int focus_button_id;
    int num_arrows;
    int focus_arrow_id;
    int num_tabs;
    int tabs[MAX_TABS]; // what each tab shows
    int route_button; // -1 when the tab has no route
    int first_row_button;
    int buttons_per_row;
    int arrows_per_row;
    int map_button;
    int drawn_route_state; // the state of the route when the background was drawn
    uint8_t tooltip[300];
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

static void add_arrows(int x, int y, void (*callback)(int, int), int resource)
{
    // the original pair of arrows (window/advisor/labor.c): down, then up
    arrow_button down = { x, y, 17, 24, callback, resource, 1, 0, 0 };
    arrow_button up = { x + 24, y, 15, 24, callback, resource, 0, 0, 0 };
    arrows[data.num_arrows++] = down;
    arrows[data.num_arrows++] = up;
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

static int row_y(int index)
{
    return FIRST_ROW_Y + ROW_HEIGHT * index;
}

static void init_buttons(void)
{
    data.num_buttons = 0;
    data.num_arrows = 0;
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
        add_button(TAB_X + TAB_STEP * i, TAB_Y, 16 * TAB_WIDTH_BLOCKS, TAB_HEIGHT - 2, button_tab, data.tabs[i], 0);
    }
    data.route_button = -1;
    if (player_tab_shown()) {
        data.route_button = data.num_buttons;
        add_button(ROUTE_X, ROUTE_Y, ROUTE_WIDTH, ROUTE_HEIGHT, button_route, 0, 0);
    }
    // the rows shown: the resource (its original settings), then the button of the tab, if any, and its arrows
    data.first_row_button = data.num_buttons;
    for (int i = 0; i < ROWS_SHOWN; i++) {
        int resource = RESOURCE_MIN + scrollbar.scroll_position + i;
        int y = row_y(i);
        add_button(TABLE_X + 2, y, X_STOCK + 4 - TABLE_X, ROW_HEIGHT, button_resource, resource, 0);
        if (empire_tab_shown()) {
            add_button(X_EMPIRE_STATUS, y + 2, W_EMPIRE_STATUS, BUTTON_HEIGHT, button_empire_status, resource, 0);
        } else if (stock_tab_shown()) {
            add_arrows(X_KEEP, y, arrow_keep, resource);
            add_arrows(X_LIMIT, y, arrow_limit, resource);
        } else {
            add_arrows(X_MY_PRICE, y, arrow_price, resource);
            add_button(X_BUY, y + 2, W_BUY, BUTTON_HEIGHT, button_buy, resource, 0);
        }
    }
    data.buttons_per_row = (data.num_buttons - data.first_row_button) / ROWS_SHOWN;
    data.arrows_per_row = data.num_arrows / ROWS_SHOWN;
    data.map_button = data.num_buttons;
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
    scrollbar_init(&scrollbar, scrollbar.scroll_position, NUM_RESOURCES);
    init_buttons();
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 4, 10, 10);
    lang_text_draw(54, 0, 60, 12, FONT_LARGE_BLACK);
    data.drawn_route_state = route_state();
    return ADVISOR_HEIGHT;
}

static void player_name(int player_id, uint8_t *text, int max_length)
{
    string_copy(translation_for(TR_MP_PLAYER), text, max_length - 4);
    string_from_int(text + string_length(text), player_id + 1, 0);
}

// a number whose last digit is on the given edge, so that the digits of a column line up
static void draw_number_right(int value, int force_sign, int right, int y, font_t font)
{
    uint8_t text[32] = { 0 };
    string_from_int(text, value, force_sign);
    text_draw(text, right - text_get_width(text, font), y, font, 0);
}

static void draw_text_right(const uint8_t *text, int right, int y, font_t font)
{
    text_draw(text, right - text_get_width(text, font), y, font, 0);
}

// a header over a column of numbers: its last letter on their right edge
static void draw_header_right(translation_key text, int right)
{
    const uint8_t *str = translation_for(text);
    text_draw(str, right - text_get_width(str, FONT_SMALL_PLAIN), HEADER_Y, FONT_SMALL_PLAIN, COLOR_WHITE);
}

// a header over a button or a pair of arrows and their value
static void draw_header_centered(translation_key text, int x, int width)
{
    text_draw_centered(translation_for(text), x, HEADER_Y, width, FONT_SMALL_PLAIN, COLOR_WHITE);
}

static void draw_row_button(int index, const uint8_t *text)
{
    const generic_button *b = &buttons[index];
    button_border_draw(b->x, b->y, b->width, b->height, data.focus_button_id == index + 1);
    text_draw_centered(text, b->x, b->y + 5, b->width, FONT_NORMAL_WHITE, 0);
}

// the value after a pair of arrows, on the right edge of its place
static void draw_arrows_value(int x, int y, int value, const uint8_t *text)
{
    if (text) {
        draw_text_right(text, x + ARROWS_WIDTH + VALUE_WIDTH, y + 6, FONT_NORMAL_WHITE);
    } else {
        draw_number_right(value, 0, x + ARROWS_WIDTH + VALUE_WIDTH, y + 6, FONT_NORMAL_WHITE);
    }
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

// whether the empire is where the city buys it, the cheapest source (shown in green, D-060)
static int empire_supplies(int resource, int cheaper_player)
{
    return city_resource_trade_status(resource) == TRADE_STATUS_IMPORT && empire_can_import_resource(resource) &&
        cheaper_player < 0;
}

static translation_key empire_status_text(int resource)
{
    int status = city_resource_trade_status(resource);
    return status == TRADE_STATUS_IMPORT ? TR_MP_EMPIRE_IMPORTS :
        status == TRADE_STATUS_EXPORT ? TR_MP_EMPIRE_EXPORTS : TR_MP_EMPIRE_NO_TRADE;
}

// the empire tab: what the city does with it, the price of Rome, and the price that applies, the portorium added on
// an import and taken off on an export (D-060; the detail in a tooltip)
static void draw_empire(int resource, int y, int button, int cheaper_player)
{
    if (city_resource_is_stockpiled(resource)) {
        lang_text_draw_centered(54, 3, X_EMPIRE_STATUS, y + 6, W_EMPIRE_STATUS, FONT_NORMAL_WHITE);
    } else if (empire_can_import_resource(resource) || empire_can_export_resource(resource)) {
        draw_row_button(button, translation_for(empire_status_text(resource)));
    }
    draw_number_right(trade_price_rome(resource), 0, X_ROME_PRICE, y + 6, FONT_NORMAL_WHITE);
    int is_export;
    int price = empire_price(resource, &is_export);
    if (price) {
        draw_number_right(price, 0, X_EMPIRE_PRICE, y + 6,
            !is_export && empire_supplies(resource, cheaper_player) ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
    }
}

// the stock tab: the stock kept, below which nothing is sold, and the stock at which the city stops buying, both for
// the empire and for the players (T4.5, D-070)
static void draw_bounds(int resource, int y)
{
    draw_arrows_value(X_KEEP, y, city_resource_export_over(resource), 0);
    int limit = mp_trade_buy_limit(local_player(), resource);
    draw_arrows_value(X_LIMIT, y, limit, limit > 0 ? 0 : translation_for(TR_MP_TRADE_NO_LIMIT));
}

static translation_key purchase_reason(int state, int long_text)
{
    switch (state) {
        case MP_TRADE_PURCHASE_NO_STOCK: return long_text ? TR_MP_PURCHASE_TIP_NO_STOCK : TR_MP_PURCHASE_NO_STOCK;
        case MP_TRADE_PURCHASE_NO_WAREHOUSE:
            return long_text ? TR_MP_PURCHASE_TIP_NO_WAREHOUSE : TR_MP_PURCHASE_NO_WAREHOUSE;
        case MP_TRADE_PURCHASE_NO_ROAD: return long_text ? TR_MP_PURCHASE_TIP_NO_ROAD : TR_MP_PURCHASE_NO_ROAD;
        case MP_TRADE_PURCHASE_NO_ROOM: return long_text ? TR_MP_PURCHASE_TIP_NO_ROOM : TR_MP_PURCHASE_NO_ROOM;
        case MP_TRADE_PURCHASE_NO_MONEY: return long_text ? TR_MP_PURCHASE_TIP_NO_MONEY : TR_MP_PURCHASE_NO_MONEY;
        case MP_TRADE_PURCHASE_LIMIT: return long_text ? TR_MP_PURCHASE_TIP_LIMIT : TR_MP_PURCHASE_LIMIT;
        case MP_TRADE_PURCHASE_AT_WAR: return long_text ? TR_MP_PURCHASE_TIP_AT_WAR : TR_MP_PURCHASE_AT_WAR;
        default: return 0;
    }
}

// Why no caravan of the selected player left this month with the resource, when the city buys it from him over an
// open route and nothing is on its way (T5.7, D-075); 0 otherwise
static translation_key why_nothing_comes(int resource, int long_text)
{
    int me = local_player();
    if (mp_trade_loads_on_the_way(data.tab, me, resource) > 0 || !mp_trade_buys_from(me, data.tab, resource) ||
        !mp_trade_route_is_open(me, data.tab)) {
        return 0;
    }
    return purchase_reason(mp_trade_purchase_state(data.tab, me, resource), long_text);
}

// the selected player: my price to him, his price to me (green when the cheaper source), whether I buy from him, and
// what his caravans bring me, or in short why none left this month (T5.7, D-075)
static void draw_partner(int resource, int y, int button, int cheaper_player)
{
    int me = local_player();
    draw_arrows_value(X_MY_PRICE, y, mp_trade_price(me, data.tab, resource), 0);
    draw_number_right(mp_trade_price(data.tab, me, resource), 0, X_HIS_PRICE, y + 6,
        cheaper_player == data.tab ? FONT_NORMAL_GREEN : FONT_NORMAL_WHITE);
    draw_row_button(button, translation_for(mp_trade_buys_from(me, data.tab, resource) ? TR_MP_YES : TR_MP_NO));
    int loads = mp_trade_loads_on_the_way(data.tab, me, resource);
    if (loads > 0) {
        draw_number_right(loads, 0, X_ON_THE_WAY, y + 6, FONT_NORMAL_WHITE);
    } else {
        translation_key reason = why_nothing_comes(resource, 0);
        if (reason) {
            const uint8_t *str = translation_for(reason);
            text_draw(str, X_ON_THE_WAY - text_get_width(str, FONT_SMALL_PLAIN), y + 7, FONT_SMALL_PLAIN,
                COLOR_WHITE);
        }
    }
}

static void draw_headers(void)
{
    draw_header_right(TR_MP_TRADE_STOCK, X_STOCK);
    if (empire_tab_shown()) {
        draw_header_centered(TR_MP_TRADE_STATUS, X_EMPIRE_STATUS, W_EMPIRE_STATUS);
        draw_header_right(TR_MP_TRADE_ROME, X_ROME_PRICE);
        draw_header_right(TR_MP_TRADE_PRICE, X_EMPIRE_PRICE);
    } else if (stock_tab_shown()) {
        draw_header_centered(TR_MP_TRADE_KEEP, X_KEEP - 20, ARROWS_WIDTH + VALUE_WIDTH + 40);
        draw_header_centered(TR_MP_TRADE_BUY_LIMIT, X_LIMIT - 20, ARROWS_WIDTH + VALUE_WIDTH + 40);
    } else {
        draw_header_centered(TR_MP_TRADE_I_SELL, X_MY_PRICE, ARROWS_WIDTH + VALUE_WIDTH);
        draw_header_right(TR_MP_TRADE_HE_SELLS, X_HIS_PRICE);
        draw_header_centered(TR_MP_TRADE_I_BUY, X_BUY - 10, W_BUY + 20);
        draw_header_right(TR_MP_RESOURCE_ON_THE_WAY, X_ON_THE_WAY);
    }
}

// the route with the selected player: the button says what a click does
static translation_key route_button_text(void)
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

// the one help line of the tab; on the tab of a player, the state of the route while it is not open, and what to do
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
    text_draw(text, HELP_X, HELP_Y, FONT_SMALL_PLAIN, 0);
}

// The name of a player on a tab, in his colour: darker on the parchment of a button, with a dark shadow on the stone of
// the selected tab, like the white texts of the table, so that the yellow and the blue stay readable
static void draw_tab_player(int player_id, int x, int y, int width, int selected)
{
    uint8_t name[64];
    player_name(player_id, name, 64);
    color_t color = mp_colors_player(player_id);
    if (selected) {
        text_draw_centered(name, x + 1, y + 1, width, FONT_NORMAL_PLAIN, COLOR_SHADOW);
    } else {
        int r = ((color >> 16) & 0xff) * 2 / 3;
        int g = ((color >> 8) & 0xff) * 2 / 3;
        int b = (color & 0xff) * 2 / 3;
        color = (color & 0xff000000) | (r << 16) | (g << 8) | b;
    }
    text_draw_centered(name, x, y, width, FONT_NORMAL_PLAIN, color);
}

// The empire, the other players and the stocks, in a row on the top edge of the table. The selected tab is cut in the
// sunken stone of the table and opens into it; the others are buttons of the original game.
static void draw_tabs(void)
{
    for (int i = 0; i < data.num_tabs; i++) {
        const generic_button *b = &buttons[i];
        int tab = data.tabs[i];
        int selected = tab == data.tab;
        if (selected) {
            // the sunken panel, without its bottom border, over the top border of the table
            graphics_set_clip_rectangle(b->x, TAB_Y, b->width, TABLE_Y + 4 - TAB_Y);
            inner_panel_draw(b->x, TAB_Y, TAB_WIDTH_BLOCKS, 3);
            graphics_reset_clip_rectangle();
        } else {
            button_border_draw(b->x, b->y, b->width, b->height, data.focus_button_id == i + 1);
        }
        int text_y = TAB_Y + 6;
        if (tab == STOCK_TAB || tab == EMPIRE_TAB) {
            text_draw_centered(translation_for(tab == STOCK_TAB ? TR_MP_TRADE_STOCK_TAB : TR_MP_TRADE_EMPIRE),
                b->x, text_y, b->width, selected ? FONT_NORMAL_WHITE : FONT_NORMAL_BLACK, 0);
        } else {
            draw_tab_player(tab, b->x, text_y, b->width, selected);
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
    if (data.route_button >= 0) {
        const generic_button *route = &buttons[data.route_button];
        button_border_draw(route->x, route->y, route->width, route->height,
            data.focus_button_id == data.route_button + 1);
        text_draw_centered(translation_for(route_button_text()), route->x, route->y + 7, route->width,
            FONT_NORMAL_BLACK, 0);
    }

    // one row per resource shown
    inner_panel_draw(TABLE_X, TABLE_Y, TABLE_WIDTH_BLOCKS, TABLE_HEIGHT_BLOCKS);
    draw_tabs();
    draw_headers();
    int button = data.first_row_button;
    for (int i = 0; i < ROWS_SHOWN; i++) {
        int resource = RESOURCE_MIN + scrollbar.scroll_position + i;
        int y = row_y(i);
        if (data.focus_button_id == button + 1) {
            button_border_draw(buttons[button].x, buttons[button].y, buttons[button].width, buttons[button].height, 1);
        }
        image_draw(image_group(GROUP_RESOURCE_ICONS) + resource + resource_image_offset(resource, RESOURCE_IMAGE_ICON),
            X_ICON, y + 1);
        lang_text_draw(23, resource, X_NAME, y + 6, FONT_NORMAL_WHITE);
        draw_number_right(city_resource_count(resource), 0, X_STOCK, y + 6, FONT_NORMAL_WHITE);
        // in green, the cheaper source: the empire or this player (the empire always sells, D-060)
        int cheaper_player = mp_trade_cheaper_player(resource);
        if (empire_tab_shown()) {
            draw_empire(resource, y, button + 1, cheaper_player);
        } else if (stock_tab_shown()) {
            draw_bounds(resource, y);
        } else {
            draw_partner(resource, y, button + 1, cheaper_player);
        }
        button += data.buttons_per_row;
    }
    arrow_buttons_draw(0, 0, arrows, data.num_arrows);
    scrollbar_draw(&scrollbar);
    draw_help();

    button_border_draw(100, 398, 200, 23, data.focus_button_id == data.map_button + 1);
    lang_text_draw_centered(54, 30, 100, 404, 200, FONT_NORMAL_BLACK);
    button_border_draw(400, 398, 200, 23, data.focus_button_id == data.map_button + 2);
    lang_text_draw_centered(54, 2, 400, 404, 200, FONT_NORMAL_BLACK);
}

int window_mp_trade_handle_mouse(const mouse *m)
{
    if (scrollbar_handle_mouse(&scrollbar, m)) {
        data.focus_button_id = 0;
        data.focus_arrow_id = 0;
        return 1;
    }
    if (arrow_buttons_handle_mouse(m, 0, 0, arrows, data.num_arrows, &data.focus_arrow_id)) {
        data.focus_button_id = 0;
        return 1;
    }
    return generic_buttons_handle_mouse(m, 0, 0, buttons, data.num_buttons, &data.focus_button_id);
}

// the row under the mouse, or -1
static int row_at(int y)
{
    if (y < FIRST_ROW_Y || y >= row_y(ROWS_SHOWN)) {
        return -1;
    }
    return (y - FIRST_ROW_Y) / ROW_HEIGHT;
}

static void set_tooltip(tooltip_context *c, const uint8_t *text)
{
    c->type = TOOLTIP_BUTTON;
    c->precomposed_text = text;
}

static void set_tooltip_key(tooltip_context *c, translation_key key)
{
    set_tooltip(c, translation_for(key));
}

// appends a text or a number to the tooltip being composed
static void tooltip_add_text(const uint8_t *text)
{
    int length = string_length(data.tooltip);
    string_copy(text, data.tooltip + length, (int) sizeof(data.tooltip) - length);
}

static void tooltip_add_number(int value, int force_sign)
{
    uint8_t number[16];
    string_from_int(number, value, force_sign);
    tooltip_add_text(number);
}

// "Price of Rome 200, portorium +100."
static const uint8_t *compose_empire_price(int resource, int is_export)
{
    data.tooltip[0] = 0;
    tooltip_add_text(translation_for(TR_MP_TRADE_TIP_ROME));
    tooltip_add_number(trade_price_rome(resource), 0);
    tooltip_add_text(translation_for(TR_MP_TRADE_TIP_PORTORIUM));
    int duty = trade_price_duty(resource);
    tooltip_add_number(is_export ? -duty : duty, 1);
    tooltip_add_text((const uint8_t *) ".");
    return data.tooltip;
}

// "The empire sells it to you at 300 Dn, portorium included. In green: the cheaper of the two."
static const uint8_t *compose_empire_sells(int resource)
{
    if (!empire_can_import_resource_potentially(resource)) {
        return translation_for(TR_MP_TRADE_TIP_EMPIRE_NONE);
    }
    data.tooltip[0] = 0;
    tooltip_add_text(translation_for(TR_MP_TRADE_TIP_EMPIRE_SELLS));
    tooltip_add_number(trade_price_buy(resource), 0);
    tooltip_add_text(translation_for(TR_MP_TRADE_TIP_EMPIRE_SELLS_2));
    return data.tooltip;
}

// "8 loads on their way to your warehouses."
static const uint8_t *compose_on_the_way(int loads)
{
    data.tooltip[0] = 0;
    tooltip_add_number(loads, 0);
    tooltip_add_text(translation_for(TR_MP_TRADE_TIP_ON_THE_WAY));
    return data.tooltip;
}

// the tooltip of a value of the table, under the mouse at (x, y) in the row of the resource
static void tooltip_for_value(tooltip_context *c, int resource, int x)
{
    if (empire_tab_shown()) {
        int is_export;
        if (x >= X_ROME_PRICE + 20 && x < X_EMPIRE_PRICE + 4 && empire_price(resource, &is_export)) {
            set_tooltip(c, compose_empire_price(resource, is_export));
        }
    } else if (player_tab_shown()) {
        if (x >= X_HIS_PRICE - 60 && x < X_HIS_PRICE + 8) {
            set_tooltip(c, compose_empire_sells(resource));
        } else if (x >= X_BUY + W_BUY + 8 && x < X_ON_THE_WAY + 8) {
            int loads = mp_trade_loads_on_the_way(data.tab, local_player(), resource);
            translation_key reason = why_nothing_comes(resource, 1);
            if (loads > 0) {
                set_tooltip(c, compose_on_the_way(loads));
            } else if (reason) {
                set_tooltip_key(c, reason);
            }
        }
    }
}

void window_mp_trade_get_tooltip(tooltip_context *c)
{
    if (data.focus_arrow_id) {
        // on the stock tab, the first pair of a row is the stock kept, the second one the limit
        int first_pair = (data.focus_arrow_id - 1) % data.arrows_per_row < 2;
        set_tooltip_key(c, !stock_tab_shown() ? TR_MP_TRADE_TIP_MY_PRICE :
            first_pair ? TR_MP_TRADE_TIP_KEEP : TR_MP_TRADE_TIP_LIMIT);
        return;
    }
    int button = data.focus_button_id - 1;
    if (button >= data.map_button) {
        c->type = TOOLTIP_BUTTON;
        c->text_id = button == data.map_button ? 41 : 106; // map and prices of the empire
    } else if (button >= data.first_row_button) {
        if ((button - data.first_row_button) % data.buttons_per_row == 0) {
            c->type = TOOLTIP_BUTTON;
            c->text_id = 107; // the settings of the resource
        } else {
            set_tooltip_key(c, empire_tab_shown() ? TR_MP_TRADE_TIP_STATUS : TR_MP_TRADE_TIP_BUY);
        }
    } else if (button < 0) {
        int x = c->mouse_x - screen_dialog_offset_x();
        int row = row_at(c->mouse_y - screen_dialog_offset_y());
        if (row >= 0 && x >= TABLE_X && x < TABLE_RIGHT) {
            tooltip_for_value(c, RESOURCE_MIN + scrollbar.scroll_position + row, x);
        }
    }
}

static void on_scroll(void)
{
    // the buttons of the rows shown at once, the frame under the scrollbar on the next frame
    init_buttons();
    window_invalidate();
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

static void arrow_price(int resource, int is_down)
{
    int price = mp_trade_price(local_player(), data.tab, resource) + (is_down ? -PRICE_STEP : PRICE_STEP);
    mp_action_set_sell_price(data.tab, resource, price < 1 ? 1 : price);
}

static void button_buy(int resource, int param2)
{
    mp_action_set_buys_from(data.tab, resource, !mp_trade_buys_from(local_player(), data.tab, resource));
}

static void arrow_keep(int resource, int is_down)
{
    mp_action_change_export_over(resource, is_down ? -STOCK_STEP : STOCK_STEP);
}

static void arrow_limit(int resource, int is_down)
{
    mp_action_change_buy_limit(resource, is_down ? -STOCK_STEP : STOCK_STEP);
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

void window_mp_trade_show_resource(int player_id, int resource)
{
    // the resource in the rows shown, as low as it may be
    int position = resource - RESOURCE_MIN - ROWS_SHOWN + 1;
    if (position > scrollbar.scroll_position || resource - RESOURCE_MIN < scrollbar.scroll_position) {
        scrollbar.scroll_position = calc_bound(position, 0, NUM_RESOURCES - ROWS_SHOWN);
    }
    window_mp_trade_show_partner(player_id);
}
