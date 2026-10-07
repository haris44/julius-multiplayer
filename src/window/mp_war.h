#ifndef WINDOW_MP_WAR_H
#define WINDOW_MP_WAR_H

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

#endif // WINDOW_MP_WAR_H
