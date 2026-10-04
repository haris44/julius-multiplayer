#include "player_context.h"

#include "building/barracks.h"
#include "building/building.h"
#include "building/count.h"
#include "building/granary.h"
#include "building/list.h"
#include "city/culture.h"
#include "city/data.h"
#include "city/labor.h"
#include "city/message.h"
#include "city/resource.h"
#include "city/victory.h"
#include "core/log.h"
#include "core/random.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/name.h"

#include <stdlib.h>
#include <string.h>

#define MAX_REGIONS 32

typedef struct {
    void *state;
    int size;
    int offset;
    const char *name;
} region;

int player_context_current_player = 0;
int player_context_player_count = 1;

static struct {
    region regions[MAX_REGIONS];
    int num_regions;
    int total_size;
    unsigned char *slots[PLAYER_CONTEXT_MAX_PLAYERS];
    int num_players;
    int current;
} data = { .num_players = 1 };

void player_context_register(void *state, int size, const char *name)
{
    if (data.num_regions >= MAX_REGIONS || data.slots[0]) {
        log_error("Too many or late player context registrations", name, 0);
        return;
    }
    region *r = &data.regions[data.num_regions++];
    r->state = state;
    r->size = size;
    r->offset = data.total_size;
    r->name = name;
    data.total_size += size;
}

void player_context_init(void)
{
    if (data.num_regions) {
        return;
    }
    city_data_register_player_state();
    city_culture_register_player_state();
    building_count_register_player_state();
    building_granary_register_player_state();
    building_barracks_register_player_state();
    formation_register_player_state();
    city_victory_register_player_state();
    city_resource_register_player_state();
    city_message_register_player_state();
    city_labor_register_player_state();
    random_register_player_state();
    building_list_register_player_state();
    building_register_player_state();
    figure_register_player_state();
    figure_name_register_player_state();
}

static int allocate_slots(void)
{
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        if (!data.slots[p]) {
            data.slots[p] = calloc(1, data.total_size);
            if (!data.slots[p]) {
                return 0;
            }
        }
    }
    return 1;
}

static void save_live(int player_id)
{
    for (int i = 0; i < data.num_regions; i++) {
        memcpy(data.slots[player_id] + data.regions[i].offset, data.regions[i].state, data.regions[i].size);
    }
}

static void load_live(int player_id)
{
    for (int i = 0; i < data.num_regions; i++) {
        memcpy(data.regions[i].state, data.slots[player_id] + data.regions[i].offset, data.regions[i].size);
    }
}

void player_context_set_num_players(int num_players)
{
    if (num_players < 1 || num_players > PLAYER_CONTEXT_MAX_PLAYERS || !allocate_slots()) {
        return;
    }
    if (data.current != 0) {
        player_context_switch(0);
    }
    // every city starts as a copy of the current one; callers then reinitialize the new cities
    save_live(0);
    for (int p = 1; p < num_players; p++) {
        memcpy(data.slots[p], data.slots[0], data.total_size);
    }
    data.num_players = num_players;
    data.current = 0;
    player_context_player_count = num_players;
    player_context_current_player = 0;
}

int player_context_num_players(void)
{
    return data.num_players;
}

int player_context_current(void)
{
    return data.current;
}

void player_context_switch(int player_id)
{
    if (player_id == data.current || player_id < 0 || player_id >= data.num_players || !data.slots[0]) {
        return;
    }
    save_live(data.current);
    load_live(player_id);
    data.current = player_id;
    player_context_current_player = player_id;
}

void player_context_flush(void)
{
    if (allocate_slots()) {
        save_live(data.current);
    }
}

int player_context_state_size(void)
{
    return data.total_size;
}

const unsigned char *player_context_slot(int player_id)
{
    return data.slots[player_id];
}

void player_context_set_slot(int player_id, const unsigned char *state)
{
    if (player_id == data.current || !allocate_slots()) {
        return;
    }
    memcpy(data.slots[player_id], state, data.total_size);
}
