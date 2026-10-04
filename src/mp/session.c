#include "session.h"

#include "building/construction.h"
#include "game/player_context.h"
#include "game/time.h"
#include "map/owner.h"
#include "mp/actions.h"

#include <stddef.h>
#include <stdint.h>

static struct {
    int networked;
    int local_player_id;
    int next_sequence;
    mp_command_sink sink;
    mp_command_executor observer;
} data;

void mp_session_init_offline(void)
{
    data.networked = 0;
    data.local_player_id = 0;
    data.next_sequence = 0;
    data.sink = NULL;
    mp_command_queue_clear();
}

void mp_session_init_network(int local_player_id, mp_command_sink sink)
{
    data.networked = 1;
    data.local_player_id = local_player_id;
    data.next_sequence = 0;
    data.sink = sink;
    mp_command_queue_clear();
}

int mp_session_local_player_id(void)
{
    return data.local_player_id;
}

int mp_session_is_networked(void)
{
    return data.networked;
}

void mp_session_set_execution_observer(mp_command_executor observer)
{
    data.observer = observer;
}

void mp_command_submit(mp_command *command)
{
    command->player_id = data.local_player_id;
    command->sequence = data.next_sequence++;
    if (data.networked && data.sink) {
        data.sink(command);
    } else {
        command->tick = game_time_absolute_tick();
        mp_command_execute(command);
    }
}

static void execute_build(const mp_command *command)
{
    building_construction_placement placement = {
        .type = command->args[0],
        .sub_type = command->args[1],
        .x_start = command->args[2],
        .y_start = command->args[3],
        .x_end = command->args[4],
        .y_end = command->args[5],
        .road_orientation = command->args[6],
        .fort_answer = (int8_t) (command->args[7] & 0xff),
        .bridge_answer = (int8_t) ((command->args[7] >> 8) & 0xff)
    };
    building_construction_execute(&placement);
}

void mp_command_execute(const mp_command *command)
{
    if (data.observer) {
        data.observer(command);
    }
    // a command acts on the city of the player who sent it (city 0 when cities are shared)
    int previous_player = player_context_current();
    int player_id = command->player_id < player_context_num_players() ? command->player_id : 0;
    player_context_switch(player_id);
    switch (command->type) {
        case MP_COMMAND_BUILD:
            map_owner_set_builder(player_id); // what is built belongs to the player (map/owner.h)
            execute_build(command);
            map_owner_set_builder(MAP_OWNER_NONE);
            break;
        case MP_COMMAND_CITY_ACTION:
            mp_actions_execute(command);
            break;
        default:
            break;
    }
    player_context_switch(previous_player);
}

void mp_command_run_scheduled(void)
{
    if (mp_command_queue_size()) {
        mp_command_queue_run_due(game_time_absolute_tick(), mp_command_execute);
    }
}
