#ifndef WINDOW_MP_TRADE_H
#define WINDOW_MP_TRADE_H

/**
 * Trade between players (doc/mp/DECISIONS.md D-043): for each other player, the trade route, the price asked for
 * each resource, his price, and what is bought from him
 */
void window_mp_trade_show(void);

/**
 * The same window, on the tab of that player
 */
void window_mp_trade_show_partner(int player_id);

#endif // WINDOW_MP_TRADE_H
