#include "trade.h"

#include "building/building.h"
#include "building/warehouse.h"
#include "city/finance.h"
#include "city/resource.h"
#include "city/warning.h"
#include "core/image.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
#include "map/road_access.h"
#include "map/grid.h"
#include "building/storage.h"
#include "core/lang.h"
#include "core/string.h"
#include "empire/trade_prices.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "mp/session.h"
#include "translation/translation.h"

#include <string.h>

#define MAX_PRICE 9999

// state of each city: the prices it asks each buyer (0: the price of the empire), what it buys from each seller
static int16_t asked_prices[PLAYER_CONTEXT_MAX_PLAYERS][PLAYER_CONTEXT_MAX_PLAYERS][RESOURCE_MAX];
static uint8_t buys[PLAYER_CONTEXT_MAX_PLAYERS][PLAYER_CONTEXT_MAX_PLAYERS][RESOURCE_MAX];

static int notifications;

// state of each city: the players it proposed a trade route to; a route is open when both proposed it
static uint8_t proposed[PLAYER_CONTEXT_MAX_PLAYERS][PLAYER_CONTEXT_MAX_PLAYERS];

#define MAX_CARAVAN_LOADS 8
#define ACTION_GOING FIGURE_ACTION_222_MP_CARAVAN_GOING

static int is_player(int player_id)
{
    return player_id >= 0 && player_id < player_context_num_players();
}

static int is_resource(int resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_MAX;
}

int mp_trade_price(int seller, int buyer, int resource)
{
    if (!is_player(seller) || !is_player(buyer) || !is_resource(resource)) {
        return 0;
    }
    int price = asked_prices[seller][buyer][resource];
    return price > 0 ? price : trade_price_buy_base(resource);
}

static void append(uint8_t *text, const uint8_t *part)
{
    int length = string_length(text);
    string_copy(part, text + length, 200 - length);
}

static void append_number(uint8_t *text, int value)
{
    uint8_t number[16];
    string_from_int(number, value, 0);
    append(text, number);
}

// "Player 2 now sells marble at 180 Dn (was 150)", only on the computer of the buyer
static void tell_buyer(int seller, int resource, int old_price, int new_price)
{
    notifications++;
    uint8_t text[200] = { 0 };
    append(text, translation_for(TR_MP_PRICE_CHANGED_PLAYER));
    append_number(text, seller + 1);
    append(text, translation_for(TR_MP_PRICE_CHANGED_SELLS));
    append(text, lang_get_string(23, resource));
    append(text, (const uint8_t *) " ");
    append_number(text, new_price);
    append(text, translation_for(TR_MP_PRICE_CHANGED_WAS));
    append_number(text, old_price);
    append(text, (const uint8_t *) ")");
    city_warning_show_to_local_player(text);
}

void mp_trade_set_price(int buyer, int resource, int price)
{
    int seller = player_context_current_player;
    if (!is_player(buyer) || buyer == seller || !is_resource(resource) || price < 1 || price > MAX_PRICE) {
        return;
    }
    int old_price = mp_trade_price(seller, buyer, resource);
    asked_prices[seller][buyer][resource] = (int16_t) price;
    if (price != old_price && buys[buyer][seller][resource] && buyer == mp_session_local_player_id()) {
        tell_buyer(seller, resource, old_price, price);
    }
}

int mp_trade_buys_from(int buyer, int seller, int resource)
{
    if (!is_player(seller) || !is_player(buyer) || !is_resource(resource)) {
        return 0;
    }
    return buys[buyer][seller][resource];
}

void mp_trade_set_buys_from(int seller, int resource, int buys_it)
{
    int buyer = player_context_current_player;
    if (!is_player(seller) || seller == buyer || !is_resource(resource)) {
        return;
    }
    buys[buyer][seller][resource] = buys_it ? 1 : 0;
}

int mp_trade_notifications(void)
{
    return notifications;
}

void mp_trade_reset_extra_state(void)
{
    memset(asked_prices, 0, sizeof(asked_prices));
    memset(buys, 0, sizeof(buys));
    memset(proposed, 0, sizeof(proposed));
}

void mp_trade_save_extra_state(buffer *buf)
{
    int p = player_context_current_player;
    for (int buyer = 0; buyer < PLAYER_CONTEXT_MAX_PLAYERS; buyer++) {
        for (int r = 0; r < RESOURCE_MAX; r++) {
            buffer_write_i16(buf, asked_prices[p][buyer][r]);
        }
    }
    buffer_write_raw(buf, buys[p], sizeof(buys[p]));
    buffer_write_raw(buf, proposed[p], sizeof(proposed[p]));
}

