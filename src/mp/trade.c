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
#include "core/log.h"
#include "core/string.h"
#include "empire/trade_prices.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "game/rules.h"
#include "mp/session.h"
#include "mp/war.h"
#include "translation/translation.h"

#include <stdio.h>
#include <string.h>

#define MAX_PRICE 9999

// state of each city: the prices it asks each buyer (0: the price of the empire), what it buys from each seller
static int16_t asked_prices[PLAYER_CONTEXT_MAX_PLAYERS][PLAYER_CONTEXT_MAX_PLAYERS][RESOURCE_MAX];
static uint8_t buys[PLAYER_CONTEXT_MAX_PLAYERS][PLAYER_CONTEXT_MAX_PLAYERS][RESOURCE_MAX];

// state of each city: the stock of each resource at which it stops buying it (0: no limit of its own, T4.5, D-070)
static int16_t buy_limits[PLAYER_CONTEXT_MAX_PLAYERS][RESOURCE_MAX];

#define BOUNDS_VERSION 1

// what became of each purchase at the last monthly departure of the caravans, [seller][buyer][resource]: display and
// log only, never read by the simulation (see mp_trade_purchase_state); known again a month after a game is loaded
static uint8_t purchase_states[PLAYER_CONTEXT_MAX_PLAYERS][PLAYER_CONTEXT_MAX_PLAYERS][RESOURCE_MAX];

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
    return price > 0 ? price : trade_price_rome(resource);
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

// price changes not yet seen by the local buyer (display only, see mp_price_alert); at most one per seller and resource
static struct {
    mp_price_alert items[PLAYER_CONTEXT_MAX_PLAYERS * RESOURCE_MAX];
    int count;
} alerts;

// only on the computer of the buyer: a full-screen alert, drawn by window/mp_price_alert
static void tell_buyer(int seller, int resource, int old_price, int new_price)
{
    notifications++;
    for (int i = 0; i < alerts.count; i++) {
        mp_price_alert *alert = &alerts.items[i];
        if (alert->seller != seller || alert->resource != resource) {
            continue;
        }
        alert->new_price = new_price;
        if (alert->new_price == alert->old_price) {
            // back to the price the buyer knew: nothing to tell any more
            alerts.count--;
            memmove(alert, alert + 1, (alerts.count - i) * sizeof(mp_price_alert));
        }
        return;
    }
    mp_price_alert *alert = &alerts.items[alerts.count++];
    alert->seller = seller;
    alert->resource = resource;
    alert->old_price = old_price;
    alert->new_price = new_price;
}

int mp_trade_num_price_alerts(void)
{
    return alerts.count;
}

const mp_price_alert *mp_trade_price_alert(int index)
{
    return index >= 0 && index < alerts.count ? &alerts.items[index] : 0;
}

void mp_trade_clear_price_alerts(void)
{
    alerts.count = 0;
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

// ---------- stock limits (T4.5, D-070) ----------

int mp_trade_buy_limit(int player_id, int resource)
{
    if (!game_rules_is_multiplayer() || player_id < 0 || player_id >= PLAYER_CONTEXT_MAX_PLAYERS ||
        !is_resource(resource)) {
        return 0;
    }
    return buy_limits[player_id][resource];
}

static int16_t valid_limit(int limit)
{
    return (int16_t) (limit < 0 ? 0 : limit > MP_TRADE_MAX_BUY_LIMIT ? MP_TRADE_MAX_BUY_LIMIT : limit);
}

void mp_trade_change_buy_limit(int resource, int delta)
{
    if (!game_rules_is_multiplayer() || !is_resource(resource)) {
        return;
    }
    int16_t *limit = &buy_limits[player_context_current_player][resource];
    *limit = valid_limit(*limit + delta);
}

int mp_trade_empire_buy_limit(int resource)
{
    return mp_trade_buy_limit(player_context_current_player, resource);
}

// loads the buyer may still receive before his stock reaches his limit, counting what caravans bring him; -1: no limit
static int room_under_limit(int buyer, int resource)
{
    int limit = mp_trade_buy_limit(buyer, resource);
    if (limit <= 0) {
        return -1;
    }
    int previous = player_context_current();
    player_context_switch(buyer);
    int stock = city_resource_count(resource);
    player_context_switch(previous);
    for (int seller = 0; seller < player_context_num_players(); seller++) {
        stock += mp_trade_loads_on_the_way(seller, buyer, resource);
    }
    return stock < limit ? limit - stock : 0;
}

void mp_trade_reset_bounds(void)
{
    memset(buy_limits, 0, sizeof(buy_limits));
}

void mp_trade_save_bounds(buffer *buf)
{
    buffer_write_i32(buf, BOUNDS_VERSION);
    buffer_write_i32(buf, PLAYER_CONTEXT_MAX_PLAYERS);
    buffer_write_i32(buf, RESOURCE_MAX);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        for (int r = 0; r < RESOURCE_MAX; r++) {
            buffer_write_i16(buf, buy_limits[p][r]);
        }
    }
}

