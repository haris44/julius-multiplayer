#ifndef MP_SAVEGAME_H
#define MP_SAVEGAME_H

#include "game/rules.h"

#include <stdint.h>

/**
 * @file
 * Multiplayer saved games (.mpsav): the whole simulation of every city, with wide fields
 * (doc/mp/DECISIONS.md D-011, D-024). Loading one continues the game exactly.
 */

/**
 * Visits every piece of the multiplayer state in a fixed order: the world, then each city
 * @param visitor Called with the name, data and size of every piece
 */
typedef void (*mp_savegame_visitor)(const char *name, const unsigned char *data, int size, void *userdata);
void mp_savegame_visit(mp_savegame_visitor visitor, void *userdata);

int mp_savegame_write(const char *filename);

int mp_savegame_read(const char *filename);

/**
 * Number of cities of a multiplayer saved game, 0 if the file is not one
 */
int mp_savegame_num_players(const char *filename);

/**
 * The rules of a multiplayer saved game, without loading it
 * @param filename Saved game (.mpsav or .mpmap)
 * @param rules Filled with the saved rules
 * @return 1 when the file has valid rules, 0 otherwise
 */
int mp_savegame_read_rules(const char *filename, game_rules_settings *rules);

/**
 * @return Whether the current game needs the multiplayer format (several cities or a large grid)
 */
int mp_savegame_is_needed(void);

#endif // MP_SAVEGAME_H