void mp_trade_load_extra_state(buffer *buf)
{
    int p = player_context_current_player;
    for (int buyer = 0; buyer < PLAYER_CONTEXT_MAX_PLAYERS; buyer++) {
        for (int r = 0; r < RESOURCE_MAX; r++) {
            asked_prices[p][buyer][r] = buffer_read_i16(buf);
        }
    }
    buffer_read_raw(buf, buys[p], sizeof(buys[p]));
    buffer_read_raw(buf, proposed[p], sizeof(proposed[p]));
}

// ---------- trade routes ----------

int mp_trade_route_is_open(int a, int b)
{
    return is_player(a) && is_player(b) && a != b && proposed[a][b] && proposed[b][a];
}

int mp_trade_route_is_proposed(int from, int to)
{
    return is_player(from) && is_player(to) && proposed[from][to];
}

void mp_trade_propose_route(int other, int propose)
{
    int self = player_context_current_player;
    if (!is_player(other) || other == self || proposed[self][other] == (propose ? 1 : 0)) {
        return;
    }
    proposed[self][other] = propose ? 1 : 0;
    if (other == mp_session_local_player_id()) {
        notifications++;
        uint8_t text[200] = { 0 };
        append(text, translation_for(TR_MP_PRICE_CHANGED_PLAYER));
        append_number(text, self + 1);
        append(text, translation_for(!propose ? TR_MP_ROUTE_CLOSED :
            proposed[other][self] ? TR_MP_ROUTE_OPENED : TR_MP_ROUTE_PROPOSED));
        city_warning_show_to_local_player(text);
    }
}

// ---------- caravans ----------

typedef struct {
    int treasury;
    int warehouse_id;
    int road_x;
    int road_y;
    int road_network_id;
} buyer_view;

// what the seller may know of a buyer: his money, a warehouse of his that takes the resource
static void look_at_buyer(int buyer, int resource, buyer_view *v)
{
    int previous = player_context_current();
    player_context_switch(buyer);
    v->treasury = city_finance_treasury();
    v->warehouse_id = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END && !v->warehouse_id; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_WAREHOUSE || !b->has_road_access) {
            continue;
        }
        const building_storage *storage = building_storage_get(b->storage_id);
        if (storage->resource_state[resource] == BUILDING_STORAGE_STATE_NOT_ACCEPTING || storage->empty_all) {
            continue;
        }
        map_point road;
        if (map_has_road_access(b->x, b->y, b->size, &road)) {
            v->warehouse_id = i;
            v->road_x = road.x;
            v->road_y = road.y;
            v->road_network_id = b->road_network_id;
        }
    }
    player_context_switch(previous);
}

static int has_caravan_to(int buyer, int resource)
{
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f) &&
            BUILDING_OWNER(f->destination_building_id) == buyer && f->resource_id == resource) {
            return 1;
        }
    }
    return 0;
}

static building *seller_warehouse(int road_network_id, map_point *road)
{
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_WAREHOUSE && b->has_road_access &&
            b->road_network_id == road_network_id && map_has_road_access(b->x, b->y, b->size, road)) {
            return b;
        }
    }
    return 0;
}

// a caravan for each resource the buyer buys, one at a time per resource, within what he can pay this month
static void dispatch_to(int buyer)
{
    int seller = player_context_current_player;
    int budget = -1; // what the buyer can still spend, once looked at
    for (int resource = RESOURCE_NONE + 1; resource < RESOURCE_MAX; resource++) {
        // the settings of the empire do not apply: a buyer says what he buys from whom, a seller sells beyond his
        // export threshold what he does not stockpile
        if (!buys[buyer][seller][resource] || city_resource_is_stockpiled(resource) ||
            has_caravan_to(buyer, resource)) {
            continue;
        }
        int available = city_resource_count(resource) - city_resource_export_over(resource);
        if (available <= 0) {
            continue;
        }
        buyer_view v;
        look_at_buyer(buyer, resource, &v);
        if (budget < 0) {
            budget = v.treasury;
        }
        int price = mp_trade_price(seller, buyer, resource);
        int affordable = price > 0 ? budget / price : 0;
        int loads = available < MAX_CARAVAN_LOADS ? available : MAX_CARAVAN_LOADS;
        loads = loads < affordable ? loads : affordable;
        if (!v.warehouse_id || loads <= 0) {
            continue;
        }
        map_point road;
        building *from = seller_warehouse(v.road_network_id, &road);
        if (!from) {
            continue; // no road between the two cities
        }
        loads = building_warehouses_remove_resource(resource, loads);
        if (loads <= 0) {
            continue;
        }
        figure *f = figure_create(FIGURE_TRADE_CARAVAN, road.x, road.y, DIR_0_TOP);
        f->action_state = ACTION_GOING;
        f->building_id = from->id;
        f->destination_building_id = v.warehouse_id;
        f->destination_x = v.road_x;
        f->destination_y = v.road_y;
        f->destination_grid_offset = map_grid_offset(v.road_x, v.road_y);
        f->resource_id = resource;
        f->loads_sold_or_carrying = loads;
        f->terrain_usage = TERRAIN_USAGE_ROADS;
        budget -= loads * price;
    }
}

