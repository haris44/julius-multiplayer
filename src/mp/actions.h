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
    MP_ACTION_LEGION_MOVE = 16,           /**< formation id, x, y (back to the fort when clicking it) */
    MP_ACTION_LEGION_RETURN_HOME = 17,    /**< formation id */
    MP_ACTION_LEGION_CHANGE_LAYOUT = 18,  /**< formation id, layout */
    MP_ACTION_LEGION_TOGGLE_EMPIRE_SERVICE = 19, /**< formation id */
    MP_ACTION_DISPATCH_DISTANT_BATTLE = 20,
    MP_ACTION_CLEAR_EMPIRE_SERVICE_LEGIONS = 21,
    MP_ACTION_SEND_REQUEST = 22,          /**< request id */
    MP_ACTION_MISSIONARY_MOVE = 23,       /**< figure id, x, y */
    MP_ACTION_TRAIN_MISSIONARY = 24,      /**< mission building id */
    MP_ACTION_SET_SELL_PRICE = 25,        /**< buyer, resource, price */
    MP_ACTION_SET_BUYS_FROM = 26,         /**< seller, resource, 1 to buy or 0 */
    MP_ACTION_PROPOSE_ROUTE = 27,         /**< other player, 1 to propose or 0 to withdraw */
    MP_ACTION_SEND_GIFT = 28,             /**< size (GIFT_MODEST to GIFT_LAVISH): to Caesar, from the savings */
    MP_ACTION_SET_SALARY = 29,            /**< salary rank (0 to 10, at most the rank in multiplayer) */
    MP_ACTION_DONATE = 30,                /**< amount of denarii, from the savings to the treasury */
    MP_ACTION_CHANGE_BUY_LIMIT = 31,      /**< resource, delta: stock at which the city stops buying it (T4.5) */
    MP_ACTION_DECLARE_WAR = 32,           /**< other player, form (mp_war_kind) (T5.5) */
    MP_ACTION_PROPOSE_PEACE = 33,         /**< other player, 1 to propose or 0 to withdraw (T5.5) */
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
void mp_action_missionary_move(int figure_id, int x, int y);
void mp_action_train_missionary(int mission_id);
void mp_action_set_sell_price(int buyer, int resource, int price);
void mp_action_set_buys_from(int seller, int resource, int buys);
void mp_action_propose_route(int other, int propose);
void mp_action_open_trade_route(int empire_city_id);
void mp_action_cycle_trade_status(int resource);
void mp_action_change_export_over(int resource, int delta);
void mp_action_toggle_stockpiled(int resource);
void mp_action_toggle_mothballed(int resource);
void mp_action_legion_move(int formation_id, int x, int y);
void mp_action_legion_return_home(int formation_id);
void mp_action_legion_change_layout(int formation_id, int layout);
void mp_action_legion_toggle_empire_service(int formation_id);
void mp_action_dispatch_distant_battle(void);
void mp_action_clear_empire_service_legions(void);
void mp_action_send_request(int request_id);
void mp_action_send_gift(int size);
void mp_action_set_salary(int rank);
void mp_action_donate(int amount);
void mp_action_change_buy_limit(int resource, int delta);
void mp_action_declare_war(int other, int form);
void mp_action_propose_peace(int other, int propose);

/**
 * Applies an MP_COMMAND_CITY_ACTION command to the simulation
 */
void mp_actions_execute(const mp_command *command);

#endif // MP_ACTIONS_H
