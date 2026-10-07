#include "actions.h"

#include "building/building.h"
#include "building/count.h"
#include "building/menu.h"
#include "building/storage.h"
#include "city/buildings.h"
#include "city/emperor.h"
#include "city/festival.h"
#include "city/finance.h"
#include "city/labor.h"
#include "city/ratings.h"
#include "city/resource.h"
#include "empire/city.h"
#include "game/player_context.h"
#include "mp/caesar.h"
#include "mp/missionary.h"
#include "mp/session.h"
#include "mp/trade.h"
#include "mp/war.h"
#include "city/military.h"
#include "figure/formation.h"
#include "figure/formation_legion.h"
#include "map/grid.h"
#include "scenario/request.h"

static void submit3(int action, int arg1, int arg2, int arg3)
{
    mp_command command = { .type = MP_COMMAND_CITY_ACTION, .args = { action, arg1, arg2, arg3 } };
    mp_command_submit(&command);
}

static void submit(int action, int arg1, int arg2)
{
    submit3(action, arg1, arg2, 0);
}

void mp_action_change_taxes(int delta)
{
    submit(MP_ACTION_CHANGE_TAXES, delta, 0);
}

void mp_action_change_wages(int delta)
{
    submit(MP_ACTION_CHANGE_WAGES, delta, 0);
}

void mp_action_set_labor_priority(int category, int priority)
{
    submit(MP_ACTION_SET_LABOR_PRIORITY, category, priority);
}

void mp_action_festival_select_god(int god)
{
    submit(MP_ACTION_FESTIVAL_SELECT_GOD, god, 0);
}

void mp_action_festival_select_size(int size)
{
    submit(MP_ACTION_FESTIVAL_SELECT_SIZE, size, 0);
}

void mp_action_festival_hold(void)
{
    submit(MP_ACTION_FESTIVAL_HOLD, 0, 0);
}

void mp_action_storage_cycle_resource(int building_id, int resource)
{
    submit(MP_ACTION_STORAGE_CYCLE_RESOURCE, building_id, resource);
}

void mp_action_storage_toggle_empty_all(int building_id)
{
    submit(MP_ACTION_STORAGE_TOGGLE_EMPTY_ALL, building_id, 0);
}

void mp_action_storage_accept_none(int building_id)
{
    submit(MP_ACTION_STORAGE_ACCEPT_NONE, building_id, 0);
}

void mp_action_set_trade_center(int building_id)
{
    submit(MP_ACTION_SET_TRADE_CENTER, building_id, 0);
}

void mp_action_open_trade_route(int empire_city_id)
{
    submit(MP_ACTION_OPEN_TRADE_ROUTE, empire_city_id, 0);
}

void mp_action_cycle_trade_status(int resource)
{
    submit(MP_ACTION_CYCLE_TRADE_STATUS, resource, 0);
}

void mp_action_change_export_over(int resource, int delta)
{
    submit(MP_ACTION_CHANGE_EXPORT_OVER, resource, delta);
}

void mp_action_toggle_stockpiled(int resource)
{
    submit(MP_ACTION_TOGGLE_STOCKPILED, resource, 0);
}

void mp_action_toggle_mothballed(int resource)
{
    submit(MP_ACTION_TOGGLE_MOTHBALLED, resource, 0);
}

void mp_action_missionary_move(int figure_id, int x, int y)
{
    submit3(MP_ACTION_MISSIONARY_MOVE, figure_id, x, y);
}

void mp_action_train_missionary(int mission_id)
{
    submit(MP_ACTION_TRAIN_MISSIONARY, mission_id, 0);
}

void mp_action_set_sell_price(int buyer, int resource, int price)
{
    submit3(MP_ACTION_SET_SELL_PRICE, buyer, resource, price);
}

void mp_action_set_buys_from(int seller, int resource, int buys)
{
    submit3(MP_ACTION_SET_BUYS_FROM, seller, resource, buys);
}

void mp_action_propose_route(int other, int propose)
{
    submit(MP_ACTION_PROPOSE_ROUTE, other, propose);
}

void mp_action_legion_move(int formation_id, int x, int y)
{
    submit3(MP_ACTION_LEGION_MOVE, formation_id, x, y);
}

void mp_action_legion_return_home(int formation_id)
{
    submit(MP_ACTION_LEGION_RETURN_HOME, formation_id, 0);
}

void mp_action_legion_change_layout(int formation_id, int layout)
{
    submit(MP_ACTION_LEGION_CHANGE_LAYOUT, formation_id, layout);
}

