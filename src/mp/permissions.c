#include "permissions.h"

#include "empire/city.h"
#include "game/player_context.h"
#include "game/resource.h"

static const int SHARED_RESOURCES[] = {
    RESOURCE_CLAY, RESOURCE_TIMBER, RESOURCE_OLIVES, RESOURCE_VINES, RESOURCE_MARBLE
};
#define NUM_SHARED_RESOURCES (int) (sizeof(SHARED_RESOURCES) / sizeof(SHARED_RESOURCES[0]))

#define IRON_PLAYER 0

void mp_permissions_share_out(void)
{
    int num_players = player_context_num_players();
    if (num_players < 2) {
        return;
    }
    int previous = player_context_current();
    // the cities are copies: the materials of the first one are those of all of them
    player_context_switch(0);
    int allowed[NUM_SHARED_RESOURCES];
    for (int i = 0; i < NUM_SHARED_RESOURCES; i++) {
        allowed[i] = empire_city_our_production_allowed(SHARED_RESOURCES[i]);
    }
    int has_iron = empire_city_our_production_allowed(RESOURCE_IRON);
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        empire_city_set_our_production_allowed(RESOURCE_IRON, has_iron && p == IRON_PLAYER);
        // in turn, starting after the iron player so that every player gets something
        int turn = 1;
        for (int i = 0; i < NUM_SHARED_RESOURCES; i++) {
            if (!allowed[i]) {
                continue;
            }
            int owner = turn % num_players;
            empire_city_set_our_production_allowed(SHARED_RESOURCES[i], owner == p);
            turn++;
        }
    }
    player_context_switch(previous);
}
