#ifndef WINDOW_MP_LOBBY_H
#define WINDOW_MP_LOBBY_H

/**
 * Local network game lobby: host a game from a map or a saved game, or join one found on the network
 */
void window_mp_lobby_show(void);

/**
 * The lobby, with a multiplayer saved game chosen to be hosted (the load dialog, T5.4)
 */
void window_mp_lobby_show_saved_game(const char *filename);

/**
 * Tests: whether the lobby lists this file among the games to host
 */
int window_mp_lobby_lists_file(const char *filename);

/**
 * Tests: the file chosen to be hosted ("" for a new game) and the number of players chosen
 */
const char *window_mp_lobby_selected_file(void);
int window_mp_lobby_num_players(void);

/**
 * Shows the city of the local player once a network game starts (mp_lockstep started callback)
 */
void window_mp_lobby_show_started_game(void);

#endif // WINDOW_MP_LOBBY_H
