#ifndef MP_TRADE_H
#define MP_TRADE_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * @file
 * Trade between players (doc/mp/DECISIONS.md D-019, D-043): every seller sets a price for each resource and each
 * buying player; every buyer says which resources he buys from which player. When a seller changes the price of a
 * resource a player buys from him, that player is told.
 */

/**
 * Price a seller asks a buyer for a load of the resource: his own price, else the price of the empire before its
 * multiplayer surcharge
 */
int mp_trade_price(int seller, int buyer, int resource);

/**
 * The current city sets the price it asks a buyer (command of its player)
 */
void mp_trade_set_price(int buyer, int resource, int price);

/**
 * Whether a buyer buys the resource from a seller
 */
int mp_trade_buys_from(int buyer, int seller, int resource);

/**
 * The current city buys, or no longer buys, the resource from a seller (command of its player)
 */
void mp_trade_set_buys_from(int seller, int resource, int buys);

/**
 * Price changes told to the local player since the start (user interface, tests)
 */
int mp_trade_notifications(void);

/**
 * Trade routes: open when both players proposed them (D-019)
 */
int mp_trade_route_is_open(int a, int b);
int mp_trade_route_is_proposed(int from, int to);

/**
 * The current city proposes, or withdraws, a trade route to another player (command of its player)
 */
void mp_trade_propose_route(int other, int propose);

/**
 * Monthly: the current city sends a caravan to each player it trades with, carrying up to 8 loads of a resource
 * that player buys from it and can pay for, beyond its export threshold and unless it stockpiles it
 */
void mp_trade_dispatch_caravans(void);

/**
 * Whether the figure is a caravan between players
 */
int mp_trade_is_caravan(const figure *f);

/**
 * Action of a caravan between players: it follows the roads to the warehouse of the buyer, who pays on delivery
 */
void mp_trade_caravan_action(figure *f);

void mp_trade_reset_extra_state(void);
void mp_trade_save_extra_state(buffer *buf);
void mp_trade_load_extra_state(buffer *buf);

#endif // MP_TRADE_H
