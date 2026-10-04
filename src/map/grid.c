#include "grid.h"

#include "map/data.h"

#include <string.h>

#define OFFSET(x,y) (x + GRID_SIZE * y)

struct map_data_t map_data;

int map_grid_stride = 162;

// Offsets are computed from the grid side; tables are refreshed when it changes
static const int DIRECTION_XY[8][2] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
};
static int DIRECTION_DELTA[8];

#define MAX_ADJACENT 21
static const int ADJACENT_XY[][MAX_ADJACENT][2] = {
    {{0, 0}},
    {{0,-1}, {1,0}, {0,1}, {-1,0}},
    {{0,-1}, {1,-1}, {2,0}, {2,1}, {1,2}, {0,2}, {-1,1}, {-1,0}},
    {
        {0,-1}, {1,-1}, {2,-1},
        {3,0}, {3,1}, {3,2},
        {2,3}, {1,3}, {0,3},
        {-1,2}, {-1,1}, {-1,0}
    },
    {
        {0,-1}, {1,-1}, {2,-1}, {3,-1},
        {4,0}, {4,1}, {4,2}, {4,3},
        {3,4}, {2,4}, {1,4}, {0,4},
        {-1,3}, {-1,2}, {-1,1}, {-1,0}
    },
    {
        {0,-1}, {1,-1}, {2,-1}, {3,-1}, {4,-1},
        {5,0}, {5,1}, {5,2}, {5,3}, {5,4},
        {4,5}, {3,5}, {2,5}, {1,5}, {0,5},
        {-1,4}, {-1,3}, {-1,2}, {-1,1}, {-1,0}
    },
};
static const int ADJACENT_COUNT[] = {1, 4, 8, 12, 16, 20};
static int ADJACENT_OFFSETS[6][MAX_ADJACENT];
static int tables_stride;

static void update_tables(void)
{
    if (tables_stride == map_grid_stride) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        DIRECTION_DELTA[i] = OFFSET(DIRECTION_XY[i][0], DIRECTION_XY[i][1]);
    }
    for (int size = 0; size < 6; size++) {
        for (int i = 0; i < MAX_ADJACENT; i++) {
            // each list ends with 0, as the original constant tables
            ADJACENT_OFFSETS[size][i] = i < ADJACENT_COUNT[size] && size > 0 ?
                OFFSET(ADJACENT_XY[size][i][0], ADJACENT_XY[size][i][1]) : 0;
        }
    }
    tables_stride = map_grid_stride;
}

void map_grid_set_stride(int stride)
{
    if (stride > 0 && stride <= GRID_MAX_SIZE) {
        map_grid_stride = stride;
        update_tables();
    }
}

void map_grid_init(int width, int height, int start_offset, int border_size)
{
    // the grid side is the map width plus its border: 162 for classic maps
    map_grid_set_stride(width + border_size);
    map_data.width = width;
    map_data.height = height;
    map_data.start_offset = start_offset;
    map_data.border_size = border_size;
}

int map_grid_is_valid_offset(int grid_offset)
{
    return grid_offset >= 0 && grid_offset < GRID_SIZE * GRID_SIZE;
}

int map_grid_offset(int x, int y)
{
    return map_data.start_offset + x + y * GRID_SIZE;
}

int map_grid_offset_to_x(int grid_offset)
{
    return (grid_offset - map_data.start_offset) % GRID_SIZE;
}

int map_grid_offset_to_y(int grid_offset)
{
    return (grid_offset - map_data.start_offset) / GRID_SIZE;
}

int map_grid_delta(int x, int y)
{
    return y * GRID_SIZE + x;
}

int map_grid_add_delta(int grid_offset, int x, int y)
{
    int raw_x = grid_offset % GRID_SIZE;
    int raw_y = grid_offset / GRID_SIZE;
    if (raw_x + x < 0 || raw_x + x >= GRID_SIZE ||
        raw_y + y < 0 || raw_y + y >= GRID_SIZE) {
        return -1;
    }
    return grid_offset + map_grid_delta(x, y);
}

int map_grid_direction_delta(int direction)
{
    if (direction >= 0 && direction < 8) {
        update_tables();
        return DIRECTION_DELTA[direction];
    } else {
        return 0;
    }
}

void map_grid_size(int *width, int *height)
{
    *width = map_data.width;
    *height = map_data.height;
}

int map_grid_width(void)
{
    return map_data.width;
}

int map_grid_height(void)
{
    return map_data.height;
}

