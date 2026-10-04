#ifndef GAME_SPEED_H
#define GAME_SPEED_H

int game_speed_get_elapsed_ticks(void);

/**
 * Ticks to run in a network game: only the game speed and the pause count, not the open windows
 */
int game_speed_get_elapsed_ticks_multiplayer(void);

#endif // GAME_SPEED_H