void mp_action_legion_toggle_empire_service(int formation_id)
{
    submit(MP_ACTION_LEGION_TOGGLE_EMPIRE_SERVICE, formation_id, 0);
}

void mp_action_dispatch_distant_battle(void)
{
    submit(MP_ACTION_DISPATCH_DISTANT_BATTLE, 0, 0);
}

void mp_action_clear_empire_service_legions(void)
{
    submit(MP_ACTION_CLEAR_EMPIRE_SERVICE_LEGIONS, 0, 0);
}

void mp_action_send_request(int request_id)
{
    submit(MP_ACTION_SEND_REQUEST, request_id, 0);
}

void mp_action_send_gift(int size)
{
    submit(MP_ACTION_SEND_GIFT, size, 0);
}

void mp_action_set_salary(int rank)
{
    submit(MP_ACTION_SET_SALARY, rank, 0);
}

void mp_action_donate(int amount)
{
    submit(MP_ACTION_DONATE, amount, 0);
}

void mp_action_change_buy_limit(int resource, int delta)
{
    submit(MP_ACTION_CHANGE_BUY_LIMIT, resource, delta);
}

void mp_action_declare_war(int other, int form)
{
    submit(MP_ACTION_DECLARE_WAR, other, form);
}

void mp_action_propose_peace(int other, int propose)
{
    submit(MP_ACTION_PROPOSE_PEACE, other, propose);
}

// Legion that can receive orders; the user interface made the same checks before
static formation *legion_of(int formation_id)
{
    if (formation_id < FORMATION_FIRST || formation_id >= FORMATION_END) {
        return 0; // only legions of the player who sent the command
    }
    formation *m = formation_get(formation_id);
    if (!m->in_use || !m->is_legion || m->in_distant_battle) {
        return 0;
    }
    return m;
}

static int valid_resource(int resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_MAX;
}

static int storage_of(int building_id)
{
    // only the buildings of the player who sent the command (the current player)
    if (building_id < BUILDING_FIRST || building_id >= BUILDING_END) {
        return -1;
    }
    building *b = building_get(building_id);
    if (b->state != BUILDING_STATE_IN_USE || (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY)) {
        return -1;
    }
    return b->storage_id;
}