void mp_trade_load_bounds(buffer *buf)
{
    mp_trade_reset_bounds();
    int version = buffer_read_i32(buf);
    int num_players = buffer_read_i32(buf);
    int num_resources = buffer_read_i32(buf);
    if (version < 1 || version > BOUNDS_VERSION || num_players != PLAYER_CONTEXT_MAX_PLAYERS ||
        num_resources != RESOURCE_MAX) {
        return;
    }
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        for (int r = 0; r < RESOURCE_MAX; r++) {
            buy_limits[p][r] = valid_limit(buffer_read_i16(buf));
        }
    }
}

void mp_trade_reset_extra_state(void)
{
    memset(asked_prices, 0, sizeof(asked_prices));
    memset(buys, 0, sizeof(buys));
    memset(proposed, 0, sizeof(proposed));
    mp_trade_reset_bounds();
    memset(purchase_states, 0, sizeof(purchase_states));
    alerts.count = 0;
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
    memset(purchase_states[p], 0, sizeof(purchase_states[p]));
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

// road networks of the warehouses of a seller, with one of its warehouses on each
#define MAX_SELLER_NETWORKS 64
typedef struct {
    int count;
    int network[MAX_SELLER_NETWORKS];
    int warehouse_id[MAX_SELLER_NETWORKS];
    map_point road[MAX_SELLER_NETWORKS];
} seller_view;

typedef struct {
    int treasury;
    int room; // loads of the resource his warehouses can still store
    int accepts; // a warehouse of his with a road takes the resource
    int warehouse_id; // one of them on a road network of the seller
    int road_x;
    int road_y;
    int network_index; // in the seller_view
} buyer_view;

// the warehouses of the current city (the seller) from which a caravan may leave: the first one on each road network
static void look_at_seller(seller_view *v)
{
    v->count = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END && v->count < MAX_SELLER_NETWORKS; i++) {
        building *b = building_get(i);
        map_point road;
        if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_WAREHOUSE || !b->has_road_access ||
            !map_has_road_access(b->x, b->y, b->size, &road)) {
            continue;
        }
        int known = 0;
        for (int n = 0; n < v->count && !known; n++) {
            known = v->network[n] == b->road_network_id;
        }
        if (!known) {
            v->network[v->count] = b->road_network_id;
            v->warehouse_id[v->count] = i;
            v->road[v->count] = road;
            v->count++;
        }
    }
}

// loads of the resource a warehouse can still store, as a delivery fills it (building_warehouse_add_resource)
static int warehouse_room(building *warehouse, int resource)
{
    int room = 0;
    building *space = warehouse;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id <= 0) {
            break;
        }
        if (!space->subtype.warehouse_resource_id || space->subtype.warehouse_resource_id == resource) {
            room += space->loads_stored < 4 ? 4 - space->loads_stored : 0;
        }
    }
    return room;
}