void mp_trade_dispatch_caravans(void)
{
    if (player_context_num_players() <= 1 || game_time_day() != 0) {
        return; // once a month
    }
    int seller = player_context_current_player;
    for (int buyer = 0; buyer < player_context_num_players(); buyer++) {
        if (mp_trade_route_is_open(seller, buyer)) {
            dispatch_to(buyer);
        }
    }
}

int mp_trade_is_caravan(const figure *f)
{
    return f->type == FIGURE_TRADE_CARAVAN && f->action_state == ACTION_GOING;
}

// "Player 2 delivered to you: 8 marble for 1200 Dn", or that his caravan found no room, on the computer of the buyer
static void tell_delivery(int seller, int resource, int loads, int paid)
{
    notifications++;
    uint8_t text[200] = { 0 };
    append(text, translation_for(TR_MP_PRICE_CHANGED_PLAYER));
    append_number(text, seller + 1);
    if (loads > 0) {
        append(text, translation_for(TR_MP_DELIVERED));
        append_number(text, loads);
        append(text, (const uint8_t *) " ");
        append(text, lang_get_string(23, resource));
        append(text, translation_for(TR_MP_DELIVERED_FOR));
        append_number(text, paid);
        append(text, (const uint8_t *) " Dn");
    } else {
        append(text, translation_for(TR_MP_DELIVERY_NO_ROOM));
        append(text, lang_get_string(23, resource));
    }
    city_warning_show_to_local_player(text);
}

// the caravan reached the warehouse of the buyer: he stores what he can and pays for it, the rest goes back
static void deliver(figure *f)
{
    int seller = player_context_current_player;
    int buyer = BUILDING_OWNER(f->destination_building_id);
    int resource = f->resource_id;
    int price = mp_trade_price(seller, buyer, resource);
    int loads = f->loads_sold_or_carrying;
    int stored = 0;
    player_context_switch(buyer);
    for (int i = BUILDING_FIRST; i < BUILDING_END && stored < loads; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_WAREHOUSE) {
            while (stored < loads && building_warehouse_add_resource(b, resource)) {
                stored++;
            }
        }
    }
    for (int i = 0; i < stored; i++) {
        city_finance_process_import(price);
    }
    player_context_switch(seller);
    for (int i = 0; i < stored; i++) {
        city_finance_process_export(price);
    }
    if (stored < loads) {
        building_warehouses_add_resource(resource, loads - stored);
    }
    f->loads_sold_or_carrying = 0;
    if (buyer == mp_session_local_player_id()) {
        tell_delivery(seller, resource, stored, stored * price);
    }
}

// ---------- the empire as the dearer source ----------

int mp_trade_cheaper_player(int resource)
{
    int buyer = player_context_current_player;
    if (player_context_num_players() <= 1 || !is_resource(resource)) {
        return -1;
    }
    int cheapest = -1;
    int best = trade_price_buy(resource); // the empire, with its multiplayer surcharge
    for (int seller = 0; seller < player_context_num_players(); seller++) {
        if (seller == buyer || !buys[buyer][seller][resource] || !mp_trade_route_is_open(buyer, seller)) {
            continue;
        }
        int price = mp_trade_price(seller, buyer, resource);
        if (price < best) {
            best = price;
            cheapest = seller;
        }
    }
    return cheapest;
}

int mp_trade_empire_may_sell(int resource)
{
    return mp_trade_cheaper_player(resource) < 0;
}

void mp_trade_caravan_action(figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    figure_movement_move_ticks(f, 1);
    if (f->direction == DIR_FIGURE_AT_DESTINATION) {
        deliver(f);
        f->state = FIGURE_STATE_DEAD;
    } else if (f->direction == DIR_FIGURE_REROUTE) {
        figure_route_remove(f);
    } else if (f->direction == DIR_FIGURE_LOST) {
        // the road was cut: the goods go back to the warehouses of the seller
        building_warehouses_add_resource(f->resource_id, f->loads_sold_or_carrying);
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_update(f, image_group(GROUP_FIGURE_TRADE_CARAVAN));
}
