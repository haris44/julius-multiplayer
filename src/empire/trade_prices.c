#include "trade_prices.h"
#include "game/player_context.h"
#include "game/rules.h"

struct trade_price {
    int32_t buy;
    int32_t sell;
};

static const struct trade_price DEFAULT_PRICES[RESOURCE_MAX] = {
    {0, 0}, {28, 22}, {38, 30}, {38, 30}, // wheat, vegetables, fruit
    {42, 34}, {44, 36}, {44, 36}, {215, 160}, // olives, vines, meat, wine
    {180, 140}, {60, 40}, {50, 35}, {40, 30}, // oil, iron, timber, clay
    {200, 140}, {250, 180}, {200, 150}, {180, 140} // marble, weapons, furniture, pottery
};

static struct trade_price prices[RESOURCE_MAX];

void trade_prices_reset(void)
{
    for (int i = 0; i < RESOURCE_MAX; i++) {
        prices[i] = DEFAULT_PRICES[i];
    }
}

// multiplayer: the portorium, the duty on what enters and leaves the province, in percent of the price of Rome (D-060)
#define PORTORIUM_PERCENT 50

int trade_price_rome(resource_type resource)
{
    return prices[resource].buy;
}

int trade_price_portorium_percent(void)
{
    return game_rules_is_multiplayer() ? PORTORIUM_PERCENT : 0;
}

int trade_price_duty(resource_type resource)
{
    return prices[resource].buy * trade_price_portorium_percent() / 100;
}

int trade_price_buy(resource_type resource)
{
    // multiplayer: the price of Rome plus the portorium (D-060)
    return game_rules_is_multiplayer() ? trade_price_rome(resource) + trade_price_duty(resource) : prices[resource].buy;
}

int trade_price_sell(resource_type resource)
{
    // multiplayer: the price of Rome minus the portorium (D-060)
    return game_rules_is_multiplayer() ? trade_price_rome(resource) - trade_price_duty(resource) : prices[resource].sell;
}

int trade_price_change(resource_type resource, int amount)
{
    if (amount < 0 && prices[resource].sell <= 0) {
        // cannot lower the price to negative
        return 0;
    }
    if (amount < 0 && prices[resource].sell <= -amount) {
        prices[resource].buy = 2;
        prices[resource].sell = 0;
    } else {
        prices[resource].buy += amount;
        prices[resource].sell += amount;
    }
    return 1;
}

void trade_prices_save_state(buffer *buf)
{
    for (int i = 0; i < RESOURCE_MAX; i++) {
        buffer_write_i32(buf, prices[i].buy);
        buffer_write_i32(buf, prices[i].sell);
    }
}

void trade_prices_load_state(buffer *buf)
{
    for (int i = 0; i < RESOURCE_MAX; i++) {
        prices[i].buy = buffer_read_i32(buf);
        prices[i].sell = buffer_read_i32(buf);
    }
}

void trade_prices_register_player_state(void)
{
    player_context_register(&prices, sizeof(prices), "trade_prices");
}