void mp_actions_execute(const mp_command *command)
{
    int arg1 = command->args[1];
    int arg2 = command->args[2];
    int arg3 = command->args[3];
    int storage_id;
    formation *m;
    // Each case makes the same calls as the user interface button it replaces
    switch (command->args[0]) {
        case MP_ACTION_CHANGE_TAXES:
            city_finance_change_tax_percentage(arg1);
            city_finance_estimate_taxes();
            city_finance_calculate_totals();
            break;
        case MP_ACTION_CHANGE_WAGES:
            city_labor_change_wages(arg1);
            city_finance_estimate_wages();
            city_finance_calculate_totals();
            break;
        case MP_ACTION_SET_LABOR_PRIORITY:
            if (arg1 >= 0 && arg1 < 9 && arg2 >= 0 && arg2 <= 9) {
                city_labor_set_priority(arg1, arg2);
            }
            break;
        case MP_ACTION_FESTIVAL_SELECT_GOD:
            if (arg1 >= 0 && arg1 < 5) {
                city_festival_select_god(arg1);
            }
            break;
        case MP_ACTION_FESTIVAL_SELECT_SIZE:
            if (!city_finance_out_of_money()) {
                city_festival_select_size(arg1);
            }
            break;
        case MP_ACTION_FESTIVAL_HOLD:
            if (!city_finance_out_of_money()) {
                city_festival_schedule();
            }
            break;
        case MP_ACTION_STORAGE_CYCLE_RESOURCE:
            storage_id = storage_of(arg1);
            if (storage_id >= 0 && valid_resource(arg2)) {
                building_storage_cycle_resource_state(storage_id, arg2);
            }
            break;
        case MP_ACTION_STORAGE_TOGGLE_EMPTY_ALL:
            storage_id = storage_of(arg1);
            if (storage_id >= 0) {
                building_storage_toggle_empty_all(storage_id);
            }
            break;
        case MP_ACTION_STORAGE_ACCEPT_NONE:
            storage_id = storage_of(arg1);
            if (storage_id >= 0) {
                building_storage_accept_none(storage_id);
            }
            break;
        case MP_ACTION_SET_TRADE_CENTER:
            if (storage_of(arg1) >= 0) {
                city_buildings_set_trade_center(arg1);
            }
            break;
        case MP_ACTION_OPEN_TRADE_ROUTE:
            if (arg1 > 0 && arg1 < 41 && !empire_city_get(arg1)->is_open) {
                empire_city_open_trade(arg1);
                building_menu_update();
            }
            break;
        case MP_ACTION_CYCLE_TRADE_STATUS:
            if (valid_resource(arg1)) {
                city_resource_cycle_trade_status(arg1);
            }
            break;
        case MP_ACTION_CHANGE_EXPORT_OVER:
            if (valid_resource(arg1)) {
                city_resource_change_export_over(arg1, arg2);
            }
            break;
        case MP_ACTION_TOGGLE_STOCKPILED:
            if (valid_resource(arg1)) {
                city_resource_toggle_stockpiled(arg1);
            }
            break;
        case MP_ACTION_TOGGLE_MOTHBALLED:
            if (valid_resource(arg1) && building_count_industry_total(arg1) > 0) {
                city_resource_toggle_mothballed(arg1);
            }
            break;
        case MP_ACTION_LEGION_MOVE:
            m = legion_of(arg1);
            if (m && !m->cursed_by_mars && arg2 >= 0 && arg3 >= 0 && arg2 < GRID_SIZE && arg3 < GRID_SIZE) {
                int other_formation_id = formation_legion_at_building(map_grid_offset(arg2, arg3));
                if (other_formation_id && other_formation_id == arg1) {
                    formation_legion_return_home(m);
                } else {
                    formation_legion_move_to(m, arg2, arg3);
                }
            }
            break;
        case MP_ACTION_LEGION_RETURN_HOME:
            m = legion_of(arg1);
            if (m) {
                formation_legion_return_home(m);
            }
            break;
        case MP_ACTION_LEGION_CHANGE_LAYOUT:
            m = legion_of(arg1);
            if (m) {
                formation_legion_change_layout(m, arg2);
            }
            break;
        case MP_ACTION_LEGION_TOGGLE_EMPIRE_SERVICE:
            if (arg1 >= FORMATION_FIRST && arg1 < FORMATION_END) {
                formation_toggle_empire_service(arg1);
                formation_calculate_figures();
            }
            break;
        case MP_ACTION_DISPATCH_DISTANT_BATTLE:
            formation_legions_dispatch_to_distant_battle();
            break;
        case MP_ACTION_CLEAR_EMPIRE_SERVICE_LEGIONS:
            city_military_clear_empire_service_legions();
            break;
        case MP_ACTION_MISSIONARY_MOVE:
            mp_missionary_move(arg1, arg2, arg3);
            break;
        case MP_ACTION_TRAIN_MISSIONARY:
            mp_mission_train_missionary(arg1);
            break;
        case MP_ACTION_SET_SELL_PRICE:
            mp_trade_set_price(arg1, arg2, arg3);
            break;
        case MP_ACTION_SET_BUYS_FROM:
            mp_trade_set_buys_from(arg1, arg2, arg3);
            break;
        case MP_ACTION_PROPOSE_ROUTE:
            mp_trade_propose_route(arg1, arg2);
            break;
        case MP_ACTION_CHANGE_BUY_LIMIT:
            mp_trade_change_buy_limit(arg1, arg2);
            break;
        case MP_ACTION_DECLARE_WAR:
            mp_war_declare(arg1, arg2);
            break;
        case MP_ACTION_PROPOSE_PEACE:
            mp_war_propose_peace(arg1, arg2);
            break;
        case MP_ACTION_SEND_REQUEST:
            scenario_request_dispatch(arg1);
            break;
        case MP_ACTION_SEND_GIFT:
            if (arg1 >= GIFT_MODEST && arg1 <= GIFT_LAVISH) {
                city_emperor_calculate_gift_costs();
                int savings = city_emperor_personal_savings();
                if (city_emperor_set_gift_size(arg1)) {
                    city_emperor_send_gift();
                    if (city_emperor_personal_savings() < savings) {
                        mp_caesar_gift_sent(player_context_current_player, arg1);
                    }
                }
            }
            break;
        case MP_ACTION_SET_SALARY:
            if (mp_caesar_is_active()) {
                // Rome pays everybody the salary of his rank: the choice is gone, the command brings the salary up to
                // date at once (D-076)
                city_emperor_set_paid_rank(mp_caesar_salary_rank_limit(player_context_current_player));
                city_finance_update_salary();
                city_ratings_update_favor_explanation();
            } else if (arg1 >= 0 && arg1 <= 10) {
                city_emperor_set_salary_rank(arg1);
                city_finance_update_salary();
                city_ratings_update_favor_explanation();
            }
            break;
        case MP_ACTION_DONATE:
            city_emperor_set_donation_amount(arg1);
            city_emperor_donate_savings_to_city();
            break;
        default:
            break;
    }
}
