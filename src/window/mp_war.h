#ifndef WINDOW_MP_WAR_H
#define WINDOW_MP_WAR_H

#include <stdint.h>

/**
 * War and peace with the other players (T5.5, D-077), opened from the military advisor in a multiplayer game: the
 * state of the war with each player, and the buttons to declare a brutal or an honourable war, to propose peace or
 * withdraw the proposal. Declaring a war asks for a second click.
 */
void window_mp_war_show(void);

/**
 * Whether the military advisor offers the window: a multiplayer game with other players
 */
int window_mp_war_is_available(void);

/**
 * The line of the military advisor about the wars of the local player: "At war with player 2" (with the enemy
 * soldiers in his land, M10.8), or the notice of an honourable war (M10.4)
 * @return 1 when he is at war or in a notice
 */
int window_mp_war_advisor_text(uint8_t *text, int size);

/**
 * The line of the military advisor about the losses of the local player in his wars with the other players, and those
 * of his enemies (M10.8): "Losses: 8 soldiers, 1 legion; the enemy: 2 soldiers"
 * @return 1 when he lost or killed soldiers of a player
 */
int window_mp_war_advisor_losses_text(uint8_t *text, int size);

#endif // WINDOW_MP_WAR_H