void map_grid_bound(int *x, int *y)
{
    if (*x < 0) {
        *x = 0;
    }
    if (*y < 0) {
        *y = 0;
    }
    if (*x >= map_data.width) {
        *x = map_data.width - 1;
    }
    if (*y >= map_data.height) {
        *y = map_data.height - 1;
    }
}

void map_grid_bound_area(int *x_min, int *y_min, int *x_max, int *y_max)
{
    if (*x_min < 0) {
        *x_min = 0;
    }
    if (*y_min < 0) {
        *y_min = 0;
    }
    if (*x_max >= map_data.width) {
        *x_max = map_data.width - 1;
    }
    if (*y_max >= map_data.height) {
        *y_max = map_data.height - 1;
    }
}

void map_grid_get_area(int x, int y, int size, int radius,
                       int *x_min, int *y_min, int *x_max, int *y_max)
{
    *x_min = x - radius;
    *y_min = y - radius;
    *x_max = x + size + radius - 1;
    *y_max = y + size + radius - 1;
    map_grid_bound_area(x_min, y_min, x_max, y_max);
}

void map_grid_start_end_to_area(
    int x_start, int y_start, int x_end, int y_end, int *x_min, int *y_min, int *x_max, int *y_max)
{
    if (x_start < x_end) {
        *x_min = x_start;
        *x_max = x_end;
    } else {
        *x_min = x_end;
        *x_max = x_start;
    }
    if (y_start < y_end) {
        *y_min = y_start;
        *y_max = y_end;
    } else {
        *y_min = y_end;
        *y_max = y_start;
    }
    map_grid_bound_area(x_min, y_min, x_max, y_max);
}

int map_grid_is_inside(int x, int y, int size)
{
    return x >= 0 && x + size <= map_data.width && y >= 0 && y + size <= map_data.height;
}

const int *map_grid_adjacent_offsets(int size)
{
    update_tables();
    return ADJACENT_OFFSETS[size];
}

void map_grid_clear_i8(int8_t *grid)
{
    memset(grid, 0, GRID_SIZE * GRID_SIZE * sizeof(int8_t));
}

void map_grid_clear_u8(uint8_t *grid)
{
    memset(grid, 0, GRID_SIZE * GRID_SIZE * sizeof(uint8_t));
}

void map_grid_clear_u16(uint16_t *grid)
{
    memset(grid, 0, GRID_SIZE * GRID_SIZE * sizeof(uint16_t));
}

void map_grid_clear_i16(int16_t *grid)
{
    memset(grid, 0, GRID_SIZE * GRID_SIZE * sizeof(int16_t));
}

void map_grid_init_i8(int8_t *grid, int8_t value)
{
    memset(grid, value, GRID_SIZE * GRID_SIZE * sizeof(int8_t));
}

void map_grid_and_u8(uint8_t *grid, uint8_t mask)
{
    for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
        grid[i] &= mask;
    }
}

void map_grid_and_u16(uint16_t *grid, uint16_t mask)
{
    for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
        grid[i] &= mask;
    }
}

void map_grid_copy_u8(const uint8_t *src, uint8_t *dst)
{
    memcpy(dst, src, GRID_SIZE * GRID_SIZE * sizeof(uint8_t));
}

void map_grid_copy_u16(const uint16_t *src, uint16_t *dst)
{
    memcpy(dst, src, GRID_SIZE * GRID_SIZE * sizeof(uint16_t));
}

void map_grid_save_state_u8(const uint8_t *grid, buffer *buf)
{
    buffer_write_raw(buf, grid, GRID_SIZE * GRID_SIZE);
}

void map_grid_save_state_i8(const int8_t *grid, buffer *buf)
{
    buffer_write_raw(buf, grid, GRID_SIZE * GRID_SIZE);
}

void map_grid_save_state_u16(const uint16_t *grid, buffer *buf)
{
    for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
        buffer_write_u16(buf, grid[i]);
    }
}

void map_grid_load_state_u8(uint8_t *grid, buffer *buf)
{
    buffer_read_raw(buf, grid, GRID_SIZE * GRID_SIZE);
}

void map_grid_load_state_i8(int8_t *grid, buffer *buf)
{
    buffer_read_raw(buf, grid, GRID_SIZE * GRID_SIZE);
}

void map_grid_load_state_u16(uint16_t *grid, buffer *buf)
{
    for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
        grid[i] = buffer_read_u16(buf);
    }
}

int map_grid_pair_offset(int pair)
{
    int y = ((pair + 512 + 1024 * 64) >> 10) - 64;
    int x = pair - 1024 * y;
    return x + GRID_SIZE * y;
}

