#ifndef MP_TRADE_H
#define MP_TRADE_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * @file
 * Trade between players (doc/mp/DECISIONS.md D-019, D-043, D-048): every seller sets a price for each resource and
 * each buying player; every buyer says which resources he buys from which player. When a seller changes the price of
 * a resource a player buys from him, that player sees a full-screen alert. The empire only sells a city what no
 * player sells it cheaper.
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
 * A price change shown to the local buyer in a full-screen alert (window/mp_price_alert): one line per seller and
 * resource, from the price he knew to the current one. Display state of this computer only, never saved: the
 * simulation does not read it.
 */
typedef struct {
    int seller;
    int resource;
    int old_price;
    int new_price;
} mp_price_alert;

int mp_trade_num_price_alerts(void);
const mp_price_alert *mp_trade_price_alert(int index);

/**
 * The local player closed the alert
 */
void mp_trade_clear_price_alerts(void);

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
 * Monthly: the current city sends caravans to each player it trades with, one per resource that player buys from it,
 * each carrying up to 8 loads he can pay for, beyond its export threshold and unless it stockpiles it; the buyer is
 * told of every delivery
 */
void mp_trade_dispatch_caravans(void);

/**
 * The player who sells the resource to the current city cheaper than the empire does, over an open route, and from
 * whom it buys it, whether or not he has some to sell: drying up his stock is part of the game (D-048)
 * @return His id, or -1 when the empire is the cheapest source (always in a classic game)
 */
int mp_trade_cheaper_player(int resource);

/**
 * Whether the traders of the empire may sell the resource to the current city: not when it buys it from a player
 * who sells it cheaper (D-048), even when he has none left. Always in a classic game.
 */
int mp_trade_empire_may_sell(int resource);

/**
 * The player the current city buys the resource from at the lowest price over an open route, whether or not the
 * empire is cheaper (interface)
 * @return His id, or -1
 */
int mp_trade_cheapest_seller(int resource);

/**
 * Loads of the resource that caravans of the seller are carrying to the buyer (interface)
 */
int mp_trade_loads_on_the_way(int seller, int buyer, int resource);

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
