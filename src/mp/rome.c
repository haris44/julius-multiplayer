#include "rome.h"

#include "city/labor.h"
#include "city/message.h"
#include "game/player_context.h"
#include "game/rules.h"

int mp_rome_draws_wage_events(void)
{
    return !game_rules_is_multiplayer() || player_context_current() == 0;
}

void mp_rome_share_wages(int message_type)
{
    if (!game_rules_is_multiplayer() || player_context_num_players() < 2) {
        return;
    }
    int previous = player_context_current();
    int wages = city_labor_wages_rome();
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p == previous) {
            continue;
        }
        player_context_switch(p);
        if (city_labor_wages_rome() != wages) {
            city_labor_set_wages_rome(wages);
            city_message_post(1, message_type, 0, 0);
        }
    }
    player_context_switch(previous);
}
