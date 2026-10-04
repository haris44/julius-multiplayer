#ifndef MAP_GRID_H
#define MAP_GRID_H

#include "core/buffer.h"

#include <stdint.h>

/**
 * Largest grid side. Grids always hold this many tiles; only the first GRID_SIZE * GRID_SIZE are used.
 */
#define GRID_MAX_SIZE 512
#define GRID_MAX_TILES (GRID_MAX_SIZE * GRID_MAX_SIZE)

/**
 * Side of the grid (stride between two rows of tiles), set when a map is loaded:
 * 162 for every classic map and saved game, larger for multiplayer maps.
 */
extern int map_grid_stride;
#define GRID_SIZE map_grid_stride

typedef struct {
    uint8_t items[GRID_MAX_TILES];
} grid_u8;

typedef struct {
    int8_t items[GRID_MAX_TILES];
} grid_i8;

typedef struct {
    uint16_t items[GRID_MAX_TILES];
} grid_u16;

typedef struct {
    int16_t items[GRID_MAX_TILES];
} grid_i16;

/**
 * Changes the grid side. All grids keep their tile indexes, so this is only valid before a map is loaded.
 * @param stride New side, at most GRID_MAX_SIZE
 */
void map_grid_set_stride(int stride);

/**
 * Compile-time encoding of a small (x, y) tile delta, for constant tables of offsets:
 * map_grid_pair_offset turns it into a grid offset for the current grid side
 */
#define GRID_PAIR(x, y) ((x) + 1024 * (y))
int map_grid_pair_offset(int pair);

void map_grid_init(int width, int height, int start_offset, int border_size);

int map_grid_is_valid_offset(int grid_offset);

int map_grid_offset(int x, int y);

int map_grid_offset_to_x(int grid_offset);

int map_grid_offset_to_y(int grid_offset);

int map_grid_delta(int x, int y);

/**
 * Adds the specified X and Y to the given offset with error checking
 * @return New grid offset, or -1 if the x/y would wrap around to a different row/column
 */
int map_grid_add_delta(int grid_offset, int x, int y);

int map_grid_direction_delta(int direction);

void map_grid_size(int *width, int *height);

int map_grid_width(void);

int map_grid_height(void);

void map_grid_bound(int *x, int *y);

void map_grid_bound_area(int *x_min, int *y_min, int *x_max, int *y_max);

void map_grid_get_area(int x, int y, int size, int radius,
                       int *x_min, int *y_min, int *x_max, int *y_max);

void map_grid_start_end_to_area(int x_start, int y_start, int x_end, int y_end,
                                int *x_min, int *y_min, int *x_max, int *y_max);

int map_grid_is_inside(int x, int y, int size);

const int *map_grid_adjacent_offsets(int size);


void map_grid_clear_u8(uint8_t *grid);

void map_grid_clear_i8(int8_t *grid);

void map_grid_clear_u16(uint16_t *grid);

void map_grid_clear_i16(int16_t *grid);

void map_grid_init_i8(int8_t *grid, int8_t value);

void map_grid_and_u8(uint8_t *grid, uint8_t mask);

void map_grid_and_u16(uint16_t *grid, uint16_t mask);

void map_grid_copy_u8(const uint8_t *src, uint8_t *dst);

void map_grid_copy_u16(const uint16_t *src, uint16_t *dst);


void map_grid_save_state_u8(const uint8_t *grid, buffer *buf);

void map_grid_save_state_i8(const int8_t *grid, buffer *buf);

void map_grid_save_state_u16(const uint16_t *grid, buffer *buf);

void map_grid_load_state_u8(uint8_t *grid, buffer *buf);

void map_grid_load_state_i8(int8_t *grid, buffer *buf);

void map_grid_load_state_u16(uint16_t *grid, buffer *buf);

#endif // MAP_GRID_H
