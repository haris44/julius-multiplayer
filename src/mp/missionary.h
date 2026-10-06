#ifndef MP_MISSIONARY_H
#define MP_MISSIONARY_H

#include "figure/figure.h"

/**
 * @file
 * Missions and missionaries of the multiplayer games with territories (doc/mp/DECISIONS.md D-037). The mission
 * no longer serves natives: it takes possession of a zone and trains missionaries. Every player starts with a
 * missionary, moved like a legion; a mission is built within MP_MISSIONARY_RANGE tiles of one of his
 * missionaries. A mission is free while the player owns no land, then it costs marble.
 */

#define MP_MISSIONARY_RANGE 20
#define MP_MISSION_MARBLE_LOADS 30
#define MP_MISSIONARY_TRAINING_COST 300

/**
 * Whether the missionary is one of the multiplayer missionaries (and not a walker of a classic mission)
 */
int mp_missionary_is_scout(const figure *f);

/**
 * The first living missionary of the current city, 0 without any
 */
int mp_missionary_first(void);

/**
 * Whether the current city has a mission, built or being built: until then, its player has to found one
 */
int mp_mission_exists(void);

/**
 * Creates a missionary of the current city, waiting at a tile
 * @param mission_id Mission that trained him, 0 for the one of the start
 */
figure *mp_missionary_create(int mission_id, int x, int y);

/**
 * Action of a multiplayer missionary, every tick
 */
void mp_missionary_action(figure *f);

/**
 * Sends a missionary of the current city to a tile (command of his player)
 */
void mp_missionary_move(int figure_id, int x, int y);

/**
 * Whether a missionary of the current city stands within range of the area
 */
int mp_missionary_is_near(int x, int y, int size);

/**
 * Whether the current city may build a mission on this area: no tile of another player, a missionary nearby
 */
int mp_mission_allows_place(int x, int y, int size);

/**
 * Marble loads the next mission of the current city costs: none while it owns no land
 */
int mp_mission_marble_cost(void);

/**
 * Takes the marble of a new mission from the warehouses of the current city
 * @return 0 when there is not enough of it (nothing taken)
 */
int mp_mission_pay(void);

/**
 * The missionary trained by a mission of the current city, 0 if none is alive
 */
int mp_mission_missionary(int mission_id);

/**
 * A mission of the current city trains a missionary, if it has none and the city has the money (command)
 */
void mp_mission_train_missionary(int mission_id);

#endif // MP_MISSIONARY_H
