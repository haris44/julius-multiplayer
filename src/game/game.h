#ifndef GAME_GAME_H
#define GAME_GAME_H

int game_pre_init(void);

int game_init(void);

int game_init_editor(void);

int game_reload_language(void);

void game_run(void);

void game_draw(void);

void game_exit_editor(void);

void game_exit(void);

/**
 * Exits the game without persisting settings, used by automated runs
 */
void game_exit_without_saving_settings(void);

#endif // GAME_GAME_H
