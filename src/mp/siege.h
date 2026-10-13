#ifndef MP_SIEGE_H
#define MP_SIEGE_H

#include "figure/figure.h"

/**
 * @file
 * Sieges between players at war (M10.2, doc/mp/DECISIONS.md D-081), after the first slice of the war (mp/war.c,
 * D-077):
 * - the gatehouses of a player are closed to the soldiers of the players fighting him: they go around them when they
 *   can, else they break them on their way, as the invaders of the original game do; everybody else passes;
 * - a soldier standing with his legion breaks the aqueducts of the enemy next to him, and those of Caesar that water
 *   the enemy (D-034, D-055), never one over a road.
 *
 * Nothing here exists in a classic game, nor in a multiplayer game while nobody fights (mp_war_any_fighting): every
 * function is then a no-op.
 */

/**
 * Whether the figure is a soldier of a player fighting another player: the gatehouses of his enemies are closed to
 * him
 */
int mp_siege_is_soldier_at_war(const figure *f);

/**
 * Route of a soldier at war over land (figure_route_add): around the gatehouses of his enemies when he can, else
 * through them, to break them on his way
 * @return Whether he can travel to his destination; the routing distances then lead him there
 */
int mp_siege_soldier_can_travel(const figure *f);

/**
 * The next tile of a soldier: when it is a gatehouse of a player fighting his own, the soldier attacks it with the
 * damage of the invaders of the original game (it collapses past 150); once it collapsed, he walks on
 * @return 1 when the gatehouse stops him
 */
int mp_siege_soldier_breaks_gate(figure *f, int grid_offset);

/**
 * A soldier standing with his legion breaks an aqueduct next to him: one of a player fighting his own, or one of
 * Caesar that waters such a player (it collapses past 10, as any building of the original game). Never an aqueduct
 * over a road; the reservoirs, roads and bridge of Caesar stay indestructible.
 * @return 1 when he attacks one
 */
int mp_siege_soldier_attack_aqueduct(figure *f);

#endif // MP_SIEGE_H
