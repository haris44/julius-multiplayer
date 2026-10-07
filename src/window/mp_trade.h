#ifndef WINDOW_MP_TRADE_H
#define WINDOW_MP_TRADE_H

#include "input/mouse.h"

/**
 * @file
 * The trade advisor in a game of several players (D-043, D-051, T5.6): tabs under the title, with few columns each.
 * The empire: what the city does with it, the price of Rome, the portorium and the price that applies. One tab per
 * other player: the price asked to him, his price, the empire to compare with, whether the city buys from him and what
 * his caravans bring, with the route in the corner of the title. The stocks: when the city sells and when it stops
 * buying. A help line under the table, and no text over another one, whatever the language, at four players and with
 * four-digit prices (tools/mp-trade4-test.sh).
 */

/**
 * Whether the trade advisor shows this page instead of the one of the original game
 */
int window_mp_trade_is_active(void);

int window_mp_trade_draw_background(void);
void window_mp_trade_draw_foreground(void);
int window_mp_trade_handle_mouse(const mouse *m);
int window_mp_trade_get_tooltip_text(void);

/**
 * Opens the trade advisor on the tab of that player
 */
void window_mp_trade_show_partner(int player_id);

#endif // WINDOW_MP_TRADE_H
