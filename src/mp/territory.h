#ifndef MP_TERRITORY_H
#define MP_TERRITORY_H

#include "building/type.h"
#include "core/buffer.h"

/**
 * @file
 * Territories (doc/mp/DECISIONS.md D-036): on the prepared maps, a player builds only in his zone. The zone
 * follows the living city: every tile within MP_TERRITORY_RADIUS of one of his "settled" buildings (an inhabited
 * house, a building with employees) or of a mission. A tile belongs to the first player who got it, so no tower
 * can be built next to the city of another player. Roads, aqueducts and walls are built anywhere.
 */

#define MP_TERRITORY_RADIUS 20

/**
 * Whether territories apply: a multiplayer game with the territories rule
 */
int mp_territory_is_active(void);

/**
 * @return Player owning the tile, or -1
 */
int mp_territory_owner(int grid_offset);

/**
 * Whether the current player may put a building of this type on this area (always when territories do not apply)
 */
int mp_territory_allows_building(building_type type, int x, int y, int size);

/**
 * Whether the current player may put a building of this type on this tile
 */
int mp_territory_allows_tile(building_type type, int grid_offset);

/**
 * Daily update of the zone of the current city: claims the free tiles near its settled buildings and missions,
 * releases those it no longer covers; its buildings left outside collapse after three months
 */
void mp_territory_update_city(void);

/**
 * Whether the current city owns at least one tile
 */
int mp_territory_owns_land(void);

void mp_territory_clear(void);

/** Days each building of the current city has spent outside its zone (state of each city) */
void mp_territory_reset_extra_state(void);
void mp_territory_save_extra_state(buffer *buf);
void mp_territory_load_extra_state(buffer *buf);

void mp_territory_save_state(buffer *buf);

void mp_territory_load_state(buffer *buf);

#endif // MP_TERRITORY_H
