#include "trade.h"

#include "city/warning.h"
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
}
