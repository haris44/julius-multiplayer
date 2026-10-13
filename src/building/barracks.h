#ifndef BUILDING_BARRACKS_H
#define BUILDING_BARRACKS_H

#include "building/building.h"
#include "map/point.h"

// Most loads of timber the barracks of a multiplayer game keeps for its javelin throwers (D-058)
#define BARRACKS_TIMBER_MAX 5

/**
 * Barracks to deliver a load of weapons to, or of timber in a multiplayer game
 */
int building_get_barracks_for_weapon(int resource, int road_network_id, map_point *dst);

void building_barracks_add_weapon(building *barracks);

void building_barracks_add_timber(building *barracks);

/**
 * Loads of timber of the barracks of the current city (multiplayer only)
 */
int building_barracks_timber_stored(void);

/**
 * Whether the city has a fort of javelin throwers
 */
int building_barracks_has_javelin_legion(void);

int building_barracks_create_soldier(building *barracks, int x, int y);

int building_barracks_create_tower_sentry(building *barracks, int x, int y);

void building_barracks_request_tower_sentry(void);

void building_barracks_decay_tower_sentry_request(void);

int building_barracks_has_tower_sentry_request(void);

void building_barracks_save_state(buffer *buf);

void building_barracks_load_state(buffer *buf);

/**
 * Extra state (game/extra_state.h): the timber of the barracks
 */
void building_barracks_reset_extra_state(void);
void building_barracks_save_extra_state(buffer *buf);
void building_barracks_load_extra_state(buffer *buf);

/**
 * Registers the per-city state of this module (game/player_context.h)
 */
void building_barracks_register_player_state(void);

#endif // BUILDING_BARRACKS_H
