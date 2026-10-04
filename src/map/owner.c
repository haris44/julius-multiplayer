#include "owner.h"

#include "map/grid.h"

// player id + 1, 0 when nobody claimed the tile
static grid_u8 owner;
static int builder = MAP_OWNER_NONE;

static int simulating;

void map_owner_set_simulating(int value)
{
    simulating = value;
}

int map_owner_is_simulating(void)
{
    return simulating;
}

void map_owner_set_builder(int player_id)
{
    builder = player_id;
}

int map_owner_builder(void)
{
    return builder;
}

int map_owner_get(int grid_offset)
{
    int value = owner.items[grid_offset];
    return value ? value - 1 : 0;
}

int map_owner_get_claimed(int grid_offset)
{
    return owner.items[grid_offset] - 1;
}

void map_owner_set(int grid_offset, int player_id)
{
    owner.items[grid_offset] = player_id < 0 ? 0 : player_id + 1;
}

void map_owner_clear_all(void)
{
    map_grid_clear_u8(owner.items);
}

void map_owner_save_state(buffer *buf)
{
    map_grid_save_state_u8(owner.items, buf);
}

void map_owner_load_state(buffer *buf)
{
    map_grid_load_state_u8(owner.items, buf);
}
