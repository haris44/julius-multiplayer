#ifndef EMPIRE_TRADE_PRICES_H
#define EMPIRE_TRADE_PRICES_H

#include "core/buffer.h"
#include "game/resource.h"

/**
 * @file
 * Trade prices.
 */

/**
 * Reset trade prices to the default
 */
void trade_prices_reset(void);

/**
 * Get the buy price for the resource
 * @param resource Resource
 */
int trade_price_buy(resource_type resource);

/**
 * Price of Rome (multiplayer, doc/mp/DECISIONS.md D-060): the one price of the empire for the resource, the original
 * buying price with the price changes of the game. Also the price a player asks another one by default (D-043).
 */
int trade_price_rome(resource_type resource);

/**
 * Rate of the portorium, in percent of the price of Rome (multiplayer, D-060): 50. Always 0 in a classic game.
 */
int trade_price_portorium_percent(void);

/**
 * Portorium on a load of the resource traded with the empire: added to the price of Rome when buying, taken off when
 * selling (multiplayer, D-060). Always 0 in a classic game.
 */
int trade_price_duty(resource_type resource);

/**
 * Get the sell price for the resource
 * @param resource Resource
 */
int trade_price_sell(resource_type resource);

/**
 * Change the trade price for resource by amount
 * @param resource Resource to change
 * @param amount Amount to change, can be positive or negative
 * @return True if the price has been changed
 */
int trade_price_change(resource_type resource, int amount);

/**
 * Save trade prices to buffer
 * @param buf Buffer
 */
void trade_prices_save_state(buffer *buf);

/**
 * Load trade prices from buffer
 * @param buf Buffer
 */
void trade_prices_load_state(buffer *buf);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void trade_prices_register_player_state(void);

#endif // EMPIRE_TRADE_PRICES_H
