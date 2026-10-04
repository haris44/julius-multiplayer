#include "actions.h"

#include "building/building.h"
#include "building/count.h"
#include "building/menu.h"
#include "building/storage.h"
#include "city/buildings.h"
#include "city/festival.h"
#include "city/finance.h"
#include "city/labor.h"
#include "city/resource.h"
#include "empire/city.h"
#include "mp/session.h"

static void submit(int action, int arg1, int arg2)
{
    mp_command command = { .type = MP_COMMAND_CITY_ACTION, .args = { action, arg1, arg2 } };
    mp_command_submit(&command);
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

static int valid_resource(int resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_MAX;
}

static int storage_of(int building_id)
{
    if (building_id <= 0 || building_id >= MAX_BUILDINGS) {
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
    int storage_id;
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
        default:
            break;
    }
}