// what the seller may know of a buyer: his money, the room his warehouses have for the resource, a warehouse of his
// that takes the resource on a road network that a warehouse of the seller is on. Not only his first warehouse: one
// on a road of its own, joined to nothing, must not stop the caravans that his other warehouses can receive (T5.7)
static void look_at_buyer(int buyer, int resource, const seller_view *seller, buyer_view *v)
{
    int previous = player_context_current();
    player_context_switch(buyer);
    v->treasury = city_finance_treasury();
    v->room = 0;
    v->accepts = 0;
    v->warehouse_id = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_WAREHOUSE) {
            continue;
        }
        // a delivery fills any of his warehouses (deliver)
        v->room += warehouse_room(b, resource);
        if (!b->has_road_access || v->warehouse_id) {
            continue;
        }
        const building_storage *storage = building_storage_get(b->storage_id);
        map_point road;
        if (storage->resource_state[resource] == BUILDING_STORAGE_STATE_NOT_ACCEPTING || storage->empty_all ||
            !map_has_road_access(b->x, b->y, b->size, &road)) {
            continue;
        }
        v->accepts = 1;
        for (int n = 0; n < seller->count; n++) {
            if (seller->network[n] == b->road_network_id) {
                v->warehouse_id = i;
                v->road_x = road.x;
                v->road_y = road.y;
                v->network_index = n;
                break;
            }
        }
    }
    player_context_switch(previous);
}

// a caravan whose players went to war goes back to a warehouse of its own player, with its goods (T5.5, D-077)
static int is_going_home(const figure *f)
{
    return BUILDING_OWNER(f->destination_building_id) == FIGURE_OWNER(f->id);
}

// what the buyer owes for the loads that caravans bring him, at the prices of their sellers: he pays on arrival, so
// this is no longer his to spend
static int value_on_the_way(int buyer)
{
    int value = 0;
    for (int i = 1; i < player_context_num_players() * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f) && !is_going_home(f) &&
            BUILDING_OWNER(f->destination_building_id) == buyer) {
            value += f->loads_sold_or_carrying * mp_trade_price(FIGURE_OWNER(i), buyer, f->resource_id);
        }
    }
    return value;
}

static void set_purchase_state(int seller, int buyer, int resource, int state)
{
    uint8_t *known = &purchase_states[seller][buyer][resource];
    if (*known != state) {
        // in the log of every computer, for the players who wonder why nothing comes (T5.7)
        static const char *TEXTS[] = { "unknown", "a caravan leaves", "the seller has none to sell",
            "the buyer has no warehouse taking it", "no road joins their warehouses", "the buyer has no room left",
            "the buyer cannot pay", "the buyer reached his stock limit", "the players are at war" };
        char text[100];
        snprintf(text, sizeof(text), "player %d to player %d, resource %d: %s", seller + 1, buyer + 1, resource,
            state >= 0 && state < (int) (sizeof(TEXTS) / sizeof(TEXTS[0])) ? TEXTS[state] : "?");
        log_info("Trade between players:", text, 0);
    }
    *known = (uint8_t) state;
}

