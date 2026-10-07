#ifndef MP_TRADE_H
#define MP_TRADE_H

#include "core/buffer.h"
#include "figure/figure.h"

/**
 * @file
 * Trade between players (doc/mp/DECISIONS.md D-019, D-043, D-048, D-060): every seller sets a price for each
 * resource and each buying player; every buyer says which resources he buys from which player. When a seller changes
 * the price of a resource a player buys from him, that player sees a full-screen alert. No portorium between players;
 * the empire always sells, at the price of Rome plus the portorium.
 */

/**
 * Price a seller asks a buyer for a load of the resource: his own price, else the price of Rome (no portorium
 * between players, D-060)
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
 * Highest stock limit a city may set (T4.5)
 */
#define MP_TRADE_MAX_BUY_LIMIT 400

/**
 * Stock of the resource at which a city stops buying it, from the empire and from the other players (T4.5, D-070):
 * nothing is bought once its warehouses hold that many loads. 0: no limit of its own (the original rule of the
 * empire, and no limit between players). Always 0 in a classic game.
 */
int mp_trade_buy_limit(int player_id, int resource);

/**
 * The current city raises or lowers its stock limit of the resource, between 0 and MP_TRADE_MAX_BUY_LIMIT
 * (command of its player; nothing in a classic game)
 */
void mp_trade_change_buy_limit(int resource, int delta);

/**
 * The stock limit of the current city for imports from the empire (empire/empire.c), or 0 for the original rule
 */
int mp_trade_empire_buy_limit(int resource);

/**
 * The stock limits of every city: a piece of the multiplayer saved game, in the checksum. An older game has none:
 * mp_trade_reset_bounds gives it no limit.
 */
void mp_trade_reset_bounds(void);
void mp_trade_save_bounds(buffer *buf);
void mp_trade_load_bounds(buffer *buf);

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
 * even while those of the previous months are on their way (T5.7), each carrying up to 8 loads he can pay for and
 * store (what is on the way counted as spent and stored), beyond its export threshold and unless it stockpiles it,
 * and no more than what his stock limit leaves room for, to a warehouse of his on a road network one of its
 * warehouses is on; the buyer is told of every delivery
 */
void mp_trade_dispatch_caravans(void);

/**
 * What became of a purchase at the last monthly departure of the caravans (T5.7)
 */
typedef enum {
    MP_TRADE_PURCHASE_UNKNOWN = 0, /**< not looked at since the start of the game or since it was loaded */
    MP_TRADE_PURCHASE_SENT, /**< a caravan left */
    MP_TRADE_PURCHASE_NO_STOCK, /**< the seller has none above his export threshold, or stockpiles it */
    MP_TRADE_PURCHASE_NO_WAREHOUSE, /**< no warehouse of the buyer with a road takes the resource */
    MP_TRADE_PURCHASE_NO_ROAD, /**< no road joins a warehouse of the seller to one of those of the buyer */
    MP_TRADE_PURCHASE_NO_ROOM, /**< the warehouses of the buyer are full, counting what is already on its way */
    MP_TRADE_PURCHASE_NO_MONEY, /**< the buyer cannot pay a single load, counting what is already on its way */
    MP_TRADE_PURCHASE_LIMIT, /**< the buyer reached his stock limit (T4.5) */
    MP_TRADE_PURCHASE_AT_WAR /**< the two players are at war, or in the notice of a war (T5.5) */
} mp_trade_purchase;

/**
 * What became of the purchase of the resource by the buyer from the seller at the last departure of the caravans:
 * display only (trade page, log of the game), never read by the simulation
 * @return One of mp_trade_purchase
 */
int mp_trade_purchase_state(int seller, int buyer, int resource);

/**
 * The player who sells the resource to the current city cheaper than the empire does (portorium included), over an
 * open route, and from whom it buys it, whether or not he has some to sell (interface: shown in green). The empire
 * sells anyway (D-060): this does not stop its traders.
 * @return His id, or -1 when the empire is the cheapest source (always in a classic game)
 */
int mp_trade_cheaper_player(int resource);

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
