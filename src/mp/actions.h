#ifndef MP_ACTIONS_H
#define MP_ACTIONS_H

#include "mp/command.h"

/**
 * @file
 * City management actions of the player, sent as MP_COMMAND_CITY_ACTION commands.
 *
 * Each function is called by the user interface where it used to change the city directly, and
 * the command performs exactly the calls the button used to make. A classic game therefore behaves
 * as before, and a network game applies the action on every computer at the same tick.
 */

typedef enum {
    MP_ACTION_CHANGE_TAXES = 1,           /**< delta */
    MP_ACTION_CHANGE_WAGES = 2,           /**< delta */
    MP_ACTION_SET_LABOR_PRIORITY = 3,     /**< category, priority */
    MP_ACTION_FESTIVAL_SELECT_GOD = 4,    /**< god */
    MP_ACTION_FESTIVAL_SELECT_SIZE = 5,   /**< size */
    MP_ACTION_FESTIVAL_HOLD = 6,
    MP_ACTION_STORAGE_CYCLE_RESOURCE = 7, /**< building id, resource */
    MP_ACTION_STORAGE_TOGGLE_EMPTY_ALL = 8, /**< building id */
    MP_ACTION_STORAGE_ACCEPT_NONE = 9,    /**< building id */
    MP_ACTION_SET_TRADE_CENTER = 10,      /**< building id */
    MP_ACTION_OPEN_TRADE_ROUTE = 11,      /**< empire city id */
    MP_ACTION_CYCLE_TRADE_STATUS = 12,    /**< resource */
    MP_ACTION_CHANGE_EXPORT_OVER = 13,    /**< resource, delta */
    MP_ACTION_TOGGLE_STOCKPILED = 14,     /**< resource */
    MP_ACTION_TOGGLE_MOTHBALLED = 15,     /**< resource */
    MP_ACTION_MAX
} mp_action_type;

void mp_action_change_taxes(int delta);
void mp_action_change_wages(int delta);
void mp_action_set_labor_priority(int category, int priority);
void mp_action_festival_select_god(int god);
void mp_action_festival_select_size(int size);
void mp_action_festival_hold(void);
void mp_action_storage_cycle_resource(int building_id, int resource);
void mp_action_storage_toggle_empty_all(int building_id);
void mp_action_storage_accept_none(int building_id);
void mp_action_set_trade_center(int building_id);
void mp_action_open_trade_route(int empire_city_id);
void mp_action_cycle_trade_status(int resource);
void mp_action_change_export_over(int resource, int delta);
void mp_action_toggle_stockpiled(int resource);
void mp_action_toggle_mothballed(int resource);

/**
 * Applies an MP_COMMAND_CITY_ACTION command to the simulation
 */
void mp_actions_execute(const mp_command *command);

#endif // MP_ACTIONS_H
