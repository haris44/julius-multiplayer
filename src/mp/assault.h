#ifndef MP_ASSAULT_H
#define MP_ASSAULT_H

#include "core/buffer.h"

/**
 * @file
 * The order "attack" of a legion at war (M10.2, doc/mp/DECISIONS.md D-087), after the first slice of the war
 * (mp/war.c, D-077) and the sieges (mp/siege.c, D-081).
 *
 * The player picks a target in the city of a player he fights: a building, or a tile of that city. The legion marches
 * next to the building picked (or the nearest one to the tile), and its soldiers standing with it pull down what of
 * the enemy stands around them (mp_war_soldier_attack_buildings: buildings, walls and gatehouses, D-077, D-081). Once
 * nothing of the enemy is left next to them, the legion moves on to the next building of that player: the nearest to
 * its standard among those within MP_ASSAULT_RADIUS tiles of the target picked, as the invaders of the original game
 * go from one building to the next. On its way it fights the soldiers of the enemy as any legion at war.
 *
 * The order ends when no building of that player is left within the radius (the legion stays where it stands), at
 * peace (the legion then goes home as any other, D-077), when the player gives the legion another order, or when the
 * legion cannot go on (back at its fort, fleeing, sent to a distant battle, without soldiers).
 *
 * Every decision belongs to the simulation: the orders are saved in the piece "mp_assault" of the .mpsav, hence in
 * the checksum. Nothing here exists in a classic game, nor while nobody fights.
 */

/** Tiles (the larger of the two distances) around the target picked within which the legion pulls buildings down */
#define MP_ASSAULT_RADIUS 10

/**
 * The player the legion of `player_id` would attack at (x, y), for the user interface (writes nothing): the owner of
 * the building on the tile, else of the land, when `player_id` fights him and he has a building within the radius;
 * -1 otherwise
 */
int mp_assault_enemy_at(int player_id, int x, int y);

/**
 * The legion of the current player attacks (command MP_ACTION_LEGION_ATTACK). Refused (returns 0, nothing changes)
 * unless the legion belongs to the current player, has soldiers and may move, the target is in the city of a player
 * he fights (fighting, not during a notice), and the legion can stand next to a building of that player within the
 * radius
 * @return 1 when the order is given
 */
int mp_assault_order(int formation_id, int x, int y);

/**
 * Another order of the player for the legion (move, back to the fort) ends its attack
 */
void mp_assault_cancel(int formation_id);

/** Whether the legion has an order to attack */
int mp_assault_is_active(int formation_id);

/** The player the legion attacks, -1 when none */
int mp_assault_enemy(int formation_id);

/** The building the legion goes to, or whose fall it waits for; 0 when none */
int mp_assault_building(int formation_id);

/**
 * Whether the order "attack" is offered for the legion of the local player in the legion window: a multiplayer game,
 * his player fights another, and the legion has soldiers and may move
 */
int mp_assault_is_offered(int formation_id);

/**
 * Every tick, once for the world after the cities: each legion with an order chooses its next target when the soldiers
 * have nothing left to pull down around them; the orders that cannot go on end
 */
void mp_assault_update(void);

void mp_assault_reset(void);
void mp_assault_save_state(buffer *buf);
void mp_assault_load_state(buffer *buf);

#endif // MP_ASSAULT_H
