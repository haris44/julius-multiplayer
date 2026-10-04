#ifndef MP_COLORS_H
#define MP_COLORS_H

#include "graphics/color.h"

/**
 * @file
 * Colors of the players (doc/mp/ROADMAP.md M7.2): 1 blue, 2 red, 3 green, 4 yellow. The interface tints
 * what belongs to the other players; what belongs to the local player looks as in the original game.
 * Interface only: the simulation never reads it.
 */

/**
 * Color of a player, for texts and the minimap
 */
color_t mp_colors_player(int player_id);

/**
 * Tint for drawing what belongs to a player: 0 (no tint) for the local player and in a classic game
 */
color_t mp_colors_tint_for_player(int player_id);

/**
 * Tint for a building, its tile or a figure on the map (0 when none)
 */
color_t mp_colors_tint_for_building(int building_id);
color_t mp_colors_tint_for_tile(int grid_offset);
color_t mp_colors_tint_for_figure(int figure_id);

#endif // MP_COLORS_H
