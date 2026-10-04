#include "road_network.h"

#include "city/map.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "game/player_context.h"
#include "map/owner.h"

#include <string.h>

#define MAX_QUEUE 1000

// grid side only known at run time: up, right, down, left
#define ADJACENT_OFFSETS(i) ((i) == 0 ? -GRID_SIZE : (i) == 1 ? 1 : (i) == 2 ? GRID_SIZE : -1)

static grid_u8 network;

// networks found by the last update of the grid, in the original raster order
#define MAX_NETWORKS 2048
static struct {
    int count;
    int id[MAX_NETWORKS];
    int size[MAX_NETWORKS];
    int owners[MAX_NETWORKS]; // bit per player owning at least one tile
} networks;
static int current_owners;

static struct {
    int items[MAX_QUEUE];
    int head;
    int tail;
} queue;

void map_road_network_clear(void)
{
    map_grid_clear_u8(network.items);
}

int map_road_network_get(int grid_offset)
{
    return network.items[grid_offset];
}

static int mark_road_network(int grid_offset, uint8_t network_id)
{
    memset(&queue, 0, sizeof(queue));
    int guard = 0;
    int next_offset;
    int size = 1;
    do {
        if (++guard >= GRID_SIZE * GRID_SIZE) {
            break;
        }
        network.items[grid_offset] = network_id;
        current_owners |= 1 << map_owner_get(grid_offset);
        next_offset = -1;
        for (int i = 0; i < 4; i++) {
            int new_offset = grid_offset + ADJACENT_OFFSETS(i);
            if (map_routing_citizen_is_passable(new_offset) && !network.items[new_offset]) {
                if (map_routing_citizen_is_road(new_offset) || map_terrain_is(new_offset, TERRAIN_ACCESS_RAMP)) {
                    network.items[new_offset] = network_id;
                    current_owners |= 1 << map_owner_get(new_offset);
                    size++;
                    if (next_offset == -1) {
                        next_offset = new_offset;
                    } else {
                        queue.items[queue.tail++] = new_offset;
                        if (queue.tail >= MAX_QUEUE) {
                            queue.tail = 0;
                        }
                    }
                }
            }
        }
        if (next_offset == -1) {
            if (queue.head == queue.tail) {
                return size;
            }
            next_offset = queue.items[queue.head++];
            if (queue.head >= MAX_QUEUE) {
                queue.head = 0;
            }
        }
        grid_offset = next_offset;
    } while (next_offset > -1);
    return size;
}

void map_road_network_update_grid(void)
{
    map_grid_clear_u8(network.items);
    networks.count = 0;
    int network_id = 1;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_ROAD) && !network.items[grid_offset]) {
                current_owners = 0;
                int size = mark_road_network(grid_offset, network_id);
                if (networks.count < MAX_NETWORKS) {
                    networks.id[networks.count] = network_id;
                    networks.size[networks.count] = size;
                    networks.owners[networks.count] = current_owners;
                    networks.count++;
                }
                network_id++;
            }
        }
    }
}

void map_road_network_update_largest(void)
{
    // the largest networks of the current city: those with at least one of its road tiles
    city_map_clear_largest_road_networks();
    int owner_bit = 1 << player_context_current_player;
    for (int i = 0; i < networks.count; i++) {
        if (networks.owners[i] & owner_bit) {
            city_map_add_to_largest_road_networks(networks.id[i], networks.size[i]);
        }
    }
}

void map_road_network_update(void)
{
    map_road_network_update_grid();
    map_road_network_update_largest();
}
