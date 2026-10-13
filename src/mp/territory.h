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
 * The land of Caesar (doc/mp/DECISIONS.md D-082): the tiles within this distance (chebyshev) of each end of his
 * bridge belong to nobody: no zone covers them, so nothing that needs a zone is built there, and walls are refused
 * too. Players' roads stay allowed.
 */
#define MP_TERRITORY_CAESAR_LAND_RADIUS 15

/**
 * Whether territories apply: a multiplayer game with the territories rule
 */
int mp_territory_is_active(void);

/**
 * @return Player owning the tile, or -1
 */
int mp_territory_owner(int grid_offset);

/**
 * Marks the land of Caesar around a tile (an end of his bridge) when the prepared map is created; it is kept in the
 * grid of the zones, so a saved game keeps it
 */
void mp_territory_mark_caesar_land(int x, int y);

/**
 * Whether this tile is on the land of Caesar (always 0 outside the multiplayer maps). Later milestones (wrath of
 * Caesar, legions sent home at peace) ask it too.
 */
int mp_territory_is_caesar_land(int grid_offset);

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
 * The zone of the current city when the map is created, whatever the rules: they are set later
 */
void mp_territory_start_city(void);

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