// Every month, a caravan for each resource the buyer buys, even while the caravans of the previous months are still
// on their way (the rule the trade page states; on the large maps a trip lasts several months, T5.7), within what he
// can pay
static void dispatch_to(int buyer, const seller_view *sv)
{
    int seller = player_context_current_player;
    int budget = -1; // what the buyer can still spend, once looked at
    for (int resource = RESOURCE_NONE + 1; resource < RESOURCE_MAX; resource++) {
        if (!buys[buyer][seller][resource]) {
            continue;
        }
        // the settings of the empire do not apply: a buyer says what he buys from whom, a seller sells beyond his
        // export threshold what he does not stockpile
        int available = city_resource_is_stockpiled(resource) ? 0 :
            city_resource_count(resource) - city_resource_export_over(resource);
        if (available <= 0) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_NO_STOCK);
            continue;
        }
        buyer_view v;
        look_at_buyer(buyer, resource, sv, &v);
        if (!v.accepts) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_NO_WAREHOUSE);
            continue;
        }
        if (!v.warehouse_id) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_NO_ROAD);
            continue;
        }
        // room in his warehouses, but for what caravans already bring him: none leaves to come back full (T5.7)
        int room_left = v.room;
        for (int other = 0; other < player_context_num_players(); other++) {
            room_left -= mp_trade_loads_on_the_way(other, buyer, resource);
        }
        if (room_left <= 0) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_NO_ROOM);
            continue;
        }
        if (budget < 0) {
            budget = v.treasury - value_on_the_way(buyer);
            budget = budget < 0 ? 0 : budget;
        }
        int price = mp_trade_price(seller, buyer, resource);
        int affordable = price > 0 ? budget / price : 0;
        if (affordable <= 0) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_NO_MONEY);
            continue;
        }
        int loads = available < MAX_CARAVAN_LOADS ? available : MAX_CARAVAN_LOADS;
        loads = loads < affordable ? loads : affordable;
        loads = loads < room_left ? loads : room_left;
        // no more than his stock limit leaves room for (T4.5)
        int room = room_under_limit(buyer, resource);
        if (room == 0) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_LIMIT);
            continue;
        }
        if (room > 0 && loads > room) {
            loads = room;
        }
        loads = building_warehouses_remove_resource(resource, loads);
        if (loads <= 0) {
            set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_NO_STOCK);
            continue;
        }
        const map_point *road = &sv->road[v.network_index];
        figure *f = figure_create(FIGURE_TRADE_CARAVAN, road->x, road->y, DIR_0_TOP);
        if (!f->id) {
            building_warehouses_add_resource(resource, loads); // no room for one more figure: the goods stay
            continue;
        }
        f->action_state = ACTION_GOING;
        f->building_id = sv->warehouse_id[v.network_index];
        f->destination_building_id = v.warehouse_id;
        f->destination_x = v.road_x;
        f->destination_y = v.road_y;
        f->destination_grid_offset = map_grid_offset(v.road_x, v.road_y);
        f->resource_id = resource;
        f->loads_sold_or_carrying = loads;
        f->terrain_usage = TERRAIN_USAGE_ROADS;
        budget -= loads * price;
        set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_SENT);
    }
}

void mp_trade_dispatch_caravans(void)
{
    if (player_context_num_players() <= 1 || game_time_day() != 0) {
        return; // once a month
    }
    int seller = player_context_current_player;
    seller_view sv;
    int looked = 0;
    for (int buyer = 0; buyer < player_context_num_players(); buyer++) {
        if (!mp_trade_route_is_open(seller, buyer)) {
            continue;
        }
        // no caravan between players at war (T5.5), nor during the notice of an honourable war: the trade page says why
        if (mp_war_status(seller, buyer) != MP_WAR_PEACE) {
            for (int resource = RESOURCE_NONE + 1; resource < RESOURCE_MAX; resource++) {
                if (buys[buyer][seller][resource]) {
                    set_purchase_state(seller, buyer, resource, MP_TRADE_PURCHASE_AT_WAR);
                }
            }
            continue;
        }
        if (!looked) {
            look_at_seller(&sv);
            looked = 1;
        }
        dispatch_to(buyer, &sv);
    }
}

