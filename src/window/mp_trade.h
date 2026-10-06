#ifndef WINDOW_MP_TRADE_H
#define WINDOW_MP_TRADE_H

#include "input/mouse.h"

/**
 * @file
 * The trade advisor in a game of several players (D-043, D-051): the empire and the other players on one page. For
 * each resource, its stock, what the city does with the empire and at what price, and for the selected player the
 * price asked to him, his price, whether the city buys from him and what his caravans bring.
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
