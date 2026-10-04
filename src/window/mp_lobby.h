#ifndef WINDOW_MP_LOBBY_H
#define WINDOW_MP_LOBBY_H

/**
 * Local network game lobby: host a game from a map or a saved game, or join one found on the network
 */
void window_mp_lobby_show(void);

/**
 * Shows the city of the local player once a network game starts (mp_lockstep started callback)
 */
void window_mp_lobby_show_started_game(void);

#endif // WINDOW_MP_LOBBY_H