int mp_trade_purchase_state(int seller, int buyer, int resource)
{
    if (!is_player(seller) || !is_player(buyer) || !is_resource(resource)) {
        return MP_TRADE_PURCHASE_UNKNOWN;
    }
    return purchase_states[seller][buyer][resource];
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
    // the stock of the buyer may have reached his limit on the way: he takes only what it leaves room for (T4.5)
    int wanted = loads;
    int limit = mp_trade_buy_limit(buyer, resource);
    player_context_switch(buyer);
    if (limit > 0) {
        int room = limit - city_resource_count(resource);
        wanted = room < 0 ? 0 : room < loads ? room : loads;
    }
    for (int i = BUILDING_FIRST; i < BUILDING_END && stored < wanted; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_WAREHOUSE) {
            while (stored < wanted && building_warehouse_add_resource(b, resource)) {
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
    // in the log of every computer: the caravans that arrive, for the tests in the real game and the next trials
    char text[100];
    snprintf(text, sizeof(text), "player %d to player %d, resource %d: %d of %d loads delivered for %d Dn",
        seller + 1, buyer + 1, resource, stored, loads, stored * price);
    log_info("Trade between players:", text, 0);
    if (buyer == mp_session_local_player_id()) {
        tell_delivery(seller, resource, stored, stored * price);
    }
}

// ---------- the cheaper source, for the interface ----------

int mp_trade_cheaper_player(int resource)
{
    int buyer = player_context_current_player;
    if (player_context_num_players() <= 1 || !is_resource(resource)) {
        return -1;
    }
    int cheapest = -1;
    int best = trade_price_buy(resource); // the empire, portorium included
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

int mp_trade_cheapest_seller(int resource)
{
    int buyer = player_context_current_player;
    if (!is_resource(resource)) {
        return -1;
    }
    int cheapest = -1;
    for (int seller = 0; seller < player_context_num_players(); seller++) {
        if (seller == buyer || !buys[buyer][seller][resource] || !mp_trade_route_is_open(buyer, seller)) {
            continue;
        }
        if (cheapest < 0 || mp_trade_price(seller, buyer, resource) < mp_trade_price(cheapest, buyer, resource)) {
            cheapest = seller;
        }
    }
    return cheapest;
}

int mp_trade_loads_on_the_way(int seller, int buyer, int resource)
{
    if (!is_player(seller) || !is_player(buyer)) {
        return 0;
    }
    int loads = 0;
    for (int i = seller * MAX_FIGURES + 1; i < (seller + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f) && f->action_state == ACTION_GOING &&
            BUILDING_OWNER(f->destination_building_id) == buyer && f->resource_id == resource) {
            loads += f->loads_sold_or_carrying;
        }
    }
    return loads;
}

static int is_home_warehouse(building *b, map_point *road)
{
    return b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_WAREHOUSE && b->has_road_access &&
        map_has_road_access(b->x, b->y, b->size, road);
}

// A war between the seller and the buyer, notice included, stops their trade: a caravan on its way to the buyer
// turns back to the warehouse it left (or another one of the seller), is not paid, and brings its goods home, where
// they find room as any delivery. Soldiers of the enemy may still take it on its way back (T5.5, D-077).
// @return 0 when it has nowhere to go: the goods are lost
static int turn_back_at_war(figure *f)
{
    int seller = player_context_current_player;
    int buyer = BUILDING_OWNER(f->destination_building_id);
    if (buyer == seller || mp_war_status(seller, buyer) == MP_WAR_PEACE) {
        return 1;
    }
    map_point road;
    building *home = 0;
    if (BUILDING_OWNER(f->building_id) == seller && is_home_warehouse(building_get(f->building_id), &road)) {
        home = building_get(f->building_id);
    }
    for (int i = BUILDING_FIRST; i < BUILDING_END && !home; i++) {
        if (is_home_warehouse(building_get(i), &road)) {
            home = building_get(i);
        }
    }
    log_info("Trade between players: war, a caravan goes back home, resource", 0, f->resource_id);
    if (!home) {
        return 0;
    }
    f->destination_building_id = home->id;
    f->destination_x = road.x;
    f->destination_y = road.y;
    f->destination_grid_offset = map_grid_offset(road.x, road.y);
    figure_route_remove(f);
    return 1;
}

void mp_trade_caravan_action(figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    if (mp_war_intercept_caravan(f)) {
        f->state = FIGURE_STATE_DEAD; // taken by a soldier of an enemy of the seller (T5.5)
        return;
    }
    if (!turn_back_at_war(f)) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    figure_movement_move_ticks(f, 1);
    if (f->direction == DIR_FIGURE_AT_DESTINATION) {
        if (is_going_home(f)) {
            building_warehouses_add_resource(f->resource_id, f->loads_sold_or_carrying);
            f->loads_sold_or_carrying = 0;
        } else {
            deliver(f);
        }
        f->state = FIGURE_STATE_DEAD;
    } else if (f->direction == DIR_FIGURE_REROUTE) {
        figure_route_remove(f);
    } else if (f->direction == DIR_FIGURE_LOST) {
        // the road was cut: the goods go back to the warehouses of the seller
        log_info("Trade between players: a caravan found no road to the buyer, resource", 0, f->resource_id);
        building_warehouses_add_resource(f->resource_id, f->loads_sold_or_carrying);
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_update(f, image_group(GROUP_FIGURE_TRADE_CARAVAN));
}
